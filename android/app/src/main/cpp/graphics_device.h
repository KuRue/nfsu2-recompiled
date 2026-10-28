#pragma once
#include "graphics_program.h"
#include <algorithm>
#include <array>
#include <utility>
#include <vector>
// Direct3D 9 device state at a capture (taken at Present, between frames): every value a draw
// depends on that the game sets through the device. Objects are guest addresses (the guest's COM
// proxies), so the state stays valid in a session and maps onto recreated objects in a new one.
// Floats are kept as their 32-bit patterns so a load restores them bit-exactly.
namespace nfs_d3d9 {
struct Light { uint32_t index = 0; bool enabled = false; std::array<uint32_t, 26> words{}; }; // D3DLIGHT9
struct Stream { uint32_t buffer = 0, offset = 0, stride = 0, frequency = 1; };
struct DeviceState {
    uint32_t device = 0;
    std::vector<std::pair<uint32_t, uint32_t>> renderStates;   // (D3DRENDERSTATETYPE, value)
    std::vector<std::pair<uint32_t, uint32_t>> stageStates;    // (stage * 64 + D3DTEXTURESTAGESTATETYPE, value)
    std::vector<std::pair<uint32_t, uint32_t>> samplerStates;  // (sampler * 16 + D3DSAMPLERSTATETYPE, value)
    std::vector<std::pair<uint32_t, uint32_t>> textures;       // (sampler, texture address); 0 unbound
    std::vector<std::pair<uint32_t, std::array<uint32_t, 16>>> transforms; // (D3DTRANSFORMSTATETYPE, matrix)
    std::array<uint32_t, 6> viewport{};                         // X, Y, Width, Height, MinZ, MaxZ
    std::array<uint32_t, 17> material{};
    std::vector<Light> lights;
    std::array<std::array<uint32_t, 4>, 6> clipPlanes{};
    std::array<uint32_t, 4> scissor{};
    std::array<Stream, 16> streams{};
    uint32_t indices = 0, declaration = 0, fvf = 0, vertexShader = 0, pixelShader = 0;
    std::array<uint32_t, 4> renderTargets{};
    uint32_t depthStencil = 0;
    std::vector<uint32_t> vsFloat, vsInt, vsBool, psFloat, psInt, psBool; // constant registers, as words
};
constexpr uint32_t vsFloatRegisters = 256, psFloatRegisters = 224, intRegisters = 16, boolRegisters = 16;
inline bool operator==(const Light& a, const Light& b) { return a.index == b.index && a.enabled == b.enabled && a.words == b.words; }
inline bool operator==(const Stream& a, const Stream& b) { return a.buffer == b.buffer && a.offset == b.offset && a.stride == b.stride && a.frequency == b.frequency; }
inline bool operator==(const DeviceState& a, const DeviceState& b) {
    return a.device == b.device && a.renderStates == b.renderStates && a.stageStates == b.stageStates && a.samplerStates == b.samplerStates
        && a.textures == b.textures && a.transforms == b.transforms && a.viewport == b.viewport && a.material == b.material
        && a.lights == b.lights && a.clipPlanes == b.clipPlanes && a.scissor == b.scissor && a.streams == b.streams
        && a.indices == b.indices && a.declaration == b.declaration && a.fvf == b.fvf && a.vertexShader == b.vertexShader
        && a.pixelShader == b.pixelShader && a.renderTargets == b.renderTargets && a.depthStencil == b.depthStencil
        && a.vsFloat == b.vsFloat && a.vsInt == b.vsInt && a.vsBool == b.vsBool && a.psFloat == b.psFloat && a.psInt == b.psInt && a.psBool == b.psBool;
}
template<class Pairs> inline void checkSortedKeys(const Pairs& pairs, uint32_t limit, const char* message) {
    for (size_t i = 0; i < pairs.size(); ++i) check(pairs[i].first < limit && (!i || pairs[i - 1].first < pairs[i].first), message);
}
// Structure only. That each address names a live object of the right kind is checked against the
// capture's object list (capture_format.h) and, at load, against the bridge.
inline void validate(const DeviceState& state) {
    check(state.device != 0, "Device state without a device");
    check(state.renderTargets[0] != 0, "Device state without render target 0");
    checkSortedKeys(state.renderStates, 256, "Invalid device render states");
    checkSortedKeys(state.stageStates, 8 * 64, "Invalid device texture stage states");
    checkSortedKeys(state.samplerStates, 260 * 16, "Invalid device sampler states");
    checkSortedKeys(state.textures, 260, "Invalid device texture bindings");
    for (const auto& item : state.textures) check(item.first < 16 || (item.first >= 257 && item.first <= 259), "Invalid device sampler");
    checkSortedKeys(state.transforms, 512, "Invalid device transforms");
    check(state.lights.size() <= 64, "Too many device lights");
    for (size_t i = 0; i < state.lights.size(); ++i) check(!i || state.lights[i - 1].index < state.lights[i].index, "Device lights must be sorted");
    for (const auto& stream : state.streams) check(stream.buffer || (!stream.offset && !stream.stride), "Unbound stream with an offset");
    check(state.vsFloat.size() == vsFloatRegisters * 4 && state.psFloat.size() == psFloatRegisters * 4, "Invalid device float constants");
    check(state.vsInt.size() == intRegisters * 4 && state.psInt.size() == intRegisters * 4, "Invalid device integer constants");
    check(state.vsBool.size() == boolRegisters && state.psBool.size() == boolRegisters, "Invalid device boolean constants");
    for (uint32_t value : state.vsBool) check(value <= 1, "Invalid device boolean constant");
    for (uint32_t value : state.psBool) check(value <= 1, "Invalid device boolean constant");
}
// Every object address the state refers to, with the bridge kinds it may be.
struct DeviceReference { uint32_t address; uint32_t kinds; };  // kinds: bit per bridge kind
inline std::vector<DeviceReference> references(const DeviceState& state) {
    constexpr uint32_t texture = (1u << 3) | (1u << 4) | (1u << 14), surface = 1u << 5;
    std::vector<DeviceReference> out{{state.device, 1u << 2}};
    for (const auto& item : state.textures) if (item.second) out.push_back({item.second, texture});
    for (const auto& stream : state.streams) if (stream.buffer) out.push_back({stream.buffer, 1u << 6});
    if (state.indices) out.push_back({state.indices, 1u << 7});
    if (state.declaration) out.push_back({state.declaration, 1u << 10});
    if (state.vertexShader) out.push_back({state.vertexShader, 1u << 8});
    if (state.pixelShader) out.push_back({state.pixelShader, 1u << 9});
    for (uint32_t target : state.renderTargets) if (target) out.push_back({target, surface});
    if (state.depthStencil) out.push_back({state.depthStencil, surface});
    return out;
}
// Byte form used by the capture file: a sequence of 32-bit words.
inline void serialize(const DeviceState& state, std::vector<uint32_t>& out) {
    auto pairs = [&](const auto& list) { out.push_back(uint32_t(list.size())); for (const auto& item : list) { out.push_back(item.first); out.push_back(item.second); } };
    auto words = [&](const auto& list) { out.push_back(uint32_t(list.size())); out.insert(out.end(), list.begin(), list.end()); };
    out.push_back(state.device);
    pairs(state.renderStates); pairs(state.stageStates); pairs(state.samplerStates); pairs(state.textures);
    out.push_back(uint32_t(state.transforms.size()));
    for (const auto& item : state.transforms) { out.push_back(item.first); out.insert(out.end(), item.second.begin(), item.second.end()); }
    out.insert(out.end(), state.viewport.begin(), state.viewport.end());
    out.insert(out.end(), state.material.begin(), state.material.end());
    out.push_back(uint32_t(state.lights.size()));
    for (const auto& light : state.lights) { out.push_back(light.index); out.push_back(light.enabled ? 1u : 0u); out.insert(out.end(), light.words.begin(), light.words.end()); }
    for (const auto& plane : state.clipPlanes) out.insert(out.end(), plane.begin(), plane.end());
    out.insert(out.end(), state.scissor.begin(), state.scissor.end());
    for (const auto& stream : state.streams) { out.push_back(stream.buffer); out.push_back(stream.offset); out.push_back(stream.stride); out.push_back(stream.frequency); }
    for (uint32_t value : {state.indices, state.declaration, state.fvf, state.vertexShader, state.pixelShader}) out.push_back(value);
    out.insert(out.end(), state.renderTargets.begin(), state.renderTargets.end());
    out.push_back(state.depthStencil);
    words(state.vsFloat); words(state.vsInt); words(state.vsBool); words(state.psFloat); words(state.psInt); words(state.psBool);
}
template<class Next> inline DeviceState deserialize(Next&& next) {
    DeviceState state;
    auto count = [&](uint32_t limit) { uint32_t n = next(); check(n <= limit, "Invalid device state count"); return n; };
    auto pairs = [&](auto& list, uint32_t limit) { uint32_t n = count(limit); for (uint32_t i = 0; i < n; ++i) { uint32_t key = next(); list.emplace_back(key, next()); } };
    auto words = [&](std::vector<uint32_t>& list, uint32_t limit) { uint32_t n = count(limit); for (uint32_t i = 0; i < n; ++i) list.push_back(next()); };
    state.device = next();
    pairs(state.renderStates, 256); pairs(state.stageStates, 8 * 64); pairs(state.samplerStates, 260 * 16); pairs(state.textures, 260);
    uint32_t transforms = count(512);
    for (uint32_t i = 0; i < transforms; ++i) { std::array<uint32_t, 16> matrix{}; uint32_t type = next(); for (auto& v : matrix) v = next(); state.transforms.emplace_back(type, matrix); }
    for (auto& v : state.viewport) v = next();
    for (auto& v : state.material) v = next();
    uint32_t lights = count(64);
    for (uint32_t i = 0; i < lights; ++i) { Light light; light.index = next(); light.enabled = next() != 0; for (auto& v : light.words) v = next(); state.lights.push_back(light); }
    for (auto& plane : state.clipPlanes) for (auto& v : plane) v = next();
    for (auto& v : state.scissor) v = next();
    for (auto& stream : state.streams) { stream.buffer = next(); stream.offset = next(); stream.stride = next(); stream.frequency = next(); }
    for (uint32_t* v : {&state.indices, &state.declaration, &state.fvf, &state.vertexShader, &state.pixelShader}) *v = next();
    for (auto& v : state.renderTargets) v = next();
    state.depthStencil = next();
    words(state.vsFloat, vsFloatRegisters * 4); words(state.vsInt, intRegisters * 4); words(state.vsBool, boolRegisters);
    words(state.psFloat, psFloatRegisters * 4); words(state.psInt, intRegisters * 4); words(state.psBool, boolRegisters);
    validate(state);
    return state;
}
}
