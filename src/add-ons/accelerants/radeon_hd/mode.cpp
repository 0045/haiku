/*
 * Copyright 2006-2011, Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 *
 * Support for i915 chipset and up based on the X driver,
 * Copyright 2006-2007 Intel Corporation.
 *
 * Authors:
 *		Axel Dörfler, axeld@pinc-software.de
 *		Alexander von Gluck, kallisti5@unixzen.com
 */


#include "accelerant_protos.h"
#include "accelerant.h"
#include "utility.h"
#include "mode.h"

#include <stdio.h>
#include <string.h>
#include <math.h>

#include <create_display_modes.h>


#define TRACE_MODE
#ifdef TRACE_MODE
extern "C" void _sPrintf(const char *format, ...);
#	define TRACE(x...) _sPrintf("radeon_hd: " x)
#else
#	define TRACE(x...) ;
#endif


status_t
create_mode_list(void)
{
	const color_space kRadeonHDSpaces[] = {B_RGB32_LITTLE, B_RGB24_LITTLE,
		B_RGB16_LITTLE, B_RGB15_LITTLE, B_CMAP8};

	detect_crt_ranges();

	gInfo->mode_list_area = create_display_modes("radeon HD modes",
		gInfo->shared_info->has_edid ? &gInfo->shared_info->edid_info : NULL,
		NULL, 0, kRadeonHDSpaces,
		sizeof(kRadeonHDSpaces) / sizeof(kRadeonHDSpaces[0]),
		is_mode_supported, &gInfo->mode_list, &gInfo->shared_info->mode_count);
	if (gInfo->mode_list_area < B_OK)
		return gInfo->mode_list_area;

	gInfo->shared_info->mode_list_area = gInfo->mode_list_area;

	return B_OK;
}


//	#pragma mark -


uint32
radeon_accelerant_mode_count(void)
{
	TRACE("%s\n", __func__);

	return gInfo->shared_info->mode_count;
}


status_t
radeon_get_mode_list(display_mode *modeList)
{
	TRACE("%s\n", __func__);
	memcpy(modeList, gInfo->mode_list,
		gInfo->shared_info->mode_count * sizeof(display_mode));
	return B_OK;
}


status_t
radeon_get_edid_info(void* info, size_t size, uint32* edid_version)
{
	TRACE("%s\n", __func__);
	if (!gInfo->shared_info->has_edid)
		return B_ERROR;
	if (size < sizeof(struct edid1_info))
		return B_BUFFER_OVERFLOW;

	memcpy(info, &gInfo->shared_info->edid_info, sizeof(struct edid1_info));
	*edid_version = EDID_VERSION_1;

	return B_OK;
}


static void
get_color_space_format(const display_mode &mode, uint32 &colorMode,
	uint32 &bytesPerRow, uint32 &bitsPerPixel)
{
	uint32 bytesPerPixel;

	switch (mode.space) {
		case B_RGB32_LITTLE:
			colorMode = DISPLAY_CONTROL_RGB32;
			bytesPerPixel = 4;
			bitsPerPixel = 32;
			break;
		case B_RGB16_LITTLE:
			colorMode = DISPLAY_CONTROL_RGB16;
			bytesPerPixel = 2;
			bitsPerPixel = 16;
			break;
		case B_RGB15_LITTLE:
			colorMode = DISPLAY_CONTROL_RGB15;
			bytesPerPixel = 2;
			bitsPerPixel = 15;
			break;
		case B_CMAP8:
		default:
			colorMode = DISPLAY_CONTROL_CMAP8;
			bytesPerPixel = 1;
			bitsPerPixel = 8;
			break;
	}

	bytesPerRow = mode.virtual_width * bytesPerPixel;
}


// Blacks the screen out, useful for mode setting
static void
CardBlankSet(bool blank)
{
	int blackColorReg;
	int blankControlReg;

	blackColorReg = D1CRTC_BLACK_COLOR;
	blankControlReg = D1CRTC_BLANK_CONTROL;

	Write32(CRT, blackColorReg, 0);
	Write32Mask(CRT, blankControlReg, blank ? 1 << 8 : 0, 1 << 8);
}


static void
CardFBSet(display_mode *mode)
{
	uint32 colorMode;
	uint32 bytesPerRow;
	uint32 bitsPerPixel;

	get_color_space_format(*mode, colorMode, bytesPerRow, bitsPerPixel);

	#if 0
	// Disable VGA mode to enable Radeon extended registers
	Write32Mask(VGA, VGA_RENDER_CONTROL, 0, 0x00030000);
	Write32Mask(VGA, VGA_MODE_CONTROL, 0, 0x00000030);
	Write32Mask(VGA, VGA_HDP_CONTROL, 0x00010010, 0x00010010);
	Write32Mask(VGA, gRegister->vgaControl, 0, D1VGA_MODE_ENABLE
		| D1VGA_TIMING_SELECT | D1VGA_SYNC_POLARITY_SELECT);
	#endif

	// disable R/B swap, disable tiling, disable 16bit alpha, etc.
	Write32Mask(CRT, gRegister->grphEnable, 1, 0x00000001);
	Write32(CRT, gRegister->grphControl, 0);

	// set color mode on video card
	switch (mode->space) {
		case B_CMAP8:
			Write32Mask(CRT, gRegister->grphControl,
				0, 0x00000703);
			break;
		case B_RGB15_LITTLE:
			Write32Mask(CRT, gRegister->grphControl,
				0x000001, 0x00000703);
			break;
		case B_RGB16_LITTLE:
			Write32Mask(CRT, gRegister->grphControl,
				0x000101, 0x00000703);
			break;
		case B_RGB24_LITTLE:
		case B_RGB32_LITTLE:
		default:
			Write32Mask(CRT, gRegister->grphControl,
				0x000002, 0x00000703);
			break;
	}

	Write32(CRT, gRegister->grphSwapControl, 0);
		// only for chipsets > r600
		// R5xx - RS690 case is GRPH_CONTROL bit 16

	// framebuffersize = w * h * bpp  =  fb bits / 8 = bytes needed

	uint64_t fbAddress = gInfo->shared_info->frame_buffer_phys;

	// Tell GPU which frame buffer address to draw from
	if (gInfo->shared_info->device_chipset >= (RADEON_R700 | 0x70)) {
		Write32(CRT, gRegister->grphPrimarySurfaceAddrHigh,
			(fbAddress >> 32) & 0xf);
		Write32(CRT, gRegister->grphSecondarySurfaceAddrHigh,
			(fbAddress >> 32) & 0xf);
	}

	Write32(CRT, gRegister->grphPrimarySurfaceAddr,
		fbAddress & 0xffffffff);
	Write32(CRT, gRegister->grphSecondarySurfaceAddr,
		fbAddress & 0xffffffff);

	Write32(CRT, gRegister->grphPitch, bytesPerRow / 4);
	Write32(CRT, gRegister->grphSurfaceOffsetX, 0);
	Write32(CRT, gRegister->grphSurfaceOffsetY, 0);
	Write32(CRT, gRegister->grphXStart, 0);
	Write32(CRT, gRegister->grphYStart, 0);
	Write32(CRT, gRegister->grphXEnd, mode->virtual_width);
	Write32(CRT, gRegister->grphYEnd, mode->virtual_height);

	/* D1Mode registers */
	Write32(CRT, gRegister->modeDesktopHeight, mode->virtual_height);

	// update shared info
	gInfo->shared_info->bytes_per_row = bytesPerRow;
	gInfo->shared_info->current_mode = *mode;
	gInfo->shared_info->bits_per_pixel = bitsPerPixel;
}


static void
CardModeSet(display_mode *mode)
{
	display_timing& displayTiming = mode->timing;

	TRACE("%s called to do %dx%d\n",
		__func__, displayTiming.h_display, displayTiming.v_display);

	// enable read requests
	Write32Mask(CRT, gRegister->grphControl, 0, 0x01000000);

	// *** Horizontal
	Write32(CRT, gRegister->crtHTotal,
		displayTiming.h_total - 1);

	// Calculate blanking
	uint16 frontPorch = displayTiming.h_sync_start - displayTiming.h_display;
	uint16 backPorch = displayTiming.h_total - displayTiming.h_sync_end;

	uint16 blankStart = frontPorch - OVERSCAN;
	uint16 blankEnd = backPorch;

	Write32(CRT, gRegister->crtHBlank,
		blankStart | (blankEnd << 16));

	Write32(CRT, gRegister->crtHSync,
		(displayTiming.h_sync_end - displayTiming.h_sync_start) << 16);

	// set flag for neg. H sync. M76 Register Reference Guide 2-256
	Write32Mask(CRT, gRegister->crtHPolarity,
		displayTiming.flags & B_POSITIVE_HSYNC ? 0 : 1, 0x1);

	// *** Vertical
	Write32(CRT, gRegister->crtVTotal,
		displayTiming.v_total - 1);

	frontPorch = displayTiming.v_sync_start - displayTiming.v_display;
	backPorch = displayTiming.v_total - displayTiming.v_sync_end;

	blankStart = frontPorch - OVERSCAN;
	blankEnd = backPorch;

	Write32(CRT, gRegister->crtVBlank,
		blankStart | (blankEnd << 16));

	// Set Interlace if specified within mode line
	if (displayTiming.flags & B_TIMING_INTERLACED) {
		Write32(CRT, gRegister->crtInterlace, 0x1);
		Write32(CRT, gRegister->modeDataFormat, 0x1);
	} else {
		Write32(CRT, gRegister->crtInterlace, 0x0);
		Write32(CRT, gRegister->modeDataFormat, 0x0);
	}

	Write32(CRT, gRegister->crtVSync,
		(displayTiming.v_sync_end - displayTiming.v_sync_start) << 16);

	// set flag for neg. V sync. M76 Register Reference Guide 2-258
	Write32Mask(CRT, gRegister->crtVPolarity,
		displayTiming.flags & B_POSITIVE_VSYNC ? 0 : 1, 0x1);

	/*	set D1CRTC_HORZ_COUNT_BY2_EN to 0;
		should only be set to 1 on 30bpp DVI modes
	*/
	Write32Mask(CRT, gRegister->crtCountControl, 0x0, 0x1);
}


static void
CardModeScale(display_mode *mode)
{
	Write32(CRT, gRegister->viewportSize,
		mode->timing.v_display | (mode->timing.h_display << 16));
	Write32(CRT, gRegister->viewportStart, 0);

	// For now, no overscan support
	Write32(CRT, D1MODE_EXT_OVERSCAN_LEFT_RIGHT,
		(OVERSCAN << 16) | OVERSCAN); // LEFT | RIGHT
	Write32(CRT, D1MODE_EXT_OVERSCAN_TOP_BOTTOM,
		(OVERSCAN << 16) | OVERSCAN); // TOP | BOTTOM

	// No scaling
	Write32(CRT, gRegister->sclUpdate, (1<<16));// Lock
	Write32(CRT, gRegister->sclEnable, 0);
	Write32(CRT, gRegister->sclTapControl, 0);
	Write32(CRT, gRegister->modeCenter, 0);
	Write32(CRT, gRegister->sclUpdate, 0);		// Unlock

	#if 0
	// Auto scale keeping aspect ratio
	Write32(CRT, regOffset + D1MODE_CENTER, 1);

	Write32(CRT, regOffset + D1SCL_UPDATE, 0);
	Write32(CRT, regOffset + D1SCL_FLIP_CONTROL, 0);

	Write32(CRT, regOffset + D1SCL_ENABLE, 1);
	Write32(CRT, regOffset + D1SCL_HVSCALE, 0x00010001);

	Write32(CRT, regOffset + D1SCL_TAP_CONTROL, 0x00000101);

	Write32(CRT, regOffset + D1SCL_HFILTER, 0x00030100);
	Write32(CRT, regOffset + D1SCL_VFILTER, 0x00030100);

	Write32(CRT, regOffset + D1SCL_DITHER, 0x00001010);
	#endif
}


status_t
radeon_set_display_mode(display_mode *mode)
{
	int crtNumber = 0;

	init_registers(crtNumber);

	CardBlankSet(true);
	CardFBSet(mode);
	CardBlankSet(false);
	CardModeSet(mode);
	CardModeScale(mode);

	#if 0
	PLLSet(0, mode->timing.pixel_clock);
	PLLPower(0, RHD_POWER_ON);
	DACPower(0, RHD_POWER_ON);
	#endif

	// ensure graphics are enabled and powered on
	Write32Mask(CRT, D1GRPH_ENABLE, 0x00000001, 0x00000001);
	snooze(2);
	Write32Mask(CRT, D1CRTC_CONTROL, 0, 0x01000000); /* enable read requests */
	Write32Mask(CRT, D1CRTC_CONTROL, 1, 1);

	int32 crtstatus = Read32(CRT, D1CRTC_STATUS);
	TRACE("CRT0 Status: 0x%X\n", crtstatus);

	return B_OK;
}


status_t
radeon_get_display_mode(display_mode *_currentMode)
{
	TRACE("%s\n", __func__);

	*_currentMode = gInfo->shared_info->current_mode;
	return B_OK;
}


status_t
radeon_get_frame_buffer_config(frame_buffer_config *config)
{
	TRACE("%s\n", __func__);

	config->frame_buffer = gInfo->shared_info->frame_buffer;
	config->frame_buffer_dma = (uint8 *)gInfo->shared_info->frame_buffer_phys;

	config->bytes_per_row = gInfo->shared_info->bytes_per_row;

	return B_OK;
}


status_t
radeon_get_pixel_clock_limits(display_mode *mode, uint32 *_low, uint32 *_high)
{
	TRACE("%s\n", __func__);

	if (_low != NULL) {
		// lower limit of about 48Hz vertical refresh
		uint32 totalClocks = (uint32)mode->timing.h_total
			*(uint32)mode->timing.v_total;
		uint32 low = (totalClocks * 48L) / 1000L;

		if (low < gInfo->shared_info->pll_info.min_frequency)
			low = gInfo->shared_info->pll_info.min_frequency;
		else if (low > gInfo->shared_info->pll_info.max_frequency)
			return B_ERROR;

		*_low = low;
	}

	if (_high != NULL)
		*_high = gInfo->shared_info->pll_info.max_frequency;

	//*_low = 48L;
	//*_high = 100 * 1000000L;
	return B_OK;
}


bool
is_mode_supported(display_mode *mode)
{
	TRACE("MODE: %d ; %d %d %d %d ; %d %d %d %d\n",
		mode->timing.pixel_clock, mode->timing.h_display,
		mode->timing.h_sync_start, mode->timing.h_sync_end,
		mode->timing.h_total, mode->timing.v_display,
		mode->timing.v_sync_start, mode->timing.v_sync_end,
		mode->timing.v_total);

	// Validate modeline is within a sane range
	if (is_mode_sane(mode) != B_OK)
		return false;

	uint32 crtid = 0;

	// if we have edid info, check frequency adginst crt reported valid ranges
	if (gInfo->shared_info->has_edid) {

		uint32 hfreq = mode->timing.pixel_clock / mode->timing.h_total;
		if (hfreq > gCRT[crtid]->hfreq_max + 1
			|| hfreq < gCRT[crtid]->hfreq_min - 1) {
			TRACE("!!! hfreq : %d , hfreq_min : %d, hfreq_max : %d\n",
				hfreq, gCRT[crtid]->hfreq_min, gCRT[crtid]->hfreq_max);
			TRACE("!!! %dx%d falls outside of CRT %d's valid "
				"horizontal range.\n", mode->timing.h_display,
				mode->timing.v_display, crtid);
			return false;
		}

		uint32 vfreq = mode->timing.pixel_clock / ((mode->timing.v_total
			* mode->timing.h_total) / 1000);

		if (vfreq > gCRT[crtid]->vfreq_max + 1
			|| vfreq < gCRT[crtid]->vfreq_min - 1) {
			TRACE("!!! vfreq : %d , vfreq_min : %d, vfreq_max : %d\n",
				vfreq, gCRT[crtid]->vfreq_min, gCRT[crtid]->vfreq_max);
			TRACE("!!! %dx%d falls outside of CRT %d's valid vertical range\n",
				mode->timing.h_display, mode->timing.v_display, crtid);
			return false;
		}
		TRACE("%dx%d is within CRT %d's valid frequency range\n",
			mode->timing.h_display, mode->timing.v_display, crtid);
	}

	return true;
}


/*
 * A quick sanity check of the provided display_mode
 */
status_t
is_mode_sane(display_mode *mode)
{
	// horizontal timing
	// validate h_sync_start is less then h_sync_end
	if (mode->timing.h_sync_start > mode->timing.h_sync_end) {
		TRACE("%s: ERROR: (%dx%d) "
			"received h_sync_start greater then h_sync_end!\n",
			__func__, mode->timing.h_display, mode->timing.v_display);
		return B_ERROR;
	}
	// validate h_total is greater then h_display
	if (mode->timing.h_total < mode->timing.h_display) {
		TRACE("%s: ERROR: (%dx%d) "
			"received h_total greater then h_display!\n",
			__func__, mode->timing.h_display, mode->timing.v_display);
		return B_ERROR;
	}

	// vertical timing
	// validate v_start is less then v_end
	if (mode->timing.v_sync_start > mode->timing.v_sync_end) {
		TRACE("%s: ERROR: (%dx%d) "
			"received v_sync_start greater then v_sync_end!\n",
			__func__, mode->timing.h_display, mode->timing.v_display);
		return B_ERROR;
	}
	// validate v_total is greater then v_display
	if (mode->timing.v_total < mode->timing.v_display) {
		TRACE("%s: ERROR: (%dx%d) "
			"received v_total greater then v_display!\n",
			__func__, mode->timing.h_display, mode->timing.v_display);
		return B_ERROR;
	}

	// calculate refresh rate for given timings to whole int (in Hz)
	int refresh = mode->timing.pixel_clock * 1000
		/ (mode->timing.h_total * mode->timing.v_total);

	if (refresh < 30 || refresh > 250) {
		TRACE("%s: ERROR: (%dx%d) "
			"refresh rate of %dHz is unlikely for any kind of monitor!\n",
			__func__, mode->timing.h_display, mode->timing.v_display, refresh);
		return B_ERROR;
	}

	return B_OK;
}


// TODO : Move to a new "monitors.c" file
status_t
detect_crt_ranges()
{
	edid1_info *edid = &gInfo->shared_info->edid_info;

	int crtid = 0;
		// edid indexes are not in order

	for (uint32 index = 0; index < MAX_CRT; index++) {

		edid1_detailed_monitor *monitor
				= &edid->detailed_monitor[index];

		if (monitor->monitor_desc_type
			== EDID1_MONITOR_RANGES) {
			edid1_monitor_range range = monitor->data.monitor_range;
			gCRT[crtid]->vfreq_min = range.min_v;	/* in Hz */
			gCRT[crtid]->vfreq_max = range.max_v;
			gCRT[crtid]->hfreq_min = range.min_h;	/* in kHz */
			gCRT[crtid]->hfreq_max = range.max_h;
			TRACE("CRT %d : v_min %d : v_max %d : h_min %d : h_max %d\n",
				crtid, gCRT[crtid]->vfreq_min, gCRT[crtid]->vfreq_max,
				gCRT[crtid]->hfreq_min, gCRT[crtid]->hfreq_max);
			crtid++;
		}

	}
	return B_OK;
}
