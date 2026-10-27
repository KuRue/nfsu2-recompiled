#include "../android/app/src/main/cpp/capture_format.h"
#include <cstring>
#include <iostream>
#include <string>

namespace {
int failures = 0;
void expect(bool value, const char* what) {
    if (!value) { std::cerr << "FAIL " << what << '\n'; ++failures; }
}
void expectThrow(const char* what, const nfs_capture::Bytes& file) {
    bool threw = false;
    try { nfs_capture::decode(file); } catch (const std::runtime_error&) { threw = true; }
    expect(threw, what);
}
std::string hex64(char fill) { return std::string(64, fill); }

nfs_capture::Capture sample() {
    nfs_capture::Capture capture;
    capture.build = hex64('a');
    capture.game = hex64('b');
    capture.pageSize = 4096;
    nfs_capture::ThreadImage main;
    main.id = 1; main.capturer = true; main.resumable = true; main.wait = nfs_capture::WaitKind::Present;
    main.cpu.gpr[0] = 0x11111111; main.cpu.st[0] = 0x3ff0000000000000ull; main.cpu.xmm[1][0] = 0x7e;
    main.frames.push_back({0x401000u, 0x401010u, 0x7f000000u, -2, 1});
    nfs_capture::ThreadImage worker;
    worker.id = 4; worker.resumable = true; worker.wait = nfs_capture::WaitKind::Event; worker.waitHandle = 0xf0040001;
    worker.frames.push_back({0x520000u, 0x520020u, 0x7f000100u, 0, 0});
    nfs_capture::ThreadImage timer;
    timer.id = 9; timer.outsideGuest = true; timer.wait = nfs_capture::WaitKind::TimerIdle;
    capture.threads.push_back(main);
    capture.threads.push_back(worker);
    capture.threads.push_back(timer);
    capture.events.push_back({0xf0040001u, false, true, "race"});
    capture.mutexes.push_back({0xf0080002u, 4u, 1u, false, ""});
    capture.timers.push_back({3u, 16u, 0x430000u, 0u, true, false, -5});
    nfs_capture::Region region;
    region.address = 4096; region.bytes.assign(4096, 0x5a); region.bytes[10] = 0x11;
    capture.regions.push_back(std::move(region));
    capture.protections.push_back({4096u, 4u});
    capture.inventory.push_back({1u, 2u});
    return capture;
}
bool sameCpu(const nfs_capture::CpuImage& a, const nfs_capture::CpuImage& b) {
    if (std::memcmp(a.gpr, b.gpr, sizeof a.gpr) || std::memcmp(a.flags, b.flags, sizeof a.flags)) return false;
    if (std::memcmp(a.segments, b.segments, sizeof a.segments) || std::memcmp(a.st, b.st, sizeof a.st)) return false;
    if (std::memcmp(a.mm, b.mm, sizeof a.mm)) return false;
    if (a.fs != b.fs || a.gs != b.gs || a.cur != b.cur || a.mxcsr != b.mxcsr || a.cw != b.cw || a.top != b.top) return false;
    for (int i = 0; i < 8; ++i) if (std::memcmp(a.xmm[i], b.xmm[i], 16)) return false;
    return true;
}
bool sameFrame(const nfs_capture::Frame& a, const nfs_capture::Frame& b) {
    return a.function == b.function && a.site == b.site && a.target == b.target && a.fpuCompare == b.fpuCompare && a.direction == b.direction;
}
bool same(const nfs_capture::Capture& left, const nfs_capture::Capture& right) {
    if (left.build != right.build || left.game != right.game || left.careerRollback != right.careerRollback) return false;
    if (left.omitted != right.omitted || left.pageSize != right.pageSize) return false;
    if (left.threads.size() != right.threads.size() || left.events.size() != right.events.size()) return false;
    if (left.mutexes.size() != right.mutexes.size() || left.timers.size() != right.timers.size()) return false;
    if (left.regions.size() != right.regions.size() || left.protections.size() != right.protections.size()) return false;
    if (left.inventory.size() != right.inventory.size()) return false;
    for (size_t i = 0; i < left.threads.size(); ++i) {
        const auto& a = left.threads[i]; const auto& b = right.threads[i];
        if (a.id != b.id || a.capturer != b.capturer || a.outsideGuest != b.outsideGuest || a.resumable != b.resumable) return false;
        if (a.wait != b.wait || a.waitHandle != b.waitHandle || a.blockedAt != b.blockedAt || a.frames.size() != b.frames.size()) return false;
        if (!sameCpu(a.cpu, b.cpu)) return false;
        for (size_t n = 0; n < a.frames.size(); ++n) if (!sameFrame(a.frames[n], b.frames[n])) return false;
    }
    if (left.events[0].id != right.events[0].id || left.events[0].manual != right.events[0].manual
        || left.events[0].signaled != right.events[0].signaled || left.events[0].name != right.events[0].name) return false;
    if (left.mutexes[0].owner != right.mutexes[0].owner || left.mutexes[0].depth != right.mutexes[0].depth
        || left.mutexes[0].abandoned != right.mutexes[0].abandoned) return false;
    if (left.timers[0].dueMs != right.timers[0].dueMs || left.timers[0].periodic != right.timers[0].periodic) return false;
    if (left.regions[0].bytes != right.regions[0].bytes || left.regions[0].address != right.regions[0].address) return false;
    if (left.protections[0].flags != right.protections[0].flags || left.inventory[0].count != right.inventory[0].count) return false;
    return true;
}
}

int main() {
    using namespace nfs_capture;
    expect(sha256Hex(nullptr, 0) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", "sha256 empty");
    const uint8_t abc[] = {'a', 'b', 'c'};
    expect(sha256Hex(abc, 3) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", "sha256 abc");
    expect(waitKindOf("WaitForSingleObject(mutex)") == WaitKind::Mutex, "mutex wait name");
    expect(waitKindOf("not a wait") == WaitKind::None, "unknown wait name");

    ThreadView ready[] = {
        {1, true, true, true, false, true, WaitKind::Present, 0},
        {4, true, false, true, false, true, WaitKind::Event, 0},
        {9, true, false, true, true, false, WaitKind::TimerIdle, 0},
        {3, false, false, false, false, false, WaitKind::None, 0},
    };
    uint32_t id = 99;
    expect(quiesceBlocker(ready, 4, id) == nullptr, "quiesced race is ready");
    ThreadView slice = ready[1]; slice.wait = WaitKind::Slice; slice.resumable = false; slice.blockedAt = 0x481fd0;
    ThreadView blocked[] = {ready[0], slice, ready[2]};
    const char* reason = quiesceBlocker(blocked, 3, id);
    expect(reason && id == 4 && std::string(reason).find("resumable") != std::string::npos, "mid-function worker blocks capture");
    ThreadView outside[] = {ready[0], ThreadView{4, true, false, true, true, false, WaitKind::Event, 0}};
    expect(quiesceBlocker(outside, 2, id) != nullptr && id == 4, "guest wait cannot be outside guest code");
    ThreadView two[] = {ready[0], ready[0]};
    two[1].id = 2;
    expect(quiesceBlocker(two, 2, id) != nullptr, "two presenters are rejected");

    auto encoded = encode(sample());
    auto decoded = decode(encoded);
    expect(same(sample(), decoded), "capture round trip");
    auto path = std::filesystem::temp_directory_path() / "nfs-capture-test.nfs";
    writeAtomic(path, encoded);
    std::ifstream input(path, std::ios::binary);
    Bytes stored((std::istreambuf_iterator<char>(input)), {});
    expect(same(sample(), decode(stored)), "atomic capture readback");
    std::filesystem::remove(path);

    auto bad = encoded; bad[20] ^= 0xff; expectThrow("checksum", bad);
    auto career = sample(); career.careerRollback = 1;
    bool threw = false; try { encode(career); } catch (const std::runtime_error&) { threw = true; }
    expect(threw, "career rollback rejected");
    auto claimed = sample(); claimed.omitted = omitGraphics;
    threw = false; try { encode(claimed); } catch (const std::runtime_error&) { threw = true; }
    expect(threw, "missing host-state omission rejected");
    auto overlap = sample(); overlap.regions.push_back(overlap.regions.front());
    threw = false; try { encode(overlap); } catch (const std::runtime_error&) { threw = true; }
    expect(threw, "overlapping memory rejected");
    auto truncated = encoded; truncated.pop_back(); expectThrow("truncated", truncated);
    if (failures) { std::cerr << failures << " capture checks failed\n"; return 1; }
    std::cout << "capture quiesce and format checks passed\n";
    return 0;
}
