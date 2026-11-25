// Unit test of the host heap (monolith/src/Layers/xrRenderVK/vkHeap.cpp): small slices from linear pages,
// large ones from virtual blocks, reuse only after collect().
// Run: build/monolith/vk_heap   (exit code 0 = pass, 77 = no Vulkan device)
#include "../monolith/src/xrCore/xrCore_platform.h"
#include "vkCommonTypes.h"
#include "vkCore.h"
#include "vkHeap.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

static int failures = 0;
#define CHECK(c)                                               \
    do                                                         \
    {                                                          \
        if (!(c))                                              \
        {                                                      \
            printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); \
            ++failures;                                        \
        }                                                      \
    } while (0)

static bool overlap(const xrvk::Slice &a, const xrvk::Slice &b)
{
    return a.buffer == b.buffer && a.offset < b.offset + b.size && b.offset < a.offset + a.size;
}

int main()
{
    xrvk::set_log([](const char *line) { printf("%s\n", line); });
    if (!SDL_Init(SDL_INIT_VIDEO))
    {
        printf("SKIP: SDL_Init(VIDEO) failed: %s\n", SDL_GetError());
        return 77;
    }
    ID3D11Device *dev = nullptr;
    ID3D11DeviceContext *ctx = nullptr;
    const D3D_FEATURE_LEVEL fl = D3D_FEATURE_LEVEL_11_0;
    if (FAILED(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, &fl, 1, D3D11_SDK_VERSION, &dev, nullptr, &ctx)))
    {
        printf("SKIP: no device\n");
        return 77;
    }
    xrvk::HostHeap heap; // a heap of its own: the device's is in use by the context
    heap.init(xrvk::core()->vma);

    // small slices: aligned, mapped, disjoint, writable
    std::vector<xrvk::Slice> live;
    for (VkDeviceSize size : {1, 16, 255, 256, 4096, 65536, 256 << 10, 0})
    {
        const xrvk::Slice s = heap.alloc(size);
        CHECK(s && s.ptr && s.offset % 256 == 0 && s.size == size);
        if (s.size)
            memset(s.ptr, 0xab, s.size);
        for (const xrvk::Slice &o : live)
            CHECK(!overlap(s, o));
        live.push_back(s);
    }

    // streaming: 2000 x 64 KB released at increasing timeline values and collected two "frames" later
    // must recycle pages, not grow (125 MB streamed)
    for (const xrvk::Slice &s : live)
        heap.release(s, 1);
    heap.collect(1);
    const VkDeviceSize before = heap.bytes_in_pages();
    for (uint64_t frame = 2; frame < 2002; ++frame)
    {
        const xrvk::Slice s = heap.alloc(64 << 10);
        CHECK(s);
        heap.release(s, frame);
        heap.collect(frame - 2);
    }
    CHECK(heap.bytes_in_pages() <= before + (32ull << 20));
    printf("streamed 125 MB, heap %llu -> %llu MB\n", (unsigned long long)(before >> 20),
           (unsigned long long)(heap.bytes_in_pages() >> 20));

    // a slice that is not collected keeps its bytes while the rest of the heap streams past it
    const xrvk::Slice pinned = heap.alloc(1024);
    memset(pinned.ptr, 0x5a, 1024);
    for (uint64_t frame = 3000; frame < 3500; ++frame)
    {
        const xrvk::Slice s = heap.alloc(64 << 10);
        CHECK(!overlap(s, pinned));
        memset(s.ptr, 0x11, s.size);
        heap.release(s, frame);
        heap.collect(frame);
    }
    CHECK(std::all_of(pinned.ptr, pinned.ptr + 1024, [](uint8_t b) { return b == 0x5a; }));
    heap.release(pinned, 4000);
    heap.collect(4000);

    // large slices come from the virtual blocks and are reused after collect
    const xrvk::Slice big = heap.alloc(8 << 20);
    CHECK(big && big.va && big.size == (8u << 20));
    memset(big.ptr, 0x22, big.size);
    heap.release(big, 5000);
    heap.collect(5000);
    const VkDeviceSize after_big = heap.bytes_in_pages();
    for (int i = 0; i < 10; ++i)
    {
        const xrvk::Slice b = heap.alloc(8 << 20);
        CHECK(b);
        heap.release(b, 5001 + i);
        heap.collect(5001 + i);
    }
    CHECK(heap.bytes_in_pages() == after_big);

    heap.destroy(xrvk::core()->vma);
    CHECK(heap.bytes_in_pages() == 0);
    ctx->Release();
    dev->Release();
    printf(failures ? "FAILED (%d)\n" : "all checks passed\n", failures);
    return failures ? 1 : 0;
}
