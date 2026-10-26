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
