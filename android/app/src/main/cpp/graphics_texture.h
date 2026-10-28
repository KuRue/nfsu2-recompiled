#pragma once
#include "graphics_program.h"
#include <algorithm>
#include <cstring>
#include <vector>
// Texture contents for captures. Bytes are kept in the game's own format (the format it created
// the texture with), one tightly packed subresource per face and mip level. A load replays them
// through the guest lock path, so the CPU DXT fallback and HD texture packs apply again exactly
// as when the game uploaded them.
namespace nfs_d3d9 {
constexpr size_t maxTextureBytes = 64u << 20;      // one texture, all faces and levels
constexpr size_t maxTextureTotal = 512u << 20;     // every texture in a capture
constexpr uint32_t fourccDXT1 = 0x31545844u, fourccDXT5 = 0x35545844u;
inline bool blockFormat(uint32_t format) { return format >= fourccDXT1 && format <= fourccDXT5; }
// Bytes per row (per row of 4x4 blocks for DXT) of one level. Zero for a format a capture cannot
// describe; such a texture refuses capture rather than storing guessed sizes.
inline uint32_t textureRowBytes(uint32_t format, uint32_t width) {
    switch (format) {
    case fourccDXT1: return ((width + 3) / 4) * 8;
    case 0x32545844u: case 0x33545844u: case 0x34545844u: case fourccDXT5: return ((width + 3) / 4) * 16;
    case 20: return width * 3;                                           // R8G8B8
    case 28: case 50: case 41: return width;                             // A8, L8, P8
    case 23: case 24: case 25: case 26: case 30: case 51: case 81: case 111: case 60: return width * 2;
    case 21: case 22: case 32: case 33: case 35: case 31: case 34: case 114: case 112: case 63: case 64: return width * 4;
    case 36: case 113: case 115: return width * 8;
    case 116: return width * 16;
    default: return 0;
    }
}
inline uint32_t mipExtent(uint32_t size, uint32_t level) { return std::max(1u, size >> level); }
inline uint32_t textureRows(uint32_t format, uint32_t height) { return blockFormat(format) ? (height + 3) / 4 : height; }

struct TextureLevel { uint32_t face = 0, level = 0; std::vector<uint8_t> bytes; };
struct Texture {
    uint32_t kind = 3, width = 0, height = 0, levels = 0, usage = 0, format = 0, pool = 0;
    // Render targets and depth textures: the GPU redraws them, so no contents are kept and a
    // load leaves (or recreates) them empty.
    bool transient = false;
    std::vector<TextureLevel> contents; // face-major, every face and level, when not transient
};
struct TextureRecord {
    uint32_t address = 0, table = 0, device = 0, references = 0;
    Texture definition;
};
inline uint32_t textureFaces(const Texture& texture) { return texture.kind == 4 ? 6u : 1u; }
inline size_t levelBytes(const Texture& texture, uint32_t level) {
    uint32_t height = texture.kind == 4 ? texture.width : texture.height;
    return size_t(textureRowBytes(texture.format, mipExtent(texture.width, level))) * textureRows(texture.format, mipExtent(height, level));
}
inline void validate(const Texture& texture) {
    check(texture.kind == 3 || texture.kind == 4, "Invalid D3D9 texture kind");
    check(texture.width && texture.width <= 8192 && texture.levels && texture.levels <= 14 && texture.pool <= 3, "Invalid D3D9 texture size");
    check(texture.kind == 4 ? texture.height == texture.width : texture.height && texture.height <= 8192, "Invalid D3D9 texture height");
    check(texture.transient == bool(texture.usage & 3u), "D3D9 texture transient flag does not match its usage");
    if (texture.transient) { check(texture.contents.empty(), "Transient D3D9 texture has contents"); return; }
    check(textureRowBytes(texture.format, texture.width), "Unsupported D3D9 texture format");
    uint32_t faces = textureFaces(texture);
    check(texture.contents.size() == size_t(faces) * texture.levels, "D3D9 texture contents incomplete");
    size_t total = 0;
    for (uint32_t face = 0; face < faces; ++face) for (uint32_t level = 0; level < texture.levels; ++level) {
        const auto& item = texture.contents[face * texture.levels + level];
        check(item.face == face && item.level == level && item.bytes.size() == levelBytes(texture, level), "D3D9 texture level size mismatch");
        total += item.bytes.size();
    }
    check(total <= maxTextureBytes, "D3D9 texture too large");
}
inline bool sameMetadata(const TextureRecord& a, const TextureRecord& b) {
    const auto& x = a.definition; const auto& y = b.definition;
    return a.address == b.address && a.table == b.table && a.device == b.device && a.references == b.references
        && x.kind == y.kind && x.width == y.width && x.height == y.height && x.levels == y.levels && x.usage == y.usage
        && x.format == y.format && x.pool == y.pool && x.transient == y.transient;
}
inline bool operator==(const TextureRecord& a, const TextureRecord& b) {
    if (!sameMetadata(a, b) || a.definition.contents.size() != b.definition.contents.size()) return false;
    for (size_t i = 0; i < a.definition.contents.size(); ++i) {
        const auto& x = a.definition.contents[i]; const auto& y = b.definition.contents[i];
        if (x.face != y.face || x.level != y.level || x.bytes != y.bytes) return false;
    }
    return true;
}
// Guest-format bytes uploaded by successful Unlocks. A level becomes complete only when a lock
// covered all of it; partial locks then update it in place. Partial locks on an incomplete level
// cannot be trusted (the rest was never seen), so it stays incomplete.
class TextureShadow {
    Texture shape;
    std::vector<std::vector<uint8_t>> data;
    std::vector<bool> whole;
public:
    explicit TextureShadow(const Texture& layout) : shape(layout) {
        shape.contents.clear(); shape.transient = false;
        uint32_t faces = textureFaces(shape);
        check(textureRowBytes(shape.format, shape.width) && shape.levels && shape.levels <= 14, "Invalid texture shadow layout");
        size_t total = 0;
        for (uint32_t face = 0; face < faces; ++face) for (uint32_t level = 0; level < shape.levels; ++level) total += levelBytes(shape, level);
        check(total <= maxTextureBytes, "Texture shadow too large");
        data.resize(size_t(faces) * shape.levels); whole.assign(data.size(), false);
    }
    size_t bytes() const { size_t total = 0; for (uint32_t level = 0; level < shape.levels; ++level) total += levelBytes(shape, level); return total * textureFaces(shape); }
    bool complete(uint32_t face, uint32_t level) const { return face < textureFaces(shape) && level < shape.levels && whole[face * shape.levels + level]; }
    const std::vector<uint8_t>& level(uint32_t face, uint32_t level) const { check(complete(face, level), "Texture shadow level incomplete"); return data[face * shape.levels + level]; }
    // rows of rowBytes from source (stride sourcePitch), placed at row-block firstRow and byte column
    // firstByte of the level. coversLevel: this lock spanned the whole level.
    void update(uint32_t face, uint32_t level, uint32_t firstRow, uint32_t firstByte, uint32_t rows, uint32_t rowBytes,
                const uint8_t* source, uint32_t sourcePitch, bool coversLevel) {
        check(face < textureFaces(shape) && level < shape.levels && source && rows && rowBytes && sourcePitch >= rowBytes, "Invalid texture shadow update");
        size_t index = face * shape.levels + level;
        uint32_t height = shape.kind == 4 ? shape.width : shape.height;
        uint32_t levelRow = textureRowBytes(shape.format, mipExtent(shape.width, level)), levelRows = textureRows(shape.format, mipExtent(height, level));
        check(uint64_t(firstRow) + rows <= levelRows && uint64_t(firstByte) + rowBytes <= levelRow, "Texture shadow update outside the level");
        if (!coversLevel && !whole[index]) return;
        if (data[index].empty()) data[index].resize(size_t(levelRow) * levelRows);
        for (uint32_t row = 0; row < rows; ++row)
            std::memcpy(data[index].data() + size_t(firstRow + row) * levelRow + firstByte, source + size_t(row) * sourcePitch, rowBytes);
        if (coversLevel) whole[index] = true;
    }
    void forget(uint32_t face, uint32_t level) { if (face < textureFaces(shape) && level < shape.levels) whole[face * shape.levels + level] = false; }
    void forgetAll() { std::fill(whole.begin(), whole.end(), false); }
};
}
