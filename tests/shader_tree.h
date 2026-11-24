// Shared by the shader tests (vk_shaders.cpp, reflect_dump.cpp): an exported R4 shader tree compiled the
// way r4.cpp does it. Needs the D3D compiler API declarations (d3dcompiler.h of either backend) first.
#pragma once
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace shader_tree
{
    namespace fs = std::filesystem;

    // Like r4.cpp's includer: the renderer's shader directory first, then the shader root.
    struct Includer final : public ID3DInclude
    {
        fs::path root;
        HRESULT STDMETHODCALLTYPE Open(D3D_INCLUDE_TYPE, LPCSTR include, LPCVOID, LPCVOID *data, UINT *bytes) override
        {
            std::string name = include; // the shaders write Windows paths ("shared\\common.h"); xrCore's FS accepts both
            std::replace(name.begin(), name.end(), '\\', '/');
            for (const fs::path &p : {root / "r3" / name, root / name})
            {
                std::ifstream f(p, std::ios::binary);
                if (!f)
                    continue;
                auto *s = new std::string(std::istreambuf_iterator<char>(f), {});
                *data = s->data();
                *bytes = UINT(s->size());
                std::lock_guard<std::mutex> g(lock);
                open[s->data()] = s;
                return S_OK;
            }
            return E_FAIL;
        }
        HRESULT STDMETHODCALLTYPE Close(LPCVOID data) override
        {
            std::lock_guard<std::mutex> g(lock);
            auto it = open.find(data);
            if (it == open.end())
                return E_FAIL;
            delete it->second;
            open.erase(it);
            return S_OK;
        }
        std::mutex lock;
        std::map<const void *, std::string *> open;
    };

    // A representative default of the options r4.cpp turns into defines (one variant per file).
    inline const std::vector<D3D_SHADER_MACRO> &macros()
    {
        static const std::vector<D3D_SHADER_MACRO> m = {{"SM_5", "1"}, {"SM_4_1", "1"}, {"SMAP_size", "2048"}, {"SKIN_NONE", "1"},
            {"USE_HWSMAP", "1"}, {"USE_HWSMAP_PCF", "1"}, {"FP16_FILTER", "1"}, {"FP16_BLEND", "1"}, {"USE_BRANCHING", "1"},
            {"USE_VTF", "1"}, {"SUN_QUALITY", "2"}, {"VECTORIZED_CODE", "1"}, {"USE_SJITTER", "1"}, {"SSAO_QUALITY", "2"},
            {"SSFX_MODEXE", "1"}, {"SSFX_RAIN_QUALITY", "1"}, {"SSFX_SSR_QUALITY", "1"}, {"SSFX_WATER_QUALITY", "1"},
            {"SSFX_WATER_PARALLAX", "1"}, {"SSFX_IL_QUALITY", "1"}, {"SSFX_AO_QUALITY", "1"}, {"SSFX_POM_REFINE", "1"},
            {"SSFX_TERRA_POM_REFINE", "1"}, {"SSFX_SSS_DIR_QUALITY", "1"}, {"SSFX_SSS_OMNI_QUALITY", "1"}, {nullptr, nullptr}};
        return m;
    }

    // r3/*.vs|ps|gs|hs|ds|cs, sorted
    inline std::vector<std::string> files(const fs::path &root)
    {
        std::vector<std::string> v;
        for (const auto &e : fs::directory_iterator(root / "r3"))
        {
            const std::string ext = e.path().extension().string();
            if (ext == ".vs" || ext == ".ps" || ext == ".gs" || ext == ".hs" || ext == ".ds" || ext == ".cs")
                v.push_back(e.path().filename().string());
        }
        std::sort(v.begin(), v.end());
        return v;
    }

    inline std::string read(const fs::path &p)
    {
        std::ifstream f(p, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(f), {});
    }

    // "vs_5_0" for "x.vs"
    inline std::string target(const std::string &file)
    {
        return file.substr(file.size() - 2) + "_5_0";
    }
} // namespace shader_tree
