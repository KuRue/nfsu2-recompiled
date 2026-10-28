#pragma once
// Operand-count table and token-length handling adapted from DXVK's dxso_tables.cpp
// and dxso_decoder.cpp. Altered for bounded snapshot validation. DXVK copyright
// Philip Rebohle (2017-2021), Joshua Ashton (2019-2021), zlib/libpng license;
// the complete notice is retained in ports/dxvk_android/DXVK-LICENSE.
#include <d3d9.h>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

// Immutable program definitions, independent of guest/native pointers. Device bindings and
// COM references are restored separately. Keep native objects staged until the whole load works.
namespace nfs_d3d9 {
enum class ProgramKind : uint32_t { VertexShader = 8, PixelShader = 9, Declaration = 10 };
struct Program { ProgramKind kind; std::vector<uint8_t> bytes; };
constexpr size_t maxProgramBytes = 256 * 1024;
inline void check(bool okay, const char* message) { if (!okay) throw std::runtime_error(message); }
inline void validate(const Program& program) {
    const auto& bytes = program.bytes;
    check(!bytes.empty() && bytes.size() <= maxProgramBytes, "Invalid D3D9 program size");
    if (program.kind == ProgramKind::Declaration) {
        check(bytes.size() % sizeof(D3DVERTEXELEMENT9) == 0 && bytes.size() / sizeof(D3DVERTEXELEMENT9) <= 65,
              "Invalid D3D9 declaration size");
        for (size_t offset = 0; offset < bytes.size(); offset += sizeof(D3DVERTEXELEMENT9)) {
            D3DVERTEXELEMENT9 element; std::memcpy(&element, bytes.data() + offset, sizeof element);
            if (offset + sizeof element == bytes.size())
                check(element.Stream == 0xff && element.Type == D3DDECLTYPE_UNUSED, "Missing D3D9 declaration end");
            else check(element.Stream < 16 && element.Type < D3DDECLTYPE_UNUSED, "Invalid D3D9 declaration element");
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
template<class Shader> Program captureShader(Shader* shader, ProgramKind kind) {
    UINT size = 0;
    check(SUCCEEDED(shader->GetFunction(nullptr, &size)) && size && size <= maxProgramBytes, "Cannot size D3D9 shader");
    Program program{kind, std::vector<uint8_t>(size)}; UINT copied = size;
    check(SUCCEEDED(shader->GetFunction(program.bytes.data(), &copied)) && copied == size, "Cannot copy D3D9 shader");
    validate(program); return program;
}
inline Program captureDeclaration(IDirect3DVertexDeclaration9* declaration) {
    UINT count = 0;
    check(SUCCEEDED(declaration->GetDeclaration(nullptr, &count)) && count && count <= 65, "Cannot size D3D9 declaration");
    std::vector<D3DVERTEXELEMENT9> elements(count); UINT copied = count;
    check(SUCCEEDED(declaration->GetDeclaration(elements.data(), &copied)) && copied == count, "Cannot copy D3D9 declaration");
    Program program{ProgramKind::Declaration, std::vector<uint8_t>(count * sizeof(D3DVERTEXELEMENT9))};
    std::memcpy(program.bytes.data(), elements.data(), program.bytes.size()); validate(program); return program;
}
class StagedProgram {
    IUnknown* object = nullptr;
public:
    explicit StagedProgram(IUnknown* value = nullptr): object(value) {}
    ~StagedProgram() { if (object) object->Release(); }
    StagedProgram(const StagedProgram&) = delete;
    StagedProgram& operator=(const StagedProgram&) = delete;
    StagedProgram(StagedProgram&& other) noexcept: object(other.release()) {}
    IUnknown* get() const { return object; }
    IUnknown* release() { auto value = object; object = nullptr; return value; }
};
// Device is templated so the production path can be tested without a GPU or Android surface.
template<class Device> StagedProgram recreateProgram(Device* device, const Program& program) {
    validate(program); check(device != nullptr, "Missing D3D9 recreation device");
    IUnknown* output = nullptr; HRESULT status;
    if (program.kind == ProgramKind::Declaration) {
        std::vector<D3DVERTEXELEMENT9> elements(program.bytes.size() / sizeof(D3DVERTEXELEMENT9));
        std::memcpy(elements.data(), program.bytes.data(), program.bytes.size());
        IDirect3DVertexDeclaration9* result = nullptr; status = device->CreateVertexDeclaration(elements.data(), &result); output = result;
    } else {
        // Avoid relying on byte-vector alignment for DWORD reads in the native driver.
        std::vector<DWORD> tokens(program.bytes.size() / 4); std::memcpy(tokens.data(), program.bytes.data(), program.bytes.size());
        if (program.kind == ProgramKind::VertexShader) {
            IDirect3DVertexShader9* result = nullptr; status = device->CreateVertexShader(tokens.data(), &result); output = result;
        } else {
            IDirect3DPixelShader9* result = nullptr; status = device->CreatePixelShader(tokens.data(), &result); output = result;
        }
    }
    StagedProgram staged(output); check(SUCCEEDED(status) && output, "Cannot recreate D3D9 program"); return staged;
}
}
