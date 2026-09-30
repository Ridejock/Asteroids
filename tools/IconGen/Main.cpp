// Writes the game's icon files from the procedural icon (src/Shared/Icon.cpp):
//   <dir>/icon.ico   every size Windows asks for (16 ... 256), for the .exe resource
//   <dir>/icon.png   256 x 256, e.g. for the itch.io page
// Run it via the UpdateIcon target after changing Icon.cpp; the files are committed, so normal
// builds don't need it:  cmake --build --preset release --target UpdateIcon

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>

#include <Emerald/Core/Log.h>

#include "Icon.h"

namespace {

// An .ico file may hold PNG images directly (Windows Vista and later): a 6-byte header, one
// 16-byte directory entry per image, then the PNG files.
void PutU16(std::vector<u8>& out, u32 v)
{
    out.push_back(static_cast<u8>(v & 0xFF));
    out.push_back(static_cast<u8>((v >> 8) & 0xFF));
}

void PutU32(std::vector<u8>& out, u32 v)
{
    PutU16(out, v & 0xFFFF);
    PutU16(out, v >> 16);
}

std::vector<u8> ReadFile(const std::filesystem::path& path)
{
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

} // namespace

int main(int argc, char** argv)
{
    if (argc != 2) {
        std::fprintf(stderr, "usage: IconGen <output folder>\n");
        return 1;
    }
    Emerald::Log::Init({}); // console only
    const std::filesystem::path dir = argv[1];
    std::filesystem::create_directories(dir);

    const u32 sizes[] = {16, 20, 24, 32, 40, 48, 64, 128, 256};
    std::vector<std::vector<u8>> pngs;
    const std::filesystem::path temp = dir / "icon-temp.png";
    for (const u32 size : sizes) {
        if (!Asteroids::MakeIcon(size).SavePNG(temp))
            return 1;
        pngs.push_back(ReadFile(temp));
    }
    std::filesystem::remove(temp);
    if (!Asteroids::MakeIcon(256).SavePNG(dir / "icon.png"))
        return 1;

    std::vector<u8> ico;
    const u32 count = static_cast<u32>(std::size(sizes));
    PutU16(ico, 0);     // reserved
    PutU16(ico, 1);     // type: icon
    PutU16(ico, count); // images
    u32 offset = 6 + 16 * count;
    for (u32 i = 0; i < count; ++i) {
        ico.push_back(static_cast<u8>(sizes[i] >= 256 ? 0 : sizes[i])); // 0 means 256
        ico.push_back(static_cast<u8>(sizes[i] >= 256 ? 0 : sizes[i]));
        ico.push_back(0); // no palette
        ico.push_back(0); // reserved
        PutU16(ico, 1);   // color planes
        PutU16(ico, 32);  // bits per pixel
        PutU32(ico, static_cast<u32>(pngs[i].size()));
        PutU32(ico, offset);
        offset += static_cast<u32>(pngs[i].size());
    }
    for (const std::vector<u8>& png : pngs)
        ico.insert(ico.end(), png.begin(), png.end());

    std::ofstream out(dir / "icon.ico", std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(ico.data()), static_cast<std::streamsize>(ico.size()));
    std::printf("Wrote %s (%zu bytes) and icon.png\n", (dir / "icon.ico").string().c_str(),
                ico.size());
    Emerald::Log::Shutdown();
    return out ? 0 : 1;
}
