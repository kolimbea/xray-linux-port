// SHA-0 known-answer test for monolith/src/3rd party/crypto/sha0.h (vectors: FIPS 180 / published SHA-0 values).
// Build: clang++ -I"monolith/src/3rd party/crypto" tests/compat_sha0.cpp
#include "sha0.h"
#include <cstdio>
static bool check(const void* d, size_t n, size_t reps, const char* want)
{
    xr_sha0_ctx c; xr_sha0_init(c);
    for (size_t i = 0; i < reps; ++i) xr_sha0_update(c, d, n);
    unsigned char o[20]; xr_sha0_final(o, c);
    char hex[41]; for (int i = 0; i < 20; ++i) snprintf(hex + 2 * i, 3, "%02x", o[i]);
    const bool ok = !strcmp(hex, want);
    printf("%s %s\n", ok ? "ok  " : "FAIL", hex);
    return ok;
}
int main()
{
    char a[1000]; memset(a, 'a', sizeof(a));
    bool ok = check("abc", 3, 1, "0164b8a914cd2a5e74c4f7ff082c4d97f1edf880");
    ok &= check("", 0, 1, "f96cea198ad1dd5617ac084a3d92c6107708c0ef");
    ok &= check(a, sizeof(a), 1000, "3232affa48628a26653b5aaa44541fd90d690603");
    return ok ? 0 : 1;
}
