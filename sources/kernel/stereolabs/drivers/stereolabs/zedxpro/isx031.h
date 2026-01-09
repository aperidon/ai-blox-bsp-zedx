/**
 * Copyright (c) 2016-2017, NVIDIA Corporation.  All rights reserved.
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

#ifndef __ISX031_H__
#define __ISX031_H__

#include <linux/ioctl.h>  /* For IOCTL macros */
#include "../include/info_sysfs.h"

#define ISX031_IOCTL_SET_MODE			_IOW('o', 1, struct zedxpro_mode)
#define ISX031_IOCTL_GET_STATUS			_IOR('o', 2, __u8)
#define ISX031_IOCTL_SET_FRAME_LENGTH		_IOW('o', 3, __u32)
#define ISX031_IOCTL_SET_COARSE_TIME		_IOW('o', 4, __u32)
#define ISX031_IOCTL_SET_GAIN			_IOW('o', 5, __u16)
#define ISX031_IOCTL_GET_SENSORDATA		_IOR('o', 6, \
	 struct zedxpro_sensordata)
#define ISX031_IOCTL_SET_GROUP_HOLD		_IOW('o', 7, struct zedxpro_ae)
#define ISX031_IOCTL_SET_HDR_COARSE_TIME	_IOW('o', 8, struct zedxpro_hdr)
#define ISX031_IOCTL_SET_POWER			_IOW('o', 20, __u32)


#define ISX031_FRAME_LENGTH_ADDR		0x300A
#define ISX031_COARSE_TIME_ADDR			0x3012
#define ISX031_GAIN_ADDR			0x3014 /* GAIN ADDR */
#define ISX031_GROUP_HOLD_ADDR			0x3022 /* REG HOLD */
#define ISX031_SW_RESET_ADDR			0x3003 /* SW RESET */

#define ZEDXPRO_DRIVER_VERSION_MAJOR 1
#define ZEDXPRO_DRIVER_VERSION_MINOR 3
#define ZEDXPRO_DRIVER_VERSION_PATCH 0

#define ISX031_EEPROM_ADDRESS			0x57
#define ISX031_EEPROM_SIZE				512
#define ISX031_EEPROM_STR_SIZE			(ISX031_EEPROM_SIZE * 2)
#define ISX031_EEPROM_BLOCK_SIZE		(1 << 8)
#define ISX031_EEPROM_NUM_BLOCKS \
	(ISX031_EEPROM_SIZE / ISX031_EEPROM_BLOCK_SIZE)

#define ISX031_FUSE_ID_START_ADDR		91
#define ISX031_FUSE_ID_SIZE				8
#define ISX031_FUSE_ID_STR_SIZE			(ISX031_FUSE_ID_SIZE * 2)

#define MAX_CAM_NUMBER 16

#define MAX_RADIAL_COEFFICIENTS 6
#define MAX_TANGENTIAL_COEFFICIENTS 2
#define MAX_FISHEYE_COEFFICIENTS 6

#define CAMERA_MAX_SN_LENGTH            32
#define MAX_RLS_COLOR_CHANNELS          4
#define MAX_RLS_BREAKPOINTS             6

static int zedxpro_write_reg(struct camera_common_data *s_data,
				u16 addr, u8 val);

static inline int zedxpro_read_reg(struct camera_common_data *s_data,
				u16 addr, u8 *val);

struct zedxpro {
	struct camera_common_eeprom_data eeprom[ISX031_EEPROM_NUM_BLOCKS];
	u8 eeprom_buf[ISX031_EEPROM_SIZE];
	struct i2c_client	*i2c_client;
	const struct i2c_device_id *id;
	struct v4l2_subdev	*subdev;
	struct v4l2_ctrl_handler ctrl_handler;
	int num_ctrls;
	struct v4l2_ctrl* ctrls[1];
	struct camera_common_data	*s_data;
	struct tegracam_device		*tc_dev;
	int zedx_id;
	u32 sync_sensor_index; /* right from the dts */
	u32 channel;
	u32	frame_length;
	u32 i2c_bus;
	bool master;
	unsigned long serial_number;
	struct info_sysfs zed_sysfs;
	s8 probe_counter;
	s32 group_hold_prev;
	bool group_hold_en;
	s64 fps;
#if 1 /* TODO: switch to debug */
	// volatile int register_, value_;
#endif	
	bool ioctl_updated;
	uint8_t gyro_addr;
	uint8_t acc_addr;
};

/**
 * This allow the left and right cameras to refer to one another internally.
 * In particular, the goal is to share the serial number between 2 sensors,
 * because only the left camera has access to the eeprom.
 */
static s8 zedxpro_probe_count = 0;
static struct zedxpro *zedxpro_array[MAX_CAM_NUMBER];

struct zedxpro_mode {
	__u32 xres;
	__u32 yres;
	__u32 frame_length;
	__u32 coarse_time;
	__u32 coarse_time_short;
	__u16 gain;
	__u8 hdr_en;
};

struct zedxpro_hdr {
	__u32 coarse_time_long;
	__u32 coarse_time_short;
};

struct zedxpro_ae {
	__u32 frame_length;
	__u8  frame_length_enable;
	__u32 coarse_time;
	__u32 coarse_time_short;
	__u8  coarse_time_enable;
	__s32 gain;
	__u8  gain_enable;
};

#ifdef __KERNEL__
struct zedxpro_power_rail {
	struct regulator *dvdd;
	struct regulator *avdd;
	struct regulator *iovdd;
	struct regulator *ext_reg1;
	struct regulator *ext_reg2;
	struct clk *mclk;
	unsigned int pwdn_gpio;
	unsigned int cam1_gpio;
	unsigned int reset_gpio;
	unsigned int af_gpio;
};

struct zedxpro_platform_data {
	const char *mclk_name; /* NULL for default default_mclk */
	unsigned int cam1_gpio;
	unsigned int reset_gpio;
	unsigned int af_gpio;
	bool ext_reg;
	int (*power_on)(struct zedxpro_power_rail *pw);
	int (*power_off)(struct zedxpro_power_rail *pw);
};
#endif /* __KERNEL__ */

#endif  /* __ISX031_H__ */

