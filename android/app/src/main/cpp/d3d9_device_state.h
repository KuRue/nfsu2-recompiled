#pragma once
#include "graphics_device.h"
#include <d3d9.h>
// Reads and replays DeviceState through any device with the IDirect3DDevice9 getters and setters
// (the real device in the bridge, a mock in tests). Objects cross the guest boundary through a
// mapper: guest(native) gives the guest address of a native object (0 for none, throws for an
// object the guest never saw), and texture/buffer/... (address) give the native object a guest
// address stands for (the bridge's texture mapper applies HD texture-pack replacements).
namespace nfs_d3d9 {
// D3DRENDERSTATETYPE values a D3D9 device accepts (DEBUGMONITORTOKEN left out: it is a debug hook).
constexpr uint32_t deviceRenderStates[] = {7,8,9,14,15,16,19,20,22,23,24,25,26,27,28,29,34,35,36,37,38,48,52,53,54,55,56,57,58,59,60,
    128,129,130,131,132,133,134,135,136,137,139,140,141,142,143,145,146,147,148,151,152,154,155,156,157,158,159,160,161,162,163,
    166,167,168,170,171,172,173,174,175,176,178,179,180,181,182,183,184,185,186,187,188,189,190,191,192,193,194,195,
    198,199,200,201,202,203,204,205,206,207,208,209};
constexpr uint32_t deviceStageStates[] = {1,2,3,4,5,6,7,8,9,10,11,22,23,24,26,27,28,32};
constexpr uint32_t deviceSamplers[] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,257,258,259};
constexpr uint32_t deviceTransforms[] = {2,3,16,17,18,19,20,21,22,23,256,257,258,259};
// The object types a device's getters return. Tests specialize this for a mock device.
template<class Device> struct DeviceTypes {
    using Texture = IDirect3DBaseTexture9; using VertexBuffer = IDirect3DVertexBuffer9; using IndexBuffer = IDirect3DIndexBuffer9;
    using Declaration = IDirect3DVertexDeclaration9; using VertexShader = IDirect3DVertexShader9; using PixelShader = IDirect3DPixelShader9;
    using Surface = IDirect3DSurface9;
};
template<class T> struct Held { // releases what a getter AddRef'd
    T* p = nullptr; ~Held() { if (p) p->Release(); } T** out() { return &p; }
};
template<class Device, class Mapper> DeviceState captureDevice(Device* device, uint32_t address, Mapper& map) {
    using T = DeviceTypes<Device>;
    DeviceState state; state.device = address;
    auto ok = [](HRESULT status, const char* what) { check(SUCCEEDED(status), what); };
    for (uint32_t id : deviceRenderStates) { DWORD value = 0; ok(device->GetRenderState(D3DRENDERSTATETYPE(id), &value), "Cannot read device render state"); state.renderStates.emplace_back(id, value); }
    for (uint32_t stage = 0; stage < 8; ++stage) for (uint32_t id : deviceStageStates) {
        DWORD value = 0; ok(device->GetTextureStageState(stage, D3DTEXTURESTAGESTATETYPE(id), &value), "Cannot read texture stage state"); state.stageStates.emplace_back(stage * 64 + id, value); }
    for (uint32_t sampler : deviceSamplers) {
        for (uint32_t id = 1; id <= 13; ++id) { DWORD value = 0; ok(device->GetSamplerState(sampler, D3DSAMPLERSTATETYPE(id), &value), "Cannot read sampler state"); state.samplerStates.emplace_back(sampler * 16 + id, value); }
        Held<typename T::Texture> texture; ok(device->GetTexture(sampler, texture.out()), "Cannot read bound texture");
        state.textures.emplace_back(sampler, map.guest(texture.p));
    }
    for (uint32_t type : deviceTransforms) { D3DMATRIX matrix{}; ok(device->GetTransform(D3DTRANSFORMSTATETYPE(type), &matrix), "Cannot read transform");
        std::array<uint32_t, 16> words{}; std::memcpy(words.data(), &matrix, 64); state.transforms.emplace_back(type, words); }
    { D3DVIEWPORT9 viewport{}; ok(device->GetViewport(&viewport), "Cannot read viewport");
      state.viewport = {viewport.X, viewport.Y, viewport.Width, viewport.Height, 0, 0}; std::memcpy(&state.viewport[4], &viewport.MinZ, 4); std::memcpy(&state.viewport[5], &viewport.MaxZ, 4); }
    { D3DMATERIAL9 material{}; ok(device->GetMaterial(&material), "Cannot read material"); static_assert(sizeof material == 68, "D3DMATERIAL9"); std::memcpy(state.material.data(), &material, 68); }
    for (uint32_t index = 0; index < 8; ++index) {
        D3DLIGHT9 light{}; if (FAILED(device->GetLight(index, &light))) continue; // never set
        Light item; item.index = index; BOOL enabled = FALSE; ok(device->GetLightEnable(index, &enabled), "Cannot read light enable");
        item.enabled = enabled != FALSE; static_assert(sizeof light == 104, "D3DLIGHT9"); std::memcpy(item.words.data(), &light, 104); state.lights.push_back(item);
    }
    for (uint32_t index = 0; index < 6; ++index) { float plane[4]{}; ok(device->GetClipPlane(index, plane), "Cannot read clip plane"); std::memcpy(state.clipPlanes[index].data(), plane, 16); }
    { RECT scissor{}; ok(device->GetScissorRect(&scissor), "Cannot read scissor"); state.scissor = {uint32_t(scissor.left), uint32_t(scissor.top), uint32_t(scissor.right), uint32_t(scissor.bottom)}; }
    for (uint32_t index = 0; index < 16; ++index) {
        Held<typename T::VertexBuffer> buffer; UINT offset = 0, stride = 0, frequency = 1;
        ok(device->GetStreamSource(index, buffer.out(), &offset, &stride), "Cannot read stream source");
        ok(device->GetStreamSourceFreq(index, &frequency), "Cannot read stream frequency");
        state.streams[index] = {map.guest(buffer.p), buffer.p ? offset : 0, buffer.p ? stride : 0, frequency};
    }
    { Held<typename T::IndexBuffer> indices; ok(device->GetIndices(indices.out()), "Cannot read indices"); state.indices = map.guest(indices.p); }
    { Held<typename T::Declaration> declaration; ok(device->GetVertexDeclaration(declaration.out()), "Cannot read declaration"); state.declaration = map.guest(declaration.p); }
    { DWORD fvf = 0; ok(device->GetFVF(&fvf), "Cannot read FVF"); state.fvf = fvf; }
    { Held<typename T::VertexShader> shader; ok(device->GetVertexShader(shader.out()), "Cannot read vertex shader"); state.vertexShader = map.guest(shader.p); }
    { Held<typename T::PixelShader> shader; ok(device->GetPixelShader(shader.out()), "Cannot read pixel shader"); state.pixelShader = map.guest(shader.p); }
    for (uint32_t index = 0; index < 4; ++index) {
        Held<typename T::Surface> target; HRESULT status = device->GetRenderTarget(index, target.out());
        check(SUCCEEDED(status) || (index && status == D3DERR_NOTFOUND), "Cannot read render target");
        state.renderTargets[index] = map.guest(target.p);
    }
    { Held<typename T::Surface> depth; HRESULT status = device->GetDepthStencilSurface(depth.out());
      check(SUCCEEDED(status) || status == D3DERR_NOTFOUND, "Cannot read depth stencil"); state.depthStencil = map.guest(depth.p); }
    auto constants = [&](std::vector<uint32_t>& out, size_t words, auto get) { out.assign(words, 0); ok(get(out.data()), "Cannot read shader constants"); };
    constants(state.vsFloat, vsFloatRegisters * 4, [&](uint32_t* p) { return device->GetVertexShaderConstantF(0, reinterpret_cast<float*>(p), vsFloatRegisters); });
    constants(state.vsInt, intRegisters * 4, [&](uint32_t* p) { return device->GetVertexShaderConstantI(0, reinterpret_cast<int*>(p), intRegisters); });
    constants(state.vsBool, boolRegisters, [&](uint32_t* p) { return device->GetVertexShaderConstantB(0, reinterpret_cast<BOOL*>(p), boolRegisters); });
    constants(state.psFloat, psFloatRegisters * 4, [&](uint32_t* p) { return device->GetPixelShaderConstantF(0, reinterpret_cast<float*>(p), psFloatRegisters); });
    constants(state.psInt, intRegisters * 4, [&](uint32_t* p) { return device->GetPixelShaderConstantI(0, reinterpret_cast<int*>(p), intRegisters); });
    constants(state.psBool, boolRegisters, [&](uint32_t* p) { return device->GetPixelShaderConstantB(0, reinterpret_cast<BOOL*>(p), boolRegisters); });
    for (auto& value : state.vsBool) value = value ? 1 : 0;
    for (auto& value : state.psBool) value = value ? 1 : 0;
    validate(state);
    return state;
}
// Render targets first: setting render target 0 resets the viewport and scissor, which are
// applied after it. A declaration wins over an FVF (setting an FVF replaces the declaration).
template<class Device, class Mapper> void applyDevice(Device* device, const DeviceState& state, Mapper& map) {
    validate(state);
    auto ok = [](HRESULT status, const char* what) { check(SUCCEEDED(status), what); };
    for (uint32_t index = 0; index < 4; ++index)
        ok(device->SetRenderTarget(index, state.renderTargets[index] ? map.surface(state.renderTargets[index]) : nullptr), "Cannot restore render target");
    ok(device->SetDepthStencilSurface(state.depthStencil ? map.surface(state.depthStencil) : nullptr), "Cannot restore depth stencil");
    for (const auto& item : state.renderStates) ok(device->SetRenderState(D3DRENDERSTATETYPE(item.first), item.second), "Cannot restore render state");
    for (const auto& item : state.stageStates) ok(device->SetTextureStageState(item.first / 64, D3DTEXTURESTAGESTATETYPE(item.first % 64), item.second), "Cannot restore stage state");
    for (const auto& item : state.samplerStates) ok(device->SetSamplerState(item.first / 16, D3DSAMPLERSTATETYPE(item.first % 16), item.second), "Cannot restore sampler state");
    for (const auto& item : state.textures) ok(device->SetTexture(item.first, item.second ? map.texture(item.second) : nullptr), "Cannot restore texture binding");
    for (const auto& item : state.transforms) { D3DMATRIX matrix; std::memcpy(&matrix, item.second.data(), 64); ok(device->SetTransform(D3DTRANSFORMSTATETYPE(item.first), &matrix), "Cannot restore transform"); }
    { D3DVIEWPORT9 viewport{state.viewport[0], state.viewport[1], state.viewport[2], state.viewport[3], 0, 0};
      std::memcpy(&viewport.MinZ, &state.viewport[4], 4); std::memcpy(&viewport.MaxZ, &state.viewport[5], 4); ok(device->SetViewport(&viewport), "Cannot restore viewport"); }
    { D3DMATERIAL9 material; std::memcpy(&material, state.material.data(), 68); ok(device->SetMaterial(&material), "Cannot restore material"); }
    for (const auto& item : state.lights) { D3DLIGHT9 light; std::memcpy(&light, item.words.data(), 104);
        ok(device->SetLight(item.index, &light), "Cannot restore light"); ok(device->LightEnable(item.index, item.enabled), "Cannot restore light enable"); }
    for (uint32_t index = 0; index < 6; ++index) { float plane[4]; std::memcpy(plane, state.clipPlanes[index].data(), 16); ok(device->SetClipPlane(index, plane), "Cannot restore clip plane"); }
    { RECT scissor{LONG(state.scissor[0]), LONG(state.scissor[1]), LONG(state.scissor[2]), LONG(state.scissor[3])}; ok(device->SetScissorRect(&scissor), "Cannot restore scissor"); }
    for (uint32_t index = 0; index < 16; ++index) { const auto& stream = state.streams[index];
        ok(device->SetStreamSource(index, stream.buffer ? map.vertexBuffer(stream.buffer) : nullptr, stream.offset, stream.stride), "Cannot restore stream source");
        ok(device->SetStreamSourceFreq(index, stream.frequency), "Cannot restore stream frequency"); }
    ok(device->SetIndices(state.indices ? map.indexBuffer(state.indices) : nullptr), "Cannot restore indices");
    if (state.declaration) ok(device->SetVertexDeclaration(map.declaration(state.declaration)), "Cannot restore declaration");
    else ok(device->SetFVF(state.fvf), "Cannot restore FVF");
    ok(device->SetVertexShader(state.vertexShader ? map.vertexShader(state.vertexShader) : nullptr), "Cannot restore vertex shader");
    ok(device->SetPixelShader(state.pixelShader ? map.pixelShader(state.pixelShader) : nullptr), "Cannot restore pixel shader");
    ok(device->SetVertexShaderConstantF(0, reinterpret_cast<const float*>(state.vsFloat.data()), vsFloatRegisters), "Cannot restore VS float constants");
    ok(device->SetVertexShaderConstantI(0, reinterpret_cast<const int*>(state.vsInt.data()), intRegisters), "Cannot restore VS integer constants");
    ok(device->SetVertexShaderConstantB(0, reinterpret_cast<const BOOL*>(state.vsBool.data()), boolRegisters), "Cannot restore VS boolean constants");
    ok(device->SetPixelShaderConstantF(0, reinterpret_cast<const float*>(state.psFloat.data()), psFloatRegisters), "Cannot restore PS float constants");
    ok(device->SetPixelShaderConstantI(0, reinterpret_cast<const int*>(state.psInt.data()), intRegisters), "Cannot restore PS integer constants");
    ok(device->SetPixelShaderConstantB(0, reinterpret_cast<const BOOL*>(state.psBool.data()), boolRegisters), "Cannot restore PS boolean constants");
}
}
