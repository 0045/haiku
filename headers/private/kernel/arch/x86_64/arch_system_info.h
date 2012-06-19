/*
 * Copyright 2004-2005, Axel Dörfler, axeld@pinc-software.de. All rights reserved.
 * Copyright 2012, Alex Smith, alex@alex-smith.me.uk.
 * Distributed under the terms of the MIT License.
 */
#ifndef _KERNEL_ARCH_X86_64_SYSTEM_INFO_H
#define _KERNEL_ARCH_X86_64_SYSTEM_INFO_H


#include <OS.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _BOOT_MODE
status_t get_current_cpuid(cpuid_info *info, uint32 eax);
uint32 get_eflags(void);
void set_eflags(uint32 value);
#endif

//status_t _user_get_cpuid(cpuid_info *info, uint32 eax, uint32 cpu);

#ifdef __cplusplus
}
#endif

#endif	/* _KRENEL_ARCH_X86_64_SYSTEM_INFO_H */
