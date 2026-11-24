// Prints the reflection of every shader of an exported R4 shader tree as the engine reads it
// (R_constant_table::parse): constant buffers and variables, bound resources, VS input semantics.
// One source, two builds: against the Vulkan backend (CMake target reflect_dump_vk: DXC -> SPIR-V) and
// against the R4 path (tests/reflect_dump_r4.sh: vkd3d-shader -> DXBC). tools/reflect_compare.py
// compares the two dumps. Usage: reflect_dump <shader-root> [r3 file...] > dump.txt
#include "../monolith/src/xrCore/xrCore_platform.h"
#include <d3dcompiler.h>

#include <atomic>
#include <cstdio>
#include <thread>

#include "shader_tree.h"

static std::string dump(ID3D11ShaderReflection *r, bool vs)
{
    std::string out;
    char line[512];
    D3D11_SHADER_DESC d{};
    r->GetDesc(&d);
    for (UINT c = 0; c < d.ConstantBuffers; ++c)
    {
        ID3D11ShaderReflectionConstantBuffer *cb = r->GetConstantBufferByIndex(c);
        D3D11_SHADER_BUFFER_DESC bd{};
        if (FAILED(cb->GetDesc(&bd)))
            continue;
        snprintf(line, sizeof line, "CB %u %s size %u vars %u\n", c, bd.Name, bd.Size, bd.Variables);
        out += line;
        for (UINT v = 0; v < bd.Variables; ++v)
        {
            ID3D11ShaderReflectionVariable *var = cb->GetVariableByIndex(v);
            D3D11_SHADER_VARIABLE_DESC vd{};
            D3D11_SHADER_TYPE_DESC td{};
            var->GetDesc(&vd);
            var->GetType()->GetDesc(&td);
            snprintf(line, sizeof line, "VAR %s %s offset %u size %u class %d type %d rows %u cols %u elems %u\n", bd.Name, vd.Name,
                vd.StartOffset, vd.Size, int(td.Class), int(td.Type), td.Rows, td.Columns, td.Elements);
            out += line;
        }
    }
    for (UINT b = 0; b < d.BoundResources; ++b)
    {
        D3D11_SHADER_INPUT_BIND_DESC bd{};
        r->GetResourceBindingDesc(b, &bd);
        snprintf(line, sizeof line, "RES %s type %d slot %u count %u dim %d\n", bd.Name, int(bd.Type), bd.BindPoint, bd.BindCount,
            int(bd.Dimension));
        out += line;
    }
    if (vs)
        for (UINT i = 0; i < d.InputParameters; ++i)
        {
            D3D11_SIGNATURE_PARAMETER_DESC p{};
            r->GetInputParameterDesc(i, &p);
            if (p.SystemValueType != D3D_NAME_UNDEFINED)
                continue;
            snprintf(line, sizeof line, "IN %s%u type %d mask %u\n", p.SemanticName, p.SemanticIndex, int(p.ComponentType), p.Mask);
            out += line;
        }
    return out;
}

int main(int argc, char **argv)
{
    if (argc < 2 || !shader_tree::fs::is_directory(shader_tree::fs::path(argv[1]) / "r3"))
    {
        fprintf(stderr, "usage: %s <shader-root with r3/>\n", argv[0]);
        return 2;
    }
    const shader_tree::fs::path root = argv[1];
    // optional file names after the root: only those (the R4 reference runs one file per process, with
    // a timeout, because vkd3d-shader hangs on a few of them)
    std::vector<std::string> files = shader_tree::files(root);
    if (argc > 2)
        files.assign(argv + 2, argv + argc);
    std::vector<std::string> text(files.size());
    std::atomic<size_t> next{0};
    auto worker = [&] {
        shader_tree::Includer inc;
        inc.root = root;
        for (size_t i; (i = next++) < files.size();)
        {
            const std::string src = shader_tree::read(root / "r3" / files[i]);
            const std::string target = shader_tree::target(files[i]);
            ID3DBlob *code = nullptr, *err = nullptr;
            HRESULT hr = D3DCompile(src.data(), src.size(), "", shader_tree::macros().data(), &inc, "main", target.c_str(),
                D3DCOMPILE_PACK_MATRIX_ROW_MAJOR, 0, &code, &err);
            if (FAILED(hr) && err)
                fprintf(stderr, "%s: %.400s\n", files[i].c_str(), static_cast<const char *>(err->GetBufferPointer()));
            if (err)
                err->Release();
            if (FAILED(hr))
            {
                text[i] = "FILE " + files[i] + " failed\n";
                continue;
            }
            ID3D11ShaderReflection *r = nullptr;
            hr = D3DReflect(code->GetBufferPointer(), code->GetBufferSize(), IID_ID3D11ShaderReflection, reinterpret_cast<void **>(&r));
            text[i] = "FILE " + files[i] + (SUCCEEDED(hr) ? " ok\n" + dump(r, target[0] == 'v') : " noreflect\n");
            if (r)
                r->Release();
            code->Release();
        }
    };
    std::vector<std::thread> pool;
    for (unsigned t = 0; t < std::max(1u, std::thread::hardware_concurrency()); ++t)
        pool.emplace_back(worker);
    for (auto &t : pool)
        t.join();
    for (const auto &t : text)
        fputs(t.c_str(), stdout);
    return 0;
}
