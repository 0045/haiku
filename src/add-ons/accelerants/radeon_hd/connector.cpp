/*
 * Copyright 2006-2011, Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *      Alexander von Gluck, kallisti5@unixzen.com
 */


#include "accelerant_protos.h"
#include "accelerant.h"
#include "bios.h"
#include "connector.h"
#include "gpu.h"
#include "utility.h"

#include <Debug.h>


#undef TRACE

#define TRACE_CONNECTOR
#ifdef TRACE_CONNECTOR
#   define TRACE(x...) _sPrintf("radeon_hd: " x)
#else
#   define TRACE(x...) ;
#endif

#define ERROR(x...) _sPrintf("radeon_hd: " x)


static void
gpio_lock_i2c(void* cookie, bool lock)
{
	gpio_info *info = (gpio_info*)cookie;
	radeon_shared_info &sinfo = *gInfo->shared_info;

	uint32 buffer = 0;

	if (lock == true) {
		// hw_capable and > DCE3
		if (info->hw_capable == true
			&& sinfo.dceMajor >= 3) {
			// Switch GPIO pads to ddc mode
			buffer = Read32(OUT, info->mask_scl_reg);
			buffer &= ~(1 << 16);
			Write32(OUT, info->mask_scl_reg, buffer);
		}

		// Clear pins
		buffer = Read32(OUT, info->a_scl_reg) & ~info->a_scl_mask;
		Write32(OUT, info->a_scl_reg, buffer);
		buffer = Read32(OUT, info->a_sda_reg) & ~info->a_sda_mask;
		Write32(OUT, info->a_sda_reg, buffer);
	}

	// Set pins to input
	buffer = Read32(OUT, info->en_scl_reg) & ~info->en_scl_mask;
	Write32(OUT, info->en_scl_reg, buffer);
	buffer = Read32(OUT, info->en_sda_reg) & ~info->en_sda_mask;
	Write32(OUT, info->en_sda_reg, buffer);

	// mask GPIO pins for software use
	buffer = Read32(OUT, info->mask_scl_reg);
	if (lock == true) {
		buffer |= info->mask_scl_mask;
	} else {
		buffer &= ~info->mask_scl_mask;
	}
	Write32(OUT, info->mask_scl_reg, buffer);
	Read32(OUT, info->mask_scl_reg);

	buffer = Read32(OUT, info->mask_sda_reg);
	if (lock == true) {
		buffer |= info->mask_sda_mask;
	} else {
		buffer &= ~info->mask_sda_mask;
	}
	Write32(OUT, info->mask_sda_reg, buffer);
	Read32(OUT, info->mask_sda_reg);
}


static status_t
gpio_get_i2c_bit(void* cookie, int* _clock, int* _data)
{
	gpio_info *info = (gpio_info*)cookie;

	uint32 scl = Read32(OUT, info->y_scl_reg)
		& info->y_scl_mask;
	uint32 sda = Read32(OUT, info->y_sda_reg)
		& info->y_sda_mask;

	*_clock = (scl != 0);
	*_data = (sda != 0);

	return B_OK;
}


static status_t
gpio_set_i2c_bit(void* cookie, int clock, int data)
{
	gpio_info* info = (gpio_info*)cookie;

	uint32 scl = Read32(OUT, info->en_scl_reg)
		& ~info->en_scl_mask;
	scl |= clock ? 0 : info->en_scl_mask;
	Write32(OUT, info->en_scl_reg, scl);
	Read32(OUT, info->en_scl_reg);

	uint32 sda = Read32(OUT, info->en_sda_reg)
		& ~info->en_sda_mask;
	sda |= data ? 0 : info->en_sda_mask;
	Write32(OUT, info->en_sda_reg, sda);
	Read32(OUT, info->en_sda_reg);

	return B_OK;
}


bool
connector_read_edid(uint32 connector, edid1_info *edid)
{
	// ensure things are sane
	uint32 gpioID = gConnector[connector]->gpioID;
	if (gGPIOInfo[gpioID]->valid == false)
		return false;

	if (gConnector[connector]->type == VIDEO_CONNECTOR_LVDS) {
		// we should call connector_read_edid_lvds at some point
		ERROR("%s: LCD panel detected (LVDS), sending VESA EDID!\n",
			__func__);
		memcpy(edid, &gInfo->shared_info->edid_info, sizeof(struct edid1_info));
		return true;
	}

	i2c_bus bus;

	ddc2_init_timing(&bus);
	bus.cookie = (void*)gGPIOInfo[gpioID];
	bus.set_signals = &gpio_set_i2c_bit;
	bus.get_signals = &gpio_get_i2c_bit;

	gpio_lock_i2c(bus.cookie, true);
	status_t edid_result = ddc2_read_edid1(&bus, edid, NULL, NULL);
	gpio_lock_i2c(bus.cookie, false);

	if (edid_result != B_OK)
		return false;

	TRACE("%s: found edid monitor on connector #%" B_PRId32 "\n",
		__func__, connector);

	return true;
}


#if 0
bool
connector_read_edid_lvds(uint32 connector, edid1_info *edid)
{
	uint8 dceMajor;
	uint8 dceMinor;
	int index = GetIndexIntoMasterTable(DATA, LVDS_Info);
	uint16 offset;

	if (atom_parse_data_header(gAtomContexg, index, NULL,
		&dceMajor, &dceMinor, &offset) == B_OK) {
		lvdsInfo = (union lvds_info *)(gAtomContext->bios + offset);

		display_timing timing;
		// Pixel Clock
		timing.pixel_clock
			= B_LENDIAN_TO_HOST_INT16(lvdsInfo->info.sLCDTiming.usPixClk) * 10;
		// Horizontal
		timing.h_display
			= B_LENDIAN_TO_HOST_INT16(lvdsInfo->info.sLCDTiming.usHActive);
		timing.h_total = timing.h_display + B_LENDIAN_TO_HOST_INT16(
			lvdsInfo->info.sLCDTiming.usHBlanking_Time);
		timing.h_sync_start = timing.h_display
			+ B_LENDIAN_TO_HOST_INT16(lvdsInfo->info.sLCDTiming.usHSyncOffset);
		timing.h_sync_end = timing.h_sync_start
			+ B_LENDIAN_TO_HOST_INT16(lvdsInfo->info.sLCDTiming.usHSyncWidth);
		// Vertical
		timing.v_display
			= B_LENDIAN_TO_HOST_INT16(lvdsInfo->info.sLCDTiming.usVActive);
		timing.v_total = timing.v_display + B_LENDIAN_TO_HOST_INT16(
			lvdsInfo->info.sLCDTiming.usVBlanking_Time);
		timing.v_sync_start = timing.v_display
			+ B_LENDIAN_TO_HOST_INT16(lvdsInfo->info.sLCDTiming.usVSyncOffset);
		timing.v_sync_end = timing.v_sync_start
			+ B_LENDIAN_TO_HOST_INT16(lvdsInfo->info.sLCDTiming.usVSyncWidth);

		#if 0
		// Who cares.
		uint32 powerDelay
			= B_LENDIAN_TO_HOST_INT16(lvdsInfo->info.usOffDelayInMs);
		uint32 lcdMisc = lvdsInfo->info.ucLVDS_Misc;
		#endif

		uint16 flags = B_LENDIAN_TO_HOST_INT16(
			lvdsInfo->info.sLCDTiming.susModeMiscInfo.usAccess);

		if ((flags & ATOM_VSYNC_POLARITY) == 0)
			timing.flags |= B_POSITIVE_VSYNC;
		if ((flags & ATOM_HSYNC_POLARITY) == 0)
			timing.flags |= B_POSITIVE_HSYNC;

		// Extra flags
		if ((flags & ATOM_INTERLACE) != 0)
			timing.flags |= B_TIMING_INTERLACED;

		#if 0
		// We don't use these timing flags at the moment
		if ((flags & ATOM_COMPOSITESYNC) != 0)
			timing.flags |= MODE_FLAG_CSYNC;
		if ((flags & ATOM_DOUBLE_CLOCK_MODE) != 0)
			timing.flags |= MODE_FLAG_DBLSCAN;
		#endif

		// TODO: generate a fake EDID with information above
	}
}
#endif


status_t
connector_attach_gpio(uint32 id, uint8 hw_line)
{
	gConnector[id]->gpioID = 0;
	for (uint32 i = 0; i < ATOM_MAX_SUPPORTED_DEVICE; i++) {
		if (gGPIOInfo[i]->hw_line != hw_line)
			continue;
		gConnector[id]->gpioID = i;
		return B_OK;
	}

	TRACE("%s: couldn't find GPIO for connector %" B_PRIu32 "\n",
		__func__, id);
	return B_ERROR;
}


status_t
gpio_probe()
{
	radeon_shared_info &info = *gInfo->shared_info;

	int index = GetIndexIntoMasterTable(DATA, GPIO_I2C_Info);

	uint8 tableMajor;
	uint8 tableMinor;
	uint16 tableOffset;
	uint16 tableSize;

	if (atom_parse_data_header(gAtomContext, index, &tableSize,
		&tableMajor, &tableMinor, &tableOffset) != B_OK) {
		ERROR("%s: could't read GPIO_I2C_Info table from AtomBIOS index %d!\n",
			__func__, index);
		return B_ERROR;
	}

	struct _ATOM_GPIO_I2C_INFO *i2c_info
		= (struct _ATOM_GPIO_I2C_INFO *)(gAtomContext->bios + tableOffset);

	uint32 numIndices = (tableSize - sizeof(ATOM_COMMON_TABLE_HEADER))
		/ sizeof(ATOM_GPIO_I2C_ASSIGMENT);

	if (numIndices > ATOM_MAX_SUPPORTED_DEVICE) {
		ERROR("%s: ERROR: AtomBIOS contains more GPIO_Info items then I"
			"was prepared for! (seen: %" B_PRIu32 "; max: %" B_PRIu32 ")\n",
			__func__, numIndices, (uint32)ATOM_MAX_SUPPORTED_DEVICE);
		return B_ERROR;
	}

	for (uint32 i = 0; i < numIndices; i++) {
		ATOM_GPIO_I2C_ASSIGMENT *gpio = &i2c_info->asGPIO_Info[i];

		if (info.dceMajor >= 3) {
			if (i == 4 && B_LENDIAN_TO_HOST_INT16(gpio->usClkMaskRegisterIndex)
				== 0x1fda && gpio->sucI2cId.ucAccess == 0x94) {
				gpio->sucI2cId.ucAccess = 0x14;
				TRACE("%s: BUG: GPIO override for DCE 3 occured\n", __func__);
			}
		}

		if (info.dceMajor >= 4) {
			if (i == 7 && B_LENDIAN_TO_HOST_INT16(gpio->usClkMaskRegisterIndex)
				== 0x1936 && gpio->sucI2cId.ucAccess == 0) {
				gpio->sucI2cId.ucAccess = 0x97;
				gpio->ucDataMaskShift = 8;
				gpio->ucDataEnShift = 8;
				gpio->ucDataY_Shift = 8;
				gpio->ucDataA_Shift = 8;
				TRACE("%s: BUG: GPIO override for DCE 4 occured\n", __func__);
			}
		}

		// populate gpio information
		gGPIOInfo[i]->hw_line
			= gpio->sucI2cId.ucAccess;
		gGPIOInfo[i]->hw_capable
			= (gpio->sucI2cId.sbfAccess.bfHW_Capable) ? true : false;

		// GPIO mask (Allows software to control the GPIO pad)
		// 0 = chip access; 1 = only software;
		gGPIOInfo[i]->mask_scl_reg
			= B_LENDIAN_TO_HOST_INT16(gpio->usClkMaskRegisterIndex) * 4;
		gGPIOInfo[i]->mask_sda_reg
			= B_LENDIAN_TO_HOST_INT16(gpio->usDataMaskRegisterIndex) * 4;
		gGPIOInfo[i]->mask_scl_mask
			= (1 << gpio->ucClkMaskShift);
		gGPIOInfo[i]->mask_sda_mask
			= (1 << gpio->ucDataMaskShift);

		// GPIO output / write (A) enable
		// 0 = GPIO input (Y); 1 = GPIO output (A);
		gGPIOInfo[i]->en_scl_reg
			= B_LENDIAN_TO_HOST_INT16(gpio->usClkEnRegisterIndex) * 4;
		gGPIOInfo[i]->en_sda_reg
			= B_LENDIAN_TO_HOST_INT16(gpio->usDataEnRegisterIndex) * 4;
		gGPIOInfo[i]->en_scl_mask
			= (1 << gpio->ucClkEnShift);
		gGPIOInfo[i]->en_sda_mask
			= (1 << gpio->ucDataEnShift);

		// GPIO output / write (A)
		gGPIOInfo[i]->a_scl_reg
			= B_LENDIAN_TO_HOST_INT16(gpio->usClkA_RegisterIndex) * 4;
		gGPIOInfo[i]->a_sda_reg
			= B_LENDIAN_TO_HOST_INT16(gpio->usDataA_RegisterIndex) * 4;
		gGPIOInfo[i]->a_scl_mask
			= (1 << gpio->ucClkA_Shift);
		gGPIOInfo[i]->a_sda_mask
			= (1 << gpio->ucDataA_Shift);

		// GPIO input / read (Y)
		gGPIOInfo[i]->y_scl_reg
			= B_LENDIAN_TO_HOST_INT16(gpio->usClkY_RegisterIndex) * 4;
		gGPIOInfo[i]->y_sda_reg
			= B_LENDIAN_TO_HOST_INT16(gpio->usDataY_RegisterIndex) * 4;
		gGPIOInfo[i]->y_scl_mask
			= (1 << gpio->ucClkY_Shift);
		gGPIOInfo[i]->y_sda_mask
			= (1 << gpio->ucDataY_Shift);

		// ensure data is valid
		gGPIOInfo[i]->valid = (gGPIOInfo[i]->mask_scl_reg) ? true : false;

		TRACE("%s: GPIO @ %" B_PRIu32 ", valid: %s, hw_line: 0x%" B_PRIX32 "\n",
			__func__, i, gGPIOInfo[i]->valid ? "true" : "false",
			gGPIOInfo[i]->hw_line);
	}

	return B_OK;
}
