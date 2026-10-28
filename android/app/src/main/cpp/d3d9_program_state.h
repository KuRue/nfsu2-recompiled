#pragma once
#include "graphics_program.h"
#include <d3d9.h>
namespace nfs_d3d9 {
static_assert(sizeof(D3DVERTEXELEMENT9) == 8, "D3D9 declaration wire size");
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
