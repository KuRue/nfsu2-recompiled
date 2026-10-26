/* Original cooperative-thread fixtures, no game code. */
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
begin waiter
 fld1
 mov eax,1
 mov ecx,0xffffffff
 call import_wait
 mov [edi],eax
 fstp qword ptr[edi+8]
 inc dword ptr[0x480100]
 ret
end waiter
begin signaler
 fld qword ptr[data_double]
 mov eax,3
 call import_sleep
 mov eax,1
 call import_signal
 fstp qword ptr[edi+8]
 ret
end signaler
begin timeout_waiter
 mov eax,1
 mov ecx,3
 call import_wait
 mov [edi],eax
 ret
end timeout_waiter
begin sleeper
 mov eax,4
 call import_sleep
 mov eax,42
 mov [edi],eax
 ret
end sleeper
begin twice_waiter
 mov eax,1
 mov ecx,0xffffffff
 call import_wait
 inc dword ptr[0x480100]
 mov eax,1
 mov ecx,0xffffffff
 call import_wait
 inc dword ptr[0x480100]
 mov [edi],eax
 ret
end twice_waiter
begin import_wait
 ud2
 ret
end import_wait
begin import_signal
 ud2
 ret
end import_signal
begin import_sleep
 ud2
 ret
end import_sleep
begin locker
 mov eax,2
 mov ecx,0xffffffff
 call import_acquire
 mov ebx,eax
 mov eax,2
 mov ecx,0xffffffff
 call import_acquire
 mov esi,[0x480100]
 mov eax,2
 call import_sleep
 inc esi
 mov [0x480100],esi
 mov eax,2
 call import_release
 mov eax,2
 call import_release
 imul eax,eax,16
 add ebx,eax
 mov eax,2
 call import_release
 imul eax,eax,256
 add ebx,eax
 mov [edi],ebx
 ret
end locker
begin abandoner
 mov eax,2
 mov ecx,0xffffffff
 call import_acquire
 mov [edi],eax
 ret
end abandoner
begin abandon_waiter
 mov eax,1
 call import_sleep
 mov eax,2
 mov ecx,0xffffffff
 call import_acquire
 mov [edi],eax
 mov eax,2
 call import_release
 ret
end abandon_waiter
begin long_holder
 mov eax,2
 mov ecx,0xffffffff
 call import_acquire
 mov [edi],eax
 mov eax,10
 call import_sleep
 mov eax,2
 call import_release
 ret
end long_holder
begin mutex_timeout_waiter
 mov eax,1
 call import_sleep
 mov eax,2
 xor ecx,ecx
 call import_acquire
 mov ebx,eax
 mov eax,2
 mov ecx,3
 call import_acquire
 add ebx,eax
 mov eax,2
 mov ecx,0xffffffff
 call import_acquire
 add ebx,eax
 mov [edi],ebx
 mov eax,2
 call import_release
 ret
end mutex_timeout_waiter
begin import_acquire
 ud2
 ret
end import_acquire
begin import_release
 ud2
 ret
end import_release
data_double: .double 2.25
