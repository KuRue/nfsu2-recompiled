#pragma once
// Capture of one quiesced guest moment. This file is not a restore: graphics,
// audio, heap metadata and file contents are recorded as omitted on purpose,
// and careerRollback must be zero so a later restore cannot roll career files back.
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
#include <algorithm>
#include <fcntl.h>
#include <unistd.h>

namespace nfs_capture {
constexpr uint32_t schema = 2; // 2 adds open files; 1 is still read
constexpr uint32_t omitGraphics = 1u, omitAudio = 2u, omitHeap = 4u, omitFileContents = 8u;
// Heap bookkeeping and sound objects are stored from schema 2; omitHeap and omitAudio are clear
// only when they are.
constexpr uint32_t omitRequired = omitGraphics | omitFileContents;
constexpr uint64_t maxBytes = 768ull << 20;
constexpr size_t maxThreads = 64, maxFrames = 256, maxObjects = 4096, maxRegions = 4096, maxName = 260;

enum class WaitKind : uint32_t {
    None = 0, Slice = 1, Present = 2, Sleep = 3, Event = 4, Mutex = 5, Thread = 6,
    Multiple = 7, CriticalSection = 8, TimerIdle = 9, Suspended = 10, Message = 11, Socket = 12
};

inline WaitKind waitKindOf(const char* name) {
    if (!name || !name[0]) return WaitKind::Slice;
    auto is = [&](const char* value) { return std::strcmp(name, value) == 0; };
    if (is("Sleep")) return WaitKind::Sleep;
    if (is("WaitForSingleObject(event)")) return WaitKind::Event;
    if (is("WaitForSingleObject(mutex)")) return WaitKind::Mutex;
    if (is("WaitForSingleObject(thread)")) return WaitKind::Thread;
    if (is("WaitForMultipleObjects(all)") || is("WaitForMultipleObjects(any)")) return WaitKind::Multiple;
    if (is("EnterCriticalSection")) return WaitKind::CriticalSection;
    if (is("TimerIdle")) return WaitKind::TimerIdle;
    if (is("Suspended")) return WaitKind::Suspended;
    if (is("GetMessage")) return WaitKind::Message;
    if (is("socket send/connect") || is("socket recv/accept")) return WaitKind::Socket;
    if (is("Present")) return WaitKind::Present;
    return WaitKind::None;
}

inline bool safeGuestWait(WaitKind kind) {
    return kind == WaitKind::Sleep || kind == WaitKind::Event || kind == WaitKind::Mutex || kind == WaitKind::Thread
        || kind == WaitKind::Multiple || kind == WaitKind::CriticalSection || kind == WaitKind::Message || kind == WaitKind::Socket;
}
inline bool safeHostIdle(WaitKind kind) {
    return kind == WaitKind::TimerIdle || kind == WaitKind::Suspended || kind == WaitKind::Slice;
}

// One row of the decision made while the capturer holds the guest machine lock.
struct ThreadView {
    uint32_t id = 0;
    bool alive = false;
    bool capturer = false;
    bool parked = false;
    bool outsideGuest = false;
    bool resumable = false;
    WaitKind wait = WaitKind::None;
    uint32_t blockedAt = 0;
};

// Null when every live thread can be captured. Otherwise a stable reason; id names the thread.
inline const char* quiesceBlocker(const ThreadView* views, size_t count, uint32_t& id) {
    id = 0;
    int capturers = 0;
    for (size_t i = 0; i < count; ++i) {
        const ThreadView& view = views[i];
        if (view.capturer) {
            ++capturers;
            if (!view.resumable || view.outsideGuest || view.wait != WaitKind::Present) {
                id = view.id;
                return "presenting thread is not resumable";
            }
        } else if (view.alive) {
            if (!view.parked) { id = view.id; return "thread is not parked"; }
            if (view.outsideGuest) {
                if (!safeHostIdle(view.wait)) { id = view.id; return "thread left guest code outside its idle loop"; }
            } else if (!view.resumable || !safeGuestWait(view.wait)) {
                id = view.id;
                return "thread is not at a resumable wait";
            }
        }
    }
    if (capturers != 1) return "capture needs exactly one presenting thread";
    return nullptr;
}

struct CpuImage {
    uint32_t gpr[8]{};
    uint32_t fs = 0, gs = 0, cur = 0;
    uint32_t flags[4]{};
    uint32_t mxcsr = 0x1f80;
    uint16_t segments[6]{};
    uint16_t cw = 0x027f;
    uint32_t top = 0;
    uint64_t st[8]{};
    uint64_t mm[8]{};
    uint8_t xmm[8][16]{};
};
struct Frame { uint32_t function = 0, site = 0, target = 0; int32_t fpuCompare = 0, direction = 0; };
struct ThreadImage {
    uint32_t id = 0;
    bool capturer = false, outsideGuest = false, resumable = false;
    WaitKind wait = WaitKind::None;
    uint32_t waitHandle = 0, blockedAt = 0;
    CpuImage cpu;
    std::vector<Frame> frames;
};
struct EventImage { uint32_t id = 0; bool manual = false, signaled = false; std::string name; };
struct MutexImage { uint32_t id = 0, owner = 0, depth = 0; bool abandoned = false; std::string name; };
struct TimerImage {
    uint32_t id = 0, delay = 0, callback = 0, parameter = 0;
    bool periodic = false, cancelled = false;
    int64_t dueMs = 0;
};
struct Region { uint32_t address = 0; std::vector<uint8_t> bytes; };
// An open file: reopened by path at its position. Contents are not stored, and a career or
// profile file (under the Windows profile folders) is reopened as it is now, never rolled back.
// A DirectSound object. Buffer samples are not stored: the guest copy at guestSamples is in
// guest memory and the host voice is rebuilt from it. kind 1 is the device, 2 a buffer.
struct SoundImage {
    uint32_t address = 0, kind = 0, table = 0, references = 0, flags = 0, guestSamples = 0, volume = 0, pan = 0;
    bool locked = false; uint32_t lockOffset = 0, lockBytes = 0;
    bool voice = false; uint32_t sampleBytes = 0, channels = 0, bits = 0, rate = 0, frequency = 0, publishes = 0;
    double position = 0, staleFrames = 0; float left = 1, right = 1; bool playing = false, loop = false;
};
struct HeapBlock { uint32_t address = 0, capacity = 0, requested = 0; };
struct HeapImage { uint32_t id = 0, options = 0, maximum = 0; std::vector<HeapBlock> blocks; };
struct FileImage { uint32_t handle = 0, access = 0; uint64_t offset = 0; bool career = false; std::string path; };
inline bool careerPath(const std::string& path) {
    std::string lower = path; for (auto& c : lower) c = char(std::tolower(static_cast<unsigned char>(c)));
    return lower.rfind("documents/", 0) == 0 || lower.rfind("appdata/", 0) == 0;
}
struct Protect { uint32_t address = 0, flags = 4; };
struct Inventory { uint32_t kind = 0, count = 0; };
struct Capture {
    std::string build, game;
    uint32_t careerRollback = 0;
    uint32_t omitted = omitRequired | omitHeap | omitAudio;
    uint32_t pageSize = 4096;
    std::vector<ThreadImage> threads;
    std::vector<EventImage> events;
    std::vector<MutexImage> mutexes;
    std::vector<TimerImage> timers;
    std::vector<Region> regions;
    std::vector<Protect> protections;
    std::vector<Inventory> inventory;
    std::vector<FileImage> files;
    uint32_t heapNext = 0;
    std::vector<HeapImage> heaps;
    std::vector<SoundImage> sounds;
    // Graphics objects as (guest address, kind): contents are not stored, so this capture can only
    // be loaded into the session that still holds exactly these objects.
    std::vector<std::pair<uint32_t, uint32_t>> graphics;
    // Initialized critical sections (sorted). Their owner and recursion live in guest memory.
    std::vector<uint32_t> criticalSections;
};

using Bytes = std::vector<uint8_t>;
inline void require(bool value, const char* reason) { if (!value) throw std::runtime_error(reason); }
inline bool identity(const std::string& text) {
    return text.size() == 64 && text.find_first_not_of("0123456789abcdef") == std::string::npos;
}
inline bool allowedProtect(uint32_t flags) {
    return flags == 1 || flags == 2 || flags == 4 || flags == 8 || flags == 0x10 || flags == 0x20 || flags == 0x40 || flags == 0x80;
}
inline void validate(const Capture& capture) {
    require(identity(capture.build) && identity(capture.game), "Invalid capture identity");
    require(capture.careerRollback == 0, "Career files must not be rolled back");
    require((capture.omitted & omitRequired) == omitRequired, "Capture claims host state it does not contain");
    require(capture.pageSize >= 4096 && (capture.pageSize & (capture.pageSize - 1)) == 0, "Invalid capture page size");
    require(!capture.threads.empty() && capture.threads.size() <= maxThreads, "Invalid capture thread count");
    int capturers = 0;
    for (const auto& thread : capture.threads) {
        require(uint32_t(thread.wait) <= uint32_t(WaitKind::Socket), "Invalid capture wait");
        require(thread.frames.size() <= maxFrames, "Too many capture frames");
        if (thread.capturer) {
            ++capturers;
            require(!thread.outsideGuest && thread.resumable && thread.wait == WaitKind::Present && !thread.frames.empty(),
                "Presenting thread is not a resumable capture");
        } else if (thread.outsideGuest) {
            require(thread.frames.empty() && safeHostIdle(thread.wait), "Idle thread has a guest continuation");
        } else {
            require(thread.resumable && safeGuestWait(thread.wait) && !thread.frames.empty(), "Worker thread is not a resumable wait");
        }
        for (const auto& frame : thread.frames) require(frame.function && frame.site && frame.target, "Invalid capture frame");
    }
    require(capturers == 1, "Capture needs exactly one presenting thread");
    require(capture.events.size() <= maxObjects && capture.mutexes.size() <= maxObjects && capture.timers.size() <= maxObjects, "Too many kernel objects");
    for (const auto& item : capture.events) require(item.name.size() <= maxName, "Event name too long");
    for (const auto& item : capture.mutexes) require(item.name.size() <= maxName, "Mutex name too long");
    require(capture.regions.size() <= maxRegions, "Too many memory regions");
    std::vector<std::pair<uint64_t, uint64_t>> spans;
    uint64_t total = 0;
    for (const auto& region : capture.regions) {
        uint64_t end = uint64_t(region.address) + region.bytes.size();
        total += region.bytes.size();
        require(region.address >= capture.pageSize && !region.bytes.empty() && end <= 0x100000000ull && total <= maxBytes, "Invalid memory region");
        require(region.address % capture.pageSize == 0 && region.bytes.size() % capture.pageSize == 0, "Memory region is not page aligned");
        spans.emplace_back(region.address, end);
    }
    std::sort(spans.begin(), spans.end());
    for (size_t i = 1; i < spans.size(); ++i) require(spans[i - 1].second <= spans[i].first, "Overlapping memory regions");
    for (const auto& page : capture.protections) {
        require(page.address % capture.pageSize == 0 && allowedProtect(page.flags), "Invalid page protection");
    }
    require(capture.heaps.empty() == bool(capture.omitted & omitHeap), "Capture heap flag does not match its heaps");
    require(capture.heaps.size() <= maxObjects, "Too many heaps");
    size_t blocks = 0;
    for (const auto& heap : capture.heaps) blocks += heap.blocks.size();
    require(blocks <= (size_t(1) << 22), "Too many heap blocks");
    require(!(capture.omitted & omitAudio) || capture.sounds.empty(), "Capture marks audio omitted but lists sounds");
    require(capture.sounds.size() <= maxObjects, "Too many sounds");
    for (size_t i = 0; i < capture.sounds.size(); ++i) {
        const auto& sound = capture.sounds[i];
        require((sound.kind == 1 || sound.kind == 2) && sound.address && sound.table && sound.references, "Invalid capture sound");
        for (size_t j = 0; j < i; ++j) require(capture.sounds[j].address != sound.address, "Duplicate capture sound");
        if (!sound.voice) { require(!sound.sampleBytes && !sound.guestSamples && !sound.locked, "Sound without a voice has samples"); continue; }
        require(sound.kind == 2 && (sound.channels == 1 || sound.channels == 2) && (sound.bits == 8 || sound.bits == 16), "Invalid capture voice format");
        uint32_t block = sound.channels * (sound.bits / 8);
        require(sound.rate >= 100 && sound.rate <= 200000 && sound.frequency >= 100 && sound.frequency <= 200000, "Invalid capture voice rate");
        require(sound.sampleBytes && sound.sampleBytes <= (16u << 20) && sound.sampleBytes % block == 0 && sound.guestSamples, "Invalid capture voice samples");
        require(std::isfinite(sound.position) && sound.position >= 0 && sound.position < double(sound.sampleBytes / block) + 1, "Invalid capture voice position");
        require(std::isfinite(sound.staleFrames) && std::isfinite(sound.left) && std::isfinite(sound.right), "Invalid capture voice levels");
        require(!sound.locked || (sound.lockOffset < sound.sampleBytes && sound.lockBytes <= sound.sampleBytes), "Invalid capture sound lock");
    }
    require(capture.graphics.size() <= (size_t(1) << 20), "Too many graphics objects");
    for (size_t i = 1; i < capture.graphics.size(); ++i) require(capture.graphics[i - 1].first < capture.graphics[i].first, "Graphics objects must be sorted and unique");
    require(capture.criticalSections.size() <= maxObjects * 4, "Too many critical sections");
    for (size_t i = 0; i < capture.criticalSections.size(); ++i)
        require(capture.criticalSections[i] >= capture.pageSize && (!i || capture.criticalSections[i - 1] < capture.criticalSections[i]), "Critical sections must be sorted and unique");
    require(capture.files.size() <= maxObjects, "Too many open files");
    for (size_t i = 0; i < capture.files.size(); ++i) {
        const auto& file = capture.files[i];
        require(!file.path.empty() && file.path.size() <= maxName && file.path.front() != '/' && file.path.find(':') == std::string::npos, "Invalid capture file path");
        for (const auto& part : std::filesystem::path(file.path)) require(part != "..", "Capture file path leaves the game folder");
        require(file.career == careerPath(file.path), "Capture file career flag does not match its path");
        for (size_t j = 0; j < i; ++j) require(capture.files[j].handle != file.handle, "Duplicate capture file handle");
    }
}

inline void word(Bytes& out, uint32_t value) { for (unsigned i = 0; i < 4; ++i) out.push_back(uint8_t(value >> (i * 8))); }
inline void qword(Bytes& out, uint64_t value) { word(out, uint32_t(value)); word(out, uint32_t(value >> 32)); }
inline void putName(Bytes& out, const std::string& name) {
    require(name.size() <= maxName, "Name too long");
    out.push_back(uint8_t(name.size())); out.push_back(uint8_t(name.size() >> 8));
    out.insert(out.end(), name.begin(), name.end());
}
struct Reader {
    const Bytes& bytes; size_t offset = 0;
    uint32_t word() {
        require(offset <= bytes.size() && bytes.size() - offset >= 4, "Truncated capture");
        uint32_t value = 0; for (unsigned i = 0; i < 4; ++i) value |= uint32_t(bytes[offset++]) << (i * 8); return value;
    }
    uint64_t qword() { uint32_t low = word(), high = word(); return uint64_t(low) | (uint64_t(high) << 32); }
    Bytes take(size_t count) {
        require(offset <= bytes.size() && count <= bytes.size() - offset, "Truncated capture");
        Bytes out(bytes.begin() + offset, bytes.begin() + offset + count); offset += count; return out;
    }
    std::string name() {
        require(offset + 2 <= bytes.size(), "Truncated capture");
        uint32_t count = bytes[offset] | (uint32_t(bytes[offset + 1]) << 8); offset += 2;
        require(count <= maxName, "Name too long");
        Bytes raw = take(count); return std::string(raw.begin(), raw.end());
    }
};
inline uint32_t crc(const Bytes& bytes) {
    uint32_t value = ~0u;
    for (uint8_t byte : bytes) { value ^= byte; for (unsigned i = 0; i < 8; ++i) value = (value >> 1) ^ (0xedb88320u & uint32_t(-int(value & 1))); }
    return ~value;
}
inline void putCpu(Bytes& out, const CpuImage& cpu) {
    for (uint32_t value : cpu.gpr) word(out, value);
    word(out, cpu.fs); word(out, cpu.gs); word(out, cpu.cur);
    for (uint32_t value : cpu.flags) word(out, value);
    word(out, cpu.mxcsr);
    for (uint16_t value : cpu.segments) { out.push_back(uint8_t(value)); out.push_back(uint8_t(value >> 8)); }
    out.push_back(uint8_t(cpu.cw)); out.push_back(uint8_t(cpu.cw >> 8));
    word(out, cpu.top);
    for (uint64_t value : cpu.st) qword(out, value);
    for (uint64_t value : cpu.mm) qword(out, value);
    for (const auto& value : cpu.xmm) out.insert(out.end(), value, value + 16);
}
inline CpuImage takeCpu(Reader& reader) {
    CpuImage cpu;
    for (uint32_t& value : cpu.gpr) value = reader.word();
    cpu.fs = reader.word(); cpu.gs = reader.word(); cpu.cur = reader.word();
    for (uint32_t& value : cpu.flags) value = reader.word();
    cpu.mxcsr = reader.word();
    for (uint16_t& value : cpu.segments) { Bytes pair = reader.take(2); value = uint16_t(pair[0] | (pair[1] << 8)); }
    Bytes control = reader.take(2); cpu.cw = uint16_t(control[0] | (control[1] << 8));
    cpu.top = reader.word();
    for (uint64_t& value : cpu.st) value = reader.qword();
    for (uint64_t& value : cpu.mm) value = reader.qword();
    for (auto& value : cpu.xmm) { Bytes raw = reader.take(16); std::memcpy(value, raw.data(), 16); }
    return cpu;
}
inline Bytes encode(const Capture& capture) {
    validate(capture);
    Bytes payload;
    payload.insert(payload.end(), capture.build.begin(), capture.build.end());
    payload.insert(payload.end(), capture.game.begin(), capture.game.end());
    word(payload, capture.careerRollback); word(payload, capture.omitted); word(payload, capture.pageSize);
    word(payload, uint32_t(capture.threads.size()));
    for (const auto& thread : capture.threads) {
        word(payload, thread.id);
        word(payload, (thread.capturer ? 1u : 0u) | (thread.outsideGuest ? 2u : 0u) | (thread.resumable ? 4u : 0u));
        word(payload, uint32_t(thread.wait)); word(payload, thread.waitHandle); word(payload, thread.blockedAt);
        putCpu(payload, thread.cpu);
        word(payload, uint32_t(thread.frames.size()));
        for (const auto& frame : thread.frames) {
            word(payload, frame.function); word(payload, frame.site); word(payload, frame.target);
            word(payload, uint32_t(frame.fpuCompare)); word(payload, uint32_t(frame.direction));
        }
    }
    word(payload, uint32_t(capture.events.size()));
    for (const auto& item : capture.events) { word(payload, item.id); word(payload, (item.manual ? 1u : 0u) | (item.signaled ? 2u : 0u)); putName(payload, item.name); }
    word(payload, uint32_t(capture.mutexes.size()));
    for (const auto& item : capture.mutexes) {
        word(payload, item.id); word(payload, item.owner); word(payload, item.depth); word(payload, item.abandoned ? 1u : 0u); putName(payload, item.name);
    }
    word(payload, uint32_t(capture.timers.size()));
    for (const auto& timer : capture.timers) {
        word(payload, timer.id); word(payload, timer.delay); word(payload, timer.callback); word(payload, timer.parameter);
        word(payload, (timer.periodic ? 1u : 0u) | (timer.cancelled ? 2u : 0u));
        qword(payload, uint64_t(timer.dueMs));
    }
    word(payload, uint32_t(capture.regions.size()));
    for (const auto& region : capture.regions) {
        word(payload, region.address); word(payload, uint32_t(region.bytes.size()));
        payload.insert(payload.end(), region.bytes.begin(), region.bytes.end());
    }
    word(payload, uint32_t(capture.protections.size()));
    for (const auto& page : capture.protections) { word(payload, page.address); word(payload, page.flags); }
    word(payload, uint32_t(capture.inventory.size()));
    for (const auto& item : capture.inventory) { word(payload, item.kind); word(payload, item.count); }
    word(payload, uint32_t(capture.files.size()));
    for (const auto& item : capture.files) {
        word(payload, item.handle); word(payload, item.access); qword(payload, item.offset); word(payload, item.career ? 1u : 0u); putName(payload, item.path);
    }
    word(payload, uint32_t(capture.sounds.size()));
    for (const auto& s : capture.sounds) {
        for (uint32_t v : {s.address, s.kind, s.table, s.references, s.flags, s.guestSamples, s.volume, s.pan, s.lockOffset, s.lockBytes,
                           s.sampleBytes, s.channels, s.bits, s.rate, s.frequency, s.publishes}) word(payload, v);
        word(payload, (s.locked ? 1u : 0u) | (s.voice ? 2u : 0u) | (s.playing ? 4u : 0u) | (s.loop ? 8u : 0u));
        uint64_t bits; std::memcpy(&bits, &s.position, 8); qword(payload, bits); std::memcpy(&bits, &s.staleFrames, 8); qword(payload, bits);
        uint32_t level; std::memcpy(&level, &s.left, 4); word(payload, level); std::memcpy(&level, &s.right, 4); word(payload, level);
    }
    word(payload, uint32_t(capture.graphics.size()));
    for (const auto& item : capture.graphics) { word(payload, item.first); word(payload, item.second); }
    word(payload, uint32_t(capture.criticalSections.size()));
    for (uint32_t address : capture.criticalSections) word(payload, address);
    word(payload, capture.heapNext); word(payload, uint32_t(capture.heaps.size()));
    for (const auto& heap : capture.heaps) {
        word(payload, heap.id); word(payload, heap.options); word(payload, heap.maximum); word(payload, uint32_t(heap.blocks.size()));
        for (const auto& block : heap.blocks) { word(payload, block.address); word(payload, block.capacity); word(payload, block.requested); }
    }
    Bytes file = {'N','F','S','C','A','P','1'};
    word(file, schema); word(file, uint32_t(payload.size())); word(file, crc(payload));
    file.insert(file.end(), payload.begin(), payload.end());
    require(file.size() <= maxBytes + 65536, "Capture too large");
    return file;
}
inline Capture decode(const Bytes& file) {
    require(file.size() <= maxBytes + 65536, "Capture too large");
    Reader header{file};
    require(header.take(7) == Bytes({'N','F','S','C','A','P','1'}), "Not a capture file");
    uint32_t version = header.word();
    require(version == 1 || version == schema, "Unsupported capture schema");
    uint32_t size = header.word(), checksum = header.word();
    require(size == file.size() - header.offset, "Invalid capture size");
    Bytes payload = header.take(size);
    require(crc(payload) == checksum, "Capture checksum mismatch");
    Reader reader{payload};
    Capture capture;
    auto build = reader.take(64), game = reader.take(64);
    capture.build.assign(build.begin(), build.end()); capture.game.assign(game.begin(), game.end());
    capture.careerRollback = reader.word(); capture.omitted = reader.word(); capture.pageSize = reader.word();
    uint32_t threads = reader.word();
    require(threads > 0 && threads <= maxThreads, "Invalid capture thread count");
    for (uint32_t i = 0; i < threads; ++i) {
        ThreadImage thread;
        thread.id = reader.word();
        uint32_t flags = reader.word();
        thread.capturer = flags & 1u; thread.outsideGuest = flags & 2u; thread.resumable = flags & 4u;
        thread.wait = WaitKind(reader.word()); thread.waitHandle = reader.word(); thread.blockedAt = reader.word();
        thread.cpu = takeCpu(reader);
        uint32_t frames = reader.word();
        require(frames <= maxFrames, "Too many capture frames");
        for (uint32_t n = 0; n < frames; ++n) {
            Frame frame;
            frame.function = reader.word(); frame.site = reader.word(); frame.target = reader.word();
            frame.fpuCompare = int32_t(reader.word()); frame.direction = int32_t(reader.word());
            thread.frames.push_back(frame);
        }
        capture.threads.push_back(std::move(thread));
    }
    uint32_t events = reader.word(); require(events <= maxObjects, "Too many kernel objects");
    for (uint32_t i = 0; i < events; ++i) {
        EventImage item; item.id = reader.word(); uint32_t flags = reader.word(); item.manual = flags & 1u; item.signaled = flags & 2u; item.name = reader.name();
        capture.events.push_back(std::move(item));
    }
    uint32_t mutexes = reader.word(); require(mutexes <= maxObjects, "Too many kernel objects");
    for (uint32_t i = 0; i < mutexes; ++i) {
        MutexImage item; item.id = reader.word(); item.owner = reader.word(); item.depth = reader.word(); item.abandoned = reader.word() != 0; item.name = reader.name();
        capture.mutexes.push_back(std::move(item));
    }
    uint32_t timers = reader.word(); require(timers <= maxObjects, "Too many kernel objects");
    for (uint32_t i = 0; i < timers; ++i) {
        TimerImage timer;
        timer.id = reader.word(); timer.delay = reader.word(); timer.callback = reader.word(); timer.parameter = reader.word();
        uint32_t flags = reader.word(); timer.periodic = flags & 1u; timer.cancelled = flags & 2u;
        timer.dueMs = int64_t(reader.qword());
        capture.timers.push_back(timer);
    }
    uint32_t regions = reader.word(); require(regions <= maxRegions, "Too many memory regions");
    for (uint32_t i = 0; i < regions; ++i) {
        Region region; region.address = reader.word(); uint32_t count = reader.word();
        require(count <= maxBytes, "Region too large");
        region.bytes = reader.take(count);
        capture.regions.push_back(std::move(region));
    }
    uint32_t protections = reader.word(); require(protections <= maxRegions, "Too many page protections");
    for (uint32_t i = 0; i < protections; ++i) { Protect page; page.address = reader.word(); page.flags = reader.word(); capture.protections.push_back(page); }
    uint32_t inventory = reader.word(); require(inventory <= 32, "Too many inventory rows");
    for (uint32_t i = 0; i < inventory; ++i) { Inventory item; item.kind = reader.word(); item.count = reader.word(); capture.inventory.push_back(item); }
    if (version >= 2) {
        uint32_t files = reader.word(); require(files <= maxObjects, "Too many open files");
        for (uint32_t i = 0; i < files; ++i) {
            FileImage item; item.handle = reader.word(); item.access = reader.word(); item.offset = reader.qword(); item.career = reader.word() != 0; item.path = reader.name();
            capture.files.push_back(std::move(item));
        }
        uint32_t sounds = reader.word(); require(sounds <= maxObjects, "Too many sounds");
        for (uint32_t i = 0; i < sounds; ++i) {
            SoundImage s;
            for (uint32_t* v : {&s.address, &s.kind, &s.table, &s.references, &s.flags, &s.guestSamples, &s.volume, &s.pan, &s.lockOffset, &s.lockBytes,
                                &s.sampleBytes, &s.channels, &s.bits, &s.rate, &s.frequency, &s.publishes}) *v = reader.word();
            uint32_t flags = reader.word(); s.locked = flags & 1u; s.voice = flags & 2u; s.playing = flags & 4u; s.loop = flags & 8u;
            uint64_t bits = reader.qword(); std::memcpy(&s.position, &bits, 8); bits = reader.qword(); std::memcpy(&s.staleFrames, &bits, 8);
            uint32_t level = reader.word(); std::memcpy(&s.left, &level, 4); level = reader.word(); std::memcpy(&s.right, &level, 4);
            capture.sounds.push_back(s);
        }
        uint32_t graphics = reader.word(); require(graphics <= (1u << 20) && size_t(graphics) * 8 <= payload.size() - reader.offset, "Too many graphics objects");
        for (uint32_t i = 0; i < graphics; ++i) { uint32_t address = reader.word(); capture.graphics.emplace_back(address, reader.word()); }
        uint32_t sections = reader.word(); require(sections <= maxObjects * 4, "Too many critical sections");
        for (uint32_t i = 0; i < sections; ++i) capture.criticalSections.push_back(reader.word());
        capture.heapNext = reader.word();
        uint32_t heaps = reader.word(); require(heaps <= maxObjects, "Too many heaps");
        for (uint32_t i = 0; i < heaps; ++i) {
            HeapImage heap; heap.id = reader.word(); heap.options = reader.word(); heap.maximum = reader.word();
            uint32_t count = reader.word(); require(count <= (1u << 22) && size_t(count) * 12 <= payload.size() - reader.offset, "Too many heap blocks");
            for (uint32_t n = 0; n < count; ++n) { HeapBlock block; block.address = reader.word(); block.capacity = reader.word(); block.requested = reader.word(); heap.blocks.push_back(block); }
            capture.heaps.push_back(std::move(heap));
        }
    }
    require(reader.offset == payload.size(), "Unexpected trailing capture");
    validate(capture);
    return capture;
}

inline void writeAtomic(const std::filesystem::path& path, const Bytes& bytes) {
    std::filesystem::create_directories(path.parent_path());
    std::string pattern = path.string() + ".partial-XXXXXX";
    std::vector<char> name(pattern.begin(), pattern.end()); name.push_back(0);
    int fd = mkstemp(name.data()); require(fd >= 0, "Cannot create capture staging file");
    try {
        size_t written = 0;
        while (written < bytes.size()) {
            ssize_t step = ::write(fd, bytes.data() + written, bytes.size() - written);
            require(step > 0, "Cannot write capture");
            written += size_t(step);
        }
        require(fsync(fd) == 0, "Cannot sync capture");
        int closed = close(fd); fd = -1; require(closed == 0, "Cannot close capture");
        require(rename(name.data(), path.c_str()) == 0, "Cannot publish capture");
        auto parent = path.parent_path(); if (parent.empty()) parent = ".";
        int directory = open(parent.c_str(), O_RDONLY | O_DIRECTORY);
        require(directory >= 0, "Cannot open capture directory");
        int synced = fsync(directory); close(directory); require(synced == 0, "Cannot sync capture directory");
    } catch (...) { if (fd >= 0) close(fd); unlink(name.data()); throw; }
}

// Compact SHA-256. The game identity stored in a capture is the executable digest.
inline std::string sha256Hex(const uint8_t* data, size_t size) {
    uint32_t state[8] = {0x6a09e667u,0xbb67ae85u,0x3c6ef372u,0xa54ff53au,0x510e527fu,0x9b05688cu,0x1f83d9abu,0x5be0cd19u};
    static const uint32_t round[64] = {
        0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
        0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
        0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
        0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
        0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
        0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
        0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4au,0x5b9cca4fu,0x682e6ff3u,
        0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u};
    auto rot = [](uint32_t value, unsigned count) { return (value >> count) | (value << (32 - count)); };
    Bytes message(data, data + size);
    uint64_t bits = uint64_t(size) * 8;
    message.push_back(0x80);
    while ((message.size() % 64) != 56) message.push_back(0);
    for (int shift = 56; shift >= 0; shift -= 8) message.push_back(uint8_t(bits >> shift));
    for (size_t offset = 0; offset < message.size(); offset += 64) {
        uint32_t word[64];
        for (int i = 0; i < 16; ++i) {
            const uint8_t* p = message.data() + offset + i * 4;
            word[i] = (uint32_t(p[0]) << 24) | (uint32_t(p[1]) << 16) | (uint32_t(p[2]) << 8) | p[3];
        }
        for (int i = 16; i < 64; ++i) {
            uint32_t s0 = rot(word[i - 15], 7) ^ rot(word[i - 15], 18) ^ (word[i - 15] >> 3);
            uint32_t s1 = rot(word[i - 2], 17) ^ rot(word[i - 2], 19) ^ (word[i - 2] >> 10);
            word[i] = word[i - 16] + s0 + word[i - 7] + s1;
        }
        uint32_t a = state[0], b = state[1], c = state[2], d = state[3], e = state[4], f = state[5], g = state[6], h = state[7];
        for (int i = 0; i < 64; ++i) {
            uint32_t s1 = rot(e, 6) ^ rot(e, 11) ^ rot(e, 25);
            uint32_t ch = (e & f) ^ (~e & g);
            uint32_t temp1 = h + s1 + ch + round[i] + word[i];
            uint32_t s0 = rot(a, 2) ^ rot(a, 13) ^ rot(a, 22);
            uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            uint32_t temp2 = s0 + maj;
            h = g; g = f; f = e; e = d + temp1; d = c; c = b; b = a; a = temp1 + temp2;
        }
        state[0] += a; state[1] += b; state[2] += c; state[3] += d; state[4] += e; state[5] += f; state[6] += g; state[7] += h;
    }
    static const char hex[] = "0123456789abcdef";
    std::string out(64, '0');
    for (int i = 0; i < 8; ++i) for (int shift = 0; shift < 4; ++shift) {
        uint8_t byte = uint8_t(state[i] >> (24 - shift * 8));
        out[i * 8 + shift * 2] = hex[byte >> 4]; out[i * 8 + shift * 2 + 1] = hex[byte & 15];
    }
    return out;
}
inline std::string sha256File(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    require(bool(input), "Cannot hash capture identity");
    Bytes bytes((std::istreambuf_iterator<char>(input)), {});
    require(bool(input) || input.eof(), "Cannot read capture identity");
    return sha256Hex(bytes.data(), bytes.size());
}
}