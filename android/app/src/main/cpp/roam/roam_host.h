/* roam_host.h -- the Android host for the portable free-roam game side.
 *
 * `roam_game.c` is shared with the Mac native port and the Windows ASI
 * companion; this header supplies the same names for the Android recomp:
 * guest memory, guest calls, a monotonic clock, logging and the HUD/audio
 * hooks.  The game side only ever calls into the runtime from the game
 * thread (the Present hook, for now).
 */
#ifndef ROAM_HOST_H
#define ROAM_HOST_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

/* The game side carries a CPU pointer as an opaque call context.  The Android
 * runtime keeps its guest registers in globals, so the pointer is unused. */
typedef struct CPU { uint32_t eax, ecx, edx, esp, ebx, esi, edi, ebp; } CPU;

/* The guest arena (memory->base()).  Set once when the machine is created. */
extern uint8_t *g_mem;

int      roam_committed(uint32_t address);
uint32_t roam_call(CPU *c, uint32_t function, uint32_t ecx, uint32_t edx,
                   int count, const uint32_t *args);
void     roam_log(const char *format, ...);
const char *roam_option(const char *name);
long     roam_frame_number(void);
unsigned roam_heap_mb(void);
uint64_t roam_now_ns(void);

void roam_hud_marker(float *x, float *y);

int  roam_horn_held(int stick);
int  roam_horn_mine(void);
const char *roam_horn_name(int kind);
void roam_horn_begin(const float *position, const float *forward, const float *velocity);
int  roam_horn_sound(uint32_t who, const char *name, int kind, const float *position, const float *velocity);
void roam_horn_end(void);

void roam_carsfx_begin(const float *position, const float *forward, const float *velocity);
void roam_carsfx_car(uint32_t who, const char *name, const float *position, const float *velocity,
                     const void *state);
void roam_carsfx_end(void);

int roam_no_profile(const char *name);
int roam_drift_tuning(uint8_t *out);

#define ROAM_HOST_SCRIPT 0
#define RH_FE_NODES      4096

#define GPTR(a)          (g_mem + (uint32_t)(a))
#define rh_committed(a)  roam_committed((uint32_t)(a))
#define rh_call(c, fn, ecx, edx, n, args) \
    roam_call((c), (fn), (ecx), (edx), (n), (const uint32_t *)(args))
#define rlog(...)        roam_log(__VA_ARGS__)
#define rh_opt(k)        roam_option(k)
#define rh_frame()       roam_frame_number()
#define rh_heap_mb()     roam_heap_mb()
#define rh_ns()          roam_now_ns()
#define rh_hud_marker(x, y) roam_hud_marker((x), (y))
#define rh_horn_held(s)  roam_horn_held(s)
#define rh_horn_mine()   roam_horn_mine()
#define rh_horn_name(k)  roam_horn_name(k)
#define rh_horn_begin(pos, fwd, vel) roam_horn_begin((pos), (fwd), (vel))
#define rh_horn_sound(who, name, kind, p, v) roam_horn_sound((who), (name), (kind), (p), (v))
#define rh_horn_end()    roam_horn_end()
#define rh_carsfx_begin(pos, fwd, vel) roam_carsfx_begin((pos), (fwd), (vel))
#define rh_carsfx_car(who, name, p, v, st) roam_carsfx_car((who), (name), (p), (v), (st))
#define rh_carsfx_end()  roam_carsfx_end()
#define rh_no_profile(n) roam_no_profile(n)
#define rh_drift_tuning(out) roam_drift_tuning(out)

typedef FILE rh_file;
#define rh_fopen(p)      fopen((p), "w")
#define rh_fprintf       fprintf
#define rh_fflush        fflush

/* Guest memory as lvalues (x86 little-endian, as the game stores them). */
#define MR8(a)   (*(uint8_t  *)(g_mem + (uint32_t)(a)))
#define MR16(a)  (*(uint16_t *)(g_mem + (uint32_t)(a)))
#define MR32(a)  (*(uint32_t *)(g_mem + (uint32_t)(a)))
#define MRF32(a) (*(float    *)(g_mem + (uint32_t)(a)))

#ifdef __cplusplus
}
#endif
#endif
