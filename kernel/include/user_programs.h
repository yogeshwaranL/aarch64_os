/*
 * AArch64 Bare Metal OS - User Programs Header
 *
 * Symbols for embedded user program binaries
 */

#ifndef _USER_PROGRAMS_H
#define _USER_PROGRAMS_H

#include "kernel.h"

/* Embedded user programs (linked as binary data) */
extern uint8_t _binary_user_hello_bin_start[];
extern uint8_t _binary_user_hello_bin_end[];
extern uint8_t _binary_user_hello_bin_size[];

extern uint8_t _binary_user_counter_bin_start[];
extern uint8_t _binary_user_counter_bin_end[];
extern uint8_t _binary_user_counter_bin_size[];

#endif /* _USER_PROGRAMS_H */
