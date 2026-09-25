.intel_syntax noprefix
.globl _cave
_cave:
    pushfd
    pushad
    mov ebx, 0x11223344          # log base (patched)
    mov edi, [ebx]
    cmp edi, 16000
    jae 1f
    mov eax, edi
    shl eax, 8
    lea edi, [ebx+eax+16]
    mov eax, [0x7749C8]
    mov [edi], eax
    mov eax, [0x55DEC4]
    mov [edi+4], eax
    mov eax, [0x55DEC8]
    mov [edi+8], eax
    mov eax, [0x54EB70]
    mov [edi+12], eax
    mov esi, [esp+0x390]         # instance (a2)
    push edi
    add edi, 16
    mov ecx, 11
    rep movsd
    pop edi
    mov esi, [esp+0x28]          # vertex array (first stack arg of the hooked call)
    push edi
    add edi, 60
    mov ecx, 38
    rep movsd
    pop edi
    mov eax, [esp+0x2C]          # second arg (priority)
    mov [edi+212], eax
    inc dword ptr [ebx]
1:
    popad
    popfd
    push 0x416110
    ret
