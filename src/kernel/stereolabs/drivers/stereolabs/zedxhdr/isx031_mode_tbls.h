/*
 * isx031_mode_tbls.h - isx031 sensor mode tables
 *
 * Copyright (c) 2018-2019, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
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
#define ISX031_WAIT_MS	30
#define ISX031_WAIT_MS_STREAM	100
#define ISX031_SENSOR_ADDR_REG 0x8a54

#define zedxhdr_reg struct reg_8

static zedxhdr_reg zedxhdr_init_table[] = {
	{0xBF14, 0x02},
	{ISX031_TABLE_WAIT_MS, ISX031_WAIT_MS},
	{0x8AF0, 0x02},// External pulsed based sync, default  is internal sync (= x00)
	{0xBF14, 0x02},//redondency
	{ISX031_TABLE_WAIT_MS, ISX031_WAIT_MS},
	{0x8AF1, 0x00},// Level detection automode off, default is on (= x01)
	{ISX031_TABLE_WAIT_MS, ISX031_WAIT_MS},
	{0x8AFE, 0x00},// FSYNC pin is used for trigger operations
	{ISX031_TABLE_WAIT_MS, ISX031_WAIT_MS},
	{0x8AFF, 0xFF},// All gpios and FSYNC are input
	//{ISX031_TABLE_WAIT_MS, ISX031_WAIT_MS_START},
	//{0x0153, 0x00},
	{ISX031_TABLE_WAIT_MS, ISX031_WAIT_MS},
	{0x8A00, 0x17}, // Mode 23: 30 FPS, 4 lanes
	{ ISX031_TABLE_END, 0x00}
};

static zedxhdr_reg zedxhdr_start[] = {
	{0x8A01, 0x80},
	{ISX031_TABLE_WAIT_MS, ISX031_WAIT_MS_STREAM},
	{ISX031_TABLE_END, 0x00}
};

static zedxhdr_reg zedxhdr_stop[] = {
	{0x8A01, 0x00},
	{ISX031_TABLE_WAIT_MS, ISX031_WAIT_MS_STREAM},
	{ISX031_TABLE_END, 0x00}
};

// 1920x1080@30fps 4lane
static zedxhdr_reg zedxhdr_1920x1080_crop_30fps[] = {
	{0x8AA8, 0x01}, // CROP ON
	{0x8AAA, 0x80}, // HSIZE LSB
	{0x8AAB, 0x07}, // HSIZE MSB
	{0x8AAC, 0x00}, // HOFFSET LSB
	{0x8AAD, 0x00}, // HOFFSET MSB
	{0x8AAE, 0x38}, // VSIZE LSB
	{0x8AAF, 0x04}, // VSIZE MSB
	{0x8AB0, 0xE4}, // VOFFSET LSB
	{0x8AB1, 0x00}, // VOFFSET MSB

	{0x8ADA, 0x01}, // CROP DATA SEL

	{0xBF04, 0x01}, // CROP ON - Application Lock
	{0xBF06, 0x80}, // HSIZE LSB - Application Lock
	{0xBF07, 0x07}, // HSIZE MSB - Application Lock
	{0xBF08, 0x00}, // HOFFSET LSB - Application Lock
	{0xBF09, 0x00}, // HOFFSET MSB - Application Lock
	{0xBF0A, 0x38}, // VSIZE LSB - Application Lock
	{0xBF0B, 0x04}, // VSIZE MSB - Application Lock
	{0xBF0C, 0xE4}, // VOFFSET LSB - Application Lock
	{0xBF0D, 0x00}, // VOFFSET MSB - Application Lock

	{0x8A01, 0x00}, // Transition to start-up state, this appear to be necessary for the first opening to work properly
	{ISX031_TABLE_WAIT_MS, ISX031_WAIT_MS_STREAM},
	{ISX031_TABLE_END, 0x00}
};

// 960x600@30fps 4lane
static zedxhdr_reg zedxhdr_960x600_crop_30fps[] = {
	{0x8AA8, 0x01}, // CROP ON
	{0x8AAA, 0xC0}, // HSIZE LSB
	{0x8AAB, 0x03}, // HSIZE MSB
	{0x8AAC, 0xE0}, // HOFFSET LSB
	{0x8AAD, 0x01}, // HOFFSET MSB
	{0x8AAE, 0x58}, // VSIZE LSB
	{0x8AAF, 0x02}, // VSIZE MSB
	{0x8AB0, 0xD4}, // VOFFSET LSB
	{0x8AB1, 0x01}, // VOFFSET MSB

	{0x8ADA, 0x01}, // CROP DATA SEL

	{0xBF04, 0x01}, // CROP ON - Application Lock
	{0xBF06, 0xC0}, // HSIZE LSB - Application Lock
	{0xBF07, 0x03}, // HSIZE MSB - Application Lock
	{0xBF08, 0xE0}, // HOFFSET LSB - Application Lock
	{0xBF09, 0x01}, // HOFFSET MSB - Application Lock
	{0xBF0A, 0x58}, // VSIZE LSB - Application Lock
	{0xBF0B, 0x02}, // VSIZE MSB - Application Lock
	{0xBF0C, 0xD4}, // VOFFSET LSB - Application Lock
	{0xBF0D, 0x01}, // VOFFSET MSB - Application Lock

	{0x8A01, 0x00}, // Transition to start-up state, this appear to be necessary for the first opening to work properly
	{ISX031_TABLE_WAIT_MS, ISX031_WAIT_MS_STREAM},
	{ISX031_TABLE_END, 0x00}
};

// 1280x720@30fps 4lane
static zedxhdr_reg zedxhdr_1280x720_crop_30fps[] = {
	{0x8AA8, 0x01}, // CROP ON
	{0x8AAA, 0x00}, // HSIZE LSB
	{0x8AAB, 0x05}, // HSIZE MSB
	{0x8AAC, 0x40}, // HOFFSET LSB
	{0x8AAD, 0x01}, // HOFFSET MSB
	{0x8AAE, 0xD0}, // VSIZE LSB
	{0x8AAF, 0x02}, // VSIZE MSB
	{0x8AB0, 0x98}, // VOFFSET LSB
	{0x8AB1, 0x01}, // VOFFSET MSB

	{0x8ADA, 0x01}, // CROP DATA SEL

	{0xBF04, 0x01}, // CROP ON - Application Lock
	{0xBF06, 0x00}, // HSIZE LSB - Application Lock
	{0xBF07, 0x05}, // HSIZE MSB - Application Lock
	{0xBF08, 0x40}, // HOFFSET LSB - Application Lock
	{0xBF09, 0x01}, // HOFFSET MSB - Application Lock
	{0xBF0A, 0xD0}, // VSIZE LSB - Application Lock
	{0xBF0B, 0x02}, // VSIZE MSB - Application Lock
	{0xBF0C, 0x98}, // VOFFSET LSB - Application Lock
	{0xBF0D, 0x01}, // VOFFSET MSB - Application Lock

	{0x8A01, 0x00}, // Transition to start-up state, this appear to be necessary for the first opening to work properly
	{ISX031_TABLE_WAIT_MS, ISX031_WAIT_MS_STREAM},
	{ISX031_TABLE_END, 0x00}
};

// 1920x1536@30fps 4lane
static zedxhdr_reg zedxhdr_1920x1536_30fps[] = {
	{0x8AA8, 0x00}, // CROP OFF
	{0xBF04, 0x00}, // CROP OFF - Application Lock
	{0x8A01, 0x00}, // Transition to start-up state, this appear to be necessary for the first opening to work properly
	{ISX031_TABLE_WAIT_MS, ISX031_WAIT_MS_STREAM},
	{ISX031_TABLE_END, 0x00}
};

enum {
	ISX031_MODE_1920X1536_30FPS,
	ISX031_MODE_1920X1080_CROP_30FPS,
	ISX031_MODE_960X600_30FPS,
	ISX031_MODE_1280X720_30FPS,
	ISX031_MODE_START_STREAM,
	ISX031_MODE_STOP_STREAM,
	ISX031_MODE_INIT,
};

static zedxhdr_reg *mode_table[] = {
	[ISX031_MODE_1920X1536_30FPS]
		= zedxhdr_1920x1536_30fps,
	[ISX031_MODE_1920X1080_CROP_30FPS]
		= zedxhdr_1920x1080_crop_30fps,
	[ISX031_MODE_960X600_30FPS]
		= zedxhdr_960x600_crop_30fps,
	[ISX031_MODE_1280X720_30FPS]
		= zedxhdr_1280x720_crop_30fps,
	[ISX031_MODE_START_STREAM]
		= zedxhdr_start,
	[ISX031_MODE_STOP_STREAM]
		= zedxhdr_stop,
	[ISX031_MODE_INIT]
		= zedxhdr_init_table,
};

static const int zedxhdr_7fps[] = {
	7,
};

static const int zedxhdr_15fps[] = {
	15,
};

static const int zedxhdr_30fps[] = {
	30,
};

static const int zedxhdr_60fps[] = {
	60,
};

static const int zedxhdr_120fps[] = {
	120,
};

static const struct camera_common_frmfmt zedxhdr_frmfmt[] = {
	{{1920, 1536}, zedxhdr_30fps, 1, 0, ISX031_MODE_1920X1536_30FPS},
	{{1920, 1080}, zedxhdr_30fps, 1, 0, ISX031_MODE_1920X1080_CROP_30FPS},
	{{960, 600}, zedxhdr_30fps, 1, 0, ISX031_MODE_960X600_30FPS},
	{{1280, 720}, zedxhdr_30fps, 1, 0, ISX031_MODE_1280X720_30FPS},
};

#endif /* __ISX031_I2C_TABLES__ */