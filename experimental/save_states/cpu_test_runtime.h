#pragma once
#include "snapshot.h"
#include <guest_memory.h>
#include <iostream>
#define RECOMP_GENERATED_CODE
#include <recomp_types.h>
// x86 guests permit unaligned data. Use byte-aligned alias types in this host
// harness, just as the pinned runtime already does on ARM hosts.
typedef uint16_t __attribute__((aligned(1),may_alias)) proto_u16;
typedef uint32_t __attribute__((aligned(1),may_alias)) proto_u32;
typedef uint64_t __attribute__((aligned(1),may_alias)) proto_u64;
typedef float __attribute__((aligned(1),may_alias)) proto_f32;
typedef double __attribute__((aligned(1),may_alias)) proto_f64;
#undef MEM16
#undef MEM32
#undef MEM64
#undef MEMF
#undef MEMD
#define MEM16(a) (*(volatile proto_u16*)ADDR(a))
#define MEM32(a) (*(volatile proto_u32*)ADDR(a))
#define MEM64(a) (*(volatile proto_u64*)ADDR(a))
#define MEMF(a) (*(volatile proto_f32*)ADDR(a))
#define MEMD(a) (*(volatile proto_f64*)ADDR(a))
uint32_t g_eax=0,g_ecx=0,g_edx=0,g_ebx=0,g_esp=0,g_ebp=0,g_esi=0,g_edi=0;
double g_st[8]{};int g_fp_top=0;uint16_t g_fpu_cw=0x27f;
uint16_t g_seg_cs=0,g_seg_ds=0,g_seg_es=0,g_seg_fs=0,g_seg_gs=0;
uint32_t g_fs_base=0,g_gs_base=0,g_cur_func=0,g_mxcsr=0x1f80;
uint64_t g_mm[8]{};V128 g_xmm[8]{};ptrdiff_t g_mem_base=0;
