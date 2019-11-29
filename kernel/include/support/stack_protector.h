#ifndef __STACK_PROTECTOR_H__
#define __STACK_PROTECTOR_H__

#include <types.h>

void check_stack_canary(void);
void init_stack_canary(vaddr_t stack_bottom);
void init_boot_stack_canary(void);

#endif
