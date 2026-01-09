/*
 * imx678.c - imx678 sensor driver
 *
 * Copyright (c) 2023, Leopard Imaging Inc. All rights reserved.
 * Based on Copyright (c) 2023, NVIDIA CORPORATION. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/* #define DEBUG */
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/module.h>

#include <linux/seq_file.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>

#include <media/tegracam_core.h>
#include "imx678.h"
#include "imx678_mode_tbls.h"
#include "../include/parse_folder.h"
#include "../include/info_sysfs.h"

#define xstr(s) str(s)
#define str(s) #s
#define DRV_STR_VERSION xstr(IMX678_DRIVER_VERSION_MAJOR)"."xstr(IMX678_DRIVER_VERSION_MINOR)"."xstr(IMX678_DRIVER_VERSION_PATCH)""

#define IMX678_GAIN_REG_MAX			(10)
#define IMX678_DEFAULT_FRAME_LENGTH	(2250)
#define IMX678_MAX_FRAME_LENGTH	(69420)
#define IMX678_MASK_LSB_3_BITS			(0x07)
#define IMX678_MASK_LSB_8_BITS			(0x00ff)

// #define IMX274_DOL_4K_MODE_HMAX 		0x04C4

#define IMX678_ANALOG_GAIN_ADDR_LSB		0x3070 /* ANALOG GAIN LSB */
#define IMX678_ANALOG_GAIN_ADDR_MSB		0x3071 /* ANALOG GAIN MSB */
#define DOL_IMX678_ANALOG_GAIN_ADDR_LSB		0x3072 /* ANALOG GAIN LSB */
#define DOL_IMX678_ANALOG_GAIN_ADDR_MSB		0x3073 /* ANALOG GAIN MSB */

#define IMX678_VMAX_ADDR_MSB			0x302A
#define IMX678_VMAX_ADDR_MID			0x3029
#define IMX678_VMAX_ADDR_LSB			0x3028
#define IMX678_SHR_ADDR_MSB			 0x3052
#define IMX678_SHR_ADDR_MID			 0x3051
#define IMX678_SHR_ADDR_LSB			 0x3050

#define IMX678_SHR0_ADDR_MSB			0x3052
#define IMX678_SHR0_ADDR_MID			0x3051
#define IMX678_SHR0_ADDR_LSB			0x3050

#define IMX678_SHR1_ADDR_MSB			0x3056
#define IMX678_SHR1_ADDR_MID			0x3055
#define IMX678_SHR1_ADDR_LSB			0x3054

#define IMX678_SHR2_ADDR_MSB			0x305a
#define IMX678_SHR2_ADDR_MID			0x3059
#define IMX678_SHR2_ADDR_LSB			0x3058

#define IMX678_RHS1_ADDR_MSB			0x3062
#define IMX678_RHS1_ADDR_MID			0x3061
#define IMX678_RHS1_ADDR_LSB			0x3060

#define IMX678_VMAX_ADDR_MSB			0x302A
#define IMX678_VMAX_ADDR_MID			0x3029
#define IMX678_VMAX_ADDR_LSB			0x3028

#define IMX678_HMAX_ADDR_MID			0x302d
#define IMX678_HMAX_ADDR_LSB			0x302c

#define VMAX					0x08CA

#define IMX678_GROUP_HOLD_ADDR		0x3001
#define IMX678_DOL_MODE			0x301A

#define IMX678_K_FACTOR 1000LL
#define IMX678_M_FACTOR 1000000LL
#define IMX678_G_FACTOR 1000000000LL
#define IMX678_T_FACTOR 1000000000000LL

#define IMX678_MAX_GAIN_DEC 240
#define IMX678_MAX_GAIN_DB  72

#define IMX678_MAX_BLACK_LEVEL_10BPP 1023
#define IMX678_MAX_BLACK_LEVEL_12BPP 4095
#define IMX678_DEFAULT_BLACK_LEVEL_10BPP 50
#define IMX678_DEFAULT_BLACK_LEVEL_12BPP 200

#define IMX678_MIN_SHR0_LENGTH 8
#define IMX678_MIN_INTEGRATION_LINES 2

#define IMX678_INCK 74250000LL

#define ZED_CAMERA_CID_EEPROM_DATA (TEGRA_CAMERA_CID_BASE+300)

extern int set_bitrate_Dser(int channel, u32 i2c_bus, u8 val);
extern int set_bitrate_Ser(int channel, u8 val);
extern int ser_translate_addr(int zedx_id, unsigned int src_addr, unsigned int dest_addr);
extern int ser_reset_translate(int zedx_id);
int dser_get_gmsl_port(int channel, int zedx_id);
//extern int dser_enable_gmsl_link(int channel,int zedx_id);
extern int dser_open_all_gmsl_link(int channel);

int custom_s_ctrl(struct v4l2_ctrl *ctrl);

extern int ser_get_acc_addr(int zedx_id);
extern int ser_get_gyro_addr(int zedx_id);

extern int fps_set_Dser(int channel, s64 val);

static const struct of_device_id imx678_of_match[] = {
	{ .compatible = "stereolabs,zedxone_uhd",},
	{ },
};
MODULE_DEVICE_TABLE(of, imx678_of_match);

#if 0
static int imx678_set_custom_ctrls(struct v4l2_ctrl *ctrl);
static const struct v4l2_ctrl_ops imx678_custom_ctrl_ops = {
	.s_ctrl = imx678_set_custom_ctrls,
};
#endif

const char * const imx678_data_rate_menu[] = {
	[IMX678_2376_MBPS] = "2376 Mbps/lane",
	[IMX678_2079_MBPS] = "2079 Mbps/lane",
	[IMX678_1782_MBPS] = "1782 Mbps/lane",
	[IMX678_1440_MBPS] = "1440 Mbps/lane",
	[IMX678_1188_MBPS] = "1188 Mbps/lane",
	[IMX678_891_MBPS] = "891 Mbps/lane",
	[IMX678_720_MBPS] = "720 Mbps/lane",
	[IMX678_594_MBPS] = "594 Mbps/lane",
};

static const u32 ctrl_cid_list[] = {
	TEGRA_CAMERA_CID_GAIN,
	TEGRA_CAMERA_CID_EXPOSURE,
	TEGRA_CAMERA_CID_EXPOSURE_SHORT,
	TEGRA_CAMERA_CID_FRAME_RATE,
	TEGRA_CAMERA_CID_HDR_EN,
	TEGRA_CAMERA_CID_SENSOR_MODE_ID,
	TEGRA_CAMERA_CID_STEREO_EEPROM,
	TEGRA_CAMERA_CID_EEPROM_DATA,
};

static const struct v4l2_ctrl_ops custom_ctrl_ops = {
	.s_ctrl = custom_s_ctrl,
};

static struct v4l2_ctrl_config cfg_list[] = {
	{
		.ops = &custom_ctrl_ops,
		.id = ZED_CAMERA_CID_EEPROM_DATA,
		.name = "ZED EEPROM data",
		.type = V4L2_CTRL_TYPE_STRING,
		.min = 1024/2,
		.max = (1024/2)+1,
		.step = 1,
	},
};


// static struct v4l2_ctrl_config imx678_custom_ctrl_list[] = {
//	 {
//		 .ops = &imx678_custom_ctrl_ops,
//		 .id = TEGRA_CAMERA_CID_DATA_RATE,
//		 .name = "Data Rate",
//		 .type = V4L2_CTRL_TYPE_MENU,
//		 .min = 0,
//		 .max = ARRAY_SIZE(imx678_data_rate_menu) - 1,
//		 .def = 0,
//		 .qmenu = imx678_data_rate_menu,
//	 },
//	 // {
//	 //	 .ops = &imx678_custom_ctrl_ops,
//	 //	 .id = TEGRA_CAMERA_CID_TEST_PATTERN,
//	 //	 .name = "Test Pattern",
//	 //	 .type = V4L2_CTRL_TYPE_MENU,
//	 //	 .min = 0,
//	 //	 .max = ARRAY_SIZE(imx678_test_pattern_menu) - 1,
//	 //	 .def = 0,
//	 //	 .qmenu = imx678_test_pattern_menu,
//	 // },
// };

/**
 * struct fisheye_lens_distorion_coeff - Coefficients for the distortion model.
 * @coeff_count: uint 32 - Number of radial coefficients.
 * @k: float* - Table of coefficients.
 * @mapping_type: uint 32 - 0 -> equidistant, 1 -> equisolid,
 *							2 -> orthographic, 3 -> stereographic
 *
 * For lens correction when a wide FOV is used.
 */
typedef struct
{
	u32 coeff_count;
	float k[MAX_FISHEYE_COEFFICIENTS];
	u32 mapping_type;
} fisheye_lens_distortion_coeff;

/**
 * struct polynomial_lens_distorion_coeff - Coefficients for the distortion model.
 * @radial_coeff_count: uint 32 - Number of radial coefficients.
 * @k: float* - Table of radial coefficients.
 * @tangential_coeff_count: uint 32 - Number of tangential coefficients.
 * @p: float* - Table of tangential coefficients.
 *
 * For lens correction when a wide FOV is used.
 */
typedef struct
{
	u32 radial_coeff_count;
	float k[MAX_RADIAL_COEFFICIENTS];
	u32 tangential_coeff_count;
	float p[MAX_TANGENTIAL_COEFFICIENTS];
} polynomial_lens_distortion_coeff;

/**
 * struct camera_intrinsics - Camera calibration settings.
 * @width: uint 32 - Width of the image in pixel.
 * @height: uint32 - Height of the image in pixel.
 * @fx: float - focal length accross the x axis in pixel.
 * @fy: float - focal length accross the y axis in pixel.
 * @tangential_coeff_count: uint 32 - Number of tangential coefficients.
 * @skew: float - Skew of the sensor.
 * @cx: float - x coordinate of the optical center in pixels.
 * @cy: float - y coordinate of the optical center in pixels.
 * @distortion_type: uint32 - type of distortion, depending on the lens.
 *					0: pinhole, assuming polynomial distortion
 *					1: fisheye, assuming fisheye distortion)
 *					2: ocam (omini-directional)
 * @distortion_coefficients: union of structures - Distortion coefficients
 *							 corresponding to the lens. See the structs above
 *							 for more details.
 *
 * Camera and lens intrinsic calibration settings.
 */
typedef struct
{
	u32 width, height;
	float fx, fy;
	float skew;
	float cx, cy;
	u32 distortion_type;
	union distortion_coefficients
	{
		polynomial_lens_distortion_coeff poly;
		fisheye_lens_distortion_coeff fisheye;
	} dist_coeff;
} camera_intrinsics;

/**
 * struct camera_extrinsics - Camera and IMU rotation and translation parameters.
 * @rx: float - Rotation parameter expressed in Rodrigues notation:
 *		angle = sqrt(rx^2+ry^2+rz^2), unit axis = s = [rx,ry,rz]/angle.
 * @ry: float - Rotation parameter expressed in Rodrigues notation.
 * @rz: float - Rotation parameter expressed in Rodrigues notation.
 * @tx: float - Translation parameter.
 * @ty: float - Translation parameter.
 * @tz: float - Translation parameter.
 *
 * All rotations and translations are done with respect to the same reference point.
 */
typedef struct
{
	float rx, ry, rz;
	float tx, ty, tz;
} camera_extrinsics;

/**
 * struct imu_params - IMU calibration settings.
 * @linear_acceleration_bias: float[3] - 3D vector containing the accelerometer bias.
 * 						Must be added to the accelerometer readings.
 * @angular_velocity_bias: float[3] - 3D vector containing the gyroscope bias.
 * 						Must be added to the gyroscope readings.
 * @gravity_acceleration: float[3] - Gravity acceleration when the camera is horizontal.
 * @extr: camera_extrinsincs - Rotation and transltation parameters.
 * @update_rate: float
 * @linear_acceleration_noise_density: float - Accelerometer noise parameter
 * @linear_acceleration_random_walk: float - Accelerometer noise parameter
 * @angular_velocity_noise_density: float - Gyroscope noise parameter
 * @angular_velocity_random_walk: float - Gyroscope noise parameter
 *
 * IMU calibration settings.
 */
typedef struct
{
	float linear_acceleration_bias[3];
	float angular_velocity_bias[3];
	float gravity_acceleration[3];
	camera_extrinsics extr;
} imu_params_v1;

typedef struct {
	/*
	 * Noise model parameters
	 */
#if !IS_ENABLED(CONFIG_TEGRA_L4T_351)
	float update_rate;
	float linear_acceleration_noise_density;
	float linear_acceleration_random_walk;
	float angular_velocity_noise_density;
	float angular_velocity_random_walk;
#endif
} imu_params_noise_m;

typedef struct {
	imu_params_v1 imu_data_v1;
	imu_params_noise_m nm;
} imu_params_v2;

#if !IS_ENABLED(CONFIG_TEGRA_L4T_351)
/**
 * radial_lsc_params - Radial LSC parameters for image correction
 *
 * @image_height: Image height
 * @image_width: Image width
 * @n_channels: Number of color channels
 * @rls_x0: X-coordinate of center point for each color channel
 * @rls_y0: Y-coordinate of center point for each color channel
 * @ekxx: Ellipse xx parameter for each color channel
 * @ekxy: Ellipse xy parameter for each color channel
 * @ekyx: Ellipse yx parameter for each color channel
 * @ekyy: Ellipse yy parameter for each color channel
 * @rls_n_points: Number of breakpoints in LSC radial transfer function
 * @rls_rad_tf_x: LSC radial transfer function X for each color channel and breakpoint
 * @rls_rad_tf_y: LSC radial transfer function Y for each color channel and breakpoint
 * @rls_rad_tf_slope: LSC radial transfer function slope for each color channel and breakpoint
 * @r_scale: rScale parameter
 *
 * Structure containing parameters for radial LSC (Lens Shading Correction)
 * for image correction purposes.
 */
typedef struct {
	u16 image_height;
	u16 image_width;
	u8 n_channels;
	float rls_x0[MAX_RLS_COLOR_CHANNELS];
	float rls_y0[MAX_RLS_COLOR_CHANNELS];
	double ekxx[MAX_RLS_COLOR_CHANNELS];
	double ekxy[MAX_RLS_COLOR_CHANNELS];
	double ekyx[MAX_RLS_COLOR_CHANNELS];
	double ekyy[MAX_RLS_COLOR_CHANNELS];
	u8 rls_n_points;
	float rls_rad_tf_x[MAX_RLS_COLOR_CHANNELS][MAX_RLS_BREAKPOINTS];
	float rls_rad_tf_y[MAX_RLS_COLOR_CHANNELS][MAX_RLS_BREAKPOINTS];
	float rls_rad_tf_slope[MAX_RLS_COLOR_CHANNELS][MAX_RLS_BREAKPOINTS];
	u8 r_scale;
} radial_lsc_params;
#endif

/**
 * struct NvCamSyncSensorCalibData - Camera calibration settings.
 * @cam_intr: struct camera_intrinsics - Intrinsics calibration parameters, see above.
 * @cam_extr: struct camera_extrinsics - Extrinsics calibration parameters, see above.
 * @imu_present: uint 8 - IMU availability. 0 means no IMU is present.
 * @imu: struct imu_params - IMU calibration settings.
 * @serial_number: u8* - table containing the ZED serial number.
 * @rls: radial_lsc_params - Radial lens shading correction parameters
 *				for the optics.
 *
 * Camera calibration settings.
 */
typedef struct
{
	camera_intrinsics cam_intr;
	camera_extrinsics cam_extr;
	u8 imu_present;
	imu_params_v2 imu;
#if !IS_ENABLED(CONFIG_TEGRA_L4T_351)
	u8 serial_number[CAMERA_MAX_SN_LENGTH];
	radial_lsc_params rls;
#endif
} NvCamSyncSensorCalibData;

/**
 * struct LiEeprom_Content_Struct - Structure representing the EEPROM content.
 * @version: uint 32 - EEPROM layout version.
 * @factory_data: uint 32 - Factory flag to set when factory flashed.
 * 				It needs to be reset to 0 when the user modifies it.
 * @left_cam_intr: struct camera_intrinsics - Left camera intrinsics calibration parameters,
 * 				see above.
 * @right_cam_intr: struct camera_intrinsics - Right camera intrinsics calibration parameters,
 * 				see above.
 * @cam_extr: struct camera_extrinsics - Extrinsics calibration parameters
 * 			for the cameras, see above.
 * @imu_present: uint 32 - IMU availability. 0 means no IMU is present.
 * @imu: struct imu_params - IMU calibration settings.
 * @serial_number: u8* - u8 table of containing the serial number of the ZED X.
 * @left_rls: radial_lsc_params - Radial lens shading correction parameters for
 *			the left camera sensor.
 * @right_rls: radial_lsc_params - Radial lens shading correction parameters for
 *			the right camera sensor.
 *
 * Calibration parameters of the ZED-X. Parsed using the EEPROM content.
 */
typedef struct
{
	u32 version;
	u32 factory_data;
	camera_intrinsics left_cam_intr;
	camera_intrinsics right_cam_intr;
	camera_extrinsics cam_extr;
	u32 imu_present;
	imu_params_v1 imu;
#if !IS_ENABLED(CONFIG_TEGRA_L4T_351)
	u8 serial_number[CAMERA_MAX_SN_LENGTH];
	imu_params_noise_m nm;
	radial_lsc_params left_rls;
	radial_lsc_params right_rls;
#endif
} LiEeprom_Content_Struct;

struct imx678 {
	struct camera_common_eeprom_data eeprom[IMX678_EEPROM_NUM_BLOCKS];
	u8 eeprom_buf[IMX678_EEPROM_SIZE];
	struct i2c_client	*i2c_client;
	const struct i2c_device_id	*id;
	struct v4l2_subdev			*subdev;
	struct v4l2_ctrl_handler ctrl_handler;
	int num_ctrls;
	struct v4l2_ctrl* ctrls[1];
	NvCamSyncSensorCalibData	EepromCalib;
	struct camera_common_data	*s_data;
	struct tegracam_device		*tc_dev;
	struct mutex				streaming_lock;
	int			zedx_id;
	s64			last_exposure_long;
	s64			last_exposure_short;
	u32 		sync_sensor_index;
	u32			channel;
	u32			frame_length;
	u32			vmax;
	u32			i2c_bus;
	u32			line_time;
	u16			fine_integ_time;
	u8			data_rate;
	bool		master;
	unsigned long serial_number;
	struct info_sysfs zed_sysfs;
	s8			probe_counter;
	bool ioctl_updated;
	uint8_t gyro_addr;
	uint8_t acc_addr;
};

/**
 * This allow the left and right cameras to refer to one another internally.
 * In particular, the goal is to share the serial number between 2 sensors,
 * because only the left camera has access to the eeprom.
 */
static s8 zed4k_probe_count = 0;
static struct imx678 *zed4k_array[N_IMX678];

/**
 * const struct regmap_config - imx678 register mapping configuration.
 * @reg_bits: uint 32 - 16 bits register addresses.
 * @val_bits: uint 32 - 16 bits register contents.
 * @cache_type: enum - Supported cache type.
 *
 * Register configuration of the ar0234 camera sensor.
 */
static const struct regmap_config sensor_regmap_config = {
	.reg_bits = 16,
	.val_bits = 8,
	.cache_type = REGCACHE_RBTREE,
};

/**
 * zedx_links_check() - Check the status of the GMSL links.
 * @priv: Driver private data.
 * @addr: Table of u8, each entry is the indice of a connected cable.
 *
 * Get the connected GMSL links and return their indices with the u8 table
 * provided as an argument. This table must have a size of 4, for a quad
 * deserializer.
 *
 * Context: Non critical function, can sleep.
 * Return: The number of detected links in case of success,
 * or 5 if it a slave device calls the function,
 * and a negative errno in case of error.
 */
// static int zedx_links_check(struct imx678 *priv, u8 *links)
// {
// 	struct tegracam_device *tc_dev = priv->tc_dev;
// 	struct device *dev = tc_dev->dev;
// 	int ret = 0;

// 	// This functions returns the number of connected GMSL links,
// 	// and their indices in the links pointer
// 	ret = links_check_Dser(priv->channel, links);

// 	dev_info(dev, "%s: %d link(s) detected\n", __func__, ret);

// 	if (ret < 0)
// 	{
// 		dev_warn(dev, "%s: fail while reading the GMSL links state\n", __func__);
// 		return -ENODEV;
// 	}

// 	return ret;
// }
static inline void imx678_get_frame_length_regs(imx678_reg *regs,
				u32 frame_length)
{
	regs->addr = IMX678_VMAX_ADDR_MSB;
	regs->val = (frame_length >> 16) & 0xff;
	(regs + 1)->addr = IMX678_VMAX_ADDR_MID;
	(regs + 1)->val = (frame_length >> 8) & 0xff;
	(regs + 2)->addr = IMX678_VMAX_ADDR_LSB;
	(regs + 2)->val = (frame_length) & 0xff;
}

static inline void imx678_get_coarse_time_regs(imx678_reg *regs,
				u32 shs)
{
	regs->addr = IMX678_SHR_ADDR_MSB;
	regs->val = (shs >> 16) & 0xff;
	(regs + 1)->addr = IMX678_SHR_ADDR_MID;
	(regs + 1)->val = (shs >> 8) & 0xff;
	(regs + 2)->addr = IMX678_SHR_ADDR_LSB;
	(regs + 2)->val = shs & 0xff;
}

static inline void imx678_get_coarse_time_dol0_regs(imx678_reg *regs,
		u32 shs)
{
	regs->addr = IMX678_SHR0_ADDR_MSB;
	regs->val = (shs >> 16) & 0xff;
	(regs + 1)->addr = IMX678_SHR0_ADDR_MID;
	(regs + 1)->val = (shs >> 8) & 0xff;
	(regs + 2)->addr = IMX678_SHR0_ADDR_LSB;
	(regs + 2)->val = shs & 0xff;
}

static inline void imx678_get_coarse_time_dol1_regs(imx678_reg *regs,
		u32 shs)
{
	regs->addr = IMX678_SHR1_ADDR_MSB;
	regs->val = (shs >> 16) & 0xff;
	(regs + 1)->addr = IMX678_SHR1_ADDR_MID;
	(regs + 1)->val = (shs >> 8) & 0xff;
	(regs + 2)->addr = IMX678_SHR1_ADDR_LSB;
	(regs + 2)->val = shs & 0xff;
}

static inline void imx678_get_coarse_time_dol2_regs(imx678_reg *regs,
		u32 shs)
{
	regs->addr = IMX678_SHR2_ADDR_MSB;
	regs->val = (shs >> 16) & 0xff;
	(regs + 1)->addr = IMX678_SHR2_ADDR_MID;
	(regs + 1)->val = (shs >> 8) & 0xff;
	(regs + 2)->addr = IMX678_SHR2_ADDR_LSB;
	(regs + 2)->val = shs & 0xff;
}

static inline void imx678_get_RHS1_val(struct camera_common_data *s_data,
		u32 *shs)
{
	int err = 0;
	u32 reg_val_1 = 0;
	u32 reg_val_2 = 0;
	u32 reg_val_3 = 0;
	err = regmap_read(s_data->regmap, IMX678_RHS1_ADDR_MSB, &reg_val_3);
	err = regmap_read(s_data->regmap, IMX678_RHS1_ADDR_MID, &reg_val_2);
	err = regmap_read(s_data->regmap, IMX678_RHS1_ADDR_LSB, &reg_val_1);
	*shs = reg_val_1 + (reg_val_2*16*16) +(reg_val_3*16*16*16*16);
}

static inline void imx678_get_VMAX_val(struct camera_common_data *s_data,
		u32 *shs)
{
	int err = 0;
	u32 reg_val_1 = 0;
	u32 reg_val_2 = 0;
	u32 reg_val_3 = 0;
	err = regmap_read(s_data->regmap, IMX678_VMAX_ADDR_MSB, &reg_val_3);
	err = regmap_read(s_data->regmap, IMX678_VMAX_ADDR_MID, &reg_val_2);
	err = regmap_read(s_data->regmap, IMX678_VMAX_ADDR_LSB, &reg_val_1);
	*shs = reg_val_1 + (reg_val_2*16*16) +(reg_val_3*16*16*16*16);

}

static inline void imx678_get_HMAX_val(struct camera_common_data *s_data,
		u32 *shs)
{
	int err = 0;
	u32 reg_val_1 = 0;
	u32 reg_val_2 = 0;
	err = regmap_read(s_data->regmap, IMX678_HMAX_ADDR_MID, &reg_val_2);
	err = regmap_read(s_data->regmap, IMX678_HMAX_ADDR_LSB, &reg_val_1);
	*shs = reg_val_1 + (reg_val_2*16*16);
}


static inline void imx678_calculate_gain_regs(imx678_reg *regs,
		u16 gain)
{
	regs->addr = IMX678_ANALOG_GAIN_ADDR_MSB;
	regs->val = (gain >> 8) & IMX678_MASK_LSB_3_BITS;
	(regs + 1)->addr = IMX678_ANALOG_GAIN_ADDR_LSB;
	(regs + 1)->val = gain & 0xff;
}

static inline void imx678_calculate_gain_DOL_regs(imx678_reg *regs,
		u16 gain)
{
	regs->addr = DOL_IMX678_ANALOG_GAIN_ADDR_MSB;
	regs->val = (gain >> 8) & IMX678_MASK_LSB_3_BITS;
	(regs + 1)->addr = DOL_IMX678_ANALOG_GAIN_ADDR_LSB;
	(regs + 1)->val = gain & 0xff;
}

static int test_mode;
module_param(test_mode, int, 0644);

static inline int imx678_read_reg(struct camera_common_data *s_data,
		u16 addr, u8 *val)
{
	int err = 0;
	u32 reg_val = 0;

	err = regmap_read(s_data->regmap, addr, &reg_val);
	*val = reg_val & 0xFF;

	return err;
}

static int imx678_write_reg(struct camera_common_data *s_data,
				u16 addr, u8 val)
{
	int err;
	struct device *dev = s_data->dev;

	err = regmap_write(s_data->regmap, addr, val);
	if (err && verbosity_level)
		dev_err(dev, "%s:i2c write failed, 0x%x = %x\n",
			__func__, addr, val);

	return err;
}

static int imx678_write_table(struct imx678 *priv,
		const struct zedx4k table[])
{
	//	struct camera_common_data *s_data = priv->s_data;
	struct tegracam_device *tc_dev = priv->tc_dev;
	struct device *dev = tc_dev->dev;
	int i = 0;
	int ret = 0;
	int retry = 5;

	// Camera registers
	while (table[i].source != 0x00)
	{
		if(table[i].source == 0x06)
		{
			retry = 1;

			if (table[i].addr == IMX678_TABLE_WAIT_MS)
			{
				msleep(table[i].val);
				i++;
				continue;
			}
retry_sensor:
			ret = imx678_write_reg(priv->s_data, table[i].addr, table[i].val);
			if (ret)
			{
				retry--;
				if (retry > 0)
				{
					dev_warn(dev, "ZED-X_write_reg: try %d\n", retry);
					msleep(4);
					goto retry_sensor;
				}
				return -1;
			}
			else
			{
				if (0x301a == table[i].addr || 0x3060 == table[i].addr)
					msleep(100);
			}
		}

		i++;
	}

	return 0;
}

static int imx678_power_on(struct camera_common_data *s_data)
{
	int err = 0;
	struct camera_common_power_rail *pw = s_data->power;
	struct camera_common_pdata *pdata = s_data->pdata;
	struct device *dev = s_data->dev;

	if (pdata && pdata->power_on) {
		err = pdata->power_on(pw);
		if (err)
			dev_err(dev, "%s failed.\n", __func__);
		else
			pw->state = SWITCH_ON;
		return err;
	}

	pw->state = SWITCH_ON;

	return 0;
}

static int imx678_power_off(struct camera_common_data *s_data)
{
	int err = 0;
	struct camera_common_power_rail *pw = s_data->power;
	struct camera_common_pdata *pdata = s_data->pdata;
	struct device *dev = s_data->dev;

	if (pdata && pdata->power_off) {
		err = pdata->power_off(pw);
		if (!err)
			goto power_off_done;
		else
			dev_err(dev, "%s failed.\n", __func__);
		return err;
	}

power_off_done:
	pw->state = SWITCH_OFF;

	return 0;
}

static int imx678_power_get(struct tegracam_device *tc_dev)
{
	struct device *dev = tc_dev->dev;
	struct camera_common_data *s_data = tc_dev->s_data;
	struct camera_common_power_rail *pw = s_data->power;
	struct camera_common_pdata *pdata = s_data->pdata;
	const char *mclk_name;
	const char *parentclk_name;
	struct clk *parent;
	int err = 0;

	mclk_name = pdata->mclk_name ?
			pdata->mclk_name : "cam_mclk1";
	pw->mclk = devm_clk_get(dev, mclk_name);
	if (IS_ERR(pw->mclk)) {
		dev_err(dev, "unable to get clock %s\n", mclk_name);
		return PTR_ERR(pw->mclk);
	}

	parentclk_name = pdata->parentclk_name;
	if (parentclk_name) {
		parent = devm_clk_get(dev, parentclk_name);
		if (IS_ERR(parent)) {
			dev_err(dev, "unable to get parent clcok %s",
				parentclk_name);
		} else
			clk_set_parent(pw->mclk, parent);
	}

	pw->state = SWITCH_OFF;

	return err;
}

static int imx678_power_put(struct tegracam_device *tc_dev)
{
	struct camera_common_data *s_data = tc_dev->s_data;
	struct camera_common_power_rail *pw = s_data->power;

	if (unlikely(!pw))
		return -EFAULT;

	return 0;
}

static int imx678_set_group_hold(struct tegracam_device *tc_dev, bool val)
{
	struct camera_common_data *s_data = tc_dev->s_data;
	struct device *dev = tc_dev->dev;
	int err;


	err = imx678_write_reg(s_data,
			IMX678_GROUP_HOLD_ADDR, val);
	if (err) {
		dev_dbg(dev,
			"%s: Group hold control error\n", __func__);
		return err;
	}

	return 0;
}

static int imx678_set_gain(struct tegracam_device *tc_dev, s64 val)
{
	struct camera_common_data *s_data = tc_dev->s_data;
	struct imx678 *priv = (struct imx678 *)tc_dev->priv;
	struct device *dev = tc_dev->dev;
	imx678_reg reg_list[2];
	const struct sensor_mode_properties *mode =
		&s_data->sensor_props.sensor_modes[s_data->mode];
	int err, i;
	u16 gain_reg;
	u8 DOL_MODE;

	gain_reg = val * IMX678_MAX_GAIN_DEC /
		(IMX678_MAX_GAIN_DB *
		mode->control_properties.gain_factor);

	imx678_read_reg(priv->s_data,IMX678_DOL_MODE,&DOL_MODE);

	if (DOL_MODE == 0x01) {
		imx678_calculate_gain_DOL_regs(reg_list, gain_reg);
	}
	else{
		imx678_calculate_gain_regs(reg_list, gain_reg);
	}


	for (i = 0; i < ARRAY_SIZE(reg_list); i++) {
		err = imx678_write_reg(s_data, reg_list[i].addr,
			 reg_list[i].val);
		if (err)
			goto fail;
	}
	return 0;

fail:
	dev_warn(dev, "%s: GAIN control error\n", __func__);
	return err;
}

static int imx678_set_exposure_shr_dol_long(struct tegracam_device *tc_dev,
		s64 val);

static int imx678_set_exposure_shr_dol_short(struct tegracam_device *tc_dev,
		s64 val);

static int imx678_set_exposure_shr(struct tegracam_device *tc_dev, s64 val);

static int imx678_set_frame_rate(struct tegracam_device *tc_dev, s64 val)
{
	struct camera_common_data *s_data = tc_dev->s_data;
	struct imx678 *priv = (struct imx678 *)tc_dev->priv;
	struct device *dev = tc_dev->dev;
	struct v4l2_control control;
	imx678_reg reg_list[3];
	int err;
	u32 vmax;
	u8 DOL_MODE;
	const struct sensor_mode_properties *mode =
		&s_data->sensor_props.sensor_modes[s_data->mode];
	int i = 0;


	control.id = TEGRA_CAMERA_CID_HDR_EN;
	err = camera_common_g_ctrl(priv->s_data,&control);
	imx678_read_reg(priv->s_data,IMX678_DOL_MODE,&DOL_MODE);

	if (DOL_MODE == 0x01) 
		{
			err = (int)fps_set_Dser(priv->channel, val);

			if (err == 0xEEEE)
			{
				dev_warn(dev, "%s: Unsupported value\n", __func__);

				return 0;
			}

			if (err == 0xFFFF)
			{
				dev_warn(dev, "%s: Deserializer write error\n", __func__);

				return -1;
			}
		}
	else
		{
			err = (int)fps_set_Dser(priv->channel, val);

			if (err == 0xEEEE)
			{
				dev_warn(dev, "%s: Unsupported value\n", __func__);

				return 0;
			}

			if (err == 0xFFFF)
			{
				dev_warn(dev, "%s: Deserializer write error\n", __func__);

				return -1;
			}
		}


	vmax = (((u64)mode->control_properties.framerate_factor *
		IMX678_G_FACTOR) / (val * priv->line_time));



	control.id = TEGRA_CAMERA_CID_HDR_EN;
	err = camera_common_g_ctrl(priv->s_data,&control);
	imx678_read_reg(priv->s_data,IMX678_DOL_MODE,&DOL_MODE);

	if (DOL_MODE == 0x01) {
		err = imx678_set_exposure_shr_dol_long(tc_dev,
				priv->last_exposure_long);
		if (err)
			dev_err(dev, "%s: error exposure time dol long\n",
					__func__);

		err = imx678_set_exposure_shr_dol_short(tc_dev,
				priv->last_exposure_short);
		if (err)
			dev_err(dev, "%s: error exposure time dol short\n",
					__func__);
		vmax = vmax/2;
		if(vmax<1024)
			vmax = 1024;
		priv->frame_length = vmax;

	}
	else
	{
		err = imx678_set_exposure_shr(tc_dev,
				priv->last_exposure_long);
		priv->frame_length = vmax;
	}

    // imx678_get_HMAX_val(s_data, &hmax);
	// vmax = ((IMX678_INCK * IMX678_M_FACTOR) / (val * hmax));


	if (priv->frame_length > IMX678_MAX_FRAME_LENGTH)
		priv->frame_length = IMX678_MAX_FRAME_LENGTH;

	imx678_get_frame_length_regs(reg_list, vmax);

	for (i = 0; i < 3; i++) {
		err = imx678_write_reg(priv->s_data, reg_list[i].addr,
			 reg_list[i].val);
		if (err)
			goto fail;
	}



	return 0;

fail:
	dev_dbg(dev, "%s: FRAME_LENGTH control error\n", __func__);
	return err;
}

static int imx678_set_exposure(struct tegracam_device *tc_dev, s64 val)
{
	struct imx678 *priv = (struct imx678 *)tegracam_get_privdata(tc_dev);
	struct camera_common_data *s_data = tc_dev->s_data;
	struct device *dev = s_data->dev;
	int err;
	u8 DOL_MODE;
	imx678_read_reg(priv->s_data,IMX678_DOL_MODE,&DOL_MODE);
	if(DOL_MODE == 0x01)
	{
		err = imx678_set_exposure_shr_dol_long(tc_dev,val);
		if (err)
		{
			dev_err(dev,
					"%s: error exposure time dol long override\n", __func__);
		}
	}
	else
	{
		err = imx678_set_exposure_shr(tc_dev,val);
		if (err)
		{
			dev_err(dev,
					"%s: error exposure time SHR override\n", __func__);
		}
	}
	return err;


}

struct v4l2_ctrl *fm_find_v4l2_ctrl(struct tegracam_device *tc_dev, int ctrl_id)
{
	struct camera_common_data *s_data = tc_dev->s_data;
	struct device *dev = tc_dev->dev;
	struct tegracam_ctrl_handler *handler = s_data->tegracam_ctrl_hdl;
	struct v4l2_ctrl *ctrl;
	int i;

	for (i = 0; i < handler->numctrls; i++) {
		ctrl = handler->ctrls[i];

		if ( ctrl->id == ctrl_id )
			return ctrl;
	}

	dev_warn(dev, "%s: Couldn't find control with [ %x ] id\n", 
			__func__, ctrl_id);
	return NULL;

}

static int imx678_set_exposure_shr(struct tegracam_device *tc_dev, s64 val)
{
	// struct camera_common_data *s_data = tc_dev->s_data;
	struct imx678 *priv = (struct imx678 *)tc_dev->priv;
    struct v4l2_ctrl *ctrl;
	int err;
	imx678_reg reg_list[3];
	u32 integration_time_line;
	u32 reg_shr0;
	int i = 0;

	integration_time_line = (val * IMX678_K_FACTOR) / priv->line_time ;

	reg_shr0 = priv->frame_length - integration_time_line;

    /* Value must be multiple of 2 */
    reg_shr0 = (reg_shr0 % 2) ? reg_shr0 + 1 : reg_shr0;

    if (reg_shr0 < IMX678_MIN_SHR0_LENGTH)
        reg_shr0 = IMX678_MIN_SHR0_LENGTH;
	else if (reg_shr0 > (priv->frame_length - IMX678_MIN_INTEGRATION_LINES))
		reg_shr0 = priv->frame_length - IMX678_MIN_INTEGRATION_LINES;

	imx678_get_coarse_time_regs(reg_list, reg_shr0);
	for (i = 0; i < 3; i++) {
		err = imx678_write_reg(priv->s_data, reg_list[i].addr,
				reg_list[i].val);
		// if (err)
		// 	goto fail;
	}


    /* Update new ctrl value */
    ctrl = fm_find_v4l2_ctrl(tc_dev, TEGRA_CAMERA_CID_EXPOSURE);
    if (ctrl) {
        /* Value could be adjusted, set the right value */
        *ctrl->p_new.p_s64 = val;
        /* This ctrl is affected on FRAME RATE control also */
        *ctrl->p_cur.p_s64 = val;
    }

	return err;
}

static int imx678_set_exposure_shr_dol_short(struct tegracam_device *tc_dev,
		s64 val)
{
	struct imx678 *priv = (struct imx678 *)tegracam_get_privdata(tc_dev);
	// struct camera_common_data *s_data = tc_dev->s_data;
	struct device *dev = tc_dev->dev;
	// const struct sensor_mode_properties *mode =
	// 	&s_data->sensor_props.sensor_modes[s_data->mode];
	struct v4l2_control control;
	imx678_reg reg_list[3], reg_list_long[3];
	int err;
	int hdr_en;
	u32 reg_shr0, reg_shr1, integration_time_line, integration_time_line_long, rsh1;
	// u32 reg_shr1, vmax, rsh1, coarse_time, hmax;
	int i = 0;

	control.id = TEGRA_CAMERA_CID_HDR_EN;
	err = camera_common_g_ctrl(priv->s_data, &control);
	if (err < 0) {
		dev_err(dev, "could not find device ctrl.\n");
		return err;
	}

	hdr_en = switch_ctrl_qmenu[control.value];
	if (hdr_en != SWITCH_ON) {
		dev_dbg(dev, "%s: SHR DOL1 is ignored for non-HDR mode\n",
				__func__);
		return 0;
	}

	// dev_info(dev, "%s: integration time: %lld [us]\n", __func__, val);

	integration_time_line = (val * IMX678_K_FACTOR) / priv->line_time ;
	// integration_time_line = (val * IMX678_K_FACTOR) /26666 ;

	integration_time_line_long = integration_time_line *64;

	imx678_get_RHS1_val(priv->s_data,&rsh1);


	reg_shr1 = rsh1/2 - integration_time_line;


    /* Value must be multiple of 2 */
    reg_shr1 = (reg_shr1 % 2) ? reg_shr1 + 1 : reg_shr1;

    if (reg_shr1 < IMX678_MIN_SHR0_LENGTH)
        reg_shr1 = IMX678_MIN_SHR0_LENGTH;
	else if (reg_shr1 > (rsh1/2-IMX678_MIN_INTEGRATION_LINES))
		{
		reg_shr1 =rsh1/2-IMX678_MIN_INTEGRATION_LINES;
		integration_time_line_long = ((rsh1/2) - reg_shr1)*64;
		}
	reg_shr0 = priv->frame_length*2 - integration_time_line_long;

    // if (reg_shr0 < IMX678_MIN_SHR0_LENGTH)
    //     reg_shr0 = IMX678_MIN_SHR0_LENGTH;
	// else if (reg_shr0 > (rsh1*32-IMX678_MIN_INTEGRATION_LINES))
	// 	reg_shr0 =rsh1*32-IMX678_MIN_INTEGRATION_LINES;



	// coarse_time = mode->signal_properties.pixel_clock.val *
	// 	val / mode->image_properties.line_length /
	// 	mode->control_properties.exposure_factor ;

	// hmax = IMX274_DOL_4K_MODE_HMAX;
	// imx678_get_VMAX_val(priv->s_data,&vmax);

	// reg_shr1 = vmax + rsh1 - coarse_time - 1/4;
	/*shs = 5000; rsh1 - 
	  (val * mode->signal_properties.pixel_clock.val /
	  mode->control_properties.exposure_factor -mode->image_properties.line_length
	  )/hmax - 1/4;
	  */

	imx678_get_coarse_time_dol0_regs(reg_list_long,reg_shr0);

	imx678_get_coarse_time_dol1_regs(reg_list,reg_shr1);
	for (i = 0; i < 3; i++) {
		err = imx678_write_reg(priv->s_data, reg_list[i].addr,
				reg_list[i].val);
		if (err)
			goto fail;
	}

		for (i = 0; i < 3; i++) {
		err = imx678_write_reg(priv->s_data, reg_list_long[i].addr,
				reg_list_long[i].val);
		if (err)
			goto fail;
	}
	return 0;

fail:
	dev_dbg(&priv->i2c_client->dev,
			"%s: set coarse time error\n", __func__);
	return err;
}

static int imx678_set_exposure_shr_dol_long(struct tegracam_device *tc_dev,
		s64 val)
{
	struct imx678 *priv = (struct imx678 *)tegracam_get_privdata(tc_dev);
	// struct camera_common_data *s_data = tc_dev->s_data;
	// const struct sensor_mode_properties *mode =
	// 	&s_data->sensor_props.sensor_modes[s_data->mode_prop_idx];
	imx678_reg reg_list[3];
	// int err;
	u32 integration_time_line, reg_shr0;
	// u32 hmax, reg_shr0, vmax, coarse_time;
	// int i = 0;

	integration_time_line = (val * IMX678_K_FACTOR) / priv->line_time ;

	// integration_time_line = (val * IMX678_K_FACTOR) /26666 ;


	reg_shr0 = priv->frame_length*2 - integration_time_line;

    /* Value must be multiple of 2 */
    reg_shr0 = (reg_shr0 % 2) ? reg_shr0 + 1 : reg_shr0;

    if (reg_shr0 < IMX678_MIN_SHR0_LENGTH)
        reg_shr0 = IMX678_MIN_SHR0_LENGTH;
	else if (reg_shr0 > (priv->frame_length*2  - IMX678_MIN_INTEGRATION_LINES))
		reg_shr0 = priv->frame_length*2 - IMX678_MIN_INTEGRATION_LINES;

	// coarse_time = mode->signal_properties.pixel_clock.val *
	// 	val / mode->image_properties.line_length /
	// 	mode->control_properties.exposure_factor;

	// imx678_get_VMAX_val(priv->s_data,&vmax);

	// reg_shr0 = vmax - coarse_time -1/4;

	// hmax = IMX274_DOL_4K_MODE_HMAX;

	imx678_get_coarse_time_regs(reg_list, reg_shr0);
	// for (i = 0; i < 3; i++) {
	// 	err = imx678_write_reg(priv->s_data, reg_list[i].addr,
	// 			reg_list[i].val);
	// 	if (err)
	// 		goto fail;
	// }

	/*shs = 3	000;vmax - 
	  (val * mode->signal_properties.pixel_clock.val /
	  mode->control_properties.exposure_factor - mode->image_properties.line_length
	  )/hmax - 1/4;
	  */

	return 0;

// fail:
// 	dev_dbg(&priv->i2c_client->dev,
// 			"%s: set coarse time error\n", __func__);
// 	return err;
}

/**
 * imx678_fill_string_ctrl - Fill the eeprom buffer.
 * @tc_dev: tegracam_device driver structure.
 * @v4l2_ctrl: video for linux driver structure.
 *
 * Fill the eeprom buffer memory 16 bits at a time. The eeprom
 * is written later by the eeprom driver (avoiding any bottleneck).
 *
 * Context: Non critical function, can sleep.
 * Return: 0 in case of success and a negative errno in case of error.
 */
static int imx678_fill_string_ctrl(struct tegracam_device *tc_dev,
		struct v4l2_ctrl *ctrl)
{
	struct imx678 *priv = tc_dev->priv;
	int i, ret;

	switch (ctrl->id) {
		case TEGRA_CAMERA_CID_EEPROM_DATA:
			for (i = 0; i < IMX678_EEPROM_SIZE; i++) {
				ret = sprintf(&ctrl->p_new.p_char[i*2], "%c",
						priv->eeprom_buf[i]);
				if (ret < 0)
					return -EINVAL;
			}

			break;
		default:
			return -EINVAL;
	}
	ctrl->p_cur.p_char = ctrl->p_new.p_char;


	return 0;
}

/**
 * imx678_fill_eeprom - Fill the calibration structures.
 * @tc_dev: tegracam_device driver structure.
 * @v4l2_ctrl: video for linux driver structure.
 *
 * Copy the content of the eeprom in the driver calibration structures.
 *
 * Context: Non critical function, can sleep.
 * Return: 0 in case of success and a negative errno in case of error.
 */
static int imx678_fill_eeprom(struct tegracam_device *tc_dev,
		struct v4l2_ctrl *ctrl)
{
#if !IS_ENABLED(CONFIG_TEGRA_L4T_351)
	struct imx678 *priv = tc_dev->priv;
	LiEeprom_Content_Struct tmp;
	int i = 0;
	int index = 0;
	char buf_serialNumber[32] = {0};
	u8 buf[32] = {0};
	
	memcpy(buf, &priv->eeprom_buf[256], 32);
	
	for (i = 0; i < 32; i++) {
		index += scnprintf(&buf_serialNumber[index],32-index,"%02x",buf[i]);
	}

	switch (ctrl->id)
	{
		case TEGRA_CAMERA_CID_STEREO_EEPROM:
			memset(&(priv->EepromCalib), 0, sizeof(NvCamSyncSensorCalibData));
			memset(ctrl->p_new.p, 0, sizeof(NvCamSyncSensorCalibData));
			memcpy(&tmp, priv->eeprom_buf, sizeof(LiEeprom_Content_Struct));
			/*
			if (priv->sync_sensor_index == 1) {
			
				priv->EepromCalib.cam_intr = tmp.left_cam_intr;
			} else if (priv->sync_sensor_index == 2) {
				priv->EepromCalib.cam_intr = tmp.right_cam_intr;
			} else {
				priv->EepromCalib.cam_intr = tmp.left_cam_intr;
			}
			priv->EepromCalib.cam_extr = tmp.cam_extr;
			priv->EepromCalib.imu_present = tmp.imu_present;
			priv->EepromCalib.imu = tmp.imu;
			memcpy(priv->EepromCalib.serial_number, tmp.serial_number,
					CAMERA_MAX_SN_LENGTH);

			if (priv->sync_sensor_index == 1)
				priv->EepromCalib.rls = tmp.left_rls;
			else if (priv->sync_sensor_index == 2)
				priv->EepromCalib.rls = tmp.right_rls;
			else
				priv->EepromCalib.rls = tmp.left_rls;
			*/
			memcpy(priv->EepromCalib.serial_number,buf_serialNumber,32);
			
			priv->EepromCalib.cam_intr.distortion_type = 0;

			memcpy(ctrl->p_new.p, (u8 *)&(priv->EepromCalib),
					sizeof(NvCamSyncSensorCalibData));
			break;
		default:
			return -EINVAL;
	}

	ctrl->p_cur.p = ctrl->p_new.p;
#endif
	return 0;
}

static struct tegracam_ctrl_ops imx678_ctrl_ops = {
	.numctrls = ARRAY_SIZE(ctrl_cid_list),
	.ctrl_cid_list = ctrl_cid_list,
	.string_ctrl_size = {IMX678_EEPROM_STR_SIZE},
	.compound_ctrl_size = {sizeof(NvCamSyncSensorCalibData)},
	.set_gain = imx678_set_gain,
	.set_exposure = imx678_set_exposure,
	.set_exposure_short = imx678_set_exposure_shr_dol_short,
	.set_frame_rate = imx678_set_frame_rate,
	.set_group_hold = imx678_set_group_hold,
	.fill_string_ctrl = imx678_fill_string_ctrl,
	.fill_compound_ctrl = imx678_fill_eeprom,
};

static struct camera_common_pdata
*imx678_parse_dt(struct tegracam_device *tc_dev)
{
	struct device *dev = tc_dev->dev;
	struct device_node *node = dev->of_node;
	struct camera_common_pdata *board_priv_pdata;
	const struct of_device_id *match;
	int err;

	if (!node)
		return NULL;

	match = of_match_device(imx678_of_match, dev);
	if (!match) {
		dev_err(dev, "Failed to find matching dt id\n");
		return NULL;
	}

	board_priv_pdata = devm_kzalloc(dev,
		sizeof(*board_priv_pdata), GFP_KERNEL);

	err = of_property_read_string(node, "mclk",
			&board_priv_pdata->mclk_name);
	if (err)
		dev_err(dev, "mclk not in DT\n");

	board_priv_pdata->has_eeprom =
		of_property_read_bool(node, "has-eeprom");

	return board_priv_pdata;
}

#if 0
static int imx678_set_custom_ctrls(struct v4l2_ctrl *ctrl)
{
	struct tegracam_ctrl_handler *handler = 
		container_of(ctrl->handler,
				struct tegracam_ctrl_handler, ctrl_handler);
	// const struct tegracam_ctrl_ops *ops = handler->ctrl_ops;
	struct tegracam_device *tc_dev = handler->tc_dev;
	struct imx678 *priv = (struct imx678 *)tegracam_get_privdata(tc_dev);
	int err = 0;

	switch (ctrl->id) {
		case TEGRA_CAMERA_CID_DATA_RATE:
			priv->data_rate = *ctrl->p_new.p_u8;
			break;
			// case TEGRA_CAMERA_CID_TEST_PATTERN:
			// 	err = ops->set_test_pattern(tc_dev, *ctrl->p_new.p_u32);
			//	 break;
		default:
		pr_err("%s: unknown ctrl id.\n", __func__);
		return -EINVAL;
	}

	return err;
}
#endif

// static int imx678_verify_data_rate(struct tegracam_device *tc_dev) // short version
// {
// 	struct camera_common_data *s_data = tc_dev->s_data;
// 	struct imx678 *priv = (struct imx678 *)tegracam_get_privdata(tc_dev);
// 	struct device *dev = tc_dev->dev;
// 	struct v4l2_ctrl *ctrl;
	
// 	switch(priv->data_rate) {
// 	case IMX678_891_MBPS:
// 		if(s_data->colorfmt->code == MEDIA_BUS_FMT_SRGGB12_1X12)
// 		{
// 			priv->data_rate = IMX678_720_MBPS;
// 			goto modify_ctrl;
// 		}
// 		break;
// 	case IMX678_720_MBPS:
// 		if(s_data->colorfmt->code == MEDIA_BUS_FMT_SRGGB10_1X10)
// 			{
// 				priv->data_rate = IMX678_891_MBPS;
// 				goto modify_ctrl;
// 			}
// 		break;	
// 	}
// 	return 0;

// modify_ctrl:
// 	dev_warn(dev, "%s: Selected data rate is not supported in this mode, switching to default!\n", __func__);
// 	ctrl = fm_find_v4l2_ctrl(tc_dev, TEGRA_CAMERA_CID_DATA_RATE);
// 	v4l2_ctrl_s_ctrl(ctrl, priv->data_rate);
// 	return 0;

// }

static int imx678_set_pixel_format(struct tegracam_device *tc_dev)
{
	struct device *dev = tc_dev->dev;
	struct imx678 *priv = (struct imx678 *)tegracam_get_privdata(tc_dev);
	struct camera_common_data *s_data = tc_dev->s_data;
	int err;

	switch (s_data->colorfmt->code) {
		case MEDIA_BUS_FMT_SRGGB10_1X10:
			{
				err = imx678_write_table(priv, mode_table[IMX678_10BIT_MODE]);
				err = set_bitrate_Dser(priv->channel, priv->i2c_bus, 10);
				err = set_bitrate_Ser(priv->zedx_id, 10);
				dev_dbg(tc_dev->dev,
						"%s: Pixel width set to 10 bits on deser channel %d\n",
						__func__, priv->channel);
				dev_dbg(tc_dev->dev,
						"%s: Pixel width set for sensor %d\n",
						__func__, priv->zedx_id);
				break;
			}
		case MEDIA_BUS_FMT_SRGGB12_1X12:
			{
				err = imx678_write_table(priv, mode_table[IMX678_12BIT_MODE]);
				err = set_bitrate_Dser(priv->channel, priv->i2c_bus, 12);
				err = set_bitrate_Ser(priv->zedx_id, 12);
				dev_dbg(tc_dev->dev,
						"%s: Pixel width set to 12 bits on deser channel %d\n",
						__func__, priv->channel);
				dev_dbg(tc_dev->dev,
						"%s: Pixel width set for sensor %d\n",
						__func__, priv->zedx_id);
				break;
			}
		default:
			dev_err(dev, "%s: unknown pixel format\n", __func__);
			return -EINVAL;
	}

	return err;
}

/**
 * Calculate 1H time
 */
static int imx678_calculate_line_time(struct tegracam_device *tc_dev)
{
	struct imx678 *priv = (struct imx678 *)tc_dev->priv;
    struct camera_common_data *s_data = tc_dev->s_data;
	struct device *dev = tc_dev->dev;
    u32 hmax;
    // int err;

	dev_dbg(dev, "%s:++\n", __func__);

    imx678_get_HMAX_val(s_data, &hmax);
	// hmax = 1980;
	
    // if (err) {
        // dev_err(dev, "%s: unable to read hmax\n", __func__);
        // return err;
    // }
    
    priv->line_time = (hmax*IMX678_G_FACTOR) / (IMX678_INCK);

    return 0;
}

// static int imx678_set_data_rate(struct tegracam_device *tc_dev)
// {
// 	struct imx678 *priv = (struct imx678 *)tegracam_get_privdata(tc_dev);
// 	// struct camera_common_data *s_data = tc_dev->s_data;
// 	struct device *dev = tc_dev->dev;
// 	int err;

// 	dev_dbg(dev, "%s:++\n", __func__);

// 	err = imx678_verify_data_rate(tc_dev);
// 	if (err)
// 		goto fail;

// 	// err = imx678_write_reg(s_data, DATARATE_SEL, priv->data_rate);
// 	// if (err) 
// 	// goto fail;

// 	dev_dbg(dev, "%s: Data rate: %u\n", __func__, priv->data_rate);

// 	return 0;

// fail:
// 	dev_err(dev, "%s: unable to set data rate\n", __func__);
// 	return err;
// }


static int imx678_set_mode(struct tegracam_device *tc_dev)
{
	struct imx678 *priv = (struct imx678 *)tegracam_get_privdata(tc_dev);
	struct camera_common_data *s_data = tc_dev->s_data;
	struct device *dev = tc_dev->dev;

	// u8 DOL_MODE;
	int err = 0;

	err = imx678_set_pixel_format(tc_dev);
	if (err) {
		dev_err(dev, "%s: unable to set pixel format\n", __func__);
		return err;
	}

#if 0
	imx678_read_reg(priv->s_data,IMX678_DOL_MODE,&DOL_MODE);
	if(DOL_MODE == 0x01)
	{
		err = imx678_write_table(priv, mode_table[IMX678_MODE_HDR_DISABLE]);
		if (err)
			return err;
	}
#endif

	err = imx678_write_table(priv, mode_table[IMX678_MODE_STOP_STREAM]);
	if (err)
		return err;

	err = imx678_write_table(priv, mode_table[IMX678_MODE_INIT]);

	err = imx678_write_table(priv, mode_table[imx678_frmfmt[s_data->mode_prop_idx].mode]);

	if(s_data->fmt_width<s_data->fmt_height)
	{	
		dev_dbg(dev, "%s: HDR mode\n", __func__);
		err = imx678_write_table(priv, mode_table[IMX678_MODE_HDR_ENABLE]);
	}
	else
	{
		dev_dbg(dev, "%s: None HDR mode\n", __func__);
		err = imx678_write_table(priv, mode_table[IMX678_MODE_HDR_DISABLE]);
	}

	// err = imx678_set_data_rate(tc_dev);
	// if (err) {
	// 	dev_err(dev, "%s: unable to set data rate\n", __func__);
	// 	return err;
	// }

	if (err)
		return err;

    err = imx678_calculate_line_time(tc_dev);
	if (err)
		return err;

	mdelay(100);
	return 0;
}

static int imx678_start_streaming(struct tegracam_device *tc_dev)
{
	struct imx678 *priv = (struct imx678 *)tegracam_get_privdata(tc_dev);
	struct camera_common_data *s_data = tc_dev->s_data;
	struct device *dev = s_data->dev;
	int err;

	mutex_lock(&priv->streaming_lock);
	err = imx678_write_table(priv, mode_table[IMX678_MODE_START_STREAM]);
	if (err) {
		mutex_unlock(&priv->streaming_lock);
		goto exit;
	} else {
		mutex_unlock(&priv->streaming_lock);
	}
	msleep(200);

	return 0;
exit:
	dev_err(dev, "%s: error starting stream\n", __func__);
	return err;
}

static int imx678_stop_streaming(struct tegracam_device *tc_dev)
{
	struct imx678 *priv = (struct imx678 *)tegracam_get_privdata(tc_dev);
	struct camera_common_data *s_data = tc_dev->s_data;

	struct device *dev = s_data->dev;
	int err;

	mutex_lock(&priv->streaming_lock);
	err = imx678_write_table(priv, mode_table[IMX678_MODE_STOP_STREAM]);
	if (err) {
		mutex_unlock(&priv->streaming_lock);
		goto exit;
	} else {
		mutex_unlock(&priv->streaming_lock);
	}

	return 0;
exit:
	dev_err(dev, "%s: error stopping stream\n", __func__);
	return err;
}

static struct camera_common_sensor_ops imx678_common_ops = {
	.numfrmfmts = ARRAY_SIZE(imx678_frmfmt),
	.frmfmt_table = imx678_frmfmt,
	.power_on = imx678_power_on,
	.power_off = imx678_power_off,
	.write_reg = imx678_write_reg,
	.read_reg = imx678_read_reg,
	.parse_dt = imx678_parse_dt,
	.power_get = imx678_power_get,
	.power_put = imx678_power_put,
	.set_mode = imx678_set_mode,
	.start_streaming = imx678_start_streaming,
	.stop_streaming = imx678_stop_streaming,
};


static int imx678_eeprom_device_release(struct imx678 *priv)
{
	int i;

	for (i = 0; i < IMX678_EEPROM_NUM_BLOCKS; i++) {
		if (priv->eeprom[i].i2c_client != NULL) {
			i2c_unregister_device(priv->eeprom[i].i2c_client);
			priv->eeprom[i].i2c_client = NULL;
		}
	}

	return 0;
}

static int imx678_get_eeprom_info(struct imx678 *priv)
{
	int i;
	unsigned long sn_ = 0; 
	for (i = 3; i >= 0; i--) 
	{
		sn_ = (sn_ << 8) | priv->eeprom_buf[IMX678_EEPROM_BLOCK_SIZE+i];
	}
	priv->serial_number = sn_;

	priv->zed_sysfs.model_id = priv->eeprom_buf[IMX678_EEPROM_BLOCK_SIZE+5];
	//Because ZED X and ZED One UHD share the same eeprom addr we have to check the reading doesn't come from ZED X Eeprom 
	//Only usefull with AGx Xavier
	if(priv->zed_sysfs.model_id == 3)
		return -EINVAL;

	priv->gyro_addr = ser_get_gyro_addr(priv->zedx_id);
	priv->acc_addr = ser_get_acc_addr(priv->zedx_id);

	priv->zed_sysfs.awb = priv->eeprom_buf[IMX678_EEPROM_BLOCK_SIZE+6];

	return 0;
}

static int imx678_eeprom_device_init(struct imx678 *priv)
{
	struct camera_common_pdata *pdata =  priv->s_data->pdata;
	struct device *dev = priv->s_data->dev;
	char *dev_name = "eeprom_zedxone_uhd";
	static struct regmap_config eeprom_regmap_config = {
		.reg_bits = 8,
		.val_bits = 8
	};
	int i;
	int err;

	if (!pdata->has_eeprom)
		return -EINVAL;

	for (i = 0; i < IMX678_EEPROM_NUM_BLOCKS; i++) {
		
		if (&priv->eeprom[i]==NULL)
		{
			return 0;
		}

		priv->eeprom[i].adap = i2c_get_adapter(
				priv->i2c_client->adapter->nr);
		memset(&priv->eeprom[i].brd, 0, sizeof(priv->eeprom[i].brd));
		strncpy(priv->eeprom[i].brd.type, dev_name,
				sizeof(priv->eeprom[i].brd.type));

		/* assign the EEPROM addrs which is read from DT */
		priv->eeprom[i].brd.addr = priv->zed_sysfs.eeprom_id_addr + i;
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 10, 0)
		priv->eeprom[i].i2c_client = i2c_new_device(
				priv->eeprom[i].adap, &priv->eeprom[i].brd);
#else
		priv->eeprom[i].i2c_client = i2c_new_client_device(
				priv->eeprom[i].adap, &priv->eeprom[i].brd);
#endif

		i2c_put_adapter(priv->eeprom[i].adap);

		if (IS_ERR_OR_NULL(priv->eeprom[i].i2c_client)) {
			dev_dbg(dev, "%s: Failed to probe EEPORM at addr = 0x%x \n",
					__func__, priv->eeprom[i].brd.addr);
			return -ENODEV;
		}
		priv->eeprom[i].regmap = devm_regmap_init_i2c(
				priv->eeprom[i].i2c_client, &eeprom_regmap_config);
		if (IS_ERR(priv->eeprom[i].regmap)) {
			err = PTR_ERR(priv->eeprom[i].regmap);
			imx678_eeprom_device_release(priv);
			return err;
		}
	}
	return 0;
}


static int imx678_read_eeprom(struct imx678 *priv)
{
#ifdef DEBUG
	struct camera_common_data *s_data = priv->s_data;
	struct device *dev = s_data->dev;
#endif
	int err, i;

	for (i = 0; i < IMX678_EEPROM_NUM_BLOCKS; i++) {
		err = regmap_bulk_read(priv->eeprom[i].regmap, 0,
				&priv->eeprom_buf[i * IMX678_EEPROM_BLOCK_SIZE],
				IMX678_EEPROM_BLOCK_SIZE);
		if (err) {
			return err;
		}
	}
#ifdef DEBUG
	for (i = IMX678_EEPROM_BLOCK_SIZE; i < IMX678_EEPROM_BLOCK_SIZE*2; i+=8) {
		dev_dbg(dev, "%s: %02x %02x %02x %02x %02x %02x %02x %02x",__func__,
			priv->eeprom_buf[i+0], priv->eeprom_buf[i+1], priv->eeprom_buf[i+2],
			priv->eeprom_buf[i+3], priv->eeprom_buf[i+4], priv->eeprom_buf[i+5],
			priv->eeprom_buf[i+6], priv->eeprom_buf[i+7]);
		dev_dbg(dev, "\n");
	}
#endif

	err = imx678_get_eeprom_info(priv);
	if (err)
		return err;
	return 0;
}



static int imx678_board_setup(struct imx678 *priv)
{
	struct camera_common_data *s_data = priv->s_data;
	struct device *dev = s_data->dev;
	//struct device_node *node = dev->of_node;
	//const char *str;
	int err = 0;
	bool eeprom_ctrl = 0;

	priv->zed_sysfs.gmsl_port = dser_get_gmsl_port(priv->channel,priv->zedx_id);
	if(priv->zed_sysfs.gmsl_port < 0){
		dev_err(dev,"Error %d setting gmsl link\n", err);
		return err;
	}

	msleep(500);

	/* eeprom interface */
	err = imx678_eeprom_device_init(priv);
	if (err && s_data->pdata->has_eeprom)
		dev_warn(dev,
				"Failed to allocate eeprom reg map: %d\n", err);
	eeprom_ctrl = !err;

	err = camera_common_mclk_enable(s_data);
	if (err) {
		dev_err(dev,
			"Error %d turning on mclk\n", err);
		return err;
	}

	err = imx678_power_on(s_data);
	if (err) {
		camera_common_mclk_disable(s_data);
		dev_err(dev,
			"Error %d during power on sensor\n", err);
		goto error;
	}

	if (eeprom_ctrl) {
		err = imx678_read_eeprom(priv);
		if (err) {
			dev_dbg(dev, "Error %d reading eeprom\n", err);
			imx678_power_off(s_data);
			camera_common_mclk_disable(s_data);
			return err;
		}
	}


	imx678_power_off(s_data);
	camera_common_mclk_disable(s_data);
	
	return err | !eeprom_ctrl;

error:
	dev_err(dev, "board setup failed\n");
	imx678_eeprom_device_release(priv);
	return err;
}

// static int imx678_board_setup(struct imx678 *priv)
// {
// 	struct camera_common_data *s_data = priv->s_data;
// 	struct device *dev = s_data->dev;
// 	int err = 0;


// 	/* eeprom interface */
// 	err = imx678_power_on(s_data);
// 	if (err)
// 	{
// 		dev_err(dev,"Error %d during power on sensor\n", err);
// 		return err;
// 	}

// 	return 0;
// }

static int imx678_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	dev_dbg(&client->dev, "%s\n", __func__);

	return 0;
}

int custom_s_ctrl(struct v4l2_ctrl *ctrl){
	struct imx678 *priv = container_of(ctrl->handler,
		struct imx678, ctrl_handler);
	char tmp[3]={};
	int err =0; 
	int i =0;
	uint8_t tmp_eeprom_buff[IMX678_EEPROM_SIZE/2] = {};

	switch (ctrl->id) {
	case ZED_CAMERA_CID_EEPROM_DATA:
		for(i=0;i<IMX678_EEPROM_SIZE/2;i++){ // convert hex string value to uint8_t "7b" > 123
			if((i*2) >= strlen(ctrl->p_new.p_char)){
				dev_err(&priv->i2c_client->dev,"Exceeding Cntl buffer size\r\n");
				priv->ioctl_updated = false;
				return err;
			}

			memcpy(tmp,&ctrl->p_new.p_char[i*2],2);

			if(&priv->eeprom_buf[(IMX678_EEPROM_SIZE/2)+i] == NULL){
				dev_err(&priv->i2c_client->dev,"Exceeding buffer size\r\n");
				priv->ioctl_updated = false;
				return err;
			}

			err = kstrtou8(tmp,16,&tmp_eeprom_buff[i]);

			if (err){
				dev_err(&priv->i2c_client->dev,"Writing IOCTL failed\r\n");
				priv->ioctl_updated = false;
				return err;
			}

			if(tmp_eeprom_buff[i] != priv->eeprom_buf[(IMX678_EEPROM_SIZE/2)+i]){
				dev_dbg(&priv->i2c_client->dev,"IOCTL UPDATED\r\n");
				priv->ioctl_updated = true;
			}
		}

		if(err)
			return err;
		
		memcpy(&priv->eeprom_buf[IMX678_EEPROM_SIZE/2],&tmp_eeprom_buff,sizeof(tmp_eeprom_buff));
		break;
		default:
			pr_err("%s: Unknown ctrl id %x.\n", __func__,ctrl->id);
			return -EINVAL;
	}

	return 0;
}

// This function is executed during tegracam_v4l2subdev_register(), 
// we use it to add a new custom control to write/read to the eeprom 
static int subdev_register(struct v4l2_subdev *sd){
	struct i2c_client *client = v4l2_get_subdevdata(sd);
	struct camera_common_data *s_data = to_camera_common_data(&client->dev);
	struct imx678 *priv = (struct imx678 *)s_data->priv;
	struct v4l2_ctrl *ctrl;
	int err, i, num_ctrls;

	num_ctrls = ARRAY_SIZE(cfg_list);
	v4l2_ctrl_handler_init(&priv->ctrl_handler, num_ctrls);

	for (i = 0; i < num_ctrls; i++) {
		ctrl = v4l2_ctrl_new_custom(&priv->ctrl_handler,
			&cfg_list[i], NULL);
		if (ctrl == NULL) {
			dev_err(&client->dev, "Failed to init %s ctrl\n",
				cfg_list[i].name);
			continue;
		}
		if (err)
			return err;

		if(ctrl->id == ZED_CAMERA_CID_EEPROM_DATA){ // Initialize eeprom ctrl string value
			char tmp [((IMX678_EEPROM_SIZE/2)*2)+1] = {};
			int index = 0;

			for (i = 0; i < IMX678_EEPROM_SIZE/2; i++) { // We make only the 2nd 256 bytes accessible
				index += scnprintf(&tmp[index],sizeof(tmp)-index,"%02x",priv->eeprom_buf[(IMX678_EEPROM_SIZE/2)+i]);
			}

			v4l2_ctrl_s_ctrl_string(ctrl,tmp);
		}

		priv->ctrls[i] = ctrl;
	}

	priv->num_ctrls = num_ctrls;

	// L4T 32.7: kernel version is 4.9
	#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 10, 0)
		err = v4l2_ctrl_add_handler(
			sd->ctrl_handler,
			&priv->ctrl_handler,
			NULL);
	#else
		err = v4l2_ctrl_add_handler(
			sd->ctrl_handler,
			&priv->ctrl_handler,
			NULL,0);
	#endif

	if (err){
		v4l2_ctrl_handler_free(&priv->ctrl_handler);
		return err;
	}

	return 0;
}


static const struct v4l2_subdev_internal_ops imx678_subdev_internal_ops = {
	.open = imx678_open,
	.registered = subdev_register,
};

static int duplicate_priv_info(struct imx678 *priv) {
	priv->zed_sysfs.sync_sensor_index = priv->sync_sensor_index;
	priv->zed_sysfs.name = priv->s_data->subdev.name;
	priv->zed_sysfs.acc_addr = priv->acc_addr;
	priv->zed_sysfs.gyro_addr = priv->gyro_addr;
	return 0;
}

static int imx678_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct device_node *node = dev->of_node;
	struct device_node *mux_node;
	struct tegracam_device *tc_dev;
	struct imx678 *priv;
	const char *video_name;
	const char *str;
	int err;

	if (!IS_ENABLED(CONFIG_OF) || !node)
		return -EINVAL;
	
	priv = devm_kzalloc(dev, sizeof(struct imx678), GFP_KERNEL);

	if (!priv) {
		dev_err(dev, "unable to allocate memory!\n");
		return -ENOMEM;
	}

	priv->i2c_client = client;


	err = of_property_read_string(node, "channel", &str);
	if (err)
	{
		dev_err(dev, "%s: channel not found in dts\n",__func__);
		return -EINVAL;
	}

	priv->channel = str[0] - 'a';

	if (priv->channel < 0 || priv->channel > 3)
	{
		dev_err(dev, "%s: channel %d doesn't exist\n", __func__, priv->channel);
		return -EINVAL;
	}

	if (verbosity_level>=1)
		dev_dbg(dev, "%s: channel %d\n", __func__, priv->channel);

	err = of_property_read_string(node, "zedx-id", &str);

	err = kstrtoint(str,10,&priv->zedx_id);

	if (priv->zedx_id < 0)
	{
		dev_err(dev, "%s: zedx-id %d is out of range\n", __func__, priv->zedx_id);
		return -EINVAL;
	}

	priv->master=false;
	priv->ioctl_updated = false;

	err = of_property_read_string(node, "mode", &str);
	if (err)
	{
		dev_err(dev, "%s: Mode not found, slave mode defined\n", __func__);
	}

	tc_dev = devm_kzalloc(dev,
		sizeof(struct tegracam_device), GFP_KERNEL);
	if (!tc_dev)
		return -ENOMEM;

	priv->i2c_client = tc_dev->client = client;
	tc_dev->dev = dev;

	err = of_property_read_string(node, "devnode", &video_name);
	if (err)
		dev_err(dev, "devnode not found\n");

	mux_node = of_get_parent(node);

	if (mux_node==NULL)
		dev_warn(dev, "%s: no parent node ?\n", __func__);

	of_property_read_u32(mux_node, "reg", &priv->i2c_bus);

	dev_dbg(dev, "%s: i2c bus associated to the cam = %d\n", __func__, priv->i2c_bus);

	of_node_put(mux_node);

	err = of_property_read_u32(node, "sync_sensor_index",
			&priv->sync_sensor_index);
	if (err)
		dev_err(dev, "sync name index not in DT\n");

	strncpy(tc_dev->name, video_name, sizeof(tc_dev->name));

	tc_dev->dev_regmap_config = &sensor_regmap_config;
	tc_dev->sensor_ops = &imx678_common_ops;
	tc_dev->v4l2sd_internal_ops = &imx678_subdev_internal_ops;
	tc_dev->tcctrl_ops = &imx678_ctrl_ops;

	mutex_init(&priv->streaming_lock);
	err = tegracam_device_register(tc_dev);
	if (err) {
		mutex_destroy(&priv->streaming_lock);
		dev_err(dev, "tegra camera driver registration failed\n");
		return err;
	}
	priv->tc_dev = tc_dev;
	priv->s_data = tc_dev->s_data;
	priv->subdev = &tc_dev->s_data->subdev;

	tegracam_set_privdata(tc_dev, (void *)priv);

	err = of_property_read_u32(node, "eeprom-addr", &priv->zed_sysfs.eeprom_id_addr);
	if (err || !priv->s_data->pdata->has_eeprom)
	{
		dev_err(dev, "%s: has-eeprom or eeprom-addr is not in dts\n", __func__);
		mutex_destroy(&priv->streaming_lock);
		tegracam_device_unregister(tc_dev);
		return err;
	}

	err = imx678_board_setup(priv);
	imx678_eeprom_device_release(priv);
	if (err < 0) {
		tegracam_device_unregister(tc_dev);
		mutex_destroy(&priv->streaming_lock);
		imx678_eeprom_device_release(priv);

		dev_info(dev, "%s: ZED One UHD detection error\n", __func__);
		return -1;
	}
	else if ( err == 1 )
	{
		if (IS_ERR_OR_NULL(zed4k_array[zed4k_probe_count-1]))
		{
			dev_info(dev, "%s: You need an EEPROM for your first device entry\n", __func__);
			dev_info(dev, "%s: This might also happen because a ZED X was probbed first for this i2c bus\n", __func__);
		}

		else if (priv->sync_sensor_index !=
			zed4k_array[zed4k_probe_count-1]->sync_sensor_index)
		{
			dev_dbg(dev, "%s: use serial number of the first sensor\n", __func__);
			priv->zed_sysfs.serial_number = zed4k_array[zed4k_probe_count-1]->zed_sysfs.serial_number;
			priv->zed_sysfs.model_id = zed4k_array[zed4k_probe_count-1]->zed_sysfs.model_id;
			priv->gyro_addr = zed4k_array[zed4k_probe_count-1]->gyro_addr;
			priv->acc_addr = zed4k_array[zed4k_probe_count-1]->acc_addr;
			priv->zed_sysfs.awb = zed4k_array[zed4k_probe_count-1]->zed_sysfs.awb;
			memcpy(priv->eeprom_buf , zed4k_array[zed4k_probe_count-1]->eeprom_buf, 512);
		}
	}

	dev_dbg(dev, "%s: ZED One 4K number of modes %d.\n", __func__, imx678_common_ops.numfrmfmts);

	err = imx678_write_table(priv, mode_table[IMX678_MODE_INIT]);
	err = imx678_write_table(priv, mode_table[IMX678_MODE_3840X2160]);
	if (err) {
		tegracam_device_unregister(tc_dev);
		imx678_eeprom_device_release(priv);
		mutex_destroy(&priv->streaming_lock);
		dev_info(dev, "%s: ZED-X One UHD detect error\n",__func__);
		return err;
	}

	err = tegracam_v4l2subdev_register(tc_dev, true);
	if (err) {
		tegracam_device_unregister(tc_dev);
		imx678_eeprom_device_release(priv);
		mutex_destroy(&priv->streaming_lock);
		dev_err(dev, "tegra camera subdev registration failed\n");
		return err;
	}

	dev_info(dev, "%s: Serial Number %lu",__func__,priv->serial_number);

	/* sysfs entry for this device */
	priv->zed_sysfs.tc_dev = tc_dev;

	priv->zed_sysfs.serial_number = priv->serial_number;

	/* we try to find the /dev/videoX entry associated to this sensor */
	priv->zed_sysfs.video_id = find_video_device_by_subdev_name(priv->s_data->subdev.v4l2_dev, priv->s_data->subdev.name, &priv->zed_sysfs.parent_kobj);

	if (priv->zed_sysfs.video_id<0){
		tegracam_v4l2subdev_unregister(priv->tc_dev);
		imx678_eeprom_device_release(priv);
		mutex_destroy(&priv->streaming_lock);
		tegracam_device_unregister(priv->tc_dev);
		kobject_put(&priv->zed_sysfs.info_kobj);
		dev_err(dev, "%s: Video device id not found\n", __func__);
		return priv->zed_sysfs.video_id;
	}

	/* we create the sysfs info entry */
	if (kobject_init_and_add(&priv->zed_sysfs.info_kobj, &zed_info_kobj_type, &priv->zed_sysfs.parent_kobj, "zed_info")<0) {
		tegracam_v4l2subdev_unregister(priv->tc_dev);
		imx678_eeprom_device_release(priv);
		mutex_destroy(&priv->streaming_lock);
		tegracam_device_unregister(priv->tc_dev);
		kobject_put(&priv->zed_sysfs.info_kobj);
		return -ENOMEM;
	}

	/* we duplicate some information of priv in priv->sys_info*/
	duplicate_priv_info(priv);

	priv->probe_counter = zed4k_probe_count;
	zed4k_array[priv->probe_counter] = priv;
	zed4k_probe_count++;

	return 0;
}

static void imx678_shutdown(struct i2c_client *client){
	struct camera_common_data *s_data = NULL;
	struct imx678 *priv = NULL;
	int err = 0;
	int i = 0;
	unsigned long sn_ioctl = 0;
	unsigned long sn_eeprom = 0;
	u8 buff[4] = {};

	s_data = to_camera_common_data(&client->dev);
	priv = (struct imx678 *)s_data->priv;

	if(!priv->ioctl_updated){
		dev_dbg(&priv->i2c_client->dev,"IOCTL !updated\r\n");
		return;
	}

	priv->ioctl_updated = false;

	/* eeprom interface */
	err = imx678_eeprom_device_init(priv);
	if (err)
		dev_info(&priv->i2c_client->dev, "Failed to allocate eeprom reg map: %d\n", err);

	if(priv->eeprom[1].regmap == NULL){
		dev_info(&priv->i2c_client->dev,"Regmap eeprom not set\r\n");
		err = imx678_eeprom_device_release(priv);
		return;
	}

	//err = dser_enable_gmsl_link(priv->channel,priv->zedx_id);

	err = regmap_bulk_read(priv->eeprom[1].regmap, 0, &buff, 4);
	if(err){
		dev_info(&priv->i2c_client->dev,"Failed to read eeprom to save calibrations\r\n");
		err = imx678_eeprom_device_release(priv);
		return;
	}

	for (i = 3; i >= 0; i--){
		sn_eeprom = (sn_eeprom << 8) | buff[i];
		sn_ioctl = (sn_ioctl << 8) | priv->eeprom_buf[IMX678_EEPROM_BLOCK_SIZE+i];
	}

	dev_dbg(&priv->i2c_client->dev,"Sn ioctl/eeprom: %ld %ld %ld\r\n",sn_ioctl,sn_eeprom, priv->serial_number);
	for (i = 0; i < 4; i++){
		dev_dbg(&priv->i2c_client->dev,"ioctl/eeprom: %02x/%02x\r\n",priv->eeprom_buf[IMX678_EEPROM_BLOCK_SIZE+i],buff[i]);
	}

	if(sn_eeprom != sn_ioctl){
		dev_info(&priv->i2c_client->dev,"Sn eeprom does not match\r\n");
		err = imx678_eeprom_device_release(priv);
		err = dser_open_all_gmsl_link(priv->channel);
		return;
	}

	//ZED One UHD will always be alone on its i2c-bus. No need to isolate the gmsl link to communicate to EEPROM
	//err = dser_enable_gmsl_link(priv->channel,priv->zedx_id);

	for(i = 0;i<16;++i){ // eeprom can be written only 16 bytes at a time
		err = regmap_bulk_write(priv->eeprom[1].regmap,16 * i, &priv->eeprom_buf[IMX678_EEPROM_BLOCK_SIZE + 16*i], 16); // We write only to the 2nd 256 bytes
		msleep(20);
	}

	err = imx678_eeprom_device_release(priv);
	if(err)
		dev_info(&priv->i2c_client->dev,"Failed to release eeprom %d\r\n",err);

	err = dser_open_all_gmsl_link(priv->channel);
	msleep(100);

	dev_dbg(&priv->i2c_client->dev,"Eeprom successfully written\r\n");
}


static int imx678_remove(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct camera_common_data *s_data = NULL;
	struct imx678 *priv = NULL; 

	s_data = to_camera_common_data(&client->dev);

	imx678_shutdown(client);

	priv = (struct imx678 *)s_data->priv;

	if (&priv->streaming_lock)
		mutex_destroy(&priv->streaming_lock);

	kobject_del(&priv->zed_sysfs.info_kobj);
	if (priv->tc_dev){
		tegracam_v4l2subdev_unregister(priv->tc_dev);
		imx678_eeprom_device_release(priv);
		tegracam_device_unregister(priv->tc_dev);
	}

	priv = NULL;

	zed4k_probe_count--;

	dev_info(dev, " ZEDX-ONE-UHD sensor successfully removed\n");

	return 0;
}

static const struct i2c_device_id imx678_id[] = {
	{ "zedxone_uhd", 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, imx678_id);

static struct i2c_driver imx678_i2c_driver = {
	.driver = {
		.name = "zedxone_uhd",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(imx678_of_match),
	},
	.probe = imx678_probe,
	.remove = imx678_remove,
	.shutdown = imx678_shutdown,
	.id_table = imx678_id,
};

static int __init imx678_init(void)
{
	return i2c_add_driver(&imx678_i2c_driver);
}

static void __exit imx678_exit(void)
{
	i2c_del_driver(&imx678_i2c_driver);
}

module_init(imx678_init);
module_exit(imx678_exit);

MODULE_DESCRIPTION("Media Controller driver for Stereolabs ZED-X One UHD");
MODULE_AUTHOR("STEREOLABS <support@stereolabs.com>");
MODULE_LICENSE("GPL v2");
MODULE_VERSION(DRV_STR_VERSION);
