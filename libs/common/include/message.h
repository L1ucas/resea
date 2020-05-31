#ifndef __MESSAGE_H__
#define __MESSAGE_H__

//
// TODO: Migrate into *.idl files.
//
enum abi_hook_type {
    ABI_HOOK_INITIAL = 1,
    ABI_HOOK_SYSCALL = 2,
};
/// Lower 7 bits are ASCII code and upper bits are modifier keys combination
/// such as Ctrl and Alt.
typedef uint16_t keycode_t;
#define KEY_MOD_CTRL (1 << 8)
#define KEY_MOD_ALT  (1 << 9)
#define ID(x)  (200 + x)
/// Screen text color codes. See https://wiki.osdev.org/Printing_To_Screen
typedef enum {
    COLOR_BLACK = 0,
    COLOR_BLUE = 9,
    COLOR_GREEN = 10,
    COLOR_CYAN = 11,
    COLOR_RED = 12,
    COLOR_MAGENTA = 13,
    COLOR_YELLOW = 14,
    COLOR_WHITE = 15,
    COLOR_NORMAL = 15,
} color_t;

#include <message_fields.h> /* generated by genidl.py */
#include <types.h>
#include <config.h>

#ifdef __LP64__
#define MESSAGE_SIZE 256
#else
#define MESSAGE_SIZE 32
#endif

/// Message.
struct message {
    int type;
    task_t src;
    union {
        uint8_t raw[MESSAGE_SIZE - sizeof(int) - sizeof(task_t)];
        struct {
            void *bulk_ptr;
            size_t bulk_len;
        };

        IDL_MESSAGE_FIELDS /* defined in message_fields.h */

        //
        //  TODO: Migrate into *.idl files.
        //
#ifdef CONFIG_ABI_EMU
        #define ABI_HOOK_MSG ID(5)
        struct {
            task_t task;
            enum abi_hook_type type;
            struct abi_emu_frame frame;
        } abi_hook;

        #define ABI_HOOK_REPLY_MSG ID(6)
        struct {
            struct abi_emu_frame frame;
        } abi_hook_reply;
#endif
   };
};

STATIC_ASSERT(sizeof(struct message) == MESSAGE_SIZE);
IDL_STATIC_ASSERTS /* defined in message_fields.h */

#endif
