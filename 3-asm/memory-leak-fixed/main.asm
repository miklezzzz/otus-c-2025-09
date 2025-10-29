    bits 64
    extern malloc, puts, printf, fflush, abort, free
    global main

    section   .data
empty_str: db 0x0
int_format: db "%ld ", 0x0
data: dq 4, 8, 15, 16, 23, 42
data_length: equ ($-data) / 8

    section   .text
;;; print_int proc
print_int:
    push rbp
    mov rbp, rsp
    sub rsp, 16

    mov rsi, rdi
    mov rdi, int_format
    xor rax, rax
    call printf

    xor rdi, rdi
    call fflush

    mov rsp, rbp
    pop rbp
    ret

;;; p proc
p:
    mov rax, rdi
    and rax, 1
    ret

;;; add_element proc
add_element:
    push rbp
    push rbx
    push r14
    mov rbp, rsp
    sub rsp, 16

    mov r14, rdi
    mov rbx, rsi

    mov rdi, 16
    call malloc
    test rax, rax
    jz abort

    mov [rax], r14
    mov [rax + 8], rbx

    mov rsp, rbp
    pop r14
    pop rbx
    pop rbp
    ret

;;; m proc
m:
    push rbp
    mov rbp, rsp
    sub rsp, 16

    push rbp
    push rbx

beginning:
    test rdi, rdi
    jz outm


    mov rbx, rdi
    mov rbp, rsi

    mov rdi, [rdi]
    call rsi

    mov rdi, [rbx + 8]
    mov rsi, rbp
    jmp beginning

outm:
    pop rbx
    pop rbp

    mov rsp, rbp
    pop rbp
    ret

;;; f proc
f:
    push r12
    push r13
    push r14
    push rbx

f_loop:
    test rdi, rdi
    jz outf

    mov rbx, rdi
    mov r12, rsi
    mov r13, rdx
    mov r14, [rbx + 8]

    mov rdi, [rdi]
    call r13
    test rax, rax
    jz next

    mov rdi, [rbx]
    mov rsi, r12
    call add_element
    mov rsi, rax
    jmp ff

next:
    mov rsi, r12

ff:
    push rsi
    mov rdi, rbx
    call free
    pop rsi

    mov rdi, r14
    mov rdx, r13

    jmp f_loop

outf:
    pop rbx
    pop r14
    pop r13
    pop r12

    mov rax, rsi
    ret

;;; free_element proc
free_element:
    push r14

free_loop:
    test rdi, rdi
    jz end_free

    mov r14, [rdi + 8]

    call free

    mov rdi, r14
    jmp free_loop

end_free:
    pop r14
    ret

;;; main proc

main:
    mov rbp, rsp; for correct debugging
    push rbx

    xor rax, rax
    mov rbx, data_length

adding_loop:
    mov rdi, [data - 8 + rbx * 8]
    mov rsi, rax
    call add_element
    dec rbx
    jnz adding_loop

    mov rbx, rax
    mov rdi, rax
    mov rsi, print_int
    call m

    mov rdi, empty_str
    call puts

    mov rdx, p
    xor rsi, rsi
    mov rdi, rbx
    call f

    mov rbx, rax

    mov rdi, rbx
    mov rsi, print_int
    call m

    mov rdi, empty_str
    call puts

    mov rdi, rbx
    call free_element

    pop rbx

    xor rax, rax
    ret
