/*
 * Copyright 2012 Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *		Michael Lotz, mmlr@mlotz.ch
 *      François Revol, revol@free.fr
 *		Alexander von Gluck IV, kallisti5@unixzen.com
 */

#include "arch_mailbox.h"

#include <arch_cpu.h>

#include "bcm2708.h"


//extern addr_t gPeripheralBase;
//
//
//static inline addr_t
//convert_mailbox_reg(addr_t reg)
//{
//	return gPeripheralBase + ARM_CTRL_0_MAILBOX_BASE + reg;
//}
//
//
//static inline void
//write_mailbox_reg(addr_t reg, uint32 value)
//{
//	arch_cpu_memory_write_barrier();
//	*(volatile uint32*)convert_mailbox_reg(reg) = value;
//}
//
//
//static inline uint32
//read_mailbox_reg(addr_t reg)
//{
//	uint32 result = *(volatile uint32*)convert_mailbox_reg(reg);
//	arch_cpu_memory_read_barrier();
//	return result;
//}


class ArchMailboxArmBCM2708 : public ArchMailbox {
public:
							ArchMailboxArmBCM2708(addr_t base)
								:
								ArchMailbox(base) {}
                            ~ArchMailboxArmBCM2708() {}

virtual status_t			Write(uint8 channel, uint32 value);
virtual status_t			Read(uint8 channel, uint32& value);
};


extern "C" ArchMailbox*
arch_get_mailbox_arm_bcm2708(addr_t base)
{
    return new ArchMailboxArmBCM2708(base);
}


status_t
ArchMailboxArmBCM2708::Write(uint8 channel, uint32 value)
{
	// We have to wait for the mailbox to drain if it is marked full.
	//while ((read_mailbox_reg(ARM_MAILBOX_STATUS) & ARM_MAILBOX_FULL) != 0)
	//	;

	//value &= ARM_MAILBOX_DATA_MASK;
	//write_mailbox_reg(ARM_MAILBOX_WRITE, value | channel);
	return B_OK;
}


status_t
ArchMailboxArmBCM2708::Read(uint8 channel, uint32& value)
{
	//while (true) {
	//	// Wait for something to arrive in the mailbox.
	//	if ((read_mailbox_reg(ARM_MAILBOX_STATUS) & ARM_MAILBOX_EMPTY) != 0)
	//		continue;

	//	value = read_mailbox_reg(ARM_MAILBOX_READ);
	//	if ((value & ARM_MAILBOX_CHANNEL_MASK) != channel) {
	//		// Not for us, retry.
	//		continue;
	//	}

	//	break;
	//}

	//value &= ARM_MAILBOX_DATA_MASK;
	return B_OK;
}
