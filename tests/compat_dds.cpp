// Tests for monolith/src/Common/linux/dds_codec.{h,cpp}.
// Build: clang++ -std=c++20 -Imonolith/src/Common/linux tests/compat_dds.cpp monolith/src/Common/linux/dds_codec.cpp
// Usage: compat_dds [list-of-dds-paths.txt]   (the files are only read)
#include "dds_codec.h"
#include <cstdio>
#include <fstream>
#include <iterator>
#include <map>

using namespace xr_dds;
static int failures = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL %s:%d %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

static void roundtrip_written_dds()
{
    // 8x4 BGRA8 with 2 mips, written then parsed back.
    std::vector<std::vector<uint8_t>> levels = {std::vector<uint8_t>(8 * 4 * 4), std::vector<uint8_t>(4 * 2 * 4)};
    for (size_t i = 0; i < levels[0].size(); ++i) levels[0][i] = uint8_t(i);
    auto file = write_dds_2d(FMT_B8G8R8A8_UNORM, 8, 4, 2, 1, false, levels);
    Image img; std::string err;
    CHECK(parse(file.data(), file.size(), img, err));
    CHECK(img.format == FMT_B8G8R8A8_UNORM && img.width == 8 && img.height == 4 && img.mip_levels == 2);
    CHECK(img.subresources.size() == 2 && img.subresources[1].width == 4 && img.subresources[1].row_pitch == 16);
    CHECK(!memcmp(img.subresources[0].data, levels[0].data(), levels[0].size()));
    // BC1 with DX10-only format falls back to the extension header
    auto f2 = write_dds_2d(FMT_R16G16B16A16_FLOAT, 4, 4, 1, 6, true, std::vector<std::vector<uint8_t>>(6, std::vector<uint8_t>(128)));
    CHECK(parse(f2.data(), f2.size(), img, err));
    CHECK(img.cube && img.array_size == 6 && img.format == FMT_R16G16B16A16_FLOAT);
    // truncated
    CHECK(!parse(file.data(), file.size() - 1, img, err));
}

static void bc1()
{
    uint8_t px[64];
    for (int i = 0; i < 16; ++i) { px[i * 4] = 255; px[i * 4 + 1] = 0; px[i * 4 + 2] = 0; px[i * 4 + 3] = 255; }
    uint8_t out[8];
    encode_bc1_block(px, out);
    CHECK((out[0] | out[1] << 8) == 0xF800); // pure red endpoint
    for (int i = 4; i < 8; ++i) CHECK(out[i] == 0);
    // two-colour block: black/white must map to the endpoints exactly
    for (int i = 0; i < 16; ++i) memset(px + i * 4, (i & 1) ? 255 : 0, 3);
    encode_bc1_block(px, out);
    const uint16_t c0 = out[0] | out[1] << 8, c1 = out[2] | out[3] << 8;
    CHECK(c0 == 0xFFFF && c1 == 0);
    CHECK(out[4] == 0x11); // texels 0..3 = black,white,black,white = c1,c0,c1,c0 -> indices 1,0,1,0 = 0b00010001
}

static void images()
{
    uint8_t rgba[2 * 2 * 4] = {255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 255, 255};
    auto bmp = write_bmp(rgba, 2, 2);
    CHECK(bmp.size() == 54 + 8 * 2 && bmp[0] == 'B' && bmp[54] == 255 && bmp[56] == 0); // bottom row first: blue
    auto png = write_png(rgba, 2, 2);
    CHECK(png.size() > 8 && png[1] == 'P');
    uint8_t small[4];
    resize_rgba8(rgba, 2, 2, small, 1, 1);
    CHECK(small[0] == 128 && small[1] == 128 && small[2] == 128 && small[3] == 255);
}

int main(int argc, char** argv)
{
    roundtrip_written_dds();
    bc1();
    images();
    if (argc > 1)
    {
        std::ifstream list(argv[1]);
        std::string path;
        std::map<std::string, int> stats;
        int ok = 0, bad = 0;
        while (std::getline(list, path))
        {
            std::ifstream f(path, std::ios::binary);
            std::vector<uint8_t> data((std::istreambuf_iterator<char>(f)), {});
            Image img; std::string err;
            if (parse(data.data(), data.size(), img, err))
            {
                ++ok;
                ++stats["fmt " + std::to_string(img.format) + (img.cube ? " cube" : "") + (img.convert != Convert::None ? " conv" : "")];
                for (auto& s : img.subresources) { uint32_t p; (void)convert_subresource(img, s, p); }
            }
            else { ++bad; ++stats["error: " + err]; }
        }
        for (auto& [k, v] : stats) printf("%6d  %s\n", v, k.c_str());
        printf("real files: %d parsed, %d rejected\n", ok, bad);
    }
    printf(failures ? "FAILED (%d)\n" : "all checks passed\n", failures);
    return failures ? 1 : 0;
}
