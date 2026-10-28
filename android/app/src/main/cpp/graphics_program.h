#pragma once
// Operand-count table and token-length handling adapted from DXVK's dxso_tables.cpp
// and dxso_decoder.cpp. Altered for bounded snapshot validation. DXVK copyright
// Philip Rebohle (2017-2021), Joshua Ashton (2019-2021), zlib/libpng license;
// the complete notice is retained in ports/dxvk_android/DXVK-LICENSE.
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>
namespace nfs_d3d9 {
enum class ProgramKind : uint32_t { VertexShader = 8, PixelShader = 9, Declaration = 10 };
struct Program { ProgramKind kind = ProgramKind::VertexShader; std::vector<uint8_t> bytes; };
constexpr size_t maxProgramBytes = 256 * 1024;
inline void check(bool okay, const char* message) { if (!okay) throw std::runtime_error(message); }
inline void validate(const Program& program) {
    const auto& bytes = program.bytes;
    check(!bytes.empty() && bytes.size() <= maxProgramBytes, "Invalid D3D9 program size");
    if (program.kind == ProgramKind::Declaration) {
        check(bytes.size() % 8 == 0 && bytes.size() / 8 <= 65, "Invalid D3D9 declaration size");
        for (size_t offset = 0; offset < bytes.size(); offset += 8) {
            uint32_t stream = bytes[offset] | (uint32_t(bytes[offset + 1]) << 8), type = bytes[offset + 4];
            if (offset + 8 == bytes.size()) check(stream == 0xff && type == 17, "Missing D3D9 declaration end");
            else check(stream < 16 && type < 17, "Invalid D3D9 declaration element");
        }
    } else {
        check(program.kind == ProgramKind::VertexShader || program.kind == ProgramKind::PixelShader, "Unknown D3D9 program kind");
        check(bytes.size() >= 8 && bytes.size() % 4 == 0, "Invalid D3D9 shader size");
        uint32_t version, end; std::memcpy(&version, bytes.data(), 4); std::memcpy(&end, bytes.data() + bytes.size() - 4, 4);
        check((version >> 16) == (program.kind == ProgramKind::VertexShader ? 0xfffeu : 0xffffu)
              && ((version >> 8) & 0xff) >= 1 && ((version >> 8) & 0xff) <= 3 && end == 0xffffu,
              "Invalid D3D9 shader version/end");
        // Fixed operand counts for shader model 1; models 2/3 carry lengths in tokens.
        static constexpr int lengths[] = {
            0, 2, 3, 3, 4, 3, 2, 2, 3, 3, 3, 3, 3, 3, 2, 2,
            2, 3, 4, 2, 3, 3, 3, 3, 3, 1, 2, 2, 0, 0, 1, 2,
            3, 3, 4, 2, 2, 4, 1, 0, 1, 2, 0, 0, 0, 2, 2, 2,
            5, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
            1, 1, 1, 2, 2, 2, 2, 2, 2, 2, 2, -1, 3, 2, 2, 2,
            4, 5, 2, 2, 2, 2, 2, 1, 4, 3, 4, 2, 2, 5, 3, 3,
            2
        };
        const size_t words = bytes.size() / 4;
        auto tokenAt = [&](size_t at) { uint32_t token; std::memcpy(&token, bytes.data() + at * 4, 4); return token; };
        uint32_t major = (version >> 8) & 0xff, minor = version & 0xff;
        size_t at = 1;
        while (at < words) {
            uint32_t token = tokenAt(at++), opcode = token & 0xffff;
            if (opcode == 0xffff) { check(at == words, "Trailing D3D9 shader tokens"); return; }
            size_t count;
            if (opcode == 0xfffe) count = (token >> 16) & 0x7fff;
            else if (opcode == 0xfffd) count = 0; // ps_1_4 phase
            else if (major >= 2) count = (token >> 24) & 0xf;
            else {
                check(opcode < sizeof(lengths) / sizeof(lengths[0]) && lengths[opcode] >= 0, "Unknown SM1 opcode");
                count = size_t(lengths[opcode]);
                if (minor == 4 && (opcode == 64 || opcode == 66)) ++count;
            }
            check(count < words - at, "Truncated D3D9 shader operands/comment"); // must leave room for END
            at += count;
        }
        check(false, "Missing D3D9 shader END");
    }
}
struct ProgramRecord {
    uint32_t address = 0, table = 0, device = 0, references = 0;
    Program definition;
};
inline bool operator==(const ProgramRecord& a, const ProgramRecord& b) {
    return a.address == b.address && a.table == b.table && a.device == b.device && a.references == b.references
        && a.definition.kind == b.definition.kind && a.definition.bytes == b.definition.bytes;
}
}
