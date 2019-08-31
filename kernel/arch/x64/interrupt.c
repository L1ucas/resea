#include <printk.h>
#include <timer.h>
#include <x64/apic.h>
#include <x64/interrupt.h>
#include <x64/x64.h>

static void print_regs(struct interrupt_regs *regs) {
    TRACE("RIP = %p    CS  = %p    RFL = %p", regs->rip, regs->cs, regs->rflags);
    TRACE("SS  = %p    RSP = %p    RBP = %p", regs->ss, regs->rsp, regs->rbp);
    TRACE("RAX = %p    RBX = %p    RCX = %p", regs->rax, regs->rbx, regs->rcx);
    TRACE("RDX = %p    RSI = %p    RDI = %p", regs->rdx, regs->rsi, regs->rdi);
    TRACE("R8  = %p    R9  = %p    R10 = %p", regs->r8, regs->r9, regs->r10);
    TRACE("R11 = %p    R12 = %p    R13 = %p", regs->r11, regs->r12, regs->r13);
    TRACE("R14 = %p    R15 = %p", regs->r14, regs->r15);
}

void x64_handle_interrupt(uint8_t vec, struct interrupt_regs *regs) {
    switch (vec) {
    case EXP_PAGE_FAULT: {
        vaddr_t addr = asm_read_cr2();
        TRACE("#PF: addr=%p, error=%d", addr, regs->error);
        print_regs(regs);

        if ((regs->error >> 3) & 1) {
            PANIC("#PF: RSVD bit violation");
        }

        page_fault_handler(addr, regs->error);
        break;
    }
    case APIC_TIMER_VECTOR:
        timer_interrupt_handler();
        x64_ack_interrupt();
        break;
    default:
        if (vec <= 20) {
            WARN("Exception #%d", vec);
            print_regs(regs);
            PANIC("Unhandled exception #%d", vec);
        } else {
            TRACE("Interrupt #%d", vec);
        }
    }
}