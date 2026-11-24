// Compiles one HLSL file through Common/linux/d3dcompiler_linux.cpp (vkd3d-shader) and prints the
// messages. Usage: hlsl_check <file> <entry> <profile>. Build: tools/hlsl_check.sh
#include "../monolith/src/xrCore/xrCore_platform.h"
#include <d3dcompiler.h>
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>

int main(int argc, char** argv)
{
    if (argc < 4)
    {
        fprintf(stderr, "usage: %s file entry profile\n", argv[0]);
        return 2;
    }
    std::ifstream f(argv[1], std::ios::binary);
    std::string src((std::istreambuf_iterator<char>(f)), {});
    ID3DBlob *code = nullptr, *err = nullptr;
    const HRESULT hr = D3DCompile(src.data(), src.size(), argv[1], nullptr, nullptr, argv[2], argv[3],
        D3DCOMPILE_PACK_MATRIX_ROW_MAJOR, 0, &code, &err);
    if (err)
        fputs(static_cast<const char*>(err->GetBufferPointer()), stdout);
    printf("%s (%zu bytes)\n", SUCCEEDED(hr) ? "OK" : "FAILED", code ? size_t(code->GetBufferSize()) : size_t(0));
    return SUCCEEDED(hr) ? 0 : 1;
}
