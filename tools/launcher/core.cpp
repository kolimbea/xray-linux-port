#include "core.h"

#include "json.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>
#include <sys/wait.h>
#include <unistd.h>

namespace launcher
{
    namespace
    {
        std::string lower(std::string s)
        {
            for (char &c : s)
                c = char(std::tolower((unsigned char)c));
            return s;
        }

        // Single-quoted for /bin/sh.
        std::string sh(const std::string &s)
        {
            std::string r = "'";
            for (char c : s)
                r += c == '\'' ? std::string("'\\''") : std::string(1, c);
            return r + "'";
        }

        bool run(const std::string &cmd, std::string *out = nullptr)
        {
            FILE *p = popen(cmd.c_str(), "r");
            if (!p)
                return false;
            char buf[65536];
            for (size_t n; (n = fread(buf, 1, sizeof(buf), p)) > 0;)
                if (out)
                    out->append(buf, n);
            return pclose(p) == 0;
        }

        std::string read_text(const fs::path &p)
        {
            std::ifstream f(p, std::ios::binary);
            std::stringstream s;
            s << f.rdbuf();
            return s.str();
        }

        std::string modlist_stamp(const Config &c)
        {
            std::error_code ec;
            const fs::path m = fs::path(c.mo2_dir) / "profiles" / c.profile / "modlist.txt";
            const auto size = fs::file_size(m, ec);
            if (ec)
                return {};
            const auto t = fs::last_write_time(m, ec).time_since_epoch().count();
            return std::to_string(size) + ":" + std::to_string(t);
        }

        fs::path exe_dir()
        {
            std::error_code ec;
            return fs::read_symlink("/proc/self/exe", ec).parent_path();
        }
    } // namespace

    fs::path data_dir()
    {
        const char *x = getenv("XDG_DATA_HOME");
        const fs::path base = x && *x ? fs::path(x) : fs::path(getenv("HOME") ? getenv("HOME") : ".") / ".local" / "share";
        return base / "xray-native";
    }
    fs::path game_dir() { return data_dir() / "game"; }
    fs::path versions_dir() { return data_dir() / "versions"; }

    Config load_config()
    {
        Config c;
        std::ifstream f(data_dir() / "launcher.ini");
        std::string line;
        while (std::getline(f, line))
        {
            const size_t eq = line.find('=');
            if (eq == std::string::npos)
                continue;
            const std::string k = line.substr(0, eq), v = line.substr(eq + 1);
            if (k == "anomaly")
                c.anomaly_dir = v;
            else if (k == "mo2")
                c.mo2_dir = v;
            else if (k == "profile")
                c.profile = v;
            else if (k == "version")
                c.version = v;
            else if (k == "overlay")
                c.overlay_stamp = v;
            else if (k == "lang")
                c.lang = v;
        }
        return c;
    }

    void save_config(const Config &c)
    {
        fs::create_directories(data_dir());
        std::ofstream f(data_dir() / "launcher.ini");
        f << "anomaly=" << c.anomaly_dir << "\nmo2=" << c.mo2_dir << "\nprofile=" << c.profile << "\nversion=" << c.version
          << "\noverlay=" << c.overlay_stamp << "\nlang=" << c.lang << "\n";
    }

    namespace
    {
        // A Windows path from MO2's ModOrganizer.ini ("@ByteArray(Z:\\home\\me\\anomaly)") as a Linux
        // path: Z: is the root under Wine; other drive letters through the dosdevices links of the
        // Wine prefix the MO2 instance lives in. Empty if it cannot be mapped.
        fs::path wine_path(std::string v, const fs::path &mo2)
        {
            const std::string pre = "@ByteArray(";
            if (v.compare(0, pre.size(), pre) == 0 && !v.empty() && v.back() == ')')
                v = v.substr(pre.size(), v.size() - pre.size() - 1);
            std::string out;
            for (size_t i = 0; i < v.size(); ++i)
            {
                if (v[i] == '\\')
                {
                    if (i + 1 < v.size() && v[i + 1] == '\\')
                        ++i;
                    out += '/';
                }
                else
                    out += v[i];
            }
            if (out.size() < 2 || out[1] != ':')
                return out.empty() || out[0] != '/' ? fs::path() : fs::path(out);
            const char drive = char(tolower(out[0]));
            const std::string rest = out.substr(2);
            if (drive == 'z')
                return fs::path(rest.empty() ? "/" : rest);
            std::error_code ec;
            for (fs::path p = mo2; !p.empty() && p != p.parent_path(); p = p.parent_path())
                if (fs::is_directory(p / "dosdevices", ec))
                {
                    const fs::path dev = p / "dosdevices" / (std::string(1, drive) + ":");
                    if (fs::exists(dev, ec))
                        return fs::canonical(dev, ec) / fs::path(rest).relative_path();
                    break;
                }
            return {};
        }

        // gamePath of the MO2 instance (the Anomaly it was set up for), mapped to Linux
        fs::path mo2_game_path(const fs::path &mo2)
        {
            std::ifstream f(mo2 / "ModOrganizer.ini");
            std::string line;
            while (std::getline(f, line))
            {
                if (!line.empty() && line.back() == '\r')
                    line.pop_back();
                if (line.compare(0, 9, "gamePath=") == 0)
                    return wine_path(line.substr(9), mo2);
            }
            return {};
        }
    } // namespace

    bool detect_gamma(const fs::path &root, Config &c, std::string &error)
    {
        std::error_code ec;
        auto is_anomaly = [&](const fs::path &p) { return !p.empty() && fs::exists(p / "fsgame.ltx", ec) && fs::is_directory(p / "db", ec); };
        auto is_mo2 = [&](const fs::path &p) { return !p.empty() && fs::is_directory(p / "mods", ec) && fs::is_directory(p / "profiles", ec); };
        auto first = [](const std::vector<fs::path> &list, auto &&pred) {
            for (const fs::path &p : list)
                if (pred(p))
                    return p;
            return fs::path();
        };
        const fs::path up = root.parent_path();
        // the chosen folder: a root holding both (stalker-gamma-cli: anomaly/ + gamma/), or one of them
        fs::path anomaly = first({root / "anomaly", root / "Anomaly", root}, is_anomaly);
        fs::path mo2 = first({root / "gamma", root / "GAMMA", root}, is_mo2);
        // the other one: where MO2 says its game is, next to the chosen folder, or chosen earlier
        if (!mo2.empty() && anomaly.empty())
            anomaly = first({mo2_game_path(mo2), up / "anomaly", up / "Anomaly", fs::path(c.anomaly_dir)}, is_anomaly);
        if (!anomaly.empty() && mo2.empty())
            mo2 = first({up / "gamma", up / "GAMMA", up / "G.A.M.M.A", fs::path(c.mo2_dir)}, is_mo2);
        // remembered even when the pair is incomplete, so the two folders can be chosen one after the other
        if (!anomaly.empty())
            c.anomaly_dir = anomaly.string();
        if (!mo2.empty())
            c.mo2_dir = mo2.string();
        if (anomaly.empty() && mo2.empty())
            return error = "Neither Anomaly (a folder with fsgame.ltx and db/) nor GAMMA's Mod Organizer 2 folder "
                           "(with mods/ and profiles/) is in " + root.string(),
                   false;
        if (anomaly.empty())
            return error = "GAMMA found in " + mo2.string() + ". Now choose the Anomaly folder (with fsgame.ltx and db/).", false;
        if (mo2.empty())
            return error = "Anomaly found in " + anomaly.string() + ". Now choose the GAMMA folder (with mods/ and profiles/).", false;
        std::string profile = c.profile;
        if (!fs::exists(mo2 / "profiles" / profile / "modlist.txt", ec))
        {
            profile.clear();
            for (const auto &e : fs::directory_iterator(mo2 / "profiles", ec))
                if (fs::exists(e.path() / "modlist.txt", ec))
                {
                    profile = e.path().filename().string();
                    break;
                }
            if (profile.empty())
                return error = "no MO2 profile with a modlist.txt in " + (mo2 / "profiles").string(), false;
        }
        c.profile = profile;
        return true;
    }

    bool overlay_needed(const Config &c)
    {
        std::error_code ec;
        return !fs::exists(game_dir() / "fsgame.ltx", ec) || c.overlay_stamp != modlist_stamp(c);
    }

    bool version_newer(const std::string &a, const std::string &b)
    {
        auto nums = [](const std::string &v)
        {
            std::vector<long> r;
            long cur = -1;
            for (char ch : v)
            {
                if (std::isdigit((unsigned char)ch))
                    cur = (cur < 0 ? 0 : cur) * 10 + (ch - '0');
                else if (cur >= 0)
                    r.push_back(cur), cur = -1;
            }
            if (cur >= 0)
                r.push_back(cur);
            return r;
        };
        return nums(a) > nums(b);
    }

    std::vector<std::string> installed_versions()
    {
        std::vector<std::string> v;
        std::error_code ec;
        for (const auto &e : fs::directory_iterator(versions_dir(), ec))
            if (fs::exists(e.path() / "bin" / "xray_vk", ec))
                v.push_back(e.path().filename().string());
        std::sort(v.begin(), v.end(), [](const std::string &a, const std::string &b) { return version_newer(a, b); });
        return v;
    }

    std::string adopt_running_package()
    {
        std::error_code ec;
        const fs::path pkg = exe_dir().parent_path();
        if (!fs::exists(pkg / "VERSION", ec) || !fs::exists(pkg / "bin" / "xray_vk", ec))
            return {};
        std::string tag = read_text(pkg / "VERSION");
        tag.erase(std::remove_if(tag.begin(), tag.end(), [](char c) { return std::isspace((unsigned char)c); }), tag.end());
        if (tag.empty())
            return {};
        const fs::path dst = versions_dir() / tag;
        if (fs::equivalent(pkg, dst, ec) || fs::exists(dst / "bin" / "xray_vk", ec))
            return tag;
        fs::create_directories(versions_dir());
        fs::copy(pkg, dst, fs::copy_options::recursive | fs::copy_options::copy_symlinks, ec);
        return ec ? std::string() : tag;
    }

    void fetch_releases(Task &task, std::vector<Release> &out)
    {
        task.set_status("checking for updates");
        std::string body;
        const std::string url = std::string("https://api.github.com/repos/") + kRepo + "/releases?per_page=20";
        if (!run("curl -sSfL --max-time 30 -H 'Accept: application/vnd.github+json' " + sh(url) + " 2>&1", &body))
        {
            task.fail("cannot reach GitHub: " + body.substr(0, 200));
            task.running = false;
            return;
        }
        const json::Value list = json::parse(body);
        std::vector<Release> found;
        for (const json::Value &r : list.a)
        {
            if (r["draft"].b)
                continue;
            Release rel;
            rel.tag = r["tag_name"].str();
            rel.name = r["name"].str();
            rel.notes = r["body"].str();
            rel.published = r["published_at"].str().substr(0, 10);
            rel.prerelease = r["prerelease"].b;
            for (const json::Value &a : r["assets"].a)
            {
                const std::string n = a["name"].str();
                if (n.size() > strlen(kAssetSuffix) && n.compare(n.size() - strlen(kAssetSuffix), std::string::npos, kAssetSuffix) == 0)
                {
                    rel.asset_name = n;
                    rel.asset_url = a["browser_download_url"].str();
                    rel.size = uint64_t(a["size"].n);
                }
            }
            for (const json::Value &a : r["assets"].a)
                if (a["name"].str() == rel.asset_name + ".sha256")
                    rel.sha_url = a["browser_download_url"].str();
            if (!rel.asset_url.empty())
                found.push_back(rel);
        }
        std::sort(found.begin(), found.end(), [](const Release &a, const Release &b) { return version_newer(a.tag, b.tag); });
        out = found;
        task.ok = true;
        task.running = false;
    }

    void install_release(Task &task, Release rel, Config &config)
    {
        const fs::path dl = data_dir() / "downloads";
        fs::create_directories(dl);
        const fs::path file = dl / rel.asset_name;
        {
            std::lock_guard<std::mutex> g(task.lock);
            task.download_path = file.string();
            task.download_size = rel.size;
            task.status = "downloading " + rel.tag;
        }
        std::string err;
        if (!run("curl -sSfL --retry 3 -o " + sh(file.string()) + " " + sh(rel.asset_url) + " 2>&1", &err))
            return task.fail("download failed: " + err.substr(0, 200)), task.running = false, void();
        {
            std::lock_guard<std::mutex> g(task.lock);
            task.download_path.clear();
        }
        task.progress = -1.f;
        if (!rel.sha_url.empty())
        {
            task.set_status("verifying");
            std::string want, got;
            if (!run("curl -sSfL " + sh(rel.sha_url), &want) || !run("sha256sum " + sh(file.string()), &got) ||
                want.substr(0, 64) != got.substr(0, 64))
                return task.fail("checksum mismatch: the download is damaged"), task.running = false, void();
        }
        task.set_status("unpacking");
        const fs::path tmp = versions_dir() / (".unpack-" + rel.tag);
        std::error_code ec;
        fs::remove_all(tmp, ec);
        fs::create_directories(tmp);
        if (!run("tar -xzf " + sh(file.string()) + " -C " + sh(tmp.string()) + " 2>&1", &err))
            return task.fail("unpacking failed: " + err.substr(0, 200)), task.running = false, void();
        const fs::path root = fs::exists(tmp / "xray-native") ? tmp / "xray-native" : tmp;
        const fs::path dst = versions_dir() / rel.tag;
        fs::remove_all(dst, ec);
        fs::rename(root, dst, ec);
        fs::remove_all(tmp, ec);
        fs::remove(file, ec);
        if (ec || !fs::exists(dst / "bin" / "xray_vk"))
            return task.fail("the package has no bin/xray_vk"), task.running = false, void();
        // the launcher updates itself: the installed copy is the one the desktop entry starts
        fs::copy_file(dst / "bin" / "xray-launcher", data_dir() / "xray-launcher.new", fs::copy_options::overwrite_existing, ec);
        if (!ec)
            fs::rename(data_dir() / "xray-launcher.new", data_dir() / "xray-launcher", ec);
        // keep the newest two versions (rollback), drop older ones
        const auto all = installed_versions();
        for (size_t i = 2; i < all.size(); ++i)
            if (all[i] != rel.tag)
                fs::remove_all(versions_dir() / all[i], ec);
        config.version = rel.tag;
        save_config(config);
        task.set_status("installed " + rel.tag);
        task.ok = true;
        task.running = false;
    }

    void build_overlay(Task &task, Config &config)
    {
        task.set_status("reading the mod list");
        task.progress = -1.f;
        const fs::path mo2 = config.mo2_dir, profile = mo2 / "profiles" / config.profile;
        std::vector<std::pair<std::string, fs::path>> layers{{"anomaly", config.anomaly_dir}};
        {
            std::ifstream f(profile / "modlist.txt");
            std::vector<std::string> mods;
            std::string line;
            while (std::getline(f, line))
            {
                if (!line.empty() && line.back() == '\r')
                    line.pop_back();
                if (line.size() > 1 && line[0] == '+' && !(line.size() >= 10 && line.compare(line.size() - 10, 10, "_separator") == 0))
                    mods.push_back(line.substr(1));
            }
            std::reverse(mods.begin(), mods.end()); // modlist.txt: highest priority first
            for (const auto &m : mods)
                layers.emplace_back(m, mo2 / "mods" / m);
            layers.emplace_back("overwrite", mo2 / "overwrite");
        }
        task.set_status("scanning " + std::to_string(layers.size()) + " layers");
        std::map<std::string, fs::path> winners, appdata; // lower-case relative path -> source
        std::error_code ec;
        for (size_t li = 0; li < layers.size(); ++li)
        {
            task.progress = 0.5f * float(li) / float(layers.size());
            const fs::path &base = layers[li].second;
            for (const auto &top : fs::directory_iterator(base, ec))
            {
                const std::string t = lower(top.path().filename().string());
                const bool is_appdata = t == "appdata";
                if (t != "db" && t != "gamedata" && !is_appdata)
                    continue;
                for (auto it = fs::recursive_directory_iterator(top.path(), fs::directory_options::follow_directory_symlink, ec);
                     it != fs::recursive_directory_iterator(); it.increment(ec))
                {
                    if (ec)
                        break;
                    const std::string rel = lower(fs::relative(it->path(), base, ec).string());
                    if (is_appdata && it.depth() == 0 && it->is_directory(ec))
                    {
                        const std::string sub = lower(it->path().filename().string());
                        if (sub == "savedgames" || sub == "logs" || sub == "shaders_cache" || sub == "screenshots" || sub == "reshade-shaders")
                        {
                            it.disable_recursion_pending();
                            continue;
                        }
                    }
                    if (!it->is_regular_file(ec))
                        continue;
                    (is_appdata ? appdata : winners)[rel] = it->path();
                }
            }
        }
        const fs::path game = game_dir();
        fs::create_directories(game);
        task.set_status("linking " + std::to_string(winners.size()) + " files");
        for (const char *d : {"db", "gamedata"})
            fs::remove_all(game / d, ec);
        // files the game writes inside gamedata (MO2 sends those writes to overwrite/): copies, kept
        static const char *const writable[] = {"gamedata/configs/axr_options.ltx"};
        const fs::path keep = data_dir() / "kept";
        size_t done = 0;
        for (const auto &[rel, src] : winners)
        {
            if (++done % 2000 == 0)
                task.progress = 0.5f + 0.45f * float(done) / float(winners.size());
            const fs::path dst = game / rel;
            fs::create_directories(dst.parent_path(), ec);
            if (std::find(std::begin(writable), std::end(writable), rel) != std::end(writable))
            {
                // the player's copy survives rebuilds (MCM settings live in axr_options.ltx)
                if (fs::exists(keep / rel, ec))
                    fs::copy_file(keep / rel, dst, fs::copy_options::overwrite_existing, ec);
                else
                    fs::copy_file(src, dst, fs::copy_options::overwrite_existing, ec);
            }
            else
                fs::create_symlink(src, dst, ec);
        }
        for (const auto &[rel, src] : appdata) // copied once; the player's settings are never overwritten
        {
            const fs::path dst = game / rel;
            if (fs::exists(dst, ec))
                continue;
            fs::create_directories(dst.parent_path(), ec);
            fs::copy_file(src, dst, ec);
        }
        for (const char *d : {"appdata/logs", "appdata/savedgames", "appdata/screenshots"})
            fs::create_directories(game / d, ec);
        if (!fs::exists(game / "fsgame.ltx", ec))
            fs::copy_file(fs::path(config.anomaly_dir) / "fsgame.ltx", game / "fsgame.ltx", ec);
        config.overlay_stamp = modlist_stamp(config);
        save_config(config);
        task.progress = 1.f;
        task.set_status("ready: " + std::to_string(winners.size()) + " files from " + std::to_string(layers.size()) + " layers");
        task.ok = true;
        task.running = false;
    }

    int run_game(const Config &config)
    {
        const fs::path ver = versions_dir() / config.version;
        // the player's copy of the writable gamedata files is saved after each session
        const pid_t pid = fork();
        if (pid == 0)
        {
            if (chdir(game_dir().c_str()) != 0)
                _exit(127);
            const std::string lib = (ver / "lib").string();
            const char *old = getenv("LD_LIBRARY_PATH");
            setenv("LD_LIBRARY_PATH", (old && *old ? lib + ":" + old : lib).c_str(), 1);
            const std::string exe = (ver / "bin" / "xray_vk").string();
            execl(exe.c_str(), exe.c_str(), "-nointro", (char *)nullptr);
            _exit(127);
        }
        if (pid < 0)
            return -1;
        int status = 0;
        waitpid(pid, &status, 0);
        std::error_code ec;
        const fs::path kept = data_dir() / "kept" / "gamedata" / "configs";
        fs::create_directories(kept, ec);
        fs::copy_file(game_dir() / "gamedata" / "configs" / "axr_options.ltx", kept / "axr_options.ltx",
                      fs::copy_options::overwrite_existing, ec);
        return WIFEXITED(status) ? WEXITSTATUS(status) : 128 + WTERMSIG(status);
    }

    std::string crash_report_since(int64_t since)
    {
        std::error_code ec;
        fs::path newest;
        int64_t best = since;
        for (const auto &e : fs::directory_iterator(game_dir() / "appdata" / "logs", ec))
        {
            const std::string n = e.path().filename().string();
            if (n.rfind("crash_", 0) != 0)
                continue;
            const int64_t t = atoll(n.c_str() + 6);
            if (t >= best)
                best = t, newest = e.path();
        }
        if (newest.empty())
            return {};
        std::string text = read_text(newest);
        if (text.size() > 6000)
            text = text.substr(0, 6000) + "\n...";
        return newest.string() + "\n\n" + text;
    }

    bool install_desktop_entry()
    {
        std::error_code ec;
        const fs::path self = data_dir() / "xray-launcher";
        if (!fs::exists(self, ec))
            fs::copy_file(fs::read_symlink("/proc/self/exe", ec), self, fs::copy_options::overwrite_existing, ec);
        const char *home = getenv("HOME");
        if (!home)
            return false;
        const fs::path apps = fs::path(home) / ".local" / "share" / "applications";
        fs::create_directories(apps, ec);
        std::ofstream f(apps / "xray-native.desktop");
        f << "[Desktop Entry]\nType=Application\nName=S.T.A.L.K.E.R. GAMMA (native Linux)\nComment=X-Ray engine on Vulkan: "
             "launcher and updates\nExec=" << self.string() << "\nIcon=applications-games\nTerminal=false\nCategories=Game;\n";
        return f.good();
    }
} // namespace launcher
