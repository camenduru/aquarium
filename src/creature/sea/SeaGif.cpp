// ================================================================
// SEA CREATURES - GIF89a writer
// Ported from export.py's save_gif()/export_gifs() (MIT).
// ================================================================
#include "creature/sea/SeaGif.h"

#include "creature/sea/SeaSprite.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <unordered_map>

namespace sea {
namespace {

void Put16(std::vector<unsigned char>& o, int v) {
    o.push_back((unsigned char)(v & 0xff));
    o.push_back((unsigned char)((v >> 8) & 0xff));
}

bool ParseHex(const std::string& s, int& r, int& g, int& b) {
    if (s.size() != 7 || s[0] != '#') return false;
    auto hex = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    const int v[6] = {hex(s[1]), hex(s[2]), hex(s[3]), hex(s[4]), hex(s[5]), hex(s[6])};
    for (int x : v) if (x < 0) return false;
    r = v[0] * 16 + v[1];
    g = v[2] * 16 + v[3];
    b = v[4] * 16 + v[5];
    return true;
}

struct BitWriter {
    std::vector<unsigned char>& out;
    std::uint32_t buf = 0;
    int bits = 0;
    explicit BitWriter(std::vector<unsigned char>& o) : out(o) {}
    void Write(int code, int n) {
        buf |= (std::uint32_t)code << bits;
        bits += n;
        while (bits >= 8) {
            out.push_back((unsigned char)(buf & 0xff));
            buf >>= 8;
            bits -= 8;
        }
    }
    void Flush() {
        if (bits > 0) out.push_back((unsigned char)(buf & 0xff));
        buf = 0;
        bits = 0;
    }
};

/// GIF LZW: clear/end codes, variable code width, 4096-entry dictionary.
void LzwEncode(const std::vector<unsigned char>& idx, int minCode, std::vector<unsigned char>& out) {
    const int clear = 1 << minCode;
    const int end = clear + 1;
    int codeSize = minCode + 1;
    int next = end + 1;
    BitWriter bw(out);
    std::unordered_map<int, int> dict;
    dict.reserve(4096);
    bw.Write(clear, codeSize);
    if (idx.empty()) { bw.Write(end, codeSize); bw.Flush(); return; }
    int prefix = idx[0];
    for (size_t i = 1; i < idx.size(); i++) {
        const int k = idx[i];
        const int key = (prefix << 8) | k;
        const auto it = dict.find(key);
        if (it != dict.end()) { prefix = it->second; continue; }
        bw.Write(prefix, codeSize);
        if (next < 4096) {
            dict[key] = next;
            next++;
            if (next > (1 << codeSize) && codeSize < 12) codeSize++;
        } else {
            bw.Write(clear, codeSize);
            dict.clear();
            next = end + 1;
            codeSize = minCode + 1;
        }
        prefix = k;
    }
    bw.Write(prefix, codeSize);
    bw.Write(end, codeSize);
    bw.Flush();
}

int Nearest(const std::vector<SeaRgba>& table, int r, int g, int b) {
    int best = 0;
    long bestD = 1L << 30;
    for (size_t i = 0; i < table.size(); i++) {
        const long dr = (long)table[i].r - r;
        const long dg = (long)table[i].g - g;
        const long db = (long)table[i].b - b;
        const long d = dr * dr + dg * dg + db * db;
        if (d < bestD) { bestD = d; best = (int)i; }
    }
    return best;
}

} // namespace

bool WriteSeaGif(const std::vector<std::vector<unsigned char>>& frames, int width, int height,
                 const std::vector<SeaRgba>& palette, double fps, int scale,
                 const std::string& path, const std::string& background, std::string& error) {
    if (frames.empty() || width <= 0 || height <= 0) { error = "no frames"; return false; }
    scale = ClampI(scale, 1, 8);
    int bgR = 0, bgG = 0, bgB = 0;
    const bool hasBg = !background.empty() && ParseHex(background, bgR, bgG, bgB);

    // Colour table: index 0 = transparent (or background), 1..N = sheet palette.
    std::vector<SeaRgba> table;
    table.push_back(SeaRgba((unsigned char)(hasBg ? bgR : 0), (unsigned char)(hasBg ? bgG : 0),
                            (unsigned char)(hasBg ? bgB : 0), 255));
    for (const SeaRgba& c : palette) {
        if (table.size() >= 256) break;
        table.push_back(c);
    }
    int tableSize = 2;
    while (tableSize < (int)table.size()) tableSize <<= 1;
    int sizeBits = 0;
    for (int t = tableSize; t > 2; t >>= 1) sizeBits++;   // log2(tableSize) - 1
    table.resize((size_t)tableSize, SeaRgba(0, 0, 0, 255));

    std::map<std::uint32_t, int> lookup;
    for (size_t i = 1; i < table.size(); i++) {
        const std::uint32_t key = ((std::uint32_t)table[i].r << 16) |
                                  ((std::uint32_t)table[i].g << 8) | table[i].b;
        if (lookup.find(key) == lookup.end()) lookup[key] = (int)i;
    }

    const int outW = width * scale, outH = height * scale;
    const int delay = (int)std::lround(100.0 / (fps > 0 ? fps : 10.0));
    const int minCode = Max(2, sizeBits + 1);
    const int transparent = hasBg ? -1 : 0;

    std::ofstream f(path, std::ios::binary);
    if (!f) { error = "cannot write " + path; return false; }

    std::vector<unsigned char> head;
    head.push_back('G'); head.push_back('I'); head.push_back('F');
    head.push_back('8'); head.push_back('9'); head.push_back('a');
    Put16(head, outW);
    Put16(head, outH);
    head.push_back((unsigned char)(0x80 | 0x70 | sizeBits));
    head.push_back(0);   // background index
    head.push_back(0);   // aspect
    for (const SeaRgba& c : table) {
        head.push_back(c.r);
        head.push_back(c.g);
        head.push_back(c.b);
    }
    // Netscape loop-forever extension.
    const unsigned char loop[] = {0x21, 0xFF, 0x0B, 'N', 'E', 'T', 'S', 'C', 'A', 'P', 'E',
                                  '2', '.', '0', 0x03, 0x01, 0x00, 0x00, 0x00};
    head.insert(head.end(), loop, loop + sizeof(loop));
    f.write((const char*)head.data(), (std::streamsize)head.size());

    for (const std::vector<unsigned char>& frame : frames) {
        // Index the upscaled frame.
        std::vector<unsigned char> idx((size_t)outW * outH, 0);
        for (int y = 0; y < outH; y++) {
            for (int x = 0; x < outW; x++) {
                const int sx = x / scale, sy = y / scale;
                const size_t p = ((size_t)sy * width + sx) * 4;
                int r = frame[p + 0], g = frame[p + 1], b = frame[p + 2];
                const int a = frame[p + 3];
                int index = 0;
                if (a < 128) {
                    if (!hasBg) {
                        index = 0;
                    } else {
                        r = (r * a + bgR * (255 - a)) / 255;
                        g = (g * a + bgG * (255 - a)) / 255;
                        b = (b * a + bgB * (255 - a)) / 255;
                        const std::uint32_t key = ((std::uint32_t)r << 16) |
                                                  ((std::uint32_t)g << 8) | b;
                        const auto it = lookup.find(key);
                        index = it != lookup.end() ? it->second : Nearest(table, r, g, b);
                    }
                } else {
                    const std::uint32_t key = ((std::uint32_t)r << 16) |
                                              ((std::uint32_t)g << 8) | b;
                    const auto it = lookup.find(key);
                    index = it != lookup.end() ? it->second : Nearest(table, r, g, b);
                }
                idx[(size_t)y * outW + x] = (unsigned char)index;
            }
        }

        std::vector<unsigned char> lzw;
        LzwEncode(idx, minCode, lzw);

        std::vector<unsigned char> block;
        // Graphic Control Extension.
        block.push_back(0x21);
        block.push_back(0xF9);
        block.push_back(0x04);
        block.push_back((unsigned char)(0x08 | (transparent >= 0 ? 0x01 : 0x00)));
        Put16(block, delay);
        block.push_back((unsigned char)(transparent >= 0 ? transparent : 0));
        block.push_back(0x00);
        // Image Descriptor.
        block.push_back(0x2C);
        Put16(block, 0);
        Put16(block, 0);
        Put16(block, outW);
        Put16(block, outH);
        block.push_back(0x00);
        block.push_back((unsigned char)minCode);
        // LZW sub-blocks.
        size_t at = 0;
        while (at < lzw.size()) {
            const size_t n = Min((size_t)255, lzw.size() - at);
            block.push_back((unsigned char)n);
            block.insert(block.end(), lzw.begin() + (std::ptrdiff_t)at,
                         lzw.begin() + (std::ptrdiff_t)(at + n));
            at += n;
        }
        block.push_back(0x00);
        f.write((const char*)block.data(), (std::streamsize)block.size());
    }
    const unsigned char trailer = 0x3B;
    f.write((const char*)&trailer, 1);
    if (!f.good()) { error = "cannot finish writing " + path; return false; }
    return true;
}

bool WriteSeaGifs(const SeaSheet& sheet, const std::string& dir, int scale,
                  const std::string& background, std::string& error) {
    const std::string gifDir = (std::filesystem::path(dir) / "gif").string();
    std::error_code ec;
    std::filesystem::create_directories(gifDir, ec);
    if (ec) { error = "cannot create " + gifDir; return false; }
    auto clipFrames = [&](int c) {
        std::vector<std::vector<unsigned char>> out;
        const SeaSheetClip& clip = sheet.clips[c];
        for (int f = 0; f < clip.frames; f++) {
            std::vector<unsigned char> cell((size_t)sheet.size * sheet.size * 4, 0);
            for (int y = 0; y < sheet.size; y++) {
                const size_t src = ((size_t)(clip.row * sheet.size + y) * sheet.width +
                                    (size_t)f * sheet.size) * 4;
                std::memcpy(cell.data() + (size_t)y * sheet.size * 4, sheet.rgba.data() + src,
                            (size_t)sheet.size * 4);
            }
            out.push_back(cell);
        }
        return out;
    };
    for (int c = 0; c < sheet.clipCount; c++) {
        const std::string name = SeaClipName(sheet.clips[c].clip);
        const std::string path = (std::filesystem::path(gifDir) / (name + ".gif")).string();
        if (!WriteSeaGif(clipFrames(c), sheet.size, sheet.size, sheet.colors,
                         sheet.clips[c].fps, scale, path, background, error))
            return false;
    }
    // showcase: swim_right, puff_right, turn_r2l, swim_left, puff_left, turn_l2r.
    static const int order[6] = {ClipSwimRight, ClipPuffRight, ClipTurnRightToLeft,
                                 ClipSwimLeft, ClipPuffLeft, ClipTurnLeftToRight};
    std::vector<std::vector<unsigned char>> showcase;
    for (int k = 0; k < 6; k++) {
        int c = -1;
        for (int i = 0; i < sheet.clipCount; i++)
            if (sheet.clips[i].clip == order[k]) { c = i; break; }
        if (c < 0) continue;
        const std::vector<std::vector<unsigned char>> f = clipFrames(c);
        const int loops = sheet.clips[c].loops ? 2 : 1;
        for (int l = 0; l < loops; l++)
            showcase.insert(showcase.end(), f.begin(), f.end());
    }
    const double fps = sheet.clips[0].fps;
    return WriteSeaGif(showcase, sheet.size, sheet.size, sheet.colors, fps, scale,
                       (std::filesystem::path(gifDir) / "showcase.gif").string(), background, error);
}

} // namespace sea
