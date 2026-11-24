// Copies every file under an engine FS alias (archives + loose gamedata, as the game sees them) to a
// directory. Run from a fixture holding fsgame.ltx (see tests/README.md); the source is only read.
// Usage: extract_tree <alias> <out-dir>      e.g. extract_tree '$game_shaders$' /tmp/shaders
// Build: tools/extract_tree.sh
#include "stdafx.h"
#include <cstdio>
#include <filesystem>
#include <string>
XRCORE_API LPCSTR build_date = "tool";
XRCORE_API u32 build_id = 0;
void printLuaStack() {}
enum Console_mark : int;
bool is_console_mark(Console_mark) { return false; }
LPCSTR get_modded_exes_name() { return "linux-tool"; }
std::string get_modded_exes_version_string() { return "0"; }

int main(int argc, char** argv)
{
    if (argc < 3)
    {
        fprintf(stderr, "usage: %s <alias> <out-dir>\n", argv[0]);
        return 2;
    }
    Core._initialize("xrtool", nullptr, TRUE, "fsgame.ltx");
    xr_vector<LPSTR>* files = FS.file_list_open(argv[1], FS_ListFiles);
    if (!files)
    {
        fprintf(stderr, "alias %s not found\n", argv[1]);
        return 1;
    }
    int copied = 0, failed = 0;
    for (LPSTR name : *files)
    {
        IReader* r = FS.r_open(argv[1], name);
        if (!r)
        {
            ++failed;
            continue;
        }
        std::string rel = name;
        for (char& c : rel)
            if (c == '\\')
                c = '/';
        const std::filesystem::path out = std::filesystem::path(argv[2]) / rel;
        std::filesystem::create_directories(out.parent_path());
        if (FILE* f = ::xr_linux_fopen(out.c_str(), "wb"))
        {
            fwrite(r->pointer(), 1, r->length(), f);
            fclose(f);
            ++copied;
        }
        else
            ++failed;
        FS.r_close(r);
    }
    FS.file_list_close(files);
    printf("copied %d files, %d failed\n", copied, failed);
    return failed ? 1 : 0;
}
