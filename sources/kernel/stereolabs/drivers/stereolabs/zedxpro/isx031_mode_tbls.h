/*
 * isx031_mode_tbls.h - isx031 sensor mode tables
 *
 * Copyright (c) 2018-2019, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef __ISX031_I2C_TABLES__
#define __ISX031_I2C_TABLES__

#include <media/camera_common.h>

#define ISX031_TABLE_WAIT_MS	0xff00
#define ISX031_TABLE_END	0xff01
#define ISX031_MAX_RETRIES	3
#define ISX031_WAIT_MS_STOP	1
#define ISX031_WAIT_MS_START	30
#define ISX031_WAIT_MS_STREAM	210
#define ISX031_GAIN_TABLE_SIZE 255

#define zedxpro_reg struct reg_8

static zedxpro_reg zedxpro_start[] = {
	{0xBF14, 0x02},
	{ISX031_TABLE_WAIT_MS, ISX031_WAIT_MS_START},
	{0x8AF0, 0x02},// External pulsed based sync, default  is internal sync (= x00)
	{0xBF14, 0x02},//redondency
	{ISX031_TABLE_WAIT_MS, ISX031_WAIT_MS_START},
	{0x8AF1, 0x00},// Level detection automode off, default is on (= x01)
	{ISX031_TABLE_WAIT_MS, ISX031_WAIT_MS_START},
	{0x8AFE, 0x00},// FSYNC pin is used for trigger operations
	{ISX031_TABLE_WAIT_MS, ISX031_WAIT_MS_START},
	{0x8AFF, 0xFF},// All gpios and FSYNC are input
	//{ISX031_TABLE_WAIT_MS, ISX031_WAIT_MS_START},
	//{0x0153, 0x00},
	{ISX031_TABLE_WAIT_MS, ISX031_WAIT_MS_START},
	{0x8A01, 0x00},
	{ISX031_TABLE_WAIT_MS, ISX031_WAIT_MS_START},
	{ISX031_TABLE_WAIT_MS, ISX031_WAIT_MS_START},
	{ISX031_TABLE_WAIT_MS, ISX031_WAIT_MS_START},
	{ISX031_TABLE_WAIT_MS, ISX031_WAIT_MS_START},
	{ISX031_TABLE_WAIT_MS, ISX031_WAIT_MS_START},
	{ISX031_TABLE_WAIT_MS, ISX031_WAIT_MS_START},
	{0x8A00, 0x17}, // Mode 23: 30 FPS, 4 lanes
	{ISX031_TABLE_WAIT_MS, ISX031_WAIT_MS_START},
	{ISX031_TABLE_WAIT_MS, ISX031_WAIT_MS_START},
	{ISX031_TABLE_WAIT_MS, ISX031_WAIT_MS_START},
	{ISX031_TABLE_WAIT_MS, ISX031_WAIT_MS_START},
	{ISX031_TABLE_WAIT_MS, ISX031_WAIT_MS_START},
	{0x8A01, 0x80},
	{ISX031_TABLE_WAIT_MS, ISX031_WAIT_MS_START},
	
	{ ISX031_TABLE_END, 0x00}
};

static zedxpro_reg zedxpro_stop[] = {
	//{0x301A, 0x0018},

	{ISX031_TABLE_END, 0x00}
};

//1928x1208@30fps 4lane
static zedxpro_reg zedxpro_1920x1080_crop_30fps[] = {
	{ISX031_TABLE_END, 0x00}
};

static zedxpro_reg zedxpro_1920x1536_15fps[] = {
	{ISX031_TABLE_END, 0x00}
};


enum {
	ISX031_MODE_1920X1080_CROP_30FPS,
	ISX031_MODE_1920x1536_15FPS,
	ISX031_MODE_START_STREAM,
	ISX031_MODE_STOP_STREAM,
};

static zedxpro_reg *mode_table[] = {
	[ISX031_MODE_1920X1080_CROP_30FPS]
		= zedxpro_1920x1080_crop_30fps,
	[ISX031_MODE_1920x1536_15FPS]
		= zedxpro_1920x1536_15fps,
	[ISX031_MODE_START_STREAM]
		= zedxpro_start,
	[ISX031_MODE_STOP_STREAM]
		= zedxpro_stop,
};

static const int zedxpro_7fps[] = {
	7,
};

static const int zedxpro_15fps[] = {
	15,
};

static const int zedxpro_30fps[] = {
	30,
};

static const int zedxpro_60fps[] = {
	60,
};

static const int zedxpro_120fps[] = {
	120,
};

static const struct camera_common_frmfmt zedxpro_frmfmt[] = {
	{{1920, 1536}, zedxpro_15fps, 1, 0, ISX031_MODE_1920X1080_CROP_30FPS},
};

/*
  add by dennis.jiang 2020-09-01 configuration for gmsl2
*/
// MAX9295A - Link A
// CSI Port B: 		4lanes, YUV422, VC0
// Pipe Y:			YUV422, VC0

// MAX9295A - Link B
// CSI Port B: 		4lanes, YUV422, VC0
// Pipe Z:			YUV422, VC0

// MAX9296A
// CSI Port A:		4 lanes, 1500Mbps/lane, RAW12 VC0-1
// Pipe Y:			YUV422, VC0 (Link A)
// Pipe Z:			YUV422, VC1 (Link B)

// Default Power Up States
// MAX9295A - Links A and B
// CFG0 : I2C adr 0x80
// CFG1 : GMSL2, 6Gbps, Coax

// MAX9296A
// CFG0 : I2C adr 0x90
// CFG1 : GMSL2, 6Gbps, Coax

// I2C Addresses POR (8-bit)
// MAX9295A (Link A)	: 0x80	 
// MAX9295A (Link B)	: 0x80
// MAX9296A 			: 0x90

// I2C Addresses After Configuration (8-bit)
// MAX9295A (Link A)	: 0xc0	 
// MAX9295A (Link B)	: 0x80
// MAX9296A 			: 0x90

// Cameras ISX031 image sensor 
// 0x10/4Lanes CSI2/1920x1080@30fps


#endif /* __ISX031_I2C_TABLES__ */
