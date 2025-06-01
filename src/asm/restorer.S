# restorer.S
.text
.global sigaction_restorer
sigaction_restorer:
    movq $15, %rax  # Syscall number for sigreturn (15 on x86-64)
    syscall
