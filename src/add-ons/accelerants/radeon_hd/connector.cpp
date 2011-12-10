/*
 * Copyright 2006-2011, Haiku, Inc. All Rights Reserved.
 * Distributed under the terms of the MIT License.
 *
 * Authors:
 *      Alexander von Gluck, kallisti5@unixzen.com
 */


#include "connector.h"

#include <Debug.h>

#include "accelerant_protos.h"
#include "accelerant.h"
#include "bios.h"
#include "encoder.h"
#include "gpu.h"
#include "utility.h"


#undef TRACE

#define TRACE_CONNECTOR
#ifdef TRACE_CONNECTOR
#   define TRACE(x...) _sPrintf("radeon_hd: " x)
#else
#   define TRACE(x...) ;
#endif

#define ERROR(x...) _sPrintf("radeon_hd: " x)


union aux_channel_transaction {
	PROCESS_AUX_CHANNEL_TRANSACTION_PS_ALLOCATION v1;
	PROCESS_AUX_CHANNEL_TRANSACTION_PARAMETERS_V2 v2;
};


static int
dp_aux_speak(uint32 hwLine, uint8* send, int sendBytes,
	uint8* recv, int recvBytes, uint8 delay, uint8* ack)
{
	if (hwLine == 0) {
		ERROR("%s: cannot speak on invalid GPIO pin!\n", __func__);
		return B_IO_ERROR;
	}

	union aux_channel_transaction args;
	int index = GetIndexIntoMasterTable(COMMAND, ProcessAuxChannelTransaction);

	memset(&args, 0, sizeof(args));

	unsigned char* base = (unsigned char*)gAtomContext->scratch;
	memcpy(base, send, sendBytes);

	args.v1.lpAuxRequest = 0;
	args.v1.lpDataOut = 16;
	args.v1.ucDataOutLen = 0;
	args.v1.ucChannelID = hwLine;
	args.v1.ucDelay = delay / 10;

	//if (ASIC_IS_DCE4(rdev))
	//	args.v2.ucHPD_ID = chan->rec.hpd;

	atom_execute_table(gAtomContext, index, (uint32*)&args);

	*ack = args.v1.ucReplyStatus;

	switch(args.v1.ucReplyStatus) {
		case 1:
			ERROR("%s: dp_aux_ch timeout!\n", __func__);
			return B_TIMED_OUT;
		case 2:
			ERROR("%s: dp_aux_ch flags not zero!\n", __func__);
			return B_BUSY;
		case 3:
			ERROR("%s: dp_aux_ch error!\n", __func__);
			return B_IO_ERROR;
	}

	int recvLength = args.v1.ucDataOutLen;
	if (recvLength > recvBytes)
		recvLength = recvBytes;

	if (recv && recvBytes)
		memcpy(recv, base + 16, recvLength);

	return recvLength;
}


int
dp_aux_write(uint32 hwLine, uint16 address,
	uint8* send, uint8 sendBytes, uint8 delay)
{
	uint8 auxMessage[20];
	int auxMessageBytes = sendBytes + 4;

	if (sendBytes > 16)
		return -1;

	auxMessage[0] = address;
	auxMessage[1] = address >> 8;
	auxMessage[2] = AUX_NATIVE_WRITE << 4;
	auxMessage[3] = (auxMessageBytes << 4) | (sendBytes - 1);
	memcpy(&auxMessage[4], send, sendBytes);

	uint8 retry;
	for (retry = 0; retry < 4; retry++) {
		uint8 ack;
		int result = dp_aux_speak(hwLine, auxMessage, auxMessageBytes,
			NULL, 0, delay, &ack);

		if (result == B_BUSY)
			continue;
		else if (result < B_OK)
			return result;

		if ((ack & AUX_NATIVE_REPLY_MASK) == AUX_NATIVE_REPLY_ACK)
			return sendBytes;
		else if ((ack & AUX_NATIVE_REPLY_MASK) == AUX_NATIVE_REPLY_DEFER)
			snooze(400);
		else
			return B_IO_ERROR;
	}

	return B_IO_ERROR;
}


int
dp_aux_read(uint32 hwLine, uint16 address,
	uint8* recv, int recvBytes, uint8 delay)
{
	uint8 auxMessage[4];
	int auxMessageBytes = 4;

	auxMessage[0] = address;
	auxMessage[1] = address >> 8;
	auxMessage[2] = AUX_NATIVE_READ << 4;
	auxMessage[3] = (auxMessageBytes << 4) | (recvBytes - 1);

	uint8 retry;
	for (retry = 0; retry < 4; retry++) {
		uint8 ack;
		int result = dp_aux_speak(hwLine, auxMessage, auxMessageBytes,
			recv, recvBytes, delay, &ack);

		if (result == B_BUSY)
			continue;
		else if (result < B_OK)
			return result;

		if ((ack & AUX_NATIVE_REPLY_MASK) == AUX_NATIVE_REPLY_ACK)
			return result;
		else if ((ack & AUX_NATIVE_REPLY_MASK) == AUX_NATIVE_REPLY_DEFER)
			snooze(400);
		else
			return B_IO_ERROR;
	}

	return B_IO_ERROR;
}


status_t
dp_aux_get_i2c_byte(uint32 hwLine, uint16 address, uint8* data, bool end)
{
	uint8 auxMessage[5];
	int auxMessageBytes = 4; // 4 for read

	/* Set up the command byte */
	auxMessage[2] = AUX_I2C_READ << 4;
	if (end == false)
		auxMessage[2] |= AUX_I2C_MOT << 4;

	auxMessage[0] = address;
	auxMessage[1] = address >> 8;

	auxMessage[3] = auxMessageBytes << 4;

	int retry;
	for (retry = 0; retry < 4; retry++) {
		uint8 ack;
		uint8 reply[2];
		int replyBytes = 1;

		int result = dp_aux_speak(hwLine, auxMessage, auxMessageBytes,
			reply, replyBytes, 0, &ack);
		if (result == B_BUSY)
			continue;
		else if (result < 0) {
			ERROR("%s: aux_ch failed: %d\n", __func__, result);
			return B_ERROR;
		}

		switch (ack & AUX_NATIVE_REPLY_MASK) {
			case AUX_NATIVE_REPLY_ACK:
				// I2C-over-AUX Reply field is only valid for AUX_ACK
				break;
			case AUX_NATIVE_REPLY_NACK:
				TRACE("%s: aux_ch native nack\n", __func__);
				return B_IO_ERROR;
			case AUX_NATIVE_REPLY_DEFER:
				TRACE("%s: aux_ch native defer\n", __func__);
				snooze(400);
				continue;
			default:
				TRACE("%s: aux_ch invalid native reply: 0x%02x\n",
					__func__, ack);
				return B_ERROR;
		}

		switch (ack & AUX_I2C_REPLY_MASK) {
			case AUX_I2C_REPLY_ACK:
				*data = reply[0];
				return B_OK;
			case AUX_I2C_REPLY_NACK:
				TRACE("%s: aux_i2c nack\n", __func__);
				return B_IO_ERROR;
			case AUX_I2C_REPLY_DEFER:
				TRACE("%s: aux_i2c defer\n", __func__);
				snooze(400);
				break;
			default:
				TRACE("%s: aux_i2c invalid native reply: 0x%02x\n",
					__func__, ack);
				return B_ERROR;
		}
	}

	TRACE("%s: aux i2c too many retries, giving up.\n", __func__);
	return B_ERROR;
}


status_t
dp_aux_set_i2c_byte(uint32 hwLine, uint16 address, uint8* data, bool end)
{
	uint8 auxMessage[5];
	int auxMessageBytes = 5; // 5 for write

	/* Set up the command byte */
	auxMessage[2] = AUX_I2C_WRITE << 4;
	if (end == false)
		auxMessage[2] |= AUX_I2C_MOT << 4;

	auxMessage[0] = address;
	auxMessage[1] = address >> 8;

	auxMessage[3] = auxMessageBytes << 4;
	auxMessage[4] = *data;

	int retry;
	for (retry = 0; retry < 4; retry++) {
		uint8 ack;
		uint8 reply[2];
		int replyBytes = 1;

		int result = dp_aux_speak(hwLine, auxMessage, auxMessageBytes,
			reply, replyBytes, 0, &ack);
		if (result == B_BUSY)
			continue;
		else if (result < 0) {
			ERROR("%s: aux_ch failed: %d\n", __func__, result);
			return B_ERROR;
		}

		switch (ack & AUX_NATIVE_REPLY_MASK) {
			case AUX_NATIVE_REPLY_ACK:
				// I2C-over-AUX Reply field is only valid for AUX_ACK
				break;
			case AUX_NATIVE_REPLY_NACK:
				TRACE("%s: aux_ch native nack\n", __func__);
				return B_IO_ERROR;
			case AUX_NATIVE_REPLY_DEFER:
				TRACE("%s: aux_ch native defer\n", __func__);
				snooze(400);
				continue;
			default:
				TRACE("%s: aux_ch invalid native reply: 0x%02x\n",
					__func__, ack);
				return B_ERROR;
		}

		switch (ack & AUX_I2C_REPLY_MASK) {
			case AUX_I2C_REPLY_ACK:
				// Success!
				return B_OK;
			case AUX_I2C_REPLY_NACK:
				TRACE("%s: aux_i2c nack\n", __func__);
				return B_IO_ERROR;
			case AUX_I2C_REPLY_DEFER:
				TRACE("%s: aux_i2c defer\n", __func__);
				snooze(400);
				break;
			default:
				TRACE("%s: aux_i2c invalid native reply: 0x%02x\n",
					__func__, ack);
				return B_ERROR;
		}
	}

	TRACE("%s: aux i2c too many retries, giving up.\n", __func__);
	return B_OK;
}


static void
gpio_lock_i2c(void* cookie, bool lock)
{
	gpio_info* info = (gpio_info*)cookie;

	uint32 buffer = 0;

	if (lock == true) {
		// hw_capable and > DCE3
		if (info->hw_capable == true && gInfo->shared_info->dceMajor >= 3) {
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

	// mask clock GPIO pins for software use
	buffer = Read32(OUT, info->mask_scl_reg);
	if (lock == true)
		buffer |= info->mask_scl_mask;
	else
		buffer &= ~info->mask_scl_mask;

	Write32(OUT, info->mask_scl_reg, buffer);
	Read32(OUT, info->mask_scl_reg);

	// mask data GPIO pins for software use
	buffer = Read32(OUT, info->mask_sda_reg);
	if (lock == true)
		buffer |= info->mask_sda_mask;
	else
		buffer &= ~info->mask_sda_mask;

	Write32(OUT, info->mask_sda_reg, buffer);
	Read32(OUT, info->mask_sda_reg);
}


static status_t
gpio_get_i2c_bit(void* cookie, int* _clock, int* _data)
{
	gpio_info* info = (gpio_info*)cookie;

	uint32 scl = Read32(OUT, info->y_scl_reg) & info->y_scl_mask;
	uint32 sda = Read32(OUT, info->y_sda_reg) & info->y_sda_mask;

	*_clock = scl != 0;
	*_data = sda != 0;

	return B_OK;
}


static status_t
gpio_set_i2c_bit(void* cookie, int clock, int data)
{
	gpio_info* info = (gpio_info*)cookie;

	uint32 scl = Read32(OUT, info->en_scl_reg) & ~info->en_scl_mask;
	scl |= clock ? 0 : info->en_scl_mask;
	Write32(OUT, info->en_scl_reg, scl);
	Read32(OUT, info->en_scl_reg);

	uint32 sda = Read32(OUT, info->en_sda_reg) & ~info->en_sda_mask;
	sda |= data ? 0 : info->en_sda_mask;
	Write32(OUT, info->en_sda_reg, sda);
	Read32(OUT, info->en_sda_reg);

	return B_OK;
}


bool
connector_read_edid(uint32 connectorIndex, edid1_info* edid)
{
	// ensure things are sane
	uint32 gpioID = gConnector[connectorIndex]->gpioID;
	if (gGPIOInfo[gpioID]->valid == false)
		return false;

	if (gConnector[connectorIndex]->type == VIDEO_CONNECTOR_LVDS) {
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
		__func__, connectorIndex);

	return true;
}


#if 0
bool
connector_read_edid_lvds(uint32 connectorIndex, edid1_info* edid)
{
	uint8 dceMajor;
	uint8 dceMinor;
	int index = GetIndexIntoMasterTable(DATA, LVDS_Info);
	uint16 offset;

	if (atom_parse_data_header(gAtomContexg, index, NULL,
		&dceMajor, &dceMinor, &offset) == B_OK) {
		lvdsInfo = (union lvds_info*)(gAtomContext->bios + offset);

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
connector_attach_gpio(uint32 connectorIndex, uint8 hwLine)
{
	gConnector[connectorIndex]->gpioID = 0;
	for (uint32 i = 0; i < ATOM_MAX_SUPPORTED_DEVICE; i++) {
		if (gGPIOInfo[i]->hw_line != hwLine)
			continue;
		gConnector[connectorIndex]->gpioID = i;
		return B_OK;
	}

	TRACE("%s: couldn't find GPIO for connector %" B_PRIu32 "\n",
		__func__, connectorIndex);
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

	struct _ATOM_GPIO_I2C_INFO* i2c_info
		= (struct _ATOM_GPIO_I2C_INFO*)(gAtomContext->bios + tableOffset);

	uint32 numIndices = (tableSize - sizeof(ATOM_COMMON_TABLE_HEADER))
		/ sizeof(ATOM_GPIO_I2C_ASSIGMENT);

	if (numIndices > ATOM_MAX_SUPPORTED_DEVICE) {
		ERROR("%s: ERROR: AtomBIOS contains more GPIO_Info items then I"
			"was prepared for! (seen: %" B_PRIu32 "; max: %" B_PRIu32 ")\n",
			__func__, numIndices, (uint32)ATOM_MAX_SUPPORTED_DEVICE);
		return B_ERROR;
	}

	for (uint32 i = 0; i < numIndices; i++) {
		ATOM_GPIO_I2C_ASSIGMENT* gpio = &i2c_info->asGPIO_Info[i];

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
		gGPIOInfo[i]->hw_line = gpio->sucI2cId.ucAccess;
		gGPIOInfo[i]->hw_capable
			= (gpio->sucI2cId.sbfAccess.bfHW_Capable) ? true : false;

		// GPIO mask (Allows software to control the GPIO pad)
		// 0 = chip access; 1 = only software;
		gGPIOInfo[i]->mask_scl_reg
			= B_LENDIAN_TO_HOST_INT16(gpio->usClkMaskRegisterIndex) * 4;
		gGPIOInfo[i]->mask_sda_reg
			= B_LENDIAN_TO_HOST_INT16(gpio->usDataMaskRegisterIndex) * 4;
		gGPIOInfo[i]->mask_scl_mask = 1 << gpio->ucClkMaskShift;
		gGPIOInfo[i]->mask_sda_mask = 1 << gpio->ucDataMaskShift;

		// GPIO output / write (A) enable
		// 0 = GPIO input (Y); 1 = GPIO output (A);
		gGPIOInfo[i]->en_scl_reg
			= B_LENDIAN_TO_HOST_INT16(gpio->usClkEnRegisterIndex) * 4;
		gGPIOInfo[i]->en_sda_reg
			= B_LENDIAN_TO_HOST_INT16(gpio->usDataEnRegisterIndex) * 4;
		gGPIOInfo[i]->en_scl_mask = 1 << gpio->ucClkEnShift;
		gGPIOInfo[i]->en_sda_mask = 1 << gpio->ucDataEnShift;

		// GPIO output / write (A)
		gGPIOInfo[i]->a_scl_reg
			= B_LENDIAN_TO_HOST_INT16(gpio->usClkA_RegisterIndex) * 4;
		gGPIOInfo[i]->a_sda_reg
			= B_LENDIAN_TO_HOST_INT16(gpio->usDataA_RegisterIndex) * 4;
		gGPIOInfo[i]->a_scl_mask = 1 << gpio->ucClkA_Shift;
		gGPIOInfo[i]->a_sda_mask = 1 << gpio->ucDataA_Shift;

		// GPIO input / read (Y)
		gGPIOInfo[i]->y_scl_reg
			= B_LENDIAN_TO_HOST_INT16(gpio->usClkY_RegisterIndex) * 4;
		gGPIOInfo[i]->y_sda_reg
			= B_LENDIAN_TO_HOST_INT16(gpio->usDataY_RegisterIndex) * 4;
		gGPIOInfo[i]->y_scl_mask = 1 << gpio->ucClkY_Shift;
		gGPIOInfo[i]->y_sda_mask = 1 << gpio->ucDataY_Shift;

		// ensure data is valid
		gGPIOInfo[i]->valid = gGPIOInfo[i]->mask_scl_reg ? true : false;

		TRACE("%s: GPIO @ %" B_PRIu32 ", valid: %s, hw_line: 0x%" B_PRIX32 "\n",
			__func__, i, gGPIOInfo[i]->valid ? "true" : "false",
			gGPIOInfo[i]->hw_line);
	}

	return B_OK;
}


union atom_supported_devices {
	struct _ATOM_SUPPORTED_DEVICES_INFO info;
	struct _ATOM_SUPPORTED_DEVICES_INFO_2 info_2;
	struct _ATOM_SUPPORTED_DEVICES_INFO_2d1 info_2d1;
};


status_t
connector_probe_legacy()
{
	int index = GetIndexIntoMasterTable(DATA, SupportedDevicesInfo);
	uint8 tableMajor;
	uint8 tableMinor;
	uint16 tableSize;
	uint16 tableOffset;

	if (atom_parse_data_header(gAtomContext, index, &tableSize,
		&tableMajor, &tableMinor, &tableOffset) != B_OK) {
		ERROR("%s: unable to parse data header!\n", __func__);
		return B_ERROR;
	}

	union atom_supported_devices* supportedDevices;
	supportedDevices = (union atom_supported_devices*)
		(gAtomContext->bios + tableOffset);

	uint16 deviceSupport
		= B_LENDIAN_TO_HOST_INT16(supportedDevices->info.usDeviceSupport);

	uint32 maxDevice;

	if (tableMajor > 1)
		maxDevice = ATOM_MAX_SUPPORTED_DEVICE;
	else
		maxDevice = ATOM_MAX_SUPPORTED_DEVICE_INFO;

	uint32 i;
	uint32 connectorIndex = 0;
	for (i = 0; i < maxDevice; i++) {

		gConnector[connectorIndex]->valid = false;

		// check if this connector is used
		if ((deviceSupport & (1 << i)) == 0)
			continue;

		if (i == ATOM_DEVICE_CV_INDEX) {
			TRACE("%s: skipping component video\n",
				__func__);
			continue;
		}

		ATOM_CONNECTOR_INFO_I2C ci
			= supportedDevices->info.asConnInfo[i];

		gConnector[connectorIndex]->type = kConnectorConvertLegacy[
			ci.sucConnectorInfo.sbfAccess.bfConnectorType];

		if (gConnector[connectorIndex]->type == VIDEO_CONNECTOR_UNKNOWN) {
			TRACE("%s: skipping unknown connector at %" B_PRId32
				" of 0x%" B_PRIX8 "\n", __func__, i,
				ci.sucConnectorInfo.sbfAccess.bfConnectorType);
			continue;
		}

		// TODO: give tv unique connector ids

		// Always set CRT1 and CRT2 as VGA, some cards incorrectly set
		// VGA ports as DVI
		if (i == ATOM_DEVICE_CRT1_INDEX || i == ATOM_DEVICE_CRT2_INDEX)
			gConnector[connectorIndex]->type = VIDEO_CONNECTOR_VGA;

		uint8 dac = ci.sucConnectorInfo.sbfAccess.bfAssociatedDAC;
		uint32 encoderObject = encoder_object_lookup((1 << i), dac);
		uint32 encoderID = (encoderObject & OBJECT_ID_MASK) >> OBJECT_ID_SHIFT;

		gConnector[connectorIndex]->valid = true;
		gConnector[connectorIndex]->encoder.flags = (1 << i);
		gConnector[connectorIndex]->encoder.valid = true;
		gConnector[connectorIndex]->encoder.objectID = encoderID;
		gConnector[connectorIndex]->encoder.type
			= encoder_type_lookup(encoderID, (1 << i));
		gConnector[connectorIndex]->encoder.isExternal
			= encoder_is_external(encoderID);

		connector_attach_gpio(connectorIndex, ci.sucI2cId.ucAccess);

		pll_limit_probe(&gConnector[connectorIndex]->encoder.pll);

		connectorIndex++;
	}

	// TODO: combine shared connectors

	for (i = 0; i < maxDevice; i++) {
		if (gConnector[i]->valid == true) {
			TRACE("%s: connector #%" B_PRId32 " is %s\n", __func__, i,
				get_connector_name(gConnector[i]->type));
		}
	}

	if (connectorIndex == 0) {
		TRACE("%s: zero connectors found using legacy detection\n", __func__);
		return B_ERROR;
	}

	return B_OK;
}


// r600+
status_t
connector_probe()
{
	int index = GetIndexIntoMasterTable(DATA, Object_Header);
	uint8 tableMajor;
	uint8 tableMinor;
	uint16 tableSize;
	uint16 tableOffset;

	if (atom_parse_data_header(gAtomContext, index, &tableSize,
		&tableMajor, &tableMinor, &tableOffset) != B_OK) {
		ERROR("%s: ERROR: parsing data header failed!\n", __func__);
		return B_ERROR;
	}

	if (tableMinor < 2) {
		ERROR("%s: ERROR: table minor version unknown! "
			"(%" B_PRIu8 ".%" B_PRIu8 ")\n", __func__, tableMajor, tableMinor);
		return B_ERROR;
	}

	ATOM_CONNECTOR_OBJECT_TABLE* con_obj;
	ATOM_ENCODER_OBJECT_TABLE* enc_obj;
	ATOM_OBJECT_TABLE* router_obj;
	ATOM_DISPLAY_OBJECT_PATH_TABLE* path_obj;
	ATOM_OBJECT_HEADER* obj_header;

	obj_header = (ATOM_OBJECT_HEADER*)(gAtomContext->bios + tableOffset);
	path_obj = (ATOM_DISPLAY_OBJECT_PATH_TABLE*)
		(gAtomContext->bios + tableOffset
		+ B_LENDIAN_TO_HOST_INT16(obj_header->usDisplayPathTableOffset));
	con_obj = (ATOM_CONNECTOR_OBJECT_TABLE*)
		(gAtomContext->bios + tableOffset
		+ B_LENDIAN_TO_HOST_INT16(obj_header->usConnectorObjectTableOffset));
	enc_obj = (ATOM_ENCODER_OBJECT_TABLE*)
		(gAtomContext->bios + tableOffset
		+ B_LENDIAN_TO_HOST_INT16(obj_header->usEncoderObjectTableOffset));
	router_obj = (ATOM_OBJECT_TABLE*)
		(gAtomContext->bios + tableOffset
		+ B_LENDIAN_TO_HOST_INT16(obj_header->usRouterObjectTableOffset));
	int deviceSupport = B_LENDIAN_TO_HOST_INT16(obj_header->usDeviceSupport);

	int pathSize = 0;
	int32 i = 0;

	TRACE("%s: found %" B_PRIu8 " potential display paths.\n", __func__,
		path_obj->ucNumOfDispPath);

	uint32 connectorIndex = 0;
	for (i = 0; i < path_obj->ucNumOfDispPath; i++) {

		if (connectorIndex >= ATOM_MAX_SUPPORTED_DEVICE)
			continue;

		uint8* addr = (uint8*)path_obj->asDispPath;
		ATOM_DISPLAY_OBJECT_PATH* path;
		addr += pathSize;
		path = (ATOM_DISPLAY_OBJECT_PATH*)addr;
		pathSize += B_LENDIAN_TO_HOST_INT16(path->usSize);

		uint32 connectorType;
		uint16 connectorObjectID;
		uint16 connectorFlags = B_LENDIAN_TO_HOST_INT16(path->usDeviceTag);

		if ((deviceSupport & connectorFlags) != 0) {
			uint8 con_obj_id = (B_LENDIAN_TO_HOST_INT16(path->usConnObjectId)
				& OBJECT_ID_MASK) >> OBJECT_ID_SHIFT;

			//uint8 con_obj_num
			//	= (B_LENDIAN_TO_HOST_INT16(path->usConnObjectId)
			//	& ENUM_ID_MASK) >> ENUM_ID_SHIFT;
			//uint8 con_obj_type
			//	= (B_LENDIAN_TO_HOST_INT16(path->usConnObjectId)
			//	& OBJECT_TYPE_MASK) >> OBJECT_TYPE_SHIFT;

			if (connectorFlags == ATOM_DEVICE_CV_SUPPORT) {
				TRACE("%s: Path #%" B_PRId32 ": skipping component video.\n",
					__func__, i);
				continue;
			}

			radeon_shared_info &info = *gInfo->shared_info;

			uint16 igpLaneInfo;
			if ((info.chipsetFlags & CHIP_IGP) != 0)
				ERROR("%s: TODO: IGP chip connector detection\n", __func__);
				// try non-IGP method for now
				igpLaneInfo = 0;
				connectorType = kConnectorConvert[con_obj_id];
				connectorObjectID = con_obj_id;
			else {
				igpLaneInfo = 0;
				connectorType = kConnectorConvert[con_obj_id];
				connectorObjectID = con_obj_id;
			}

			if (connectorType == VIDEO_CONNECTOR_UNKNOWN) {
				ERROR("%s: Path #%" B_PRId32 ": skipping unknown connector.\n",
					__func__, i);
				continue;
			}

			int32 j;
			for (j = 0; j < ((B_LENDIAN_TO_HOST_INT16(path->usSize) - 8) / 2);
				j++) {
				//uint16 grph_obj_id
				//	= (B_LENDIAN_TO_HOST_INT16(path->usGraphicObjIds[j])
				//	& OBJECT_ID_MASK) >> OBJECT_ID_SHIFT;
				//uint8 grph_obj_num
				//	= (B_LENDIAN_TO_HOST_INT16(path->usGraphicObjIds[j]) &
				//	ENUM_ID_MASK) >> ENUM_ID_SHIFT;
				uint8 grph_obj_type
					= (B_LENDIAN_TO_HOST_INT16(path->usGraphicObjIds[j]) &
					OBJECT_TYPE_MASK) >> OBJECT_TYPE_SHIFT;

				if (grph_obj_type == GRAPH_OBJECT_TYPE_ENCODER) {
					// Found an encoder
					// TODO: it may be possible to have more then one encoder
					int32 k;
					for (k = 0; k < enc_obj->ucNumberOfObjects; k++) {
						uint16 encoder_obj
							= B_LENDIAN_TO_HOST_INT16(
							enc_obj->asObjects[k].usObjectID);
						if (B_LENDIAN_TO_HOST_INT16(path->usGraphicObjIds[j])
							== encoder_obj) {
							ATOM_COMMON_RECORD_HEADER* record
								= (ATOM_COMMON_RECORD_HEADER*)
								((uint16*)gAtomContext->bios + tableOffset
								+ B_LENDIAN_TO_HOST_INT16(
								enc_obj->asObjects[k].usRecordOffset));
							ATOM_ENCODER_CAP_RECORD* cap_record;
							uint16 caps = 0;
							while (record->ucRecordSize > 0
								&& record->ucRecordType > 0
								&& record->ucRecordType
								<= ATOM_MAX_OBJECT_RECORD_NUMBER) {
								switch (record->ucRecordType) {
									case ATOM_ENCODER_CAP_RECORD_TYPE:
										cap_record = (ATOM_ENCODER_CAP_RECORD*)
											record;
										caps = B_LENDIAN_TO_HOST_INT16(
											cap_record->usEncoderCap);
										break;
								}
								record = (ATOM_COMMON_RECORD_HEADER*)
									((char*)record + record->ucRecordSize);
							}

							uint32 encoderID = (encoder_obj & OBJECT_ID_MASK)
								>> OBJECT_ID_SHIFT;

							uint32 encoderType = encoder_type_lookup(encoderID,
								connectorFlags);

							if (encoderType == VIDEO_ENCODER_NONE) {
								ERROR("%s: Path #%" B_PRId32 ":"
									"skipping unknown encoder.\n",
									__func__, i);
								continue;
							}

							// Set up encoder on connector if valid
							TRACE("%s: Path #%" B_PRId32 ": Found encoder "
								"%s\n", __func__, i,
								get_encoder_name(encoderType));

							gConnector[connectorIndex]->encoder.valid
								= true;
							gConnector[connectorIndex]->encoder.flags
								= connectorFlags;
							gConnector[connectorIndex]->encoder.objectID
								= encoderID;
							gConnector[connectorIndex]->encoder.type
								= encoderType;
							gConnector[connectorIndex]->encoder.isExternal
								= encoder_is_external(encoderID);
							gConnector[connectorIndex]->encoder.isDPBridge
								= encoder_is_dp_bridge(encoderID);

							pll_limit_probe(
								&gConnector[connectorIndex]->encoder.pll);
						}
					}
					// END if object is encoder
				} else if (grph_obj_type == GRAPH_OBJECT_TYPE_ROUTER) {
					ERROR("%s: TODO: Found router object?\n", __func__);
				} // END if object is router
			}

			// Set up information buses such as ddc
			if ((connectorFlags
				& (ATOM_DEVICE_TV_SUPPORT | ATOM_DEVICE_CV_SUPPORT)) == 0) {
				for (j = 0; j < con_obj->ucNumberOfObjects; j++) {
					if (B_LENDIAN_TO_HOST_INT16(path->usConnObjectId)
						== B_LENDIAN_TO_HOST_INT16(
						con_obj->asObjects[j].usObjectID)) {
						ATOM_COMMON_RECORD_HEADER* record
							= (ATOM_COMMON_RECORD_HEADER*)(gAtomContext->bios
							+ tableOffset + B_LENDIAN_TO_HOST_INT16(
							con_obj->asObjects[j].usRecordOffset));
						while (record->ucRecordSize > 0
							&& record->ucRecordType > 0
							&& record->ucRecordType
								<= ATOM_MAX_OBJECT_RECORD_NUMBER) {
							ATOM_I2C_RECORD* i2c_record;
							ATOM_I2C_ID_CONFIG_ACCESS* i2c_config;
							//ATOM_HPD_INT_RECORD* hpd_record;

							switch (record->ucRecordType) {
								case ATOM_I2C_RECORD_TYPE:
									i2c_record
										= (ATOM_I2C_RECORD*)record;
									i2c_config
										= (ATOM_I2C_ID_CONFIG_ACCESS*)
										&i2c_record->sucI2cId;
									// attach i2c gpio information for connector
									connector_attach_gpio(connectorIndex,
										i2c_config->ucAccess);
									break;
								case ATOM_HPD_INT_RECORD_TYPE:
									// TODO: HPD (Hot Plug)
									break;
							}

							// move to next record
							record = (ATOM_COMMON_RECORD_HEADER*)
								((char*)record + record->ucRecordSize);
						}
					}
				}
			}

			// TODO: aux chan transactions

			// record connector information
			TRACE("%s: Path #%" B_PRId32 ": Found %s (0x%" B_PRIX32 ")\n",
				__func__, i, get_connector_name(connectorType),
				connectorType);

			gConnector[connectorIndex]->valid = true;
			gConnector[connectorIndex]->flags = connectorFlags;
			gConnector[connectorIndex]->type = connectorType;
			gConnector[connectorIndex]->objectID = connectorObjectID;

			gConnector[connectorIndex]->encoder.isTV = false;
			gConnector[connectorIndex]->encoder.isHDMI = false;

			switch(connectorType) {
				case VIDEO_CONNECTOR_COMPOSITE:
				case VIDEO_CONNECTOR_SVIDEO:
				case VIDEO_CONNECTOR_9DIN:
					gConnector[connectorIndex]->encoder.isTV = true;
					break;
				case VIDEO_CONNECTOR_HDMIA:
				case VIDEO_CONNECTOR_HDMIB:
					gConnector[connectorIndex]->encoder.isHDMI = true;
					break;
			}

			if (gConnector[connectorIndex]->encoder.isDPBridge == true) {
				TRACE("%s: is bridge, performing bridge DDC setup\n", __func__);
				encoder_external_setup(connectorIndex, 0,
					EXTERNAL_ENCODER_ACTION_V3_DDC_SETUP);
			}

			connectorIndex++;
		} // END for each valid connector
	} // end for each display path

	return B_OK;
}


void
debug_connectors()
{
	ERROR("Currently detected connectors=============\n");
	for (uint32 id = 0; id < ATOM_MAX_SUPPORTED_DEVICE; id++) {
		if (gConnector[id]->valid == true) {
			uint32 connectorType = gConnector[id]->type;
			uint32 encoderType = gConnector[id]->encoder.type;
			uint16 encoderID = gConnector[id]->encoder.objectID;
			uint16 gpioID = gConnector[id]->gpioID;
			ERROR("Connector #%" B_PRIu32 ")\n", id);
			ERROR(" + connector:  %s\n", get_connector_name(connectorType));
			ERROR(" + encoder:    %s\n", get_encoder_name(encoderType));
			ERROR(" + encoder id: %" B_PRIu16 " (%s)\n", encoderID,
				encoder_name_lookup(encoderID));
			ERROR(" + gpio id:    %" B_PRIu16 "\n", gpioID);
			ERROR(" + gpio valid: %s\n",
				gGPIOInfo[gpioID]->valid ? "true" : "false");
			ERROR(" + hw line:    0x%" B_PRIX32 "\n",
				gGPIOInfo[gpioID]->hw_line);
		}
	}
	ERROR("==========================================\n");
}
