// Milestone M4 test of the Vulkan backend's draw path through the D3D11 interface the engine uses:
// HLSL compiled by D3DCompile, input layout, a default (static) and a dynamic vertex buffer, an index
// buffer, constant buffers in VS and PS, a texture with initial data and a sampler, depth test, blend
// state, two render targets in sequence (render to texture, then sample it), readback.
// Run: XR_VK_VALIDATION=1 build/monolith/vk_draw   (exit code 0 = pass; validation errors fail it)
#include "../monolith/src/xrCore/xrCore_platform.h"
#include "vkCommonTypes.h"
#include "vkCore.h"
#include "vk_d3dcompiler.h"

#include <SDL3/SDL.h>

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>
#include <initializer_list>

static int failures = 0;
static int validation_errors = 0;
#define CHECK(c)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(c))                                                                                                      \
        {                                                                                                              \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c);                                                         \
            ++failures;                                                                                                \
        }                                                                                                              \
    } while (0)

static void on_log(const char *line)
{
    printf("%s\n", line);
    if (strstr(line, "validation error"))
        ++validation_errors;
}

static const char *kShader = R"(
cbuffer per_draw : register(b0) { float4 offset; };
cbuffer material { float4 tint; };
Texture2D s_base;
SamplerState smp_base;
struct v2p { float4 hpos : SV_Position; float2 tc : TEXCOORD0; };
v2p vs_main(float3 P : POSITION, float2 tc : TEXCOORD0)
{
    v2p o;
    o.hpos = float4(P.xy + offset.xy, P.z, 1);
    o.tc = tc;
    return o;
}
float4 ps_main(v2p I) : SV_Target
{
    return s_base.Sample(smp_base, I.tc) * tint;
}
)";

static ID3DBlob *compile(const char *entry, const char *target)
{
    ID3DBlob *code = nullptr, *err = nullptr;
    const HRESULT hr = D3DCompile(kShader, strlen(kShader), "", nullptr, nullptr, entry, target, D3DCOMPILE_PACK_MATRIX_ROW_MAJOR, 0, &code, &err);
    if (FAILED(hr))
        printf("D3DCompile %s: %s\n", entry, err ? static_cast<const char *>(err->GetBufferPointer()) : "?");
    if (err)
        err->Release();
    return code;
}

struct Vertex
{
    float x, y, z, u, v;
};

int main()
{
    xrvk::set_log(on_log);
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        printf("SKIP: SDL_Init(VIDEO) failed: %s\n", SDL_GetError());
        return 77;
    }
    ID3D11Device *dev = nullptr;
    ID3D11DeviceContext *ctx = nullptr;
    const D3D_FEATURE_LEVEL fl = D3D_FEATURE_LEVEL_11_0;
    CHECK(SUCCEEDED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, &fl, 1, D3D11_SDK_VERSION, &dev, nullptr, &ctx)));
    if (!dev)
        return 1;

    // shaders, input layout
    ID3DBlob *vsb = compile("vs_main", "vs_5_0"), *psb = compile("ps_main", "ps_5_0");
    CHECK(vsb && psb);
    if (!vsb || !psb)
        return 1;
    ID3D11VertexShader *vs = nullptr;
    ID3D11PixelShader *ps = nullptr;
    CHECK(SUCCEEDED(dev->CreateVertexShader(vsb->GetBufferPointer(), vsb->GetBufferSize(), nullptr, &vs)));
    CHECK(SUCCEEDED(dev->CreatePixelShader(psb->GetBufferPointer(), psb->GetBufferSize(), nullptr, &ps)));
    ID3DBlob *sig = nullptr;
    CHECK(SUCCEEDED(D3DGetInputSignatureBlob(vsb->GetBufferPointer(), vsb->GetBufferSize(), &sig)));
    const D3D11_INPUT_ELEMENT_DESC elems[] = {{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
                                              {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0}};
    ID3D11InputLayout *layout = nullptr;
    CHECK(SUCCEEDED(dev->CreateInputLayout(elems, 2, sig->GetBufferPointer(), sig->GetBufferSize(), &layout)));
    {
        // a layout missing an input the shader reads fails and clears the output (D3D11): the engine
        // (CBackend::ApplyVertexLayout) does not check the result and binds whatever it got
        ID3D11InputLayout *bad = reinterpret_cast<ID3D11InputLayout *>(uintptr_t(0x40000004d));
        CHECK(FAILED(dev->CreateInputLayout(elems, 1, sig->GetBufferPointer(), sig->GetBufferSize(), &bad)));
        CHECK(bad == nullptr);
    }

    // geometry: a static full-screen quad (indexed) and a dynamic copy of it nearer to the camera
    const Vertex quad[4] = {{-1, -1, 0.5f, 0, 1}, {-1, 1, 0.5f, 0, 0}, {1, 1, 0.5f, 1, 0}, {1, -1, 0.5f, 1, 1}};
    const uint16_t idx[6] = {0, 1, 2, 0, 2, 3};
    D3D11_BUFFER_DESC bd{sizeof(quad), D3D11_USAGE_DEFAULT, D3D11_BIND_VERTEX_BUFFER, 0, 0, 0};
    D3D11_SUBRESOURCE_DATA bdata{quad, 0, 0};
    ID3D11Buffer *vb = nullptr, *ib = nullptr, *dvb = nullptr, *cb_vs = nullptr, *cb_ps = nullptr;
    CHECK(SUCCEEDED(dev->CreateBuffer(&bd, &bdata, &vb)));
    bd = {sizeof(idx), D3D11_USAGE_DEFAULT, D3D11_BIND_INDEX_BUFFER, 0, 0, 0};
    bdata.pSysMem = idx;
    CHECK(SUCCEEDED(dev->CreateBuffer(&bd, &bdata, &ib)));
    bd = {sizeof(quad), D3D11_USAGE_DYNAMIC, D3D11_BIND_VERTEX_BUFFER, D3D11_CPU_ACCESS_WRITE, 0, 0};
    CHECK(SUCCEEDED(dev->CreateBuffer(&bd, nullptr, &dvb)));
    bd = {16, D3D11_USAGE_DYNAMIC, D3D11_BIND_CONSTANT_BUFFER, D3D11_CPU_ACCESS_WRITE, 0, 0};
    CHECK(SUCCEEDED(dev->CreateBuffer(&bd, nullptr, &cb_vs)));
    CHECK(SUCCEEDED(dev->CreateBuffer(&bd, nullptr, &cb_ps)));

    // a 2x2 white texture with initial data, and a point sampler
    const uint32_t white[4] = {0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff};
    D3D11_TEXTURE2D_DESC td{2, 2, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, {1, 0}, D3D11_USAGE_DEFAULT, D3D11_BIND_SHADER_RESOURCE, 0, 0};
    D3D11_SUBRESOURCE_DATA tdata{white, 8, 0};
    ID3D11Texture2D *tex = nullptr;
    CHECK(SUCCEEDED(dev->CreateTexture2D(&td, &tdata, &tex)));
    ID3D11ShaderResourceView *tex_srv = nullptr;
    CHECK(SUCCEEDED(dev->CreateShaderResourceView(tex, nullptr, &tex_srv)));
    D3D11_SAMPLER_DESC sd{};
    sd.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
    sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.MaxLOD = 3.402823466e+38f;
    ID3D11SamplerState *smp = nullptr;
    CHECK(SUCCEEDED(dev->CreateSamplerState(&sd, &smp)));

    // render targets: rt0 (rendered, then sampled), rt1 (final), depth
    const UINT W = 64, H = 64;
    td = {W, H, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, {1, 0}, D3D11_USAGE_DEFAULT, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE, 0, 0};
    ID3D11Texture2D *rt0 = nullptr, *rt1 = nullptr, *ztex = nullptr;
    CHECK(SUCCEEDED(dev->CreateTexture2D(&td, nullptr, &rt0)));
    CHECK(SUCCEEDED(dev->CreateTexture2D(&td, nullptr, &rt1)));
    ID3D11RenderTargetView *rtv0 = nullptr, *rtv1 = nullptr;
    ID3D11ShaderResourceView *rt0_srv = nullptr;
    CHECK(SUCCEEDED(dev->CreateRenderTargetView(rt0, nullptr, &rtv0)));
    CHECK(SUCCEEDED(dev->CreateRenderTargetView(rt1, nullptr, &rtv1)));
    CHECK(SUCCEEDED(dev->CreateShaderResourceView(rt0, nullptr, &rt0_srv)));
    td.Format = DXGI_FORMAT_R24G8_TYPELESS;
    td.BindFlags = D3D11_BIND_DEPTH_STENCIL;
    CHECK(SUCCEEDED(dev->CreateTexture2D(&td, nullptr, &ztex)));
    D3D11_DEPTH_STENCIL_VIEW_DESC dsvd{DXGI_FORMAT_D24_UNORM_S8_UINT, D3D11_DSV_DIMENSION_TEXTURE2D, 0, {}};
    ID3D11DepthStencilView *dsv = nullptr;
    CHECK(SUCCEEDED(dev->CreateDepthStencilView(ztex, &dsvd, &dsv)));

    D3D11_DEPTH_STENCIL_DESC dsd{};
    dsd.DepthEnable = TRUE;
    dsd.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
    dsd.DepthFunc = D3D11_COMPARISON_LESS;
    ID3D11DepthStencilState *dss = nullptr;
    CHECK(SUCCEEDED(dev->CreateDepthStencilState(&dsd, &dss)));
    D3D11_RASTERIZER_DESC rsd{};
    rsd.FillMode = D3D11_FILL_SOLID;
    rsd.CullMode = D3D11_CULL_NONE;
    rsd.DepthClipEnable = TRUE;
    ID3D11RasterizerState *rs = nullptr;
    CHECK(SUCCEEDED(dev->CreateRasterizerState(&rsd, &rs)));
    D3D11_BLEND_DESC bld{};
    bld.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    ID3D11BlendState *bs = nullptr;
    CHECK(SUCCEEDED(dev->CreateBlendState(&bld, &bs)));

    auto write_cb = [&](ID3D11Buffer *b, float x, float y, float z, float w) {
        D3D11_MAPPED_SUBRESOURCE m{};
        CHECK(SUCCEEDED(ctx->Map(b, 0, D3D11_MAP_WRITE_DISCARD, 0, &m)));
        const float v[4] = {x, y, z, w};
        memcpy(m.pData, v, 16);
        ctx->Unmap(b, 0);
    };

    const D3D11_VIEWPORT vp{0, 0, float(W), float(H), 0, 1};
    const UINT stride = sizeof(Vertex), offset = 0;
    const float black[4] = {0, 0, 0, 1};
    for (int frame = 0; frame < 4; ++frame) // several frames: heap slice reuse, per-frame state reset
    {
        // pass 1 into rt0: white texture * tint (0,1,0,1) = green, full screen
        ctx->OMSetRenderTargets(1, &rtv0, dsv);
        ctx->ClearRenderTargetView(rtv0, black);
        ctx->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.f, 0);
        ctx->RSSetViewports(1, &vp);
        ctx->RSSetState(rs);
        ctx->OMSetDepthStencilState(dss, 0);
        ctx->OMSetBlendState(bs, nullptr, 0xffffffff);
        ctx->IASetInputLayout(layout);
        ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        ctx->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
        ctx->IASetIndexBuffer(ib, DXGI_FORMAT_R16_UINT, 0);
        ctx->VSSetShader(vs, nullptr, 0);
        ctx->PSSetShader(ps, nullptr, 0);
        ctx->VSSetConstantBuffers(0, 1, &cb_vs);
        ctx->PSSetConstantBuffers(0, 1, &cb_ps); // `material` is the PS's only cbuffer: slot 0 of the PS
        ctx->PSSetShaderResources(0, 1, &tex_srv);
        ctx->PSSetSamplers(0, 1, &smp);
        write_cb(cb_vs, 0, 0, 0, 0);
        write_cb(cb_ps, 0, 1, 0, 1);
        ctx->DrawIndexed(6, 0, 0);

        // pass 2 into rt1: sample rt0 (green) * (1,1,1,1) everywhere; then the dynamic quad, nearer and
        // shifted to the left half, tinted (1,0.5,1,1): green * that = (0,128,0)
        ctx->OMSetRenderTargets(1, &rtv1, dsv);
        ctx->ClearRenderTargetView(rtv1, black);
        ctx->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH, 1.f, 0);
        ctx->PSSetShaderResources(0, 1, &rt0_srv);
        write_cb(cb_ps, 1, 1, 1, 1);
        ctx->DrawIndexed(6, 0, 0);
        D3D11_MAPPED_SUBRESOURCE m{};
        CHECK(SUCCEEDED(ctx->Map(dvb, 0, D3D11_MAP_WRITE_DISCARD, 0, &m)));
        Vertex near_quad[4];
        memcpy(near_quad, quad, sizeof(quad));
        for (Vertex &v : near_quad)
            v.z = 0.25f;
        memcpy(m.pData, near_quad, sizeof(near_quad));
        ctx->Unmap(dvb, 0);
        ctx->IASetVertexBuffers(0, 1, &dvb, &stride, &offset);
        write_cb(cb_vs, -1, 0, 0, 0);
        write_cb(cb_ps, 1, 0.5f, 1, 1);
        ctx->DrawIndexed(6, 0, 0);
        ctx->PSSetShaderResources(0, 1, &tex_srv); // unbind rt0 before it is a render target again
        xrvk::core()->submit();
        xrvk::core()->next_frame();
    }

    // readback of rt1
    td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    td.Usage = D3D11_USAGE_STAGING;
    td.BindFlags = 0;
    td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    ID3D11Texture2D *staging = nullptr;
    CHECK(SUCCEEDED(dev->CreateTexture2D(&td, nullptr, &staging)));
    ctx->CopyResource(staging, rt1);
    D3D11_MAPPED_SUBRESOURCE m{};
    CHECK(SUCCEEDED(ctx->Map(staging, 0, D3D11_MAP_READ, 0, &m)));
    auto px = [&](int x, int y) { return static_cast<const uint8_t *>(m.pData) + y * m.RowPitch + x * 4; };
    const uint8_t *l = px(16, 32), *r = px(48, 32);
    printf("left (16,32) = %u %u %u %u, right (48,32) = %u %u %u %u\n", l[0], l[1], l[2], l[3], r[0], r[1], r[2], r[3]);
    CHECK(l[0] == 0 && std::abs(int(l[1]) - 128) <= 1 && l[2] == 0 && l[3] == 255); // dynamic quad
    CHECK(r[0] == 0 && r[1] == 255 && r[2] == 0 && r[3] == 255);                    // rt0 sampled: green
    ctx->Unmap(staging, 0);

    // the engine's "RAM textures": write-only staging textures (process memory) copied to the GPU on first
    // use; 64x64 goes through the host heap, 512x512 (1 MB) through a buffer of its own
    for (UINT size : {64u, 512u})
    {
        std::vector<uint32_t> texels(size * size);
        for (UINT i = 0; i < texels.size(); ++i)
            texels[i] = i * 2654435761u;
        D3D11_TEXTURE2D_DESC sd{size, size, 1, 1, DXGI_FORMAT_R8G8B8A8_UNORM, {1, 0}, D3D11_USAGE_STAGING, 0, D3D11_CPU_ACCESS_WRITE, 0};
        const D3D11_SUBRESOURCE_DATA init{texels.data(), size * 4, 0};
        ID3D11Texture2D *ram = nullptr, *gpu = nullptr, *back = nullptr;
        CHECK(SUCCEEDED(dev->CreateTexture2D(&sd, &init, &ram)));
        D3D11_TEXTURE2D_DESC gd = sd;
        gd.Usage = D3D11_USAGE_DEFAULT;
        gd.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        gd.CPUAccessFlags = 0;
        CHECK(SUCCEEDED(dev->CreateTexture2D(&gd, nullptr, &gpu)));
        D3D11_TEXTURE2D_DESC bd = sd;
        bd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
        CHECK(SUCCEEDED(dev->CreateTexture2D(&bd, nullptr, &back)));
        if (!ram || !gpu || !back)
            break;
        ctx->CopyResource(gpu, ram);
        ram->Release(); // as ProcessStaging does right after the copy
        ctx->CopyResource(back, gpu);
        D3D11_MAPPED_SUBRESOURCE bm{};
        CHECK(SUCCEEDED(ctx->Map(back, 0, D3D11_MAP_READ, 0, &bm)));
        bool same = true;
        for (UINT y = 0; y < size && same; ++y)
            same = memcmp(static_cast<const uint8_t *>(bm.pData) + y * bm.RowPitch, texels.data() + y * size, size * 4) == 0;
        CHECK(same);
        ctx->Unmap(back, 0);
        back->Release();
        gpu->Release();
    }

    for (IUnknown *o : std::initializer_list<IUnknown *>{staging, bs, rs, dss, dsv, ztex, rt0_srv, rtv1, rtv0, rt1, rt0, smp, tex_srv, tex,
                                                        cb_ps, cb_vs, dvb, ib, vb, layout, sig, ps, vs, psb, vsb})
        if (o)
            o->Release();
    ctx->Release();
    dev->Release();
    xrvk::destroy_core();
    SDL_Quit();
    printf("validation errors: %d\n", validation_errors);
    CHECK(validation_errors == 0);
    printf(failures ? "FAILED (%d)\n" : "all checks passed\n", failures);
    return failures ? 1 : 0;
}
