.section .note.GNU-stack,"",@progbits
.intel_syntax noprefix


.text
.global mySocketCall
mySocketCall:
    mov rax, 41      # SYS_socket
    syscall
    ret
