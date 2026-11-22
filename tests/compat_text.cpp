#include "win32_text.h"
#include <cstdio>
#include <cstring>
int main() {
  const char* u = "Привет, Зона €";              // UTF-8
  wchar_t w[64]; int n = MultiByteToWideChar(CP_UTF8, 0, u, -1, w, 64);
  char c[64]; int m = WideCharToMultiByte(1251, 0, w, -1, c, 64, 0, 0);
  wchar_t w2[64]; MultiByteToWideChar(1251, 0, c, -1, w2, 64);
  char back[64]; WideCharToMultiByte(CP_UTF8, 0, w2, -1, back, 64, 0, 0);
  int need = MultiByteToWideChar(CP_UTF8, 0, u, -1, nullptr, 0);
  printf("wchars=%d cp1251bytes=%d need=%d roundtrip=%s first=%02x\n", n, m, need, back, (unsigned char)c[0]);
  return !(n == 15 && m == 15 && need == 15 && strcmp(back, u) == 0 && (unsigned char)c[0] == 0xCF);
}
