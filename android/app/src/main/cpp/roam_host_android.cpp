/* roam_host_android.cpp -- the Android host for the portable free-roam code.
 *
 * `roam_game.c` is shared with the Mac native port and the Windows ASI
 * companion.  It reaches the game only through the names in roam_host.h;
 * this file supplies them from the Android recomp: the guest arena, guest
 * calls with ECX/EDX, a monotonic clock, logging, and the HUD projection/audio hooks. The per-frame entry runs between game
 * update and render on the guest thread.
 */
#include <android/log.h>
#include <sys/system_properties.h>

#include <atomic>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "guest_runtime.h"
#include "nfs_widescreen.h"
#include "roam/roam_host.h"
#include "roam/roam_switch.h"

extern "C" void roam_game_frame(CPU*);
extern "C" void roam_game_hud(CPU*);
extern "C" void roam_game_report(void);

extern "C" uint8_t* g_mem = nullptr;

namespace {
std::atomic<bool> roamRequested{false};
std::atomic<int> roamProperty{-1};
long roamFrames = 0;

std::string propertyName(const char* option) {
    // NFSU2_ROAM_NAME -> debug.nfs.roam.name; the game side reads the same
    // names on the Mac, and a system property is easier to set over adb.
    std::string name = option ? option : "";
    const std::string prefix = "NFSU2_ROAM_";
    if (name.rfind(prefix, 0) == 0) name.erase(0, prefix.size());
    for (char& ch : name) {
        if (ch == '_') ch = '.';
        else if (ch >= 'A' && ch <= 'Z') ch = char(ch - 'A' + 'a');
    }
    return "debug.nfs.roam." + name;
}
}  // namespace

extern "C" int roam_committed(uint32_t address) {
    return guestMemoryReadable(address, 4) && guestMemoryWritable(address, 4) ? 1 : 0;
}

extern "C" uint32_t roam_call(CPU*, uint32_t function, uint32_t ecx, uint32_t edx,
                              int count, const uint32_t* args) {
    if (count < 0) return 0;
    return guestCallRoam(function, ecx, edx, args, size_t(count));
}

extern "C" void roam_log(const char* format, ...) {
    char line[1024];
    va_list args;
    va_start(args, format);
    vsnprintf(line, sizeof line, format, args);
    va_end(args);
    __android_log_write(ANDROID_LOG_INFO, "NFSU2Roam", line);
}

extern "C" const char* roam_option(const char* name) {
    static std::string value;
    if (!name) return nullptr;
    if (const char* environment = std::getenv(name)) return environment;
    char buffer[PROP_VALUE_MAX]{};
    const std::string property = propertyName(name);
    if (__system_property_get(property.c_str(), buffer) > 0) {
        value = buffer;
        return value.c_str();
    }
    return nullptr;
}

extern "C" long roam_frame_number(void) { return roamFrames; }
extern "C" unsigned roam_heap_mb(void) { return 0; }
extern "C" uint64_t roam_now_ns(void) { return guestMonotonicNs(); }

extern "C" void roam_hud_marker(float* x, float* y) {
    nfs_widescreen_marker(x,y);
}

// Remote-car audio is separate from nameplate/map rendering.
extern "C" int roam_horn_held(int) { return 0; }
extern "C" int roam_horn_mine(void) { return 0; }
extern "C" const char* roam_horn_name(int) { return ""; }
extern "C" void roam_horn_begin(const float*, const float*, const float*) {}
extern "C" int roam_horn_sound(uint32_t, const char*, int, const float*, const float*) { return 0; }
extern "C" void roam_horn_end(void) {}

extern "C" void roam_carsfx_begin(const float*, const float*, const float*) {}
extern "C" void roam_carsfx_car(uint32_t, const char*, const float*, const float*, const void*) {}
extern "C" void roam_carsfx_end(void) {}

extern "C" int roam_switch_active(void) { return guestSwitchActive() ? 1 : 0; }

extern "C" int roam_switch_send(uint32_t dst, uint16_t sport, uint16_t dport,
                                const uint8_t* data, size_t length) {
    return guestSwitchSendRaw(1, dst, sport, dport, data, length) ? 1 : 0;
}

extern "C" int roam_no_profile(const char*) { return 0; }
extern "C" int roam_drift_tuning(uint8_t*) { return 0; }

bool guestRoamEnabled() {
    if (roamRequested.load(std::memory_order_acquire)) return true;
    int enabled = roamProperty.load(std::memory_order_acquire);
    if (enabled < 0) {
        char value[PROP_VALUE_MAX]{};
        __system_property_get("debug.nfs.roam", value);
        enabled = std::atoi(value) != 0 ? 1 : 0;
        roamProperty.store(enabled, std::memory_order_release);
    }
    return enabled != 0;
}

void setGuestRoamEnabled(bool enabled) {
    roamRequested.store(enabled, std::memory_order_release);
    if (enabled && !g_mem) g_mem = guestMemoryBase();
}

extern "C" void roamGuestFrame() {
    if (!guestRoamEnabled()) return;
    if (!g_mem) g_mem = guestMemoryBase();
    if (!g_mem) return;
    CPU cpu{};
    roam_switch_pump();
    roam_game_frame(&cpu);
    roam_game_hud(&cpu);
    ++roamFrames;
}
