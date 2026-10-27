#pragma once
// Force-included only into locally generated C; Windows generation is untouched.
#include <time.h>
#define RECOMP_GENERATED_CODE
#include "recomp_types.h"
// The PC's six-resolution selector cannot describe Android display sizes.
// Intercept its entry for direct calls as well as dispatch calls; no x86 patch.
extern int nfs_android_resolution(void);
#undef RECOMP_ENTER
#define RECOMP_ENTER(va) do { \
    g_cur_func=(va); RECOMP_FLAGS_IN(); \
    if((va)==0x005BF610u){ \
        RECOMP_REGS_OUT(); \
        if(nfs_android_resolution()){RECOMP_REGS_IN();return;} \
        RECOMP_REGS_IN(); \
    } \
} while(0)
static inline uint64_t nfs_guest_rdtsc(void) {
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    // A stable virtual 1 GHz counter, not an ARM hardware cycle counter.
    return (uint64_t)t.tv_sec * 1000000000ull + (uint64_t)t.tv_nsec;
}
#define __rdtsc() nfs_guest_rdtsc()
static inline void nfs_guest_cpuid(uint32_t leaf) {
    g_eax=g_ebx=g_ecx=g_edx=0;
    if (leaf==0) {
        g_eax=1; g_ebx=0x756e6547; g_edx=0x49656e69; g_ecx=0x6c65746e;
    } else if (leaf==1) {
        g_eax=0x00000663;
        // x87, TSC, CX8, CMOV, MMX, SSE, SSE2; no SSE3/AVX/3DNow.
        g_edx=((1u<<0)|(1u<<4)|(1u<<8)|(1u<<15)|(1u<<23)|(1u<<25)|(1u<<26)) & g_cpuid_edx1;
    } else if (leaf==0x80000000u) g_eax=0x80000001u;
}
#undef CPUID
#define CPUID(a,b,c,d) do { RECOMP_REGS_OUT(); nfs_guest_cpuid((a)); RECOMP_REGS_IN(); } while (0)

#ifdef NFS_STACK_PROBE
// Development-only: a per-thread shadow of lifted callers, so capture-point probes in the
// host can see which lifted frames are live (those would need continuation form).
extern _Thread_local uint32_t nfs_shadow[512];
extern _Thread_local uint32_t nfs_shadow_depth;
#define NFS_SHADOW_PUSH(v) do{uint32_t _d=nfs_shadow_depth++;if(_d<512u)nfs_shadow[_d]=(v);}while(0)
#define NFS_SHADOW_POP() (--nfs_shadow_depth)
#undef RECOMP_CALL
#define RECOMP_CALL(func) do { \
    uint32_t _caller = g_cur_func; \
    PUSH32(esp, RECOMP_RETADDR); \
    RECOMP_FLAGS_OUT(); RECOMP_REGS_OUT(); \
    NFS_SHADOW_PUSH(_caller); func(); NFS_SHADOW_POP(); \
    RECOMP_REGS_IN(); g_cur_func = _caller; \
} while(0)
#undef RECOMP_ICALL
#define RECOMP_ICALL(target_va) do { \
    uint32_t _va = (uint32_t)(target_va); \
    g_icall_trace[g_icall_trace_idx & (ICALL_TRACE_SIZE-1)] = _va; \
    g_icall_from[g_icall_trace_idx & (ICALL_TRACE_SIZE-1)] = g_cur_func; \
    g_icall_trace_idx++; g_icall_count++; \
    recomp_func_t _fn = recomp_lookup_manual(_va); \
    if (!_fn) _fn = recomp_lookup(_va); \
    if (!_fn) _fn = recomp_lookup_import(_va); \
    if (_fn) { uint32_t _caller = g_cur_func; PUSH32(esp, RECOMP_RETADDR); \
        RECOMP_FLAGS_OUT(); RECOMP_REGS_OUT(); \
        NFS_SHADOW_PUSH(_caller); _fn(); NFS_SHADOW_POP(); \
        RECOMP_REGS_IN(); g_cur_func = _caller; } \
    else { RECOMP_MISS("ICALL", _va); eax = 0; } \
} while(0)
#undef RECOMP_ITAIL
// A tail jump still leaves the jumping function's native C frame live: mark it (bit 31 clear,
// VA tagged by the 0x80000000 flag) so the probe counts it.
#define RECOMP_ITAIL(target_va) do { \
    uint32_t _va = (uint32_t)(target_va); \
    g_icall_trace[g_icall_trace_idx & (ICALL_TRACE_SIZE-1)] = _va; \
    g_icall_from[g_icall_trace_idx & (ICALL_TRACE_SIZE-1)] = g_cur_func; \
    g_icall_trace_idx++; g_icall_count++; \
    recomp_func_t _fn = recomp_lookup_manual(_va); \
    if (!_fn) _fn = recomp_lookup(_va); \
    if (!_fn) _fn = recomp_lookup_import(_va); \
    if (_fn) { RECOMP_FLAGS_OUT(); RECOMP_REGS_OUT(); \
        NFS_SHADOW_PUSH(g_cur_func|0x80000000u); _fn(); NFS_SHADOW_POP(); RECOMP_REGS_IN(); } \
    else if (_va == RECOMP_RETADDR) { } \
    else { RECOMP_MISS("ITAIL", _va); } \
} while(0)
#endif
