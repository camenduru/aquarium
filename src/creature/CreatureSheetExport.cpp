// ================================================================
// AQUARIUM - PNG encoder (see the header for why this is a shim)
// Copied from RoguelikeGame/src/creature/CreatureSheetExport.cpp: the
// stored-deflate zlib stream keeps the project dependency-free.
// ================================================================

#include "creature/CreatureSheetExport.h"

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <system_error>

namespace creature {
namespace {

using u32 = std::uint32_t;

u32 Crc32(const unsigned char* data, size_t n, u32 crc = 0xFFFFFFFFu)
{
    static u32 table[256];
    static bool built = false;
    if (!built)
    {
        for (u32 i = 0; i < 256; i++)
        {
            u32 c = i;
            for (int k = 0; k < 8; k++)
                c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            table[i] = c;
        }
        built = true;
    }
    for (size_t i = 0; i < n; i++)
        crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    return crc;
}

u32 Adler32(const unsigned char* data, size_t n)
{
    u32 a = 1, b = 0;
    for (size_t i = 0; i < n; i++)
    {
        a = (a + data[i]) % 65521u;
        b = (b + a) % 65521u;
    }
    return (b << 16) | a;
}

void Put32BE(std::vector<unsigned char>& v, u32 x)
{
    v.push_back((unsigned char)(x >> 24));
    v.push_back((unsigned char)(x >> 16));
    v.push_back((unsigned char)(x >> 8));
    v.push_back((unsigned char)x);
}

void PutChunk(std::vector<unsigned char>& out, const char type[4],
              const unsigned char* data, size_t n)
{
    Put32BE(out, (u32)n);
    const size_t crcStart = out.size();
    out.insert(out.end(), type, type + 4);
    if (n) out.insert(out.end(), data, data + n);
    const u32 crc = Crc32(out.data() + crcStart, out.size() - crcStart) ^ 0xFFFFFFFFu;
    Put32BE(out, crc);
}

void PutStoredZlib(std::vector<unsigned char>& out, const unsigned char* data, size_t n)
{
    out.push_back(0x78);   // CMF: deflate, 32K window
    out.push_back(0x01);   // FLG: no dict, fastest - (0x7801 % 31) == 0
    size_t off = 0;
    do
    {
        const size_t len = (n - off > 65535) ? 65535 : (n - off);
        const bool last = (off + len == n);
        out.push_back(last ? 1 : 0);              // BFINAL | BTYPE=00 (stored)
        out.push_back((unsigned char)(len & 0xFF));
        out.push_back((unsigned char)(len >> 8));
        out.push_back((unsigned char)(~len & 0xFF));
        out.push_back((unsigned char)((~len >> 8) & 0xFF));
        if (len) out.insert(out.end(), data + off, data + off + len);
        off += len;
    } while (off < n);
    Put32BE(out, Adler32(data, n));
}

} // namespace

bool EncodePng(const unsigned char* rgba, int width, int height,
               std::vector<unsigned char>& out)
{
    if (rgba == nullptr || width < 1 || height < 1) return false;

    std::vector<unsigned char> raw;
    raw.reserve(size_t(height) * (1 + size_t(width) * 4));
    for (int y = 0; y < height; y++)
    {
        raw.push_back(0);   // filter: None
        const unsigned char* row = rgba + size_t(y) * width * 4;
        raw.insert(raw.end(), row, row + size_t(width) * 4);
    }

    out.clear();
    out.reserve(raw.size() + 1024);
    static const unsigned char kSig[8] = { 137, 80, 78, 71, 13, 10, 26, 10 };
    out.insert(out.end(), kSig, kSig + 8);

    unsigned char ihdr[13];
    ihdr[0] = (unsigned char)((width >> 24) & 0xFF);
    ihdr[1] = (unsigned char)((width >> 16) & 0xFF);
    ihdr[2] = (unsigned char)((width >> 8) & 0xFF);
    ihdr[3] = (unsigned char)(width & 0xFF);
    ihdr[4] = (unsigned char)((height >> 24) & 0xFF);
    ihdr[5] = (unsigned char)((height >> 16) & 0xFF);
    ihdr[6] = (unsigned char)((height >> 8) & 0xFF);
    ihdr[7] = (unsigned char)(height & 0xFF);
    ihdr[8] = 8;    // bit depth
    ihdr[9] = 6;    // truecolour with alpha
    ihdr[10] = 0;
    ihdr[11] = 0;
    ihdr[12] = 0;
    PutChunk(out, "IHDR", ihdr, sizeof(ihdr));

    std::vector<unsigned char> z;
    PutStoredZlib(z, raw.data(), raw.size());
    PutChunk(out, "IDAT", z.data(), z.size());
    PutChunk(out, "IEND", nullptr, 0);
    return true;
}

bool WritePngFile(const unsigned char* rgba, int width, int height, const std::string& path)
{
    std::vector<unsigned char> png;
    if (!EncodePng(rgba, width, height, png)) return false;

    std::error_code ec;
    const std::filesystem::path p(path);
    if (p.has_parent_path())
        std::filesystem::create_directories(p.parent_path(), ec);

    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    if (!f.is_open()) return false;
    f.write(reinterpret_cast<const char*>(png.data()), std::streamsize(png.size()));
    return f.good();
}

} // namespace creature
