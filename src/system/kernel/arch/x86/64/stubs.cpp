/*
 * Copyright 2012, Alex Smith, alex@alex-smith.me.uk.
 * Distributed under the terms of the MIT License.
 */


// This file contains stubs for everything that's not been implemented yet on
// x86_64.


#include <cpu.h>
#include <commpage.h>
#include <debug.h>
#include <debugger.h>
#include <elf.h>
#include <elf_priv.h>
#include <real_time_clock.h>
#include <real_time_data.h>
#include <smp.h>
#include <timer.h>
#include <team.h>
#include <thread.h>
#include <vm/vm.h>

#include <boot/kernel_args.h>

#include <arch/debug.h>
#include <arch/platform.h>
#include <arch/vm_translation_map.h>
#include <arch/vm.h>
#include <arch/user_debugger.h>
#include <arch/timer.h>
#include <arch/thread.h>
#include <arch/system_info.h>
#include <arch/smp.h>
#include <arch/real_time_clock.h>
#include <arch/elf.h>


// temporary
Thread* gCurrentThread = NULL;


status_t
arch_commpage_init(void)
{
	return B_OK;
}


status_t
arch_commpage_init_post_cpus(void)
{
	return B_OK;
}


ssize_t
arch_cpu_user_strlcpy(char* to, const char* from, size_t size,
	addr_t* faultHandler)
{
	return B_BAD_ADDRESS;
}


status_t
arch_cpu_user_memcpy(void* to, const void* from, size_t size,
	addr_t* faultHandler)
{
	return B_BAD_ADDRESS;
}


status_t
arch_cpu_user_memset(void* s, char c, size_t count, addr_t* faultHandler)
{
	return B_BAD_ADDRESS;
}


void
arch_debug_save_registers(struct arch_debug_registers* registers)
{

}


void
arch_debug_stack_trace(void)
{

}


bool
arch_debug_contains_call(Thread *thread, const char *symbol,
	addr_t start, addr_t end)
{
	return false;
}


void *
arch_debug_get_caller(void)
{
	return NULL;
}


int32
arch_debug_get_stack_trace(addr_t* returnAddresses, int32 maxCount,
	int32 skipIframes, int32 skipFrames, uint32 flags)
{
	return 0;
}


void*
arch_debug_get_interrupt_pc(bool* _isSyscall)
{
	return NULL;
}


void
arch_debug_unset_current_thread(void)
{

}


bool
arch_is_debug_variable_defined(const char* variableName)
{
	return false;
}


status_t
arch_set_debug_variable(const char* variableName, uint64 value)
{
	return B_OK;
}


status_t
arch_get_debug_variable(const char* variableName, uint64* value)
{
	return B_OK;
}


ssize_t
arch_debug_gdb_get_registers(char* buffer, size_t bufferSize)
{
	return B_ERROR;
}


status_t
arch_debug_init(kernel_args *args)
{
	return B_OK;
}

void
arch_debug_call_with_fault_handler(cpu_ent* cpu, jmp_buf jumpBuffer,
	void (*function)(void*), void* parameter)
{
	// To be implemented in asm, not here.
}


status_t
arch_platform_init(struct kernel_args *args)
{
	return B_OK;
}


status_t
arch_platform_init_post_vm(struct kernel_args *args)
{
	return B_OK;
}


status_t
arch_platform_init_post_thread(struct kernel_args *args)
{
	return B_OK;
}


status_t
arch_rtc_init(struct kernel_args *args, struct real_time_data *data)
{
	return B_OK;
}


uint32
arch_rtc_get_hw_time(void)
{
	return 0;
}


void
arch_rtc_set_hw_time(uint32 seconds)
{

}


void
arch_rtc_set_system_time_offset(struct real_time_data *data, bigtime_t offset)
{

}


bigtime_t
arch_rtc_get_system_time_offset(struct real_time_data *data)
{
	return 0;
}


status_t
arch_smp_init(kernel_args *args)
{
	return B_OK;
}


status_t
arch_smp_per_cpu_init(kernel_args *args, int32 cpu)
{
	return B_OK;
}


void
arch_smp_send_broadcast_ici(void)
{

}


void
arch_smp_send_ici(int32 target_cpu)
{

}


status_t
arch_get_system_info(system_info *info, size_t size)
{
	return B_OK;
}


status_t
arch_system_info_init(struct kernel_args *args)
{
	return B_OK;
}


status_t
arch_thread_init(struct kernel_args *args)
{
	return B_OK;
}


status_t
arch_team_init_team_struct(Team *p, bool kernel)
{
	return B_ERROR;
}


status_t
arch_thread_init_thread_struct(Thread *thread)
{
	return B_ERROR;
}


void
arch_thread_init_kthread_stack(Thread* thread, void* _stack, void* _stackTop,
	void (*function)(void*), const void* data)
{

}


status_t
arch_thread_init_tls(Thread *thread)
{
	return B_ERROR;
}


void
arch_thread_context_switch(Thread *from, Thread *to)
{

}


void
arch_thread_dump_info(void *info)
{

}


status_t
arch_thread_enter_userspace(Thread* thread, addr_t entry, void* args1,
	void* args2)
{
	return B_ERROR;
}


bool
arch_on_signal_stack(Thread *thread)
{
	return false;
}


status_t
arch_setup_signal_frame(Thread* thread, struct sigaction* action,
	struct signal_frame_data* signalFrameData)
{
	return B_ERROR;
}


int64
arch_restore_signal_frame(struct signal_frame_data* signalFrameData)
{
	return 0;
}


void
arch_store_fork_frame(struct arch_fork_arg *arg)
{

}


void
arch_restore_fork_frame(struct arch_fork_arg* arg)
{

}


void
arch_timer_set_hardware_timer(bigtime_t timeout)
{
	
}


void
arch_timer_clear_hardware_timer(void)
{
	
}


int
arch_init_timer(kernel_args *args)
{
	return 0;
}


// The software breakpoint instruction (int3).
const uint8 kX86SoftwareBreakpoint[1] = { 0xcc };


void
arch_clear_team_debug_info(struct arch_team_debug_info *info)
{

}


void
arch_destroy_team_debug_info(struct arch_team_debug_info *info)
{

}


void
arch_clear_thread_debug_info(struct arch_thread_debug_info *info)
{

}


void
arch_destroy_thread_debug_info(struct arch_thread_debug_info *info)
{

}


void
arch_update_thread_single_step()
{

}


void
arch_set_debug_cpu_state(const debug_cpu_state *cpuState)
{

}


void
arch_get_debug_cpu_state(debug_cpu_state *cpuState)
{

}


status_t
arch_set_breakpoint(void *address)
{
	return B_OK;
}


status_t
arch_clear_breakpoint(void *address)
{
	return B_OK;
}


status_t
arch_set_watchpoint(void *address, uint32 type, int32 length)
{
	return B_OK;
}


status_t
arch_clear_watchpoint(void *address)
{
	return B_OK;
}


bool
arch_has_breakpoints(struct arch_team_debug_info *info)
{
	return false;
}


void
x86_init_user_debug()
{

}


status_t
arch_vm_translation_map_create_map(bool kernel, VMTranslationMap** _map)
{
	return B_ERROR;
}


status_t
arch_vm_translation_map_init(kernel_args *args,
	VMPhysicalPageMapper** _physicalPageMapper)
{
	return B_OK;
}


status_t
arch_vm_translation_map_init_post_sem(kernel_args *args)
{
	return B_OK;
}


status_t
arch_vm_translation_map_init_post_area(kernel_args *args)
{
	return B_OK;
}


status_t
arch_vm_translation_map_early_map(kernel_args *args, addr_t va, phys_addr_t pa,
	uint8 attributes, phys_addr_t (*get_free_page)(kernel_args *))
{
	return B_ERROR;
}


bool
arch_vm_translation_map_is_kernel_page_accessible(addr_t virtualAddress,
	uint32 protection)
{
	return true;
}


// Currently got generic elf.cpp #ifdef'd out for x86_64, define stub versions here.

status_t
elf_load_user_image(const char *path, Team *team, int flags, addr_t *_entry)
{
	return B_ERROR;
}

image_id
load_kernel_add_on(const char *path)
{
	return 0;
}

status_t
unload_kernel_add_on(image_id id)
{
	return B_ERROR;
}

status_t
elf_debug_lookup_symbol_address(addr_t address, addr_t *_baseAddress,
	const char **_symbolName, const char **_imageName, bool *_exactMatch)
{
	return B_ERROR;
}

addr_t
elf_debug_lookup_symbol(const char* searchName)
{
	return 0;
}

struct elf_image_info *
elf_get_kernel_image()
{
	return NULL;
}

image_id
elf_create_memory_image(const char* imageName, addr_t text, size_t textSize,
	addr_t data, size_t dataSize)
{
	return B_ERROR;
}

status_t
elf_add_memory_image_symbol(image_id id, const char* name, addr_t address,
	size_t size, int32 type)
{
	return B_ERROR;
}

status_t
elf_init(struct kernel_args *args)
{
	return B_OK;
}

status_t
get_image_symbol(image_id image, const char *name, int32 symbolType,
	void **_symbolLocation)
{
	return B_OK;
}

status_t
_user_read_kernel_image_symbols(image_id id, struct Elf32_Sym* symbolTable,
	int32* _symbolCount, char* stringTable, size_t* _stringTableSize,
	addr_t* _imageDelta)
{
	return B_ERROR;
}
