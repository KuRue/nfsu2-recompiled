/* Original synthetic assembly, no game instructions/data. */
.intel_syntax noprefix
.text
.macro begin name
.global \name
.type \name,@function
\name:
.endm
.macro end name
.size \name,.-\name
.endm
begin root_loop
 xor eax,eax
 mov ecx,6
again:
 push ecx
 call leaf
 pop ecx
 dec ecx
 jnz again
 ret
end root_loop
begin leaf
 push ebx
 mov ebx,ecx
 add eax,ebx
 call inner
 pop ebx
 ret
end leaf
begin inner
 add eax,ecx
 ret
end inner
begin root_recursive
 xor eax,eax
 mov ecx,7
 call recursive
 ret
end root_recursive
begin recursive
 cmp ecx,0
 je done
 push ecx
 dec ecx
 call recursive
 pop ecx
 add eax,ecx
done:
 ret
end recursive
begin root_indirect
 mov edx,offset leaf
 mov ecx,11
 xor eax,eax
 call edx
 ret
end root_indirect
begin root_tail
 xor eax,eax
 mov ecx,9
 call tail_one
 ret
end root_tail
begin tail_one
 jmp leaf
end tail_one
begin root_flags
 mov eax,0xffffffff
 add eax,1
 call preserve_flags
 adc eax,5
 cmp eax,6
 jne bad
 mov ebx,0x12345678
 ret
bad:
 mov ebx,0xdeadbeef
 ret
end root_flags
begin preserve_flags
 mov edx,0x87654321
 ret
end preserve_flags
begin root_cleanup
 push 17
 call stdcall_leaf
 ret
end root_cleanup
begin stdcall_leaf
 mov eax,[esp+4]
 add eax,3
 ret 4
end stdcall_leaf
begin root_signed
 mov eax,0x80000000
 cmp eax,1
 jl signed_ok
 mov ebx,0xdeadbeef
 ret
signed_ok:
 mov ebx,0x12345678
 ret
end root_signed
begin root_sse
 movups xmm0,[data_vec]
 call sse_leaf
 movups [0x480000],xmm0
 mulps xmm0,[data_other]
 movups [0x480010],xmm0
 ret
end root_sse
begin sse_leaf
 addps xmm0,[data_other]
 ret
end sse_leaf
begin root_mmx
 movq mm0,[data_mmx]
 paddd mm0,[data_mmx_other]
 call mmx_leaf
 movq [0x480020],mm0
 ret
end root_mmx
begin mmx_leaf
 paddd mm0,[data_mmx_other]
 ret
end mmx_leaf
begin root_fpu
 fld qword ptr[data_double]
 call fpu_leaf
 fstp qword ptr[0x480030]
 ret
end root_fpu
begin fpu_leaf
 fld qword ptr[data_double_other]
 faddp
 ret
end fpu_leaf
begin root_controls
 fldcw word ptr[data_control]
 fnstcw word ptr[0x480040]
 mov dword ptr[0x480080],0x3f80
 ldmxcsr [0x480080]
 stmxcsr [0x480084]
 ret
end root_controls
begin root_host
 mov eax,7
 mov ecx,11
 call import_math
 add eax,1
 call import_math
 ret
end root_host
begin import_math
 ud2
 ret
end import_math
.p2align 4
data_vec: .float 1,2,3,4
data_other: .float 2,3,4,5
data_mmx: .long 10,20
data_mmx_other: .long 3,4
data_double: .double 1.5
data_double_other: .double 2.25
data_control: .word 0xf7f
begin root_fpu_compare
 fld1
 ftst
 call preserve_flags
 fnstsw ax
 mov [0x480050],eax
 fstp qword ptr[0x480058]
 xor eax,eax
 ret
end root_fpu_compare
begin root_fpu_integer
 fild qword ptr[data_i64]
 call preserve_flags
 fistp qword ptr[0x480060]
 ret
end root_fpu_integer
data_i64: .quad 9007199254740993
