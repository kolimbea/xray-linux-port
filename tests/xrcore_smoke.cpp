// Smoke test: initialise xrCore with a real Anomaly fsgame.ltx and read configs through the FS.
#include "stdafx.h"
#include "xr_ini.h"
#include <cstdio>
#include <string>
XRCORE_API LPCSTR build_date = "test";
XRCORE_API u32 build_id = 0;
void printLuaStack() {}
enum Console_mark : int;
bool is_console_mark(Console_mark) { return false; }
LPCSTR get_modded_exes_name() { return "linux-test"; }
std::string get_modded_exes_version_string() { return "0"; }
int main()
{
    Core._initialize("xrtest", nullptr, TRUE, "fsgame.ltx");
    const bool sys = FS.exist("$game_config$", "system.ltx");
    string_path p;
    FS.update_path(p, "$game_config$", "system.ltx");
    CInifile* ini = CInifile::Create(p, TRUE);
    const bool has = ini && ini->section_exist("actor");
    Msg("smoke: system.ltx exists=%d path=%s sections=%u actor=%d", sys, p, ini ? (u32)ini->sections().size() : 0, has);
    FlushLog();
    printf("RESULT exists=%d sections=%u actor=%d\n", sys, ini ? (u32)ini->sections().size() : 0, has);
    return !(sys && has);
}
