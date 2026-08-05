/**
 * zedx_mode_tbls.h - zedx sensor mode tables
 *
 * Copyright (c) 2022-2023, Stereolabs.  All rights reserved.
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
#ifndef __ZEDX_I2C_TABLES__
#define __ZEDX_I2C_TABLES__

#include <media/camera_common.h>

/// Driver Version ///
#define ZEDX_DRIVER_VERSION_MAJOR 1
#define ZEDX_DRIVER_VERSION_MINOR 4
#define ZEDX_DRIVER_VERSION_PATCH 3

// Default is 60000000Mhz pixel clock
// #define CONFIG_90MHz
// #define CONFIG_8BIT
// #define CONFIG_81MHz

// ZED X Parameters //
#define N_ZEDX 16
#define MAX_N_GMSL_PORT 4

/// Driver Version ///
#define ZEDX_DRIVER_VERSION_MAJOR 1
#define ZEDX_DRIVER_VERSION_MINOR 4
#define ZEDX_DRIVER_VERSION_PATCH 3

#define EXTCLK 27000000

#define ZEDX_TRIGGER_CMD_REG  0x01
#define ZEDX_TRIGGER_CMD_VAL  0x0064
#define AR0234_TABLE_WAIT_MS 0xff00
#define AR0234_TABLE_END 0xff01
#define AR0234_FLL 0x300A /* Frame length line register */
#define AR0234_LLPCK 0x300C /* Line length in pix clock register */
#define AR0234_CIT 0x3012 /* Coarse integration time register */
#define AR0234_VT_PIX_CLK_DIV 0x302A
#define AR0234_VT_SYS_CLK_DIV 0x302C
#define AR0234_PRE_PLL_CLK_DIV 0x302E
#define AR0234_PLL_MULTIPLIER 0x3030
#define AR0234_ID_REG 0x3000
#define AR0234_ID_VAL 0xa56
#define AR0234_LLPCK_15FPS_MIN_HORIZ_BLK 		0x0264
#define AR0234_FLL_960X600_15FPS_MIN_HORIZ_BLK	0x1982

#ifdef CONFIG_81MHz
#define PXL_CLK 81000000
#define AR0234_LLPCK_960X600_15FPS 		0x21F7 // PXLCLK 81MHz
#define AR0234_LLPCK_1920X1200_15FPS 	0x1040 // PXLCLK 81MHz
#define AR0234_LLPCK_1920X1080_15FPS 	0x1205 // PXLCLK 81MHz
#define AR0234_FLL_960X600 				0x1FF4 // PXLCLK 81MHz
#define AR0234_FLL_1920X1200 			0x04C0 // PXLCLK 81MHz
#define AR0234_FLL_1920X1080 			0x0448 // PXLCLK 81MHz
#elif defined(CONFIG_90MHz)
#define PXL_CLK 90000000
#define AR0234_LLPCK_960X600_15FPS 		0x25BD // PXLCLK 90MHz
#define AR0234_LLPCK_1920X1200_15FPS 	0x1332 // PXLCLK 90MHz
#define AR0234_LLPCK_1920X1080_15FPS 	0x1549 // PXLCLK 90MHz
#define AR0234_FLL_960X600 				0x0268 // PXLCLK 90MHz
#define AR0234_FLL_1920X1200 			0x04C0 // PXLCLK 90MHz
#define AR0234_FLL_1920X1080 			0x0448 // PXLCLK 90MHz
#else
#define PXL_CLK 60000000
#define AR0234_LLPCK_960X600_15FPS 		0x1800 // PXLCLK 60MHz, margin ~1.0% (zero-margin: 0x1929)
#define AR0234_LLPCK_1920X1200_15FPS 	0x0CA8 // PXLCLK 60MHz, margin ~1.1% (zero-margin: 0x0CCC)
#define AR0234_LLPCK_1920X1080_15FPS 	0x0E0C // PXLCLK 60MHz, margin ~1.0% (zero-margin: 0x0E31)
#define AR0234_FLL_960X600 				0x0268 // PXLCLK 60MHz
#define AR0234_FLL_1920X1200 			0x04C0 // PXLCLK 60MHz
#define AR0234_FLL_1920X1080 			0x0448 // PXLCLK 60MHz
#endif

#define AR0234_EEPROM_ADDRESS 0x54
#define AR0234_EEPROM_ADDRESS_R 0x58
#define AR0234_EEPROM_SIZE 512
#define AR0234_EEPROM_STR_SIZE (AR0234_EEPROM_SIZE * 2)
#define AR0234_EEPROM_SIZE_PRIMARY 256
#define AR0234_EEPROM_STR_SIZE_PRIMARY (AR0234_EEPROM_SIZE_PRIMARY * 2)
#define AR0234_EEPROM_BLOCK_SIZE (1 << 8)
#define AR0234_EEPROM_NUM_BLOCKS \
	(AR0234_EEPROM_SIZE / AR0234_EEPROM_BLOCK_SIZE)

#define ar0234_reg struct reg_16

const int verbosity_level=0;

struct index_reg_8
{
	u16 source;
	u16 addr;
	u16 val;
};

static struct index_reg_8 ar0234_start[] = {
	{0x06, 0x30ce, 0x0120}, // GRR_CONTROL1 - Enables Surround View (External trigger mode) + Aligns exposure to full frame cycle
	{0x06, 0x301A, 0x295E}, /* RESET_REGISTER */
	{0x00, AR0234_TABLE_END, 0x00}
};

static struct index_reg_8 ar0234_start_master[] = {
	{0x06, 0x301A, 0x205C},
	{0x00, AR0234_TABLE_END, 0x00}
};

static struct index_reg_8 ar0234_stop[] = {
	{0x06, 0x301a, 0x2058}, // RESET_REGISTER
	{0x00, AR0234_TABLE_END, 0x00}};

#ifdef CONFIG_81MHz
static struct index_reg_8 ar0234_init_settings[] = {
	// Reset ???
	// Stop streaming
	{0x06, 0x301A, 0x2058}, // RESET_REGISTER
	// MIPI_pxlclk81_extclk27_PLL_setting
	#ifdef CONFIG_8BIT
	{0x06, 0x302A, 0x0008}, // VT_PIX_CLK_DIV
	{0x06, 0x302C, 0x0001}, // VT_SYS_CLK_DIV
	{0x06, 0x302E, 0x0003}, // PRE_PLL_CLK_DIV
	{0x06, 0x3030, 0x0044}, // PLL_MULTIPLIER
	{0x06, 0x3036, 0x0008}, // OP_PIX_CLK_DIV
	{0x06, 0x3038, 0x0002}, // OP_SYS_CLK_DIV
	#else
	{0x06, 0x302A, 0x0005}, // VT_PIX_CLK_DIV
	{0x06, 0x302C, 0x0002}, // VT_SYS_CLK_DIV
	{0x06, 0x302E, 0x0006}, // PRE_PLL_CLK_DIV
	{0x06, 0x3030, 0x00AA}, // PLL_MULTIPLIER
	{0x06, 0x3036, 0x000A}, // OP_PIX_CLK_DIV
	{0x06, 0x3038, 0x0002}, // OP_SYS_CLK_DIV
	#endif

	#ifdef CONFIG_8BIT
	{0x06, 0x31B0, 0x0070}, // FRAME_PREAMBLE
	{0x06, 0x31B2, 0x0052}, // LINE_PREAMBLE

	{0x06, 0x31B4, 0x4247}, // MIPI_TIMING_0
	{0x06, 0x31B6, 0x3215}, // MIPI_TIMING_1
	{0x06, 0x31B8, 0x704C}, // MIPI_TIMING_2
	{0x06, 0x31BA, 0x020A}, // MIPI_TIMING_3
	{0x06, 0x31BC, 0x8C08}, // MIPI_TIMING_4

	{0x06, 0x3354, 0x002A}, // MIPI_CNTRL
	#else
	{0x06, 0x31B0, 0x0071}, // FRAME_PREAMBLE
	{0x06, 0x31B2, 0x0052}, // LINE_PREAMBLE

	{0x06, 0x31B4, 0x4207}, // MIPI_TIMING_0
	{0x06, 0x31B6, 0x3214}, // MIPI_TIMING_1
	{0x06, 0x31B8, 0x704A}, // MIPI_TIMING_2
	{0x06, 0x31BA, 0x028A}, // MIPI_TIMING_3
	{0x06, 0x31BC, 0x8C08}, // MIPI_TIMING_4

	{0x06, 0x3354, 0x002B}, // MIPI_CNTRL
	#endif
	// Config mipi 4 lanes 1920 1200 12fps PxlClk 81Mhz ExtClk 27Mhz
	// {0x06, 0x301A, 0x2058},
	{0x06, 0x300A, 0x2646}, // FRAME_LENGTH_LINES
	{0x06, 0x300C, 0x0264}, // LINE_LENGTH_PCK
	{0x06, 0x3012, 0x00F8}, // COARSE_INTEGRATION_TIME
	#ifdef CONFIG_8BIT
	{0x06, 0x31AC, 0x0808}, // DATA_FORMAT_BITS
	#else
	{0x06, 0x31AC, 0x0A0A}, // DATA_FORMAT_BITS
	#endif
	{0x06, 0x306E, 0x9010}, // DATAPATH_SELECT
	{0x06, 0x3082, 0x0003}, // OPERATION_MODE_CTRL
	{0x06, 0x3040, 0xF000}, // READ_MODE
	{0x06, 0x31D0, 0x0000}, // COMPANDING
	{0x06, 0x31AE, 0x0204}, // SERIAL_FORMAT

	// Recommended settings PxlClk 81Mhz ExtClk 27Mhz
	// Recommended Common 1
	{0x06, 0x3088, 0x8050}, // SEQ_CTRL_PORT
	{0x06, 0x3086, 0x9237}, // SEQ_DATA_PORT
	{0x06, 0x3096, 0x0280}, // RESERVED_MFR_3096
	{0x06, 0x31E0, 0x0003}, // PIX_DEF_ID
	{0x06, 0x30B0, 0x0000}, // DIGITAL_TEST
	// 1D-DDC_Parameters
	{0x06, 0x3F4C, 0x121F}, // PIXEL_CORRECTION_PARAMETER_0
	{0x06, 0x3F4E, 0x121F}, // PIXEL_CORRECTION_PARAMETER_1
	{0x06, 0x3F50, 0x0B81}, // PIXEL_CORRECTION_PARAMETER_2
	// LSC_Mono_28 X - NOT USED
	// Recommended Common 2
	{0x06, 0x3ED2, 0xAA00}, // RESERVED_MFR_3ED2, ADC gain limit
	{0x06, 0x3180, 0xC24F}, // DELTA_DK_CONTROL
	// {0x06, 0x3ECC, 0x6E42},
	{0x06, 0x3ECC, 0x0E42}, // RESERVED_MFR_3ECC
	{0x06, 0x30F0, 0x2283}, // RESERVED_MFR_30F0
	{0x06, 0x3102, 0x5000}, // AE_LUMA_TARGET_REG
	{0x06, 0x3060, 0x000D}, // ANALOG_GAIN
	// Gain Table 81MHz
	{0x06, 0x30BA, 0x7602}, // RESERVED_MFR_30BA
	{0x06, AR0234_TABLE_END, 0x00},
};
#elif defined(CONFIG_90MHz)
static struct index_reg_8 ar0234_init_settings[] = {
	// Reset ???
	// Stop streaming
	{0x06, 0x301A, 0x2058}, // RESET_REGISTER
	// MIPI_pxlclk90_extclk27_PLL_setting
	{0x06, 0x302A, 0x0005}, // VT_PIX_CLK_DIV
	{0x06, 0x302C, 0x0001}, // VT_SYS_CLK_DIV
	{0x06, 0x302E, 0x0003}, // PRE_PLL_CLK_DIV
	{0x06, 0x3030, 0x0032}, // PLL_MULTIPLIER
	{0x06, 0x3036, 0x000A}, // OP_PIX_CLK_DIV
	{0x06, 0x3038, 0x0001}, // OP_SYS_CLK_DIV

	{0x06, 0x31B0, 0x0082}, // FRAME_PREAMBLE
	{0x06, 0x31B2, 0x005C}, // LINE_PREAMBLE

	{0x06, 0x31B4, 0x4248}, // MIPI_TIMING_0
	{0x06, 0x31B6, 0x4258}, // MIPI_TIMING_1
	{0x06, 0x31B8, 0x904B}, // MIPI_TIMING_2
	{0x06, 0x31BA, 0x030B}, // MIPI_TIMING_3
	{0x06, 0x31BC, 0x8D89}, // MIPI_TIMING_4

	{0x06, 0x3354, 0x002B}, // MIPI_CNTRL
	// Config mipi 4 lanes 1920 1200 12fps PxlClk 90Mhz ExtClk 27Mhz
	// {0x06, 0x301A, 0x2058},
	{0x06, 0x300A, 0x2646}, // FRAME_LENGTH_LINES
	{0x06, 0x300C, 0x0264}, // LINE_LENGTH_PCK
	{0x06, 0x3012, 0x00F8}, // COARSE_INTEGRATION_TIME
	{0x06, 0x31AC, 0x0A0A}, // DATA_FORMAT_BITS
	{0x06, 0x306E, 0x9010}, // DATAPATH_SELECT
	{0x06, 0x3082, 0x0003}, // OPERATION_MODE_CTRL
	{0x06, 0x3040, 0xF000}, // READ_MODE
	{0x06, 0x31D0, 0x0000}, // COMPANDING
	{0x06, 0x31AE, 0x0204}, // SERIAL_FORMAT

	// Recommended settings PxlClk 90Mhz ExtClk 27Mhz
	// Recommended Common 1
	{0x06, 0x3088, 0x8050}, // SEQ_CTRL_PORT
	{0x06, 0x3086, 0x9237}, // SEQ_DATA_PORT
	{0x06, 0x3096, 0x0280}, // RESERVED_MFR_3096
	{0x06, 0x31E0, 0x0003}, // PIX_DEF_ID
	{0x06, 0x30B0, 0x0000}, // DIGITAL_TEST
	// 1D-DDC_Parameters
	{0x06, 0x3F4C, 0x121F}, // PIXEL_CORRECTION_PARAMETER_0
	{0x06, 0x3F4E, 0x121F}, // PIXEL_CORRECTION_PARAMETER_1
	{0x06, 0x3F50, 0x0B81}, // PIXEL_CORRECTION_PARAMETER_2
	// LSC_Mono_28 X - NOT USED
	// Recommended Common 2
	{0x06, 0x3ED2, 0xAA00}, // RESERVED_MFR_3ED2, ADC gain limit
	{0x06, 0x3180, 0xC24F}, // DELTA_DK_CONTROL
	// {0x06, 0x3ECC, 0x6E42},
	{0x06, 0x3ECC, 0x0E42}, // RESERVED_MFR_3ECC
	{0x06, 0x30F0, 0x2283}, // RESERVED_MFR_30F0
	{0x06, 0x3102, 0x5000}, // AE_LUMA_TARGET_REG
	{0x06, 0x3060, 0x000D}, // ANALOG_GAIN
	// Gain Table 90MHz
	{0x06, 0x30BA, 0x7602}, // RESERVED_MFR_30BA
	{0x06, AR0234_TABLE_END, 0x00},
};
#else
static struct index_reg_8 ar0234_init_settings[] = {
	// Reset ???
	// Stop streaming
	{0x06, 0x301A, 0x2058}, // RESET_REGISTER
	// MIPI_pxlclk60_extclk27_PLL_setting
	{0x06, 0x302A, 0x0005}, // VT_PIX_CLK_DIV
	{0x06, 0x302C, 0x0002}, // VT_SYS_CLK_DIV
	{0x06, 0x302E, 0x0009}, // PRE_PLL_CLK_DIV
	{0x06, 0x3030, 0x00C8}, // PLL_MULTIPLIER
	{0x06, 0x3036, 0x000A}, // OP_PIX_CLK_DIV
	{0x06, 0x3038, 0x0002}, // OP_SYS_CLK_DIV

	{0x06, 0x31B0, 0x005C}, // FRAME_PREAMBLE
	{0x06, 0x31B2, 0x0046}, // LINE_PREAMBLE

	{0x06, 0x31B4, 0x31C6}, // MIPI_TIMING_0
	{0x06, 0x31B6, 0x3190}, // MIPI_TIMING_1
	{0x06, 0x31B8, 0x6049}, // MIPI_TIMING_2
	{0x06, 0x31BA, 0x0208}, // MIPI_TIMING_3
	{0x06, 0x31BC, 0x8986}, // MIPI_TIMING_4

	{0x06, 0x3354, 0x002B}, // MIPI_CNTRL
	// Config mipi 4 lanes 1920 1200 12fps PxlClk 60Mhz ExtClk 27Mhz
	// {0x06, 0x301A, 0x2058},
	{0x06, 0x300A, 0x2646}, // FRAME_LENGTH_LINES
	{0x06, 0x300C, 0x0264}, // LINE_LENGTH_PCK
	// {0x06, 0x3012, 0x00F8}, // COARSE_INTEGRATION_TIME
	{0x06, 0x31AC, 0x0A0A}, // DATA_FORMAT_BITS
	{0x06, 0x306E, 0x9010}, // DATAPATH_SELECT
	{0x06, 0x3082, 0x0003}, // OPERATION_MODE_CTRL
	{0x06, 0x3040, 0xF000}, // READ_MODE
	{0x06, 0x31D0, 0x0000}, // COMPANDING
	{0x06, 0x31AE, 0x0204}, // SERIAL_FORMAT

	// Recommended settings PxlClk 60MHz ExtClk 27MHz
	// Recommended Common 1
	{0x06, 0x3088, 0x8050}, // SEQ_CTRL_PORT
	{0x06, 0x3086, 0x9237}, // SEQ_DATA_PORT
	{0x06, 0x3096, 0x0280}, // RESERVED_MFR_3096
	{0x06, 0x31E0, 0x0003}, // PIX_DEF_ID
	{0x06, 0x30B0, 0x0000}, // DIGITAL_TEST
	// 1D-DDC_Parameters
	{0x06, 0x3F4C, 0x121F}, // PIXEL_CORRECTION_PARAMETER_0
	{0x06, 0x3F4E, 0x121F}, // PIXEL_CORRECTION_PARAMETER_1
	{0x06, 0x3F50, 0x0B81}, // PIXEL_CORRECTION_PARAMETER_2
	// LSC_Mono_28 X - NOT USED
	// Recommended Common 2
	{0x06, 0x3ED2, 0xAA00}, // RESERVED_MFR_3ED2, ADC gain limit
	{0x06, 0x3180, 0xC24F}, // DELTA_DK_CONTROL
	// {0x06, 0x3ECC, 0x6E42},
	{0x06, 0x3ECC, 0x0E42}, // RESERVED_MFR_3ECC
	{0x06, 0x30F0, 0x2283}, // RESERVED_MFR_30F0
	{0x06, 0x3102, 0x5000}, // AE_LUMA_TARGET_REG
	{0x06, 0x3060, 0x000D}, // ANALOG_GAIN
	// Gain Table 60MHz
	{0x06, 0x30BA, 0x7602}, // RESERVED_MFR_30BA
	// {0x06, 0x30ce, 0x0120}, // GRR_CONTROL1 - Enables Surround View (External trigger mode) + Aligns exposure to full frame cycle

	{0x00, AR0234_TABLE_END, 0x00},
};
#endif

static struct index_reg_8 ar0234_960x600_binning[] = {
	{0x06, 0x3002, 0x0008}, // Y_ADDR_START
	{0x06, 0x3004, 0x0008}, // X_ADDR_START
	{0x06, 0x3006, 0x04B7}, // Y_ADDR_END (1207)
	{0x06, 0x3008, 0x0787}, // X_ADDR_END (1927)
	{0x06, 0x30A2, 0x0003}, // X_ODD_INC = 3 (binning)
	{0x06, 0x30A6, 0x0003}, // Y_ODD_INC = 3 (binning)
	{0x06, 0x3040, 0xF000}, // READ_MODE
	// {0x06, 0x3012, 0x01F1}, // CIT for 15fps
	// {0x06, 0x301A, 0x295E}, /* RESET_REGISTER */

	{0x00, AR0234_TABLE_END, 0x00},
};

static struct index_reg_8 ar0234_1920x1080_cropping[] = {
	{0x06, 0x3002, 0x0044}, //Y address start (60 + 8 pixel)
	{0x06, 0x3004, 0x0008}, //X address start (8 pixel)
	{0x06, 0x3006, 0x047B}, //Y address end (1207-60)
	{0x06, 0x3008, 0x0787}, //X address end (1927)
	{0x06, 0x3040, 0xC000}, // READ_MODE = 0 ///FLIP/FLIP 0 = normal ZED-X must be flipped/flopped
	{0x06, 0x30A2, 0x0001}, // X_ODD_INC = 1 (No skip)
	{0x06, 0x30A6, 0x0001}, // Y_ODD_INC = 1 (No skip)
	// {0x06, 0x3012, 0x0193}, // CIT for 15fps

	{0x00, AR0234_TABLE_END, 0x00},
};

static struct index_reg_8 ar0234_1920x1200[] = {
	{0x06, 0x3002, 0x0008}, // Y_ADDR_START
	{0x06, 0x3004, 0x0008}, // X_ADDR_START
	{0x06, 0x3006, 0x04B7}, //Y address end (1207)
	{0x06, 0x3008, 0x0787}, //X address end (1927)
	{0x06, 0x3040, 0xC000}, // READ_MODE = 0 ///FLIP/FLIP 0 = normal ZED-X must be flipped/flopped
	{0x06, 0x30A2, 0x0001}, // X_ODD_INC = 1
	{0x06, 0x30A6, 0x0001}, // Y_ODD_INC = 1
	// {0x06, 0x3012, 0x193}, // CIT for 15fps

	{0x00, AR0234_TABLE_END, 0x00},
};

static struct index_reg_8 ar0234_context_switch[] = {

	{0x06, 0x3034, 0x0220}, // stop auto cycling
	{0x06, AR0234_TABLE_WAIT_MS, 100},
	{0x06, 0x3034, 0x02A0}, // start auto cycling
	{0x00, AR0234_TABLE_END, 0x00},
};

static struct index_reg_8 tp_colorbars[] = {
	{0x06, 0x3070, 0x2}, // TEST_PATTERN
	{0x00, AR0234_TABLE_END, 0x00},
};

enum ar0234_mode
{
	AR0234_MODE_1920X1200_NATIVE_60FPS,
	AR0234_MODE_960X600_BINNING_120FPS,
	AR0234_MODE_1920X1080_CROP_60FPS,
	AR0234_MODE_START_STREAM,
	AR0234_MODE_STOP_STREAM,
	AR0234_MODE_TEST_PATTERN,
	AR0234_MODE_CONTEXT_SWITCH,
	AR0234_MODE_START_STREAM_MASTER,
	AR0234_MODE_INIT,
};

static struct index_reg_8 *mode_table[] = {
	[AR0234_MODE_1920X1200_NATIVE_60FPS] = ar0234_1920x1200,
	[AR0234_MODE_960X600_BINNING_120FPS] = ar0234_960x600_binning,
	[AR0234_MODE_1920X1080_CROP_60FPS] = ar0234_1920x1080_cropping,
	[AR0234_MODE_START_STREAM] = ar0234_start,
	[AR0234_MODE_STOP_STREAM] = ar0234_stop,
	[AR0234_MODE_TEST_PATTERN] = tp_colorbars,
	[AR0234_MODE_CONTEXT_SWITCH] = ar0234_context_switch,
	[AR0234_MODE_START_STREAM_MASTER] = ar0234_start_master,
	[AR0234_MODE_INIT] = ar0234_init_settings,
};


#if defined(CONFIG_81MHz) || defined(CONFIG_90MHz)
static const int ar0234_HD_fps[] = {
	15,
	30,
	60,
	120};

static const int ar0234_binning_fps[] = {
	15,
	30,
	60,
	120,
	200};
#else
static const int ar0234_HD_fps[] = {
	15,
	30,
	60,
};

static const int ar0234_binning_fps[] = {
	15,
	30,
	60,
	120,
};
#endif

static const struct camera_common_frmfmt ar0234_frmfmt[] = {
	{{1920, 1200}, ar0234_HD_fps, sizeof(ar0234_HD_fps)/sizeof(ar0234_HD_fps[0]), 0, AR0234_MODE_1920X1200_NATIVE_60FPS},
	{{960, 600}, ar0234_binning_fps, sizeof(ar0234_binning_fps)/sizeof(ar0234_binning_fps[0]), 0, AR0234_MODE_960X600_BINNING_120FPS},
	{{1920, 1080}, ar0234_HD_fps, sizeof(ar0234_HD_fps)/sizeof(ar0234_HD_fps[0]), 0, AR0234_MODE_1920X1080_CROP_60FPS},
};

#endif /* __ZEDX_I2C_TABLES__ */