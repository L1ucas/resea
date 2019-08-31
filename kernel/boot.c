#include <arch.h>
#include <debug.h>
#include <ipc.h>
#include <memory.h>
#include <printk.h>
#include <process.h>
#include <thread.h>

extern char __initfs[];

static paddr_t initfs_pager(UNUSED struct vmarea *vma, vaddr_t vaddr) {
    ASSERT(((vaddr_t) &__initfs & (PAGE_SIZE - 1)) == 0 &&
           "initfs is not aligned");
    return into_paddr(__initfs + (vaddr - INITFS_ADDR));
}

static paddr_t straight_map_pager(UNUSED struct vmarea *vma, vaddr_t vaddr) {
    return vaddr;
}

// Spawns the first user process from the initfs.
static void userland(void) {
    // Create the very first user process and thread.
    struct process *user_process = process_create("memmgr");
    if (!user_process) {
        PANIC("failed to create a process");
    }

    struct thread *thread = thread_create(user_process, INITFS_ADDR,
        0 /* stack */, THREAD_INFO_ADDR, 0 /* arg */);
    if (!thread) {
        PANIC("failed to create a user thread");
    }

    // Create a channel connection between the kernel server and the user
    // process.
    struct channel *kernel_ch = channel_create(kernel_process);
    if (!kernel_ch) {
        PANIC("failed to create a channel");
    }

    struct channel *user_ch = channel_create(user_process);
    if (!user_ch) {
        PANIC("failed to create a channel");
    }

    channel_link(kernel_ch, user_ch);

    // Set up pagers.
    int flags = PAGE_WRITABLE | PAGE_USER;
    if (vmarea_add(user_process, INITFS_ADDR, INITFS_END, initfs_pager, NULL,
                   flags) != OK) {
        PANIC("failed to add a vmarea");
    }

    if (vmarea_add(user_process, STRAIGHT_MAP_ADDR, STRAIGHT_MAP_END,
                   straight_map_pager, NULL, flags) != OK) {
        PANIC("failed to add a vmarea");
    }

    // Enqueue the thread into the run queue.
    thread_resume(thread);
}

static void idle(void) {
    while (0) {
        arch_idle();
    }
}

void boot(void) {
    init_boot_stack_canary();

    INFO("Booting Resea...");
    debug_init();
    memory_init();
    arch_init();
    process_init();
    thread_init();

    userland();

    // Perform the very first context switch. The current context will become a
    // CPU-local idle thread.
    thread_switch();

    // Now we're in the CPU-local idle thread context.
    idle();
}