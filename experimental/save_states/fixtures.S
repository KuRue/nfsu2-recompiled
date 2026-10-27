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
/* Instruction groups found live at capture points on the phone (stack probe, 2026-10-27). */
begin root_narrow
 mov dword ptr[0x480200],0x80ff7f01
 movzx eax,byte ptr[0x480201]
 movsx ecx,byte ptr[0x480203]
 movzx edx,word ptr[0x480202]
 movsx ebx,word ptr[0x480202]
 mov byte ptr[0x480204],al
 mov word ptr[0x480206],cx
 add al,cl
 mov ah,dl
 movsx esi,ah
 cmp eax,0x1000
 sete dl
 setne dh
 setl cl
 seta ch
 mov [0x480208],edx
 mov [0x48020c],ecx
 xor eax,eax
 ret
end root_narrow
begin root_shift
 push ebp
 mov ebp,esp
 sub esp,8
 mov eax,0x81234567
 mov ecx,5
 shl eax,3
 mov [ebp-4],eax
 sar eax,cl
 mov edx,eax
 shr edx,7
 rol edx,4
 ror eax,cl
 mov [ebp-8],edx
 mov [0x480210],eax
 mov ecx,[ebp-4]
 mov [0x480214],ecx
 mov eax,[ebp-8]
 leave
 test eax,eax
 ret
end root_shift
begin root_divide
 mov eax,-1000
 cdq
 mov ecx,7
 idiv ecx
 mov [0x480220],eax
 mov [0x480224],edx
 mov eax,1000001
 xor edx,edx
 div ecx
 mul ecx
 mov [0x480228],eax
 mov [0x48022c],edx
 xchg eax,edx
 xor ecx,ecx
 ret
end root_divide
begin root_string
 mov edi,0x480300
 mov eax,0x11223344
 mov ecx,8
 rep stosd
 mov esi,0x480300
 mov edi,0x480340
 mov ecx,13
 rep movsb
 mov al,0x5a
 stosb
 movsd
 mov ecx,0
 rep stosb
 xor eax,eax
 ret
end root_string
begin root_x87
 fld qword ptr[data_double]
 fadd qword ptr[data_double_other]
 fmul st(0),st(0)
 fld dword ptr[data_vec+4]
 fsub st(0),st(1)
 fdivr qword ptr[data_double]
 fst qword ptr[0x480400]
 fabs
 fcomp qword ptr[data_double]
 fnstsw ax
 mov [0x480408],eax
 fimul dword ptr[data_mmx]
 fdiv qword ptr[data_double_other]
 fistp dword ptr[0x48040c]
 xor eax,eax
 ret
end root_x87
