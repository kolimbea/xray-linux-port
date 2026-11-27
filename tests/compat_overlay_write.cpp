// Writes through the overlay (symlinks into mod folders, like Mod Organizer 2) must never reach the
// mod file: xr_linux_fopen detaches the link first (Common/linux/path_resolve.cpp).
// Build: clang++ -std=c++20 -Imonolith/src/Common/linux tests/compat_overlay_write.cpp \
//        monolith/src/Common/linux/io_compat.cpp monolith/src/Common/linux/path_resolve.cpp -o /tmp/t && /tmp/t $(mktemp -d)
#include "io_compat.h"
#include "path_resolve.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
static std::string slurp(const char* p){ std::string s; if(FILE*f=::fopen(p,"rb")){char b[256];size_t n;while((n=fread(b,1,sizeof b,f))>0)s.append(b,n);fclose(f);} return s; }
int main(int, char** argv){
  std::string d = argv[1];
  std::string mod = d + "/mod.ltx", w = d + "/overlay_w.ltx", a = d + "/overlay_a.ltx";
  FILE* f = ::fopen(mod.c_str(), "w"); fputs("original\n", f); fclose(f);
  symlink(mod.c_str(), w.c_str()); symlink(mod.c_str(), a.c_str());
  f = xr_linux_fopen(w.c_str(), "w"); fputs("rewritten\n", f); fclose(f);
  f = xr_linux_fopen(a.c_str(), "a"); fputs("appended\n", f); fclose(f);
  struct stat sb; lstat(w.c_str(), &sb); bool w_regular = S_ISREG(sb.st_mode); lstat(a.c_str(), &sb); bool a_regular = S_ISREG(sb.st_mode);
  int fails = 0;
  if (slurp(mod.c_str()) != "original\n") { printf("FAIL mod file changed: %s", slurp(mod.c_str()).c_str()); ++fails; }
  if (!w_regular || slurp(w.c_str()) != "rewritten\n") { printf("FAIL w copy\n"); ++fails; }
  if (!a_regular || slurp(a.c_str()) != "original\nappended\n") { printf("FAIL a copy: %s\n", slurp(a.c_str()).c_str()); ++fails; }
  printf(fails ? "FAILED\n" : "all checks passed\n"); return fails;
}
