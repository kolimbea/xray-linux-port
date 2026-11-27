// Milestone M2 test of the Vulkan device layer (monolith/src/Layers/xrRenderVK, library xrVK_core):
// the DXGI/D3D11 call sequence dx10HW.cpp makes (factory -> adapter -> device -> swap chain -> back
// buffer -> RTV/DSV -> clear -> present), plus a readback of the cleared back buffer.
// Run: XR_VK_VALIDATION=1 build/monolith/vk_smoke   (exit code 0 = pass; validation errors fail it)
#include "../monolith/src/xrCore/xrCore_platform.h"
#include "vkCommonTypes.h"
#include "vkCore.h"

#include <SDL3/SDL.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static int failures = 0;
static int validation_errors = 0;
#define CHECK(c)                                                       \
    do                                                                 \
    {                                                                  \
        if (!(c))                                                      \
        {                                                              \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c);         \
            ++failures;                                                \
        }                                                              \
    } while (0)

static void on_log(const char* line)
{
    printf("%s\n", line);
    if (strstr(line, "validation error"))
        ++validation_errors;
}

int main()
{
    xrvk::set_log(on_log);
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        printf("SKIP: SDL_Init(VIDEO) failed: %s\n", SDL_GetError());
        return 77;
    }
    SDL_Window* w = SDL_CreateWindow("vk_smoke", 320, 240, SDL_WINDOW_VULKAN);
    CHECK(w);

    IDXGIFactory2* factory = nullptr;
    CHECK(SUCCEEDED(CreateDXGIFactory2(0, __uuidof(IDXGIFactory2), reinterpret_cast<void**>(&factory))));
    IDXGIAdapter1* adapter = nullptr;
    CHECK(SUCCEEDED(factory->EnumAdapters1(0, &adapter)));
    DXGI_ADAPTER_DESC1 ad{};
    CHECK(SUCCEEDED(adapter->GetDesc1(&ad)));
    char name[128];
    for (int i = 0; i < 128; ++i)
        name[i] = char(ad.Description[i]);
    printf("adapter 0: %s, vendor 0x%04x, %zu MB dedicated\n", name, ad.VendorId, size_t(ad.DedicatedVideoMemory >> 20));

    const D3D_FEATURE_LEVEL fl = D3D_FEATURE_LEVEL_11_0;
    D3D_FEATURE_LEVEL got{};
    ID3D11Device* dev0 = nullptr;
    ID3D11DeviceContext* ctx0 = nullptr;
    CHECK(SUCCEEDED(D3D11CreateDevice(adapter, D3D_DRIVER_TYPE_UNKNOWN, nullptr, 0, &fl, 1, D3D11_SDK_VERSION, &dev0, &got, &ctx0)));
    CHECK(got == D3D_FEATURE_LEVEL_11_0);
    ID3D11Device1* dev = nullptr;
    ID3D11DeviceContext1* ctx = nullptr;
    CHECK(SUCCEEDED(dev0->QueryInterface(&dev)));
    CHECK(SUCCEEDED(ctx0->QueryInterface(&ctx)));
    ID3DUserDefinedAnnotation* ann = nullptr;
    CHECK(SUCCEEDED(ctx->QueryInterface(__uuidof(ID3DUserDefinedAnnotation), reinterpret_cast<void**>(&ann))));

    DXGI_SWAP_CHAIN_DESC1 sd{};
    sd.Width = 320;
    sd.Height = 240;
    sd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.SampleDesc = {1, 0};
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.BufferCount = 2;
    sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    IDXGISwapChain1* sc = nullptr;
    CHECK(SUCCEEDED(factory->CreateSwapChainForHwnd(dev, static_cast<HWND>(w), &sd, nullptr, nullptr, &sc)));
    IDXGISwapChain3* sc3 = nullptr;
    CHECK(SUCCEEDED(sc->QueryInterface(&sc3)));
    UINT cs = 0;
    CHECK(SUCCEEDED(sc3->CheckColorSpaceSupport(DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709, &cs)) && cs);
    CHECK(SUCCEEDED(sc3->SetColorSpace1(DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709)));

    ID3D11Texture2D* back = nullptr;
    CHECK(SUCCEEDED(sc->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&back))));
    ID3D11RenderTargetView* rtv = nullptr;
    CHECK(SUCCEEDED(dev->CreateRenderTargetView(back, nullptr, &rtv)));

    D3D11_TEXTURE2D_DESC dd{};
    dd.Width = 320;
    dd.Height = 240;
    dd.MipLevels = 1;
    dd.ArraySize = 1;
    dd.Format = DXGI_FORMAT_R24G8_TYPELESS;
    dd.SampleDesc = {1, 0};
    dd.Usage = D3D11_USAGE_DEFAULT;
    dd.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
    ID3D11Texture2D* depth = nullptr;
    CHECK(SUCCEEDED(dev->CreateTexture2D(&dd, nullptr, &depth)));
    D3D11_DEPTH_STENCIL_VIEW_DESC dsvd{};
    dsvd.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsvd.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    ID3D11DepthStencilView* dsv = nullptr;
    CHECK(SUCCEEDED(dev->CreateDepthStencilView(depth, &dsvd, &dsv)));

    // a few frames: clear + present
    const float colour[4] = {0.25f, 0.5f, 0.75f, 1.f};
    for (int frame = 0; frame < 5; ++frame)
    {
        ctx->OMSetRenderTargets(1, &rtv, dsv);
        ctx->ClearRenderTargetView(rtv, colour);
        ctx->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.f, 0);
        CHECK(SUCCEEDED(sc->Present(1, 0)));
        SDL_PumpEvents();
    }

    // gamma control (the engine's brightness/contrast/gamma): the output offers control points and
    // accepts a curve; a non-identity curve turns on the present pass, an identity one turns it off
    {
        IDXGIOutput *out = nullptr;
        CHECK(SUCCEEDED(sc->GetContainingOutput(&out)));
        if (out)
        {
            DXGI_GAMMA_CONTROL_CAPABILITIES caps{};
            CHECK(SUCCEEDED(out->GetGammaControlCapabilities(&caps)));
            CHECK(caps.NumGammaControlPoints >= 2 && caps.MaxConvertedValue == 1.f && caps.ControlPointPositions[0] == 0.f);
            static DXGI_GAMMA_CONTROL g{};
            for (UINT i = 0; i < caps.NumGammaControlPoints; ++i)
                g.GammaCurve[i] = {1.f - caps.ControlPointPositions[i], caps.ControlPointPositions[i], 0.5f};
            CHECK(SUCCEEDED(out->SetGammaControl(&g)));
            for (int frame = 0; frame < 3; ++frame)
            {
                ctx->OMSetRenderTargets(1, &rtv, dsv);
                ctx->ClearRenderTargetView(rtv, colour);
                CHECK(SUCCEEDED(sc->Present(1, 0)));
            }
            for (UINT i = 0; i < caps.NumGammaControlPoints; ++i)
                g.GammaCurve[i] = {caps.ControlPointPositions[i], caps.ControlPointPositions[i], caps.ControlPointPositions[i]};
            CHECK(SUCCEEDED(out->SetGammaControl(&g)));
            CHECK(SUCCEEDED(sc->Present(1, 0)));
            out->Release();
        }
    }

    // VK_SMOKE_HOLD=<seconds>: keep presenting so the window can be looked at
    if (const char *hold = getenv("VK_SMOKE_HOLD"))
    {
        const uint64_t until = SDL_GetTicks() + uint64_t(atoi(hold)) * 1000;
        while (SDL_GetTicks() < until)
        {
            ctx->OMSetRenderTargets(1, &rtv, dsv);
            ctx->ClearRenderTargetView(rtv, colour);
            CHECK(SUCCEEDED(sc->Present(1, 0)));
            SDL_PumpEvents();
        }
    }

    // readback of the back buffer (what screenshots do)
    D3D11_TEXTURE2D_DESC bd;
    back->GetDesc(&bd);
    bd.Usage = D3D11_USAGE_STAGING;
    bd.BindFlags = 0;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    ID3D11Texture2D* staging = nullptr;
    CHECK(SUCCEEDED(dev->CreateTexture2D(&bd, nullptr, &staging)));
    ctx->ClearRenderTargetView(rtv, colour);
    ctx->CopyResource(staging, back);
    D3D11_MAPPED_SUBRESOURCE m{};
    CHECK(SUCCEEDED(ctx->Map(staging, 0, D3D11_MAP_READ, 0, &m)));
    const uint8_t* px = static_cast<const uint8_t*>(m.pData) + 120 * m.RowPitch + 160 * 4;
    printf("pixel (160,120) = %u %u %u %u, row pitch %u\n", px[0], px[1], px[2], px[3], m.RowPitch);
    CHECK(std::abs(int(px[0]) - 64) <= 1 && std::abs(int(px[1]) - 128) <= 1 && std::abs(int(px[2]) - 191) <= 1 && px[3] == 255);
    CHECK(m.RowPitch == 320 * 4);
    ctx->Unmap(staging, 0);

    // resize path (window size change)
    rtv->Release();
    back->Release();
    CHECK(SUCCEEDED(sc->ResizeBuffers(2, 400, 300, DXGI_FORMAT_UNKNOWN, 0)));
    CHECK(SUCCEEDED(sc->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&back))));
    back->GetDesc(&bd);
    CHECK(bd.Width == 400 && bd.Height == 300);
    CHECK(SUCCEEDED(dev->CreateRenderTargetView(back, nullptr, &rtv)));
    ctx->ClearRenderTargetView(rtv, colour);
    CHECK(SUCCEEDED(sc->Present(0, 0))); // sync interval 0: mailbox/immediate swapchain

    staging->Release();
    dsv->Release();
    depth->Release();
    rtv->Release();
    back->Release();
    sc3->Release();
    sc->Release();
    ann->Release();
    ctx->Release();
    dev->Release();
    ctx0->Release();
    dev0->Release();
    adapter->Release();
    factory->Release();
    xrvk::destroy_core();
    SDL_DestroyWindow(w);
    SDL_Quit();

    printf("validation errors: %d\n", validation_errors);
    CHECK(validation_errors == 0);
    printf(failures ? "FAILED (%d)\n" : "all checks passed\n", failures);
    return failures ? 1 : 0;
}
