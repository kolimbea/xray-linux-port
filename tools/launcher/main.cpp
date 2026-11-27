// X-Ray native launcher: installs and updates the Linux build from GitHub releases, builds the Mod
// Organizer 2 overlay of the player's GAMMA install and starts the game. SDL3 + SDL_Renderer + Dear ImGui.
#include "core.h"

#include <SDL3/SDL.h>

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"

#include <algorithm>
#include <cstdio>
#include <functional>
#include <mutex>
#include <cstring>
#include <ctime>
#include <map>
#include <string>
#include <thread>
#include <vector>

using namespace launcher;

namespace
{
    // ---- text: English or Russian (Settings; "auto" follows LANG), Russian only with a Cyrillic font ----
    bool g_ru = false;
    bool g_cyrillic = false; // a font with Cyrillic was found
    void apply_lang(const std::string &lang)
    {
        bool ru = lang == "ru";
        if (lang != "en" && lang != "ru")
        {
            // auto: Russian for the languages whose speakers usually read it, English for all others
            const char *l = getenv("LC_ALL");
            if (!l || !*l)
                l = getenv("LC_MESSAGES");
            if (!l || !*l)
                l = getenv("LANG");
            ru = false;
            for (const char *p : {"ru", "uk", "be", "kk"})
                ru = ru || (l && !strncmp(l, p, 2));
        }
        g_ru = ru && g_cyrillic;
    }
    const char *tr(const char *key)
    {
        static const std::map<std::string, std::pair<const char *, const char *>> t = {
            {"play", {"PLAY", "ИГРАТЬ"}},
            {"playing", {"The game is running", "Игра запущена"}},
            {"updates", {"Updates", "Обновления"}},
            {"settings", {"Settings", "Настройки"}},
            {"home", {"Game", "Игра"}},
            {"version", {"Version", "Версия"}},
            {"none", {"not installed", "не установлена"}},
            {"check", {"Check for updates", "Проверить обновления"}},
            {"install", {"Install", "Установить"}},
            {"installed", {"installed", "установлена"}},
            {"current", {"current", "текущая"}},
            {"use", {"Use this version", "Использовать"}},
            {"newer", {"Update available: ", "Доступно обновление: "}},
            {"gamma", {"GAMMA folder", "Папка GAMMA"}},
            {"choose", {"Choose...", "Выбрать..."}},
            {"need_gamma", {"Settings: choose the GAMMA folder (Mod Organizer 2 with mods/ and profiles/), or the folder that holds "
                            "both anomaly/ and gamma/. A separate Anomaly folder is found automatically or can be chosen next.",
                            "Настройки: выберите папку GAMMA (Mod Organizer 2 с mods/ и profiles/) или папку, где лежат anomaly/ и "
                            "gamma/. Отдельная папка Anomaly найдётся сама или её можно выбрать следующей."}},
            {"overlay", {"Rebuild mod overlay", "Пересобрать моды"}},
            {"overlay_need", {"The mod list changed: the overlay is rebuilt before the game starts.",
                              "Список модов изменился: перед запуском моды будут пересобраны."}},
            {"logs", {"Open logs folder", "Открыть папку логов"}},
            {"saves", {"Open saves folder", "Открыть папку сейвов"}},
            {"desktop", {"Add to applications menu", "Добавить в меню приложений"}},
            {"crash", {"The game crashed. The report below helps to fix it — attach it to a bug report.",
                       "Игра вылетела. Отчёт ниже помогает это исправить — приложите его к сообщению об ошибке."}},
            {"copy", {"Copy report", "Скопировать отчёт"}},
            {"close", {"Close", "Закрыть"}},
            {"alpha", {"Alpha: expect bugs. Your GAMMA install is never modified.",
                       "Альфа: возможны ошибки. Установка GAMMA не изменяется."}},
            {"no_releases", {"No releases found yet.", "Релизов пока нет."}},
            {"profile", {"MO2 profile", "Профиль MO2"}},
            {"lang", {"Language / Язык", "Язык / Language"}},
            {"lang_auto", {"Automatic (system)", "Автоматически (как в системе)"}},
        };
        auto it = t.find(key);
        return it == t.end() ? key : (g_ru ? it->second.second : it->second.first);
    }

    std::string fc_match(const char *pattern)
    {
        std::string path;
        if (FILE *p = popen((std::string("fc-match -f '%{file}' '") + pattern + "' 2>/dev/null").c_str(), "r"))
        {
            char buf[1024] = {};
            if (fgets(buf, sizeof(buf), p))
                path = buf;
            pclose(p);
        }
        return !path.empty() && fs::exists(path) ? path : std::string();
    }

    // A font with Cyrillic (Russian UI); `cyrillic` reports whether one was found. The engine's ImGui has
    // no embedded font, so some system font is always needed.
    std::string find_font(bool &cyrillic)
    {
        cyrillic = true;
        std::string path = fc_match("sans:lang=ru");
        if (!path.empty())
            return path;
        for (const char *f : {"/usr/share/fonts/google-noto/NotoSans-Regular.ttf", "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf",
                              "/usr/share/fonts/noto/NotoSans-Regular.ttf", "/usr/share/fonts/dejavu-sans-fonts/DejaVuSans.ttf",
                              "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", "/usr/share/fonts/TTF/DejaVuSans.ttf"})
            if (fs::exists(f))
                return f;
        cyrillic = false;
        return fc_match("sans");
    }

    void style()
    {
        ImGuiStyle &s = ImGui::GetStyle();
        ImGui::StyleColorsDark();
        s.WindowRounding = 0.f;
        s.FrameRounding = 6.f;
        s.ChildRounding = 8.f;
        s.GrabRounding = 6.f;
        s.FramePadding = ImVec2(14, 8);
        s.ItemSpacing = ImVec2(10, 10);
        s.WindowPadding = ImVec2(0, 0);
        ImVec4 *c = s.Colors;
        const ImVec4 bg(0.075f, 0.078f, 0.07f, 1.f), panel(0.11f, 0.115f, 0.10f, 1.f), amber(0.82f, 0.55f, 0.16f, 1.f);
        c[ImGuiCol_WindowBg] = bg;
        c[ImGuiCol_ChildBg] = panel;
        c[ImGuiCol_PopupBg] = panel;
        c[ImGuiCol_Text] = ImVec4(0.90f, 0.88f, 0.82f, 1.f);
        c[ImGuiCol_TextDisabled] = ImVec4(0.55f, 0.53f, 0.48f, 1.f);
        c[ImGuiCol_Button] = ImVec4(0.20f, 0.20f, 0.18f, 1.f);
        c[ImGuiCol_ButtonHovered] = ImVec4(0.30f, 0.29f, 0.25f, 1.f);
        c[ImGuiCol_ButtonActive] = amber;
        c[ImGuiCol_FrameBg] = ImVec4(0.16f, 0.16f, 0.14f, 1.f);
        c[ImGuiCol_PlotHistogram] = amber;
        c[ImGuiCol_Header] = ImVec4(0.25f, 0.21f, 0.13f, 1.f);
        c[ImGuiCol_HeaderHovered] = ImVec4(0.35f, 0.28f, 0.15f, 1.f);
        c[ImGuiCol_Separator] = ImVec4(0.25f, 0.24f, 0.21f, 1.f);
    }

    bool big_button(const char *label, ImVec2 size, bool primary)
    {
        if (primary)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.78f, 0.50f, 0.13f, 1.f));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.88f, 0.60f, 0.20f, 1.f));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.65f, 0.40f, 0.10f, 1.f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.08f, 0.07f, 0.05f, 1.f));
        }
        const bool r = ImGui::Button(label, size);
        if (primary)
            ImGui::PopStyleColor(4);
        return r;
    }

    void open_folder(const fs::path &p)
    {
        std::error_code ec;
        fs::create_directories(p, ec);
        SDL_OpenURL(("file://" + p.string()).c_str());
    }

    struct App
    {
        Config cfg = load_config();
        Task task;
        std::thread worker;
        std::vector<Release> releases;
        bool releases_loaded = false;
        enum Page { Home, Updates, Settings } page = Home;
        std::string gamma_error;
        std::string crash;
        std::atomic<bool> game_running{false};
        std::atomic<int> game_exit{0};
        int64_t game_started = 0;
        std::string picked_folder; // from the folder dialog (SDL thread)
        std::mutex pick_lock;
        bool want_play_after_overlay = false;

        bool busy() const { return task.running || game_running; }

        void start(std::function<void()> job)
        {
            if (worker.joinable())
                worker.join();
            task.ok = false;
            task.progress = -1.f;
            {
                std::lock_guard<std::mutex> g(task.lock);
                task.error.clear();
                task.status.clear();
            }
            task.running = true;
            worker = std::thread(std::move(job));
        }

        void check_updates()
        {
            start([this] { fetch_releases(task, releases); releases_loaded = true; });
        }
        void install(const Release &r)
        {
            start([this, r] { install_release(task, r, cfg); });
        }
        void overlay(bool then_play)
        {
            want_play_after_overlay = then_play;
            start([this] { build_overlay(task, cfg); });
        }
        void play()
        {
            if (overlay_needed(cfg))
                return overlay(true);
            game_running = true;
            game_started = int64_t(time(nullptr));
            {
                std::lock_guard<std::mutex> g(pick_lock);
                crash.clear();
            }
            if (worker.joinable())
                worker.join();
            worker = std::thread([this] {
                game_exit = run_game(cfg);
                if (game_exit != 0)
                {
                    std::string report = crash_report_since(game_started);
                    std::lock_guard<std::mutex> g(pick_lock);
                    crash = report.empty() ? "exit status " + std::to_string(game_exit) : report;
                }
                game_running = false;
            });
        }
    };

    void SDLCALL on_folder(void *userdata, const char *const *files, int)
    {
        auto *app = static_cast<App *>(userdata);
        if (files && files[0])
        {
            std::lock_guard<std::mutex> g(app->pick_lock);
            app->picked_folder = files[0];
        }
    }

    // status texts of core.cpp in the UI language
    std::string tr_status(std::string s)
    {
        if (!g_ru)
            return s;
        static const std::pair<const char *, const char *> parts[] = {
            {"checking for updates", "проверка обновлений"}, {"downloading ", "загрузка "}, {"verifying", "проверка файла"},
            {"unpacking", "распаковка"}, {"installed ", "установлена "}, {"reading the mod list", "чтение списка модов"},
            {"scanning ", "сканирование: "}, {" layers", " слоёв"}, {"linking ", "создание ссылок: "}, {"ready: ", "готово: "},
            {" files from ", " файлов из "}, {" files", " файлов"}, {"cannot reach GitHub", "нет связи с GitHub"},
            {"download failed", "ошибка загрузки"}, {"checksum mismatch: the download is damaged", "контрольная сумма не совпала: файл повреждён"},
            {"unpacking failed", "ошибка распаковки"}};
        for (const auto &[en, ru] : parts)
            for (size_t at; (at = s.find(en)) != std::string::npos;)
                s.replace(at, strlen(en), ru);
        return s;
    }

    void progress_line(App &a)
    {
        std::string status, error;
        {
            std::lock_guard<std::mutex> g(a.task.lock);
            status = tr_status(a.task.status);
            error = tr_status(a.task.error);
            if (!a.task.download_path.empty() && a.task.download_size)
            {
                std::error_code ec;
                const auto have = fs::file_size(a.task.download_path, ec);
                a.task.progress = ec ? 0.f : float(double(have) / double(a.task.download_size));
            }
        }
        if (a.task.running)
        {
            const float p = a.task.progress;
            if (p >= 0.f)
                ImGui::ProgressBar(p, ImVec2(-1, 0), status.c_str());
            else
            {
                ImGui::ProgressBar(-1.f * float(ImGui::GetTime()), ImVec2(-1, 0), status.c_str());
            }
        }
        else if (!error.empty())
            ImGui::TextColored(ImVec4(0.95f, 0.45f, 0.35f, 1.f), "%s", error.c_str());
        else if (!status.empty())
            ImGui::TextDisabled("%s", status.c_str());
    }

    void page_home(App &a)
    {
        ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);
        ImGui::TextUnformatted("S.T.A.L.K.E.R. GAMMA");
        ImGui::PopFont();
        ImGui::TextDisabled("X-Ray Monolith · Linux · Vulkan 1.3");
        ImGui::Spacing();
        ImGui::Text("%s: %s", tr("version"), a.cfg.version.empty() ? tr("none") : a.cfg.version.c_str());
        if (a.releases_loaded && !a.releases.empty() && version_newer(a.releases.front().tag, a.cfg.version))
        {
            ImGui::TextColored(ImVec4(0.9f, 0.65f, 0.25f, 1.f), "%s%s", tr("newer"), a.releases.front().tag.c_str());
            ImGui::SameLine();
            if (!a.busy() && ImGui::SmallButton(tr("install")))
                a.install(a.releases.front());
        }
        if (a.cfg.mo2_dir.empty() || a.cfg.anomaly_dir.empty())
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.65f, 0.25f, 1.f));
            ImGui::TextWrapped("%s", tr("need_gamma"));
            ImGui::PopStyleColor();
        }
        else if (overlay_needed(a.cfg))
            ImGui::TextDisabled("%s", tr("overlay_need"));
        ImGui::Spacing();
        const bool can_play = !a.busy() && !a.cfg.version.empty() && !a.cfg.mo2_dir.empty() && !a.cfg.anomaly_dir.empty();
        ImGui::BeginDisabled(!can_play);
        if (big_button(a.game_running ? tr("playing") : tr("play"), ImVec2(260, 64), true))
            a.play();
        ImGui::EndDisabled();
        ImGui::Spacing();
        progress_line(a);
        ImGui::Spacing();
        ImGui::TextDisabled("%s", tr("alpha"));
    }

    void page_updates(App &a)
    {
        ImGui::BeginDisabled(a.busy());
        if (ImGui::Button(tr("check")))
            a.check_updates();
        ImGui::EndDisabled();
        progress_line(a);
        if (a.releases_loaded && a.releases.empty())
            ImGui::TextDisabled("%s", tr("no_releases"));
        const auto local = installed_versions();
        for (const Release &r : a.releases)
        {
            ImGui::PushID(r.tag.c_str());
            const bool have = std::find(local.begin(), local.end(), r.tag) != local.end();
            const std::string head = r.tag + (r.name.empty() || r.name == r.tag ? "" : "  " + r.name) + "  (" + r.published + ")" +
                                     (r.prerelease ? "  pre-release" : "") + (r.tag == a.cfg.version ? std::string("  · ") + tr("current") : "");
            if (ImGui::CollapsingHeader(head.c_str(), r.tag == a.releases.front().tag ? ImGuiTreeNodeFlags_DefaultOpen : 0))
            {
                ImGui::TextWrapped("%s", r.notes.empty() ? "-" : r.notes.c_str());
                ImGui::BeginDisabled(a.busy());
                if (!have && ImGui::Button(tr("install")))
                    a.install(r);
                if (have && r.tag != a.cfg.version && ImGui::Button(tr("use")))
                {
                    a.cfg.version = r.tag;
                    save_config(a.cfg);
                }
                ImGui::EndDisabled();
                ImGui::TextDisabled("%s · %.0f MB", r.asset_name.c_str(), double(r.size) / 1048576.0);
            }
            ImGui::PopID();
        }
    }

    void page_settings(App &a, SDL_Window *window)
    {
        ImGui::Text("%s", tr("gamma"));
        ImGui::TextDisabled("Anomaly: %s", a.cfg.anomaly_dir.empty() ? "-" : a.cfg.anomaly_dir.c_str());
        ImGui::TextDisabled("MO2: %s", a.cfg.mo2_dir.empty() ? "-" : a.cfg.mo2_dir.c_str());
        ImGui::TextDisabled("%s: %s", tr("profile"), a.cfg.profile.c_str());
        ImGui::BeginDisabled(a.busy());
        if (ImGui::Button(tr("choose")))
            SDL_ShowOpenFolderDialog(on_folder, &a, window, nullptr, false);
        ImGui::SameLine();
        if (ImGui::Button(tr("overlay")) && !a.cfg.mo2_dir.empty() && !a.cfg.anomaly_dir.empty())
            a.overlay(false);
        ImGui::EndDisabled();
        if (!a.gamma_error.empty())
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.45f, 0.35f, 1.f));
            ImGui::TextWrapped("%s", a.gamma_error.c_str());
            ImGui::PopStyleColor();
        }
        progress_line(a);
        ImGui::Separator();
        if (ImGui::Button(tr("saves")))
            open_folder(game_dir() / "appdata" / "savedgames");
        ImGui::SameLine();
        if (ImGui::Button(tr("logs")))
            open_folder(game_dir() / "appdata" / "logs");
        ImGui::SameLine();
        if (ImGui::Button(tr("desktop")))
            install_desktop_entry();
        ImGui::TextDisabled("%s", data_dir().c_str());
        ImGui::Separator();
        // the language names stay in their own language, so the switch is readable in either UI
        static const char *const codes[] = {"auto", "en", "ru"};
        const char *names[] = {tr("lang_auto"), "English", g_cyrillic ? "Русский" : "Russian (no Cyrillic font)"};
        int cur = 0;
        for (int i = 0; i < 3; ++i)
            if (a.cfg.lang == codes[i])
                cur = i;
        ImGui::SetNextItemWidth(260);
        if (ImGui::Combo(tr("lang"), &cur, names, 3))
        {
            a.cfg.lang = codes[cur];
            save_config(a.cfg);
            apply_lang(a.cfg.lang);
        }
    }
} // namespace

// Command line without a window (tests, scripts): --gamma <folder>, --overlay, --releases.
static int command_line(int argc, char **argv)
{
    Config cfg = load_config();
    for (int i = 1; i < argc; ++i)
    {
        const std::string a = argv[i];
        if (a == "--gamma" && i + 1 < argc)
        {
            std::string error;
            const bool ok = detect_gamma(argv[++i], cfg, error);
            cfg.overlay_stamp.clear();
            save_config(cfg); // a half found is kept, as in the window
            if (!ok)
                return fprintf(stderr, "%s\n", error.c_str()), 1;
            printf("anomaly: %s\nmo2: %s\nprofile: %s\n", cfg.anomaly_dir.c_str(), cfg.mo2_dir.c_str(), cfg.profile.c_str());
        }
        else if (a == "--overlay")
        {
            Task task;
            task.running = true;
            build_overlay(task, cfg);
            printf("%s\n", task.status.c_str());
            if (!task.ok)
                return 1;
        }
        else if (a == "--releases")
        {
            Task task;
            std::vector<Release> rel;
            task.running = true;
            fetch_releases(task, rel);
            if (!task.ok)
                return fprintf(stderr, "%s\n", task.error.c_str()), 1;
            for (const Release &r : rel)
                printf("%s  %s  %s  %.0f MB\n", r.tag.c_str(), r.published.c_str(), r.asset_name.c_str(), double(r.size) / 1048576.0);
            if (rel.empty())
                printf("no releases\n");
        }
        else
            return fprintf(stderr, "usage: %s [--gamma <folder>] [--overlay] [--releases]\n", argv[0]), 2;
    }
    return 0;
}

int main(int argc, char **argv)
{
    if (argc > 1)
        return command_line(argc, argv);
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Window *window = SDL_CreateWindow("GAMMA native launcher", 980, 600, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    SDL_Renderer *renderer = window ? SDL_CreateRenderer(window, nullptr) : nullptr;
    if (!renderer)
    {
        fprintf(stderr, "SDL window/renderer: %s\n", SDL_GetError());
        return 1;
    }
    SDL_SetRenderVSync(renderer, 1);
    IMGUI_CHECKVERSION();
    // the engine's imconfig.h disables ImGui's default allocators and embedded font
    ImGui::SetAllocatorFunctions([](size_t n, void *) { return malloc(n); }, [](void *p, void *) { free(p); });
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    io.IniFilename = nullptr;
    const float scale = SDL_GetWindowDisplayScale(window) > 0 ? SDL_GetWindowDisplayScale(window) : 1.f;
    bool cyrillic = false;
    const std::string font = find_font(cyrillic);
    g_cyrillic = cyrillic;
    if (font.empty())
    {
        fprintf(stderr, "no TrueType font found (install fonts-dejavu or noto-fonts)\n");
        return 1;
    }
    {
        static const ImWchar ranges[] = {0x0020, 0x00FF, 0x0400, 0x04FF, 0x2010, 0x205E, 0};
        io.Fonts->AddFontFromFileTTF(font.c_str(), 18.f * scale, nullptr, ranges);
        io.Fonts->AddFontFromFileTTF(font.c_str(), 34.f * scale, nullptr, ranges);
    }
    style();
    ImGui::GetStyle().ScaleAllSizes(scale);
    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);

    App app;
    apply_lang(app.cfg.lang);
    adopt_running_package();
    if (app.cfg.version.empty())
    {
        const auto v = installed_versions();
        if (!v.empty())
            app.cfg.version = v.front(), save_config(app.cfg);
    }
    app.check_updates();

    // XR_LAUNCHER_SCREENSHOT=<file.bmp>: render a few frames, save the window, exit (docs, tests);
    // XR_LAUNCHER_PAGE=updates|settings picks the page.
    const char *shot = getenv("XR_LAUNCHER_SCREENSHOT");
    if (const char *pg = getenv("XR_LAUNCHER_PAGE"))
        app.page = strcmp(pg, "updates") == 0 ? App::Updates : strcmp(pg, "settings") == 0 ? App::Settings : App::Home;
    int frames = 0;
    bool quit = false;
    while (!quit)
    {
        SDL_Event e;
        while (SDL_PollEvent(&e))
        {
            ImGui_ImplSDL3_ProcessEvent(&e);
            if (e.type == SDL_EVENT_QUIT || e.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED)
                quit = !app.game_running; // keep the launcher while the game runs (crash report)
        }
        {
            std::lock_guard<std::mutex> g(app.pick_lock);
            if (!app.picked_folder.empty())
            {
                Config c = app.cfg;
                const bool ok = detect_gamma(app.picked_folder, c, app.gamma_error);
                if (ok)
                    app.gamma_error.clear();
                // a half found (Anomaly or GAMMA) is kept so the other folder can be chosen next
                if (c.anomaly_dir != app.cfg.anomaly_dir || c.mo2_dir != app.cfg.mo2_dir || ok)
                {
                    c.overlay_stamp.clear();
                    app.cfg = c;
                    save_config(app.cfg);
                }
                app.picked_folder.clear();
            }
        }
        if (!app.task.running && app.want_play_after_overlay && app.task.ok)
        {
            app.want_play_after_overlay = false;
            app.play();
        }
        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();
        ImGui::SetNextWindowPos(ImVec2(0, 0));
        ImGui::SetNextWindowSize(io.DisplaySize);
        ImGui::Begin("##root", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus);
        // navigation column
        ImGui::BeginChild("##nav", ImVec2(200 * scale, 0), ImGuiChildFlags_None);
        ImGui::Dummy(ImVec2(0, 16 * scale));
        for (auto [p, key] : {std::pair{App::Home, "home"}, {App::Updates, "updates"}, {App::Settings, "settings"}})
        {
            ImGui::SetCursorPosX(12 * scale);
            if (ImGui::Selectable(tr(key), app.page == p, 0, ImVec2(176 * scale, 34 * scale)))
                app.page = p;
        }
        ImGui::EndChild();
        ImGui::SameLine(0, 0);
        ImGui::BeginChild("##page", ImVec2(0, 0), ImGuiChildFlags_AlwaysUseWindowPadding);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(28 * scale, 24 * scale));
        ImGui::Dummy(ImVec2(0, 8 * scale));
        ImGui::Indent(24 * scale);
        ImGui::PushTextWrapPos(ImGui::GetContentRegionAvail().x - 24 * scale);
        if (app.page == App::Home)
            page_home(app);
        else if (app.page == App::Updates)
            page_updates(app);
        else
            page_settings(app, window);
        ImGui::PopTextWrapPos();
        ImGui::Unindent();
        ImGui::PopStyleVar();
        ImGui::EndChild();
        ImGui::End();

        std::string crash_text;
        {
            std::lock_guard<std::mutex> g(app.pick_lock);
            crash_text = app.crash;
        }
        if (!crash_text.empty())
            ImGui::OpenPopup("crash");
        ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x * 0.8f, io.DisplaySize.y * 0.75f));
        if (ImGui::BeginPopupModal("crash", nullptr, ImGuiWindowFlags_NoResize))
        {
            ImGui::TextWrapped("%s", tr("crash"));
            ImGui::BeginChild("##report", ImVec2(0, -50 * scale), ImGuiChildFlags_Borders);
            ImGui::TextUnformatted(crash_text.c_str());
            ImGui::EndChild();
            if (ImGui::Button(tr("copy")))
                SDL_SetClipboardText(crash_text.c_str());
            ImGui::SameLine();
            if (ImGui::Button(tr("close")))
            {
                std::lock_guard<std::mutex> g(app.pick_lock);
                app.crash.clear();
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        ImGui::Render();
        SDL_SetRenderDrawColor(renderer, 19, 20, 18, 255);
        SDL_RenderClear(renderer);
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
        if (shot && ++frames == 90 && !app.task.running)
        {
            if (SDL_Surface *s = SDL_RenderReadPixels(renderer, nullptr))
            {
                SDL_SaveBMP(s, shot);
                SDL_DestroySurface(s);
            }
            quit = true;
        }
        else if (shot && frames >= 90)
            frames = 89; // wait for the update check to finish
        SDL_RenderPresent(renderer);
    }
    if (app.worker.joinable())
        app.worker.join();
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
