// Milestone M3 check of the Vulkan backend's shader pipeline (xrRenderVK/vkShader.cpp, vkReflection.cpp):
// every R4 shader file of an exported shader tree goes through D3DCompile (DXC -> SPIR-V) the way r4.cpp
// calls it, then D3DReflect / D3DGetInputSignatureBlob, and the module for vkCreateShadersEXT is checked
// with spirv-val. Also prints the inter-stage semantics in use.
// Usage: vk_shaders <shader-root> [out-dir]   (shader-root contains r3/; out-dir gets the .spv files)
// Exit 0 = every shader that compiles also reflects and validates; compile failures are listed.
#include "../monolith/src/xrCore/xrCore_platform.h"
#include "vkCommonTypes.h"
#include "vkShader.h"
#include "vk_d3dcompiler.h"
#include "vkCore.h"
#include "vkObjects.h"
#include "shader_tree.h"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>

#include <SDL3/SDL.h>
#include <string>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

struct Result
{
    std::string file, error;
    bool compiled = false, reflected = false;
    xrvk::ShaderInfo info;
    std::vector<uint8_t> code; // D3DCompile output
};

static std::string first_error(const std::string &log)
{
    size_t p = log.find("error:");
    if (p == std::string::npos)
        p = 0;
    else
        p += 7;
    const size_t e = log.find('\n', p);
    return log.substr(p, e == std::string::npos ? std::string::npos : e - p);
}

int main(int argc, char **argv)
{
    if (argc < 2 || !fs::is_directory(fs::path(argv[1]) / "r3"))
    {
        printf("SKIP: usage: %s <shader-root with r3/> [out-dir]\n", argv[0]);
        return 77;
    }
    const fs::path root = argv[1];
    const fs::path out = argc > 2 ? fs::path(argv[2]) : fs::path("vk_shaders_out");
    fs::create_directories(out);

    std::vector<Result> results;
    for (const std::string &f : shader_tree::files(root))
        results.push_back({f});
    const auto &macros = shader_tree::macros();

    std::atomic<size_t> next{0};
    auto worker = [&] {
        shader_tree::Includer inc;
        inc.root = root;
        for (size_t i; (i = next++) < results.size();)
        {
            Result &r = results[i];
            const std::string src = shader_tree::read(root / "r3" / r.file);
            const std::string target = shader_tree::target(r.file);
            ID3DBlob *code = nullptr, *err = nullptr;
            const HRESULT hr = D3DCompile(src.data(), src.size(), "", macros.data(), &inc, "main", target.c_str(),
                D3D10_SHADER_PACK_MATRIX_ROW_MAJOR, 0, &code, &err);
            if (FAILED(hr))
            {
                r.error = err ? first_error(static_cast<const char *>(err->GetBufferPointer())) : "no error text";
                if (err)
                    err->Release();
                continue;
            }
            r.compiled = true;
            if (err)
                err->Release();
            ID3D11ShaderReflection *refl = nullptr;
            if (FAILED(D3DReflect(code->GetBufferPointer(), code->GetBufferSize(), IID_PPV_ARGS(&refl))))
                r.error = "D3DReflect failed";
            else
            {
                D3D11_SHADER_DESC d{};
                refl->GetDesc(&d);
                std::string e;
                xrvk::reflect_spirv(code->GetBufferPointer(), code->GetBufferSize(), r.info, e);
                // what the engine reads: every cbuffer and its variables, every binding
                bool ok = d.ConstantBuffers == r.info.cbuffers.size() && d.BoundResources == r.info.resources.size();
                for (UINT c = 0; ok && c < d.ConstantBuffers; ++c)
                {
                    D3D11_SHADER_BUFFER_DESC bd{};
                    ok = SUCCEEDED(refl->GetConstantBufferByIndex(c)->GetDesc(&bd));
                    for (UINT v = 0; ok && v < bd.Variables; ++v)
                    {
                        D3D11_SHADER_VARIABLE_DESC vd{};
                        D3D11_SHADER_TYPE_DESC td{};
                        ID3D11ShaderReflectionVariable *var = refl->GetConstantBufferByIndex(c)->GetVariableByIndex(v);
                        ok = SUCCEEDED(var->GetDesc(&vd)) && SUCCEEDED(var->GetType()->GetDesc(&td)) && vd.StartOffset + vd.Size <= bd.Size;
                    }
                }
                for (UINT b = 0; ok && b < d.BoundResources; ++b)
                {
                    D3D11_SHADER_INPUT_BIND_DESC bd{};
                    ok = SUCCEEDED(refl->GetResourceBindingDesc(b, &bd));
                }
                if (!ok)
                    r.error = "reflection inconsistent";
                refl->Release();
            }
            if (r.error.empty() && target[0] == 'v')
            {
                ID3DBlob *sig = nullptr;
                std::vector<xrvk::ShaderParameter> in;
                if (FAILED(D3DGetInputSignatureBlob(code->GetBufferPointer(), code->GetBufferSize(), &sig)) ||
                    !xrvk::parse_input_signature(sig->GetBufferPointer(), sig->GetBufferSize(), in))
                    r.error = "input signature failed";
                if (sig)
                    sig->Release();
            }
            r.reflected = r.error.empty();
            const auto *bytes = static_cast<const uint8_t *>(code->GetBufferPointer());
            r.code.assign(bytes, bytes + code->GetBufferSize());
            const auto stripped = xrvk::strip_reflection_info(code->GetBufferPointer(), code->GetBufferSize());
            std::ofstream(out / (r.file + ".spv"), std::ios::binary)
                .write(reinterpret_cast<const char *>(stripped.data()), std::streamsize(stripped.size() * 4));
            code->Release();
        }
    };
    std::vector<std::thread> pool;
    for (unsigned t = 0; t < std::max(1u, std::thread::hardware_concurrency()); ++t)
        pool.emplace_back(worker);
    for (auto &t : pool)
        t.join();

    size_t compiled = 0, reflected = 0, invalid = 0;
    std::map<std::string, int> errors;
    for (const Result &r : results)
    {
        compiled += r.compiled;
        reflected += r.reflected;
        if (!r.compiled)
            ++errors[r.error];
        else if (!r.reflected)
            printf("REFLECTION %s: %s\n", r.file.c_str(), r.error.c_str());
    }
    for (const Result &r : results)
    {
        if (!r.compiled)
            continue;
        const std::string cmd = "spirv-val --target-env vulkan1.3 '" + (out / (r.file + ".spv")).string() + "' 2>&1";
        FILE *p = popen(cmd.c_str(), "r");
        char buf[512];
        std::string log;
        while (p && fgets(buf, sizeof buf, p))
            log += buf;
        if (!p || pclose(p) != 0)
        {
            ++invalid;
            printf("SPIRV-VAL %s: %s", r.file.c_str(), log.substr(0, 300).c_str());
        }
    }

    // inter-stage semantics: VS/GS/DS outputs and PS/GS/HS/DS inputs
    std::map<std::string, std::set<std::string>> io;
    for (const Result &r : results)
    {
        auto add = [&](const std::vector<xrvk::ShaderParameter> &list, const char *dir) {
            for (const auto &p : list)
                if (!p.builtin)
                    io[p.semantic].insert(std::string(dir) + std::to_string(p.index) + "@" + std::to_string(p.location));
        };
        if (r.info.stage != xrvk::Stage::Vertex)
            add(r.info.inputs, "in");
        if (r.info.stage != xrvk::Stage::Pixel)
            add(r.info.outputs, "out");
    }
    printf("inter-stage semantics:\n");
    for (const auto &[name, uses] : io)
    {
        printf("  %-16s", name.c_str());
        for (const auto &u : uses)
            printf(" %s", u.c_str());
        printf("\n");
    }
    printf("compile failures by first error:\n");
    for (const auto &[e, n] : errors)
        printf("  %4d  %s\n", n, e.c_str());
    for (const Result &r : results)
        if (!r.compiled)
            printf("  failed: %s\n", r.file.c_str());
    printf("files %zu, compiled %zu, reflected %zu, spirv-val failures %zu\n", results.size(), compiled, reflected, invalid);
    // shader objects on the GPU: every shader alone, and each VS with the PS of the same name (a linked
    // pair, as most X-Ray passes use), with the validation layers watching
    size_t programs = 0, program_failures = 0;
    static std::atomic<int> validation_errors{0};
    xrvk::set_log([](const char *line) {
        if (strstr(line, "validation error") || strstr(line, "Validation Error"))
        {
            ++validation_errors;
            printf("%s\n", line);
        }
        else if (line[0] == '!' || strstr(line, "vk: instance") || strstr(line, "vk: device"))
            printf("%s\n", line);
    });
    if (!SDL_Init(SDL_INIT_VIDEO))
        printf("SKIP shader objects: SDL_Init(VIDEO) failed: %s\n", SDL_GetError());
    else
    {
        ID3D11Device *dev = nullptr;
        ID3D11DeviceContext *ctx = nullptr;
        const D3D_FEATURE_LEVEL fl = D3D_FEATURE_LEVEL_11_0;
        if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, &fl, 1, D3D11_SDK_VERSION, &dev, nullptr, &ctx)))
        {
            printf("FAIL: D3D11CreateDevice\n");
            return 1;
        }
        std::map<std::string, std::shared_ptr<const xrvk::ShaderModule>> modules; // file -> module
        for (const Result &r : results)
        {
            if (!r.reflected)
                continue;
            const char t0 = r.file[r.file.size() - 2];
            HRESULT hr = E_FAIL;
            std::shared_ptr<const xrvk::ShaderModule> m;
            auto take = [&](auto *s) {
                if (s)
                {
                    m = static_cast<xrvk::Shader<std::remove_pointer_t<decltype(s)>> *>(s)->module;
                    s->Release();
                }
            };
            if (t0 == 'v')
            {
                ID3D11VertexShader *s = nullptr;
                hr = dev->CreateVertexShader(r.code.data(), r.code.size(), nullptr, &s);
                take(s);
            }
            else if (t0 == 'p')
            {
                ID3D11PixelShader *s = nullptr;
                hr = dev->CreatePixelShader(r.code.data(), r.code.size(), nullptr, &s);
                take(s);
            }
            else if (t0 == 'g')
            {
                ID3D11GeometryShader *s = nullptr;
                hr = dev->CreateGeometryShader(r.code.data(), r.code.size(), nullptr, &s);
                take(s);
            }
            else if (t0 == 'c')
            {
                ID3D11ComputeShader *s = nullptr;
                hr = dev->CreateComputeShader(r.code.data(), r.code.size(), nullptr, &s);
                take(s);
            }
            if (FAILED(hr) || !m)
            {
                printf("FAIL Create*Shader %s\n", r.file.c_str());
                ++program_failures;
                continue;
            }
            modules[r.file] = m;
        }
        auto *device = static_cast<xrvk::Device *>(dev);
        auto build = [&](std::initializer_list<std::pair<xrvk::Stage, std::string>> stages) {
            std::shared_ptr<const xrvk::ShaderModule> set[size_t(xrvk::Stage::Count)];
            std::string names;
            for (const auto &[stage, file] : stages)
            {
                set[size_t(stage)] = modules[file];
                names += (names.empty() ? "" : " + ") + file;
            }
            ++programs;
            if (!device->program(set))
            {
                ++program_failures;
                printf("FAIL program %s\n", names.c_str());
            }
        };
        size_t pairs = 0;
        for (const auto &[file, m] : modules)
        {
            build({{m->info.stage, file}});
            const std::string base = file.substr(0, file.size() - 3);
            if (m->info.stage == xrvk::Stage::Vertex && modules.count(base + ".ps"))
            {
                build({{xrvk::Stage::Vertex, file}, {xrvk::Stage::Pixel, base + ".ps"}});
                ++pairs;
            }
        }
        printf("programs %zu (%zu linked VS+PS pairs), failures %zu, validation errors %d\n", programs, pairs, program_failures,
            int(validation_errors));
        ctx->Release();
        dev->Release();
        xrvk::destroy_core();
        SDL_Quit();
    }
    return (reflected == compiled && invalid == 0 && program_failures == 0 && validation_errors == 0) ? 0 : 1;
}
