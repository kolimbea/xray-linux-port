// Launcher logic without UI: installed versions, GitHub releases, download/verify/unpack, the Mod
// Organizer 2 overlay of a GAMMA install (as tools/make_fixture_gamma.py), starting the game.
// Everything lives in $XDG_DATA_HOME/xray-native (default ~/.local/share/xray-native).
#pragma once
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <vector>

namespace launcher
{
    namespace fs = std::filesystem;

    constexpr const char *kRepo = "kolimbea/xray-linux-port";
    constexpr const char *kAssetSuffix = "-linux-x86_64.tar.gz";

    struct Release
    {
        std::string tag, name, notes, asset_name, asset_url, sha_url, published;
        uint64_t size = 0;
        bool prerelease = false;
    };

    struct Config
    {
        std::string anomaly_dir; // Anomaly root (fsgame.ltx, db/, gamedata/)
        std::string mo2_dir;     // Mod Organizer 2 instance (mods/, overwrite/, profiles/)
        std::string profile = "G.A.M.M.A";
        std::string version;     // installed tag to run
        std::string overlay_stamp; // modlist.txt size+mtime when the overlay was built
        std::string lang = "auto"; // launcher UI language: auto (from LANG), en, ru
    };

    // A background job: the worker writes, the UI reads (status/error under the mutex).
    struct Task
    {
        std::atomic<bool> running{false};
        std::atomic<bool> ok{false};
        std::atomic<float> progress{0.f}; // 0..1, <0 = unknown
        std::mutex lock;
        std::string status, error;
        // download in progress: the UI polls the file size
        std::string download_path;
        uint64_t download_size = 0;

        void set_status(const std::string &s)
        {
            std::lock_guard<std::mutex> g(lock);
            status = s;
        }
        void fail(const std::string &e)
        {
            std::lock_guard<std::mutex> g(lock);
            error = e;
            ok = false;
        }
    };

    fs::path data_dir();
    fs::path game_dir();
    fs::path versions_dir();
    Config load_config();
    void save_config(const Config &c);

    // Finds Anomaly and the MO2 instance from a chosen folder: a root with anomaly/ + gamma/
    // (stalker-gamma-cli), or either folder itself; the other one is taken from MO2's gamePath, the
    // neighbouring folders or the configuration. Fills what it found into `c` even when it returns
    // false (with a reason), so the two folders can be chosen one after the other.
    bool detect_gamma(const fs::path &root, Config &c, std::string &error);
    bool overlay_needed(const Config &c);

    std::vector<std::string> installed_versions(); // newest first
    // Registers the package the launcher runs from (bin/../VERSION) if it is not installed yet.
    std::string adopt_running_package();

    // Workers (run them on a std::thread; they set task.ok/running at the end).
    void fetch_releases(Task &task, std::vector<Release> &out);
    void install_release(Task &task, Release release, Config &config);
    void build_overlay(Task &task, Config &config);

    // Starts the game of config.version in game_dir() and waits for it; returns the exit status.
    int run_game(const Config &config);
    // Text of the newest logs/crash_*.log written after `since` (empty if none).
    std::string crash_report_since(int64_t since);
    // ~/.local/share/applications entry that starts the installed launcher.
    bool install_desktop_entry();
    bool version_newer(const std::string &a, const std::string &b); // a > b, "v0.2.1" style tags
} // namespace launcher
