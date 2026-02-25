/*
 * isx031.c - isx031 sensor driver
 *
 * Copyright (c) 2018-2020, NVIDIA CORPORATION.  All rights reserved.
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
// #define DEBUG
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/module.h>

#include <linux/seq_file.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/media.h>
#include <linux/list.h>

#include <media/tegra_v4l2_camera.h>

#include <media/tegracam_core.h>

#include "isx031.h"
#include "isx031_mode_tbls.h"
#include "../include/parse_folder.h"
#include "zedxhdr_sysfs.h"

#define xstr(s) str(s)
#define str(s) #s
#define ZEDXHDR_DRIVER_VERSION_MAJOR 1
#define ZEDXHDR_DRIVER_VERSION_MINOR 4
#define ZEDXHDR_DRIVER_VERSION_PATCH 0
#define DRV_STR_VERSION xstr(ZEDXHDR_DRIVER_VERSION_MAJOR)"."xstr(ZEDXHDR_DRIVER_VERSION_MINOR)"."xstr(ZEDXHDR_DRIVER_VERSION_PATCH)""


#define ISX031_MAX_COARSE_DIFF			(10)

#define ISX031_GAIN_FACTOR				(1000000)
#define ISX031_MIN_GAIN					(1)
#define ISX031_MAX_GAIN					(48)
#define ISX031_DEFAULT_FRAME_LENGTH		(1522)
#define ISX031_MIN_FRAME_LENGTH			(1522)
#define ISX031_MAX_FRAME_LENGTH			(15520)
#define ISX031_DEFAULT_FPS				(30000000)
#define ISX031_MIN_COARSE_TIME			(5)
#define ISX031_MIN_EXPOSURE_COARSE		(0x0005)
#define ISX031_MAX_EXPOSURE_COARSE	\
	(ISX031_MAX_FRAME_LENGTH-ISX031_MAX_COARSE_DIFF)

#define ISX031_LINE_LENGTH              (1928)
#define ISX031_PIX_CLK					(88000000)

#define ZED_CAMERA_CID_EEPROM_DATA (TEGRA_CAMERA_CID_BASE+300)

extern int fps_set_Dser(int channel, s64 val);
extern int set_bitrate_Ser(int channel, u8 val);
extern int ser_translate_addr(int zedx_id, unsigned int src_addr, unsigned int dest_addr);
extern int ser_reset_translate(int zedx_id);
extern int dser_enable_gmsl_link(int channel,int zedx_id);
extern int dser_open_all_gmsl_link(int channel);

int custom_s_ctrl(struct v4l2_ctrl *ctrl);

extern int ser_get_acc_addr(int zedx_id);
extern int ser_get_gyro_addr(int zedx_id);

const struct of_device_id zedxhdr_of_match[] = {
	{ .compatible = "stereolabs,zedxhdr",},
	{ },
};
	
MODULE_DEVICE_TABLE(of, zedxhdr_of_match);

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


static const struct regmap_config sensor_regmap_config = {
	.reg_bits = 16,
	.val_bits = 8,
	.cache_type = REGCACHE_RBTREE,
};


static inline void zedxhdr_get_frame_length_regs(zedxhdr_reg *regs,
				u16 frame_length)
{
	regs->addr = ISX031_FRAME_LENGTH_ADDR;
	regs->val = frame_length & 0xffff;
}

static inline void zedxhdr_get_coarse_time_regs(zedxhdr_reg *regs,
				u16 coarse_time)
{
	regs->addr = ISX031_COARSE_TIME_ADDR;
	regs->val = coarse_time & 0xffff;
}

static inline void zedxhdr_get_gain_regs(zedxhdr_reg *regs,
				u16 gain)
{
	regs->addr = ISX031_GAIN_ADDR;
	regs->val = (gain) & 0xffff;
}

static inline int zedxhdr_read_reg8(struct camera_common_data *s_data,
				u16 addr, u8 *val)
{
	return 0;
}

static int zedxhdr_write_reg8(struct camera_common_data *s_data, u16 addr, u8 val)
{
	return 0;
}

static inline int zedxhdr_read_reg(struct camera_common_data *s_data,
				u16 addr, u8 *val)
{
	int err = 0;
	u32 reg_val = 0;

	err = regmap_read(s_data->regmap, addr, &reg_val);
	*val = reg_val & 0xFF;

	return err;
}

static int zedxhdr_write_reg(struct camera_common_data *s_data,
				u16 addr, u8 val)
{
	int err;
	struct device *dev = s_data->dev;

	err = regmap_write(s_data->regmap, addr, val);
	if (err)
		dev_err(dev, "%s:i2c write failed, 0x%x = %x\n",
			__func__, addr, val);

	return err;
}

static int zedxhdr_write_table(struct zedxhdr *priv,
				const zedxhdr_reg table[])
{
	
	struct camera_common_data *s_data = priv->s_data;
	
	return regmap_util_write_table_8(s_data->regmap,
					 table,
					 NULL, 0,
					 ISX031_TABLE_WAIT_MS,
					 ISX031_TABLE_END);
}

static int zedxhdr_power_on(struct camera_common_data *s_data)
{
	int err = 0;
	struct camera_common_power_rail *pw = s_data->power;
	struct camera_common_pdata *pdata = s_data->pdata;
	struct device *dev = s_data->dev;

	dev_dbg(dev, "%s: power on\n", __func__);
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

static int zedxhdr_power_off(struct camera_common_data *s_data)
{
	int err = 0;
	struct camera_common_power_rail *pw = s_data->power;
	struct camera_common_pdata *pdata = s_data->pdata;
	struct device *dev = s_data->dev;

	dev_dbg(dev, "%s:\n", __func__);

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

static int zedxhdr_power_get(struct tegracam_device *tc_dev)
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
		//return PTR_ERR(pw->mclk);
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

static int zedxhdr_power_put(struct tegracam_device *tc_dev)
{
	struct camera_common_data *s_data = tc_dev->s_data;
	struct camera_common_power_rail *pw = s_data->power;

	if (unlikely(!pw))
		return -EFAULT;

	return 0;
}

static int zedxhdr_set_group_hold(struct tegracam_device *tc_dev, bool val)
{
	struct camera_common_data *s_data = tc_dev->s_data;
	struct zedxhdr *priv = (struct zedxhdr *)tc_dev->priv;
	struct device *dev = tc_dev->dev;
	int err;

	priv->group_hold_prev = val;
	return 0;
	err = zedxhdr_write_reg(s_data,
			       ISX031_GROUP_HOLD_ADDR, val);
	if (err) {
		dev_err(dev,
			"%s: Group hold control error\n", __func__);
		return err;
	}

	return 0;
}

static int zedxhdr_set_gain(struct tegracam_device *tc_dev, s64 val)
{
	struct zedxhdr *priv = (struct zedxhdr *)tc_dev->priv;
	struct device *dev = tc_dev->dev;
	zedxhdr_reg reg_list[1];
	int err;
	s32 gain64;
	u16 gain;

	return 0;

	dev_dbg(dev, "%s: val: %lld\n", __func__, val);
	if (val < ISX031_MIN_GAIN)
		val = ISX031_MIN_GAIN;
	else if (val > ISX031_MAX_GAIN)
		val = ISX031_MAX_GAIN;

	/* translate value */
	gain64 = (s32)val;

	gain = (u16)(gain64 * 160 / 48);

	zedxhdr_get_gain_regs(reg_list, gain);

	err = zedxhdr_write_reg(priv->s_data, reg_list[0].addr,
		 reg_list[0].val);
	if (err)
		goto fail;

	return 0;

fail:
	dev_err(dev, "%s: GAIN control error\n", __func__);
	return err;
}

static int zedxhdr_set_frame_rate(struct tegracam_device *tc_dev, s64 val)
{
	struct zedxhdr *priv = (struct zedxhdr *)tc_dev->priv;	
	struct device *dev = tc_dev->dev;
	int err;

	err = (int)fps_set_Dser(priv->channel, ISX031_DEFAULT_FPS);

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

	/* fixed 30fps */
	priv->frame_length = ISX031_DEFAULT_FRAME_LENGTH;

	return 0;
}

static int zedxhdr_set_exposure(struct tegracam_device *tc_dev, s64 val)
{
	struct camera_common_data *s_data = tc_dev->s_data;
	struct zedxhdr *priv = (struct zedxhdr *)tc_dev->priv;
	struct device *dev = tc_dev->dev;
	const struct sensor_mode_properties *mode =
		&s_data->sensor_props.sensor_modes[s_data->mode];
	zedxhdr_reg reg_list[1];
	int err;
	int i = 0;
	s64 coarse_time;

	return 0;

	dev_dbg(dev, "%s: val: %lld\n", __func__, val);

	coarse_time = val * ISX031_PIX_CLK / ISX031_LINE_LENGTH
			  / mode->control_properties.exposure_factor;

	if (coarse_time > (s64)ISX031_MAX_EXPOSURE_COARSE)
		coarse_time = ISX031_MAX_EXPOSURE_COARSE;
	if (coarse_time < (s64)ISX031_MIN_EXPOSURE_COARSE)
		coarse_time = ISX031_MIN_EXPOSURE_COARSE;

	if(ISX031_MIN_FRAME_LENGTH < coarse_time)
	{
		zedxhdr_get_frame_length_regs(reg_list, (u16)coarse_time);
		for (i = 0; i < 1; i++) {
			err = zedxhdr_write_reg(priv->s_data, reg_list[i].addr,
				 reg_list[i].val);
			if (err)
				goto fail;
		}
	}

	zedxhdr_get_coarse_time_regs(reg_list, (u16)coarse_time);

	for (i = 0; i < 1; i++) {
		err = zedxhdr_write_reg(priv->s_data, reg_list[i].addr,
			 reg_list[i].val);
		if (err)
			goto fail;
	}

	return 0;

fail:
	dev_err(dev, "%s: Exposure control error\n", __func__);
	return err;
}

/**
 * isx031_fill_string_ctrl - Fill the eeprom buffer.
 * @tc_dev: tegracam_device driver structure.
 * @v4l2_ctrl: video for linux driver structure.
 *
 * Fill the eeprom buffer memory 16 bits at a time. The eeprom
 * is written later by the eeprom driver (avoiding any bottleneck).
 *
 * Context: Non critical function, can sleep.
 * Return: 0 in case of success and a negative errno in case of error.
 */
static inline int isx031_fill_string_ctrl(struct tegracam_device *tc_dev,
		struct v4l2_ctrl *ctrl)
{
	// struct zedxhdr *priv = tc_dev->priv;
	// int i, ret;

	return 0;
	// dev_err(tc_dev->dev, "%s: Unwanted excution\n", __func__);

	// switch (ctrl->id) {
	// 	case TEGRA_CAMERA_CID_EEPROM_DATA:
	// 		for (i = 0; i < ISX031_EEPROM_SIZE; i++) {
	// 			ret = sprintf(&ctrl->p_new.p_char[i*2], "%c",
	// 					priv->eeprom_buf[i]);
	// 			if (ret < 0)
	// 				return -EINVAL;
	// 		}

	// 		break;
	// 	default:
	// 		return -EINVAL;
	// }
	// ctrl->p_cur.p_char = ctrl->p_new.p_char;


	// return 0;
}

/**
 * isx031_fill_eeprom - Fill the calibration structures.
 * @tc_dev: tegracam_device driver structure.
 * @v4l2_ctrl: video for linux driver structure.
 *
 * Copy the content of the eeprom in the driver calibration structures.
 *
 * Context: Non critical function, can sleep.
 * Return: 0 in case of success and a negative errno in case of error.
 */
static inline int isx031_fill_eeprom(struct tegracam_device *tc_dev,
		struct v4l2_ctrl *ctrl)
{
// #if !IS_ENABLED(CONFIG_TEGRA_L4T_351)
	// struct zedxhdr *priv = tc_dev->priv;
	// LiEeprom_Content_Struct tmp;
	// int i = 0;
	// int index = 0;
	// char buf_serialNumber[32] = {0};
	// u8 buf[32] = {0};

	return 0;
// 	dev_err(tc_dev->dev, "%s: Unwanted excution\n", __func__);
	
// 	memcpy(buf, &priv->eeprom_buf[256], 32);
	
// 	for (i = 0; i < 32; i++) {
// 		index += scnprintf(&buf_serialNumber[index],32-index,"%02x",buf[i]);
// 	}

// 	switch (ctrl->id)
// 	{
// 		case TEGRA_CAMERA_CID_STEREO_EEPROM:
// 			memset(&(priv->EepromCalib), 0, sizeof(NvCamSyncSensorCalibData));
// 			memset(ctrl->p_new.p, 0, sizeof(NvCamSyncSensorCalibData));
// 			memcpy(&tmp, priv->eeprom_buf, sizeof(LiEeprom_Content_Struct));
// 			/*
// 			if (priv->sync_sensor_index == 1) {
			
// 				priv->EepromCalib.cam_intr = tmp.left_cam_intr;
// 			} else if (priv->sync_sensor_index == 2) {
// 				priv->EepromCalib.cam_intr = tmp.right_cam_intr;
// 			} else {
// 				priv->EepromCalib.cam_intr = tmp.left_cam_intr;
// 			}
// 			priv->EepromCalib.cam_extr = tmp.cam_extr;
// 			priv->EepromCalib.imu_present = tmp.imu_present;
// 			priv->EepromCalib.imu = tmp.imu;
// 			memcpy(priv->EepromCalib.serial_number, tmp.serial_number,
// 					CAMERA_MAX_SN_LENGTH);

// 			if (priv->sync_sensor_index == 1)
// 				priv->EepromCalib.rls = tmp.left_rls;
// 			else if (priv->sync_sensor_index == 2)
// 				priv->EepromCalib.rls = tmp.right_rls;
// 			else
// 				priv->EepromCalib.rls = tmp.left_rls;
// 			*/
// 			memcpy(priv->EepromCalib.serial_number,buf_serialNumber,32);
			
// 			priv->EepromCalib.cam_intr.distortion_type = 0;

// 			memcpy(ctrl->p_new.p, (u8 *)&(priv->EepromCalib),
// 					sizeof(NvCamSyncSensorCalibData));
// 			break;
// 		default:
// 			return -EINVAL;
// 	}

// 	ctrl->p_cur.p = ctrl->p_new.p;
// #endif
// 	return 0;
}

static struct tegracam_ctrl_ops zedxhdr_ctrl_ops = {
	.numctrls = ARRAY_SIZE(ctrl_cid_list),
	.ctrl_cid_list = ctrl_cid_list,
	.string_ctrl_size = {ISX031_EEPROM_STR_SIZE},
	.compound_ctrl_size = {1},
	.set_gain = zedxhdr_set_gain,
	.set_exposure = zedxhdr_set_exposure,
	.set_exposure_short = zedxhdr_set_exposure,
	.set_frame_rate = zedxhdr_set_frame_rate,
	.set_group_hold = zedxhdr_set_group_hold,
	.fill_string_ctrl = isx031_fill_string_ctrl,
	.fill_compound_ctrl = isx031_fill_eeprom,
};

static struct camera_common_pdata *zedxhdr_parse_dt(struct tegracam_device *tc_dev)
{
	struct device *dev = tc_dev->dev;
	struct device_node *node = dev->of_node;
	struct camera_common_pdata *board_priv_pdata;
	const struct of_device_id *match;
	int err;

	if (!node)
		return NULL;

	match = of_match_device(zedxhdr_of_match, dev);
	if (!match) {
		dev_err(dev, "Failed to find matching dt id\n");
		return NULL;
	}

	board_priv_pdata = devm_kzalloc(dev, sizeof(*board_priv_pdata), GFP_KERNEL);

	err = of_property_read_string(node, "mclk",
				      &board_priv_pdata->mclk_name);
	if (err)
		dev_err(dev, "mclk not in DT\n");

	board_priv_pdata->has_eeprom =
		of_property_read_bool(node, "has-eeprom");

	return board_priv_pdata;
}

static int zedxhdr_start_streaming(struct tegracam_device *tc_dev)
{
	struct zedxhdr *priv = (struct zedxhdr *)tegracam_get_privdata(tc_dev);
	struct device *dev = tc_dev->dev;
	int err;

	dev_dbg(dev, "%s: start streaming\n", __func__);

	err = zedxhdr_write_table(priv,
		mode_table[ISX031_MODE_START_STREAM]);
	if (err)
	{
		dev_warn(dev, "%s: zedxhdr_write_table  fail--------\n", __func__);
		return -EINVAL;
	}

	return 0;

}

static int zedxhdr_stop_streaming(struct tegracam_device *tc_dev)
{
	struct zedxhdr *priv = (struct zedxhdr *)tegracam_get_privdata(tc_dev);
	int err;

	dev_dbg(tc_dev->dev, "%s\n", __func__);

	err = zedxhdr_write_table(priv, mode_table[ISX031_MODE_STOP_STREAM]);

	// Write table will return -EREMOTEIO if the i2c device is disconnected
	// This return does not seem well handled by the application so we ignore it here
	if(err == -EREMOTEIO){
		dev_dbg(tc_dev->dev, "%s: i2c device disconnected, ignoring stop streaming error\n", __func__);
		return 0;
	}
	
	if (err){
		dev_err(tc_dev->dev, "%s: failed to stop streaming %d\n", __func__, err);
		return err;
	}

	return 0;
}

static int zedxhdr_set_mode(struct tegracam_device *tc_dev)
{
	struct zedxhdr *priv = (struct zedxhdr *)tegracam_get_privdata(tc_dev);
	struct camera_common_data *s_data = tc_dev->s_data;
	struct device *dev = tc_dev->dev;
	const struct of_device_id *match;
	int err;

	match = of_match_device(zedxhdr_of_match, dev);
	if (!match) {
		dev_err(dev, "Failed to find matching dt id\n");
		return -EINVAL;
	}

	err = zedxhdr_set_frame_rate( tc_dev, ISX031_DEFAULT_FPS );

	err = zedxhdr_write_table(priv, mode_table[s_data->mode]);
	if (err)
		return err;

	return 0;
}

static struct camera_common_sensor_ops zedxhdr_common_ops = {
	.numfrmfmts = ARRAY_SIZE(zedxhdr_frmfmt),
	.frmfmt_table = zedxhdr_frmfmt,
	.power_on = zedxhdr_power_on,
	.power_off = zedxhdr_power_off,
	.write_reg = zedxhdr_write_reg8,
	.read_reg = zedxhdr_read_reg8,
	.parse_dt = zedxhdr_parse_dt,
	.power_get = zedxhdr_power_get,
	.power_put = zedxhdr_power_put,
	.set_mode = zedxhdr_set_mode,
	.start_streaming = zedxhdr_start_streaming,
	.stop_streaming = zedxhdr_stop_streaming,
};

static int zedxhdr_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	dev_dbg(&client->dev, "%s:\n", __func__);

	return 0;
}

static int zedxhdr_eeprom_device_release(struct zedxhdr *priv)
{
	int i;

	for (i = 0; i < ISX031_EEPROM_NUM_BLOCKS; i++) {
		if (priv->eeprom[i].i2c_client != NULL) {
			i2c_unregister_device(priv->eeprom[i].i2c_client);
			priv->eeprom[i].i2c_client = NULL;
		}
	}

	return 0;
}

static int zedxhdr_eeprom_device_init(struct zedxhdr *priv)
{
	struct camera_common_pdata *pdata =  priv->s_data->pdata;
	struct device *dev = priv->s_data->dev;
	char *dev_name = "eeprom_zedxhdr";
	static struct regmap_config eeprom_regmap_config = {
		.reg_bits = 8,
		.val_bits = 8
	};
	int i;
	int err;

	if (!pdata->has_eeprom)
		return -EINVAL;

	for (i = 0; i < ISX031_EEPROM_NUM_BLOCKS; i++) {

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
			return -EINVAL;
		}
		priv->eeprom[i].regmap = devm_regmap_init_i2c(
				priv->eeprom[i].i2c_client, &eeprom_regmap_config);
		if (IS_ERR(priv->eeprom[i].regmap)) {
			err = PTR_ERR(priv->eeprom[i].regmap);
			zedxhdr_eeprom_device_release(priv);
			return err;
		}
	}
	return 0;
}


static int zedxhdr_get_eeprom_info(struct zedxhdr *priv)
{
	int i;
	unsigned long sn_ = 0;

	for (i = 3; i >= 0; i--) 
	{
		sn_ = (sn_ << 8) | priv->eeprom_buf[i];
	}

	priv->serial_number = sn_;

	priv->zed_sysfs.model_id = priv->eeprom_buf[5];
	priv->zed_sysfs.awb = priv->eeprom_buf[6];

	return 0;
}

int custom_s_ctrl(struct v4l2_ctrl *ctrl){
	struct zedxhdr *priv = container_of(ctrl->handler,
		struct zedxhdr, ctrl_handler);
	char tmp[3]={};
	int err =0; 
	int i =0;
	uint8_t tmp_eeprom_buff[ISX031_EEPROM_SIZE/2] = {};

	switch (ctrl->id) {
	case ZED_CAMERA_CID_EEPROM_DATA:
		for(i=0;i<ISX031_EEPROM_SIZE/2;i++){ // convert hex string value to uint8_t "7b" > 123
			if((i*2) >= strlen(ctrl->p_new.p_char)){
				dev_err(&priv->i2c_client->dev,"Exceeding Cntl buffer size\r\n");
				priv->ioctl_updated = false;
				return err;
			}

			memcpy(tmp,&ctrl->p_new.p_char[i*2],2);

			err = kstrtou8(tmp,16,&tmp_eeprom_buff[i]);

			if (err){
				dev_err(&priv->i2c_client->dev,"Writing IOCTL failed\r\n");
				priv->ioctl_updated = false;
				return err;
			}

			if(tmp_eeprom_buff[i] != priv->eeprom_buf[i]){
				dev_dbg(&priv->i2c_client->dev,"IOCTL UPDATED\r\n");
				priv->ioctl_updated = true;
			}
		}

		if(err)
			return err;
		
		memcpy(&priv->eeprom_buf[0],&tmp_eeprom_buff,sizeof(tmp_eeprom_buff));
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
	struct zedxhdr *priv = (struct zedxhdr *)s_data->priv;
	struct v4l2_ctrl *ctrl = NULL;
	int err = 0;
	int i, j, num_ctrls;

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
		
		if (priv->ctrl_handler.error) {
			dev_err(&client->dev, "Failed to init controls: %d\n", priv->ctrl_handler.error);
			return priv->ctrl_handler.error;
		}

		if(ctrl->id == ZED_CAMERA_CID_EEPROM_DATA){ // Initialize eeprom ctrl string value
			char tmp [((ISX031_EEPROM_SIZE/2)*2)+1] = {};
			int index = 0;

			for (j = 0; j < ISX031_EEPROM_SIZE/2; j++) { // We make only the 2nd 256 bytes accessible
				index += scnprintf(&tmp[index],sizeof(tmp)-index,"%02x",priv->eeprom_buf[j]);
			}

			err = v4l2_ctrl_s_ctrl_string(ctrl,tmp);
			if (err){
				dev_err(&client->dev, "Failed to set ctrl string\n");
				return err;
			}
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
		dev_err(&client->dev, "Error subdev register\n");
		v4l2_ctrl_handler_free(&priv->ctrl_handler);
		return err;
	}

	if(priv->ctrl_handler.error){
		dev_err(&client->dev, "Error ctrl handler subdev register\n");
		v4l2_ctrl_handler_free(&priv->ctrl_handler);
		return priv->ctrl_handler.error;
	}

	return 0;
}


static const struct v4l2_subdev_internal_ops zedxhdr_subdev_internal_ops = {
	.open = zedxhdr_open,
	.registered = subdev_register,
};

static int zedxhdr_read_eeprom(struct zedxhdr *priv)
{
	int err, i;
#ifdef DEBUG
	struct camera_common_data *s_data = priv->s_data;
	struct device *dev = s_data->dev;
#endif
	for (i = 0; i < ISX031_EEPROM_NUM_BLOCKS; i++) {
		err = regmap_bulk_read(priv->eeprom[i].regmap, 0,
				&priv->eeprom_buf[i * ISX031_EEPROM_BLOCK_SIZE],
				ISX031_EEPROM_BLOCK_SIZE);
		if (err) {
			return err;
		}
	}
#ifdef DEBUG
	for (i = 0; i < ISX031_EEPROM_BLOCK_SIZE; i+=8) {
		dev_dbg(dev, "%s: %02x %02x %02x %02x %02x %02x %02x %02x",__func__,
			priv->eeprom_buf[i+0], priv->eeprom_buf[i+1], priv->eeprom_buf[i+2],
			priv->eeprom_buf[i+3], priv->eeprom_buf[i+4], priv->eeprom_buf[i+5],
			priv->eeprom_buf[i+6], priv->eeprom_buf[i+7]);
	}
#endif

	err = zedxhdr_get_eeprom_info(priv);
	if (err)
		return err;

	return 0;
}

static int zedxhdr_board_setup(struct zedxhdr *priv)
{
	struct camera_common_data *s_data = priv->s_data;
	struct device *dev = s_data->dev;
	bool eeprom_ctrl = false;
	int err = 0, eeprom_in_use = 0;
	
	priv->zed_sysfs.gmsl_port = dser_enable_gmsl_link(priv->channel,priv->zedx_id);
	if(priv->zed_sysfs.gmsl_port < 0){
		dev_err(dev,"Error %d setting gmsl link\n", priv->zed_sysfs.gmsl_port);
		return priv->zed_sysfs.gmsl_port;
	}

	msleep(100);

	/* eeprom interface */
	err = zedxhdr_eeprom_device_init(priv);
	if (err && s_data->pdata->has_eeprom) {
		eeprom_in_use = 1;
		dev_dbg(dev,
			"Eeprom reg map already in use: %d\n", err);
	}

	eeprom_ctrl = !err;
	err = camera_common_mclk_enable(s_data);
	if (err)
	{
		if (eeprom_ctrl)
			zedxhdr_eeprom_device_release(priv);
		dev_err(dev,"Error %d turning on mclk\n", err);
		return err;
	}

	if (eeprom_ctrl) {
		err = zedxhdr_read_eeprom(priv);
		if (err) {
			dev_err(dev, "Error %d reading eeprom\n", err);
			camera_common_mclk_disable(s_data);
			return err;
		}
	}

	err = dser_open_all_gmsl_link(priv->channel);
	msleep(100);

    return err | eeprom_in_use;
}

static int duplicate_priv_info(struct zedxhdr *priv) {
	priv->zed_sysfs.sync_sensor_index = priv->sync_sensor_index;
	priv->zed_sysfs.serial_number = priv->serial_number;
	priv->zed_sysfs.name = priv->s_data->subdev.name;
	priv->zed_sysfs.acc_addr = priv->acc_addr;
	priv->zed_sysfs.gyro_addr = priv->gyro_addr;
	priv->zed_sysfs.channel =  priv->channel;
	priv->zed_sysfs.video_lock =  0;
	priv->zed_sysfs.zedx_id =  priv->zedx_id;
	return 0;
}

static int zedxhdr_i2c_read(struct i2c_client *client, u16 reg, u8 *val)
{
    struct i2c_msg msgs[2];
    u8 reg_buf[2];
    int ret;

    reg_buf[0] = (reg >> 8) & 0xFF;
    reg_buf[1] = reg & 0xFF;
	
    msgs[0].addr = client->addr;
    msgs[0].flags = 0; /* Write */
    msgs[0].len = 2;
    msgs[0].buf = reg_buf;

    msgs[1].addr = client->addr;
    msgs[1].flags = I2C_M_RD; /* Read */
    msgs[1].len = 1;
    msgs[1].buf = val;

    ret = i2c_transfer(client->adapter, msgs, 2);
    return (ret == 2) ? 0 : -EIO;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 0, 0)
static int zedxhdr_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
#else
static int zedxhdr_probe(struct i2c_client *client)
#endif
{
	struct device *dev = &client->dev;
	struct device_node *node = dev->of_node;
	struct tegracam_device *tc_dev;
	struct zedxhdr *priv;
	const char* video_name = NULL;
	const char* str = NULL;
	int err;
	u8 val;

	if (!IS_ENABLED(CONFIG_OF) || !node)
		return -EINVAL;

	priv = devm_kzalloc(dev, sizeof(struct zedxhdr), GFP_KERNEL);
	if (!priv) {
		dev_err(dev, "unable to allocate memory!\n");
		return -ENOMEM;
	}

	priv->i2c_client = client;

	err = zedxhdr_i2c_read(client, ISX031_SENSOR_ADDR_REG, &val);
	if(err || val != client->addr)
		return -ENODEV;

	dev_info(dev, "Driver Version : v%d.%d.%d\n",ZEDXHDR_DRIVER_VERSION_MAJOR,ZEDXHDR_DRIVER_VERSION_MINOR,ZEDXHDR_DRIVER_VERSION_PATCH);

	err = of_property_read_string(node, "channel", &str);
	if (err){
        dev_err(dev, "%s: channel not found in dts\n",__func__);
        return -EINVAL;
    }

	priv->channel = str[0] - 'a';
	if (priv->channel < 0 || priv->channel > 3){
		dev_err(dev, "%s: channel %d doesn't exist\n", __func__, priv->channel);
		return -EINVAL;
	}

	err = of_property_read_string(node, "zedx-id", &str);
	if (err){
        dev_err(dev, "%s: zedx-id not found in dts\n",__func__);
        return -EINVAL;
    }

	err = kstrtoint(str,10,&priv->zedx_id);

	//If dummy entry, don't continue probe - no verbose
    if (priv->zedx_id < 0) {
        return -ENODEV;
	}

	priv->master=false;
	priv->ioctl_updated = false;

	tc_dev = devm_kzalloc(dev,
			sizeof(struct tegracam_device), GFP_KERNEL);
	if (!tc_dev)
		return -ENOMEM;

	err = of_property_read_string(node, "devnode", &video_name);
	if (err){
		dev_err(dev, "Property devnode is missing from the device tree\n");
		return err;
	}

	err = of_property_read_u32(node, "sync_sensor_index",
							   &priv->sync_sensor_index);
	if (err)
		dev_err(dev, "sync name index not in DTS\n");

	tc_dev->client = priv->i2c_client;
	tc_dev->dev = &priv->i2c_client->dev;
	if(video_name != NULL){
		strncpy(tc_dev->name, video_name, sizeof(tc_dev->name));
	}

	tc_dev->dev_regmap_config = &sensor_regmap_config;
	tc_dev->sensor_ops = &zedxhdr_common_ops;
	tc_dev->v4l2sd_internal_ops = &zedxhdr_subdev_internal_ops;
	tc_dev->tcctrl_ops = &zedxhdr_ctrl_ops;

	err = tegracam_device_register(tc_dev);
	if (err) {
		dev_err(dev, "tegra camera driver registration failed\n");
		return err;
	}

	priv->tc_dev = tc_dev;
	priv->s_data = tc_dev->s_data;
	priv->subdev = &tc_dev->s_data->subdev;

	tegracam_set_privdata(tc_dev, (void *)priv);

	err = zedxhdr_write_table(priv,mode_table[ISX031_MODE_INIT]);
	if (err){
		tegracam_device_unregister(tc_dev);
		dev_err(dev, "%s: Init sensor fail", __func__);
		return -EINVAL;
	}

	/* might need to stop stream at power up */
	err = zedxhdr_write_table(priv,mode_table[ISX031_MODE_STOP_STREAM]);
	if (err){
		tegracam_device_unregister(tc_dev);
		dev_err(dev, "%s: Init sensor fail", __func__);
		return -EINVAL;
	}

	err = of_property_read_u32(node, "eeprom-addr", &priv->zed_sysfs.eeprom_id_addr);
	if (err || !priv->s_data->pdata->has_eeprom){
		dev_err(dev, "%s: has-eeprom or eeprom-addr is not in dts\n", __func__);
		tegracam_device_unregister(tc_dev);
		return err;
	}

	err = zedxhdr_board_setup(priv);
	zedxhdr_eeprom_device_release(priv);
	if (err < 0)
	{
		dev_err(dev, "board setup failed\n");
		tegracam_device_unregister(tc_dev);
		zedxhdr_eeprom_device_release(priv);
		return err;
	}
	else if ( err == 1 ) /* case where eeprom is already in use */
	{
		/* Should not happen, check anyway */
		if (IS_ERR_OR_NULL(zedxhdr_array[zedxhdr_probe_count-1]))
			dev_warn(dev, "%s: You need an EEPROM for your first device entry\n", __func__);

		/* Check if the sensors are different. If we have twice the same EEPROM and different
		 * sensor ids, then it is two sensor from the same stereocam. */
		else if (priv->sync_sensor_index !=
			zedxhdr_array[zedxhdr_probe_count-1]->sync_sensor_index)
		{
			dev_info(dev, "%s: Using first sensor's serial number\n", __func__);
			priv->zed_sysfs.serial_number = zedxhdr_array[zedxhdr_probe_count-1]->zed_sysfs.serial_number;
			priv->zed_sysfs.model_id = zedxhdr_array[zedxhdr_probe_count-1]->zed_sysfs.model_id;
			priv->zed_sysfs.awb = zedxhdr_array[zedxhdr_probe_count-1]->zed_sysfs.awb;
			memcpy(priv->eeprom_buf , zedxhdr_array[zedxhdr_probe_count-1]->eeprom_buf, 512);
		}
	}

	dev_info(&client->dev, "ZED-X HDR sensor initialisation done\n");

	dev_info(dev, "%s: Serial Number : %lu", __func__, priv->serial_number);

	err = tegracam_v4l2subdev_register(tc_dev, true);
	if (err) {
		zedxhdr_eeprom_device_release(priv);
		tegracam_device_unregister(tc_dev);
		dev_err(dev, "%s: Tegra camera subdev registration failed\n", __func__);
		return err;
	}

	priv->zed_sysfs.tc_dev = tc_dev;

	priv->acc_addr = ser_get_acc_addr(priv->zedx_id);
	if(priv->acc_addr < 0){
		dev_err(dev, "%s: Acc i2c addr not found %d\n", __func__,
			priv->acc_addr);
		return priv->acc_addr;
	}

	priv->gyro_addr = ser_get_gyro_addr(priv->zedx_id);
	if(priv->gyro_addr < 0){
		dev_err(dev, "%s: Gyro i2c addr not found %d\n", __func__,
			priv->gyro_addr);
		return priv->gyro_addr;
	}

	/* we try to find the /dev/videoX entry associated to this sensor */
	priv->zed_sysfs.video_id = find_video_device_by_subdev_name(priv->s_data->subdev.v4l2_dev, priv->s_data->subdev.name, &priv->zed_sysfs.parent_kobj);

	if (priv->zed_sysfs.video_id<0){
		tegracam_v4l2subdev_unregister(priv->tc_dev);
		zedxhdr_eeprom_device_release(priv);
		tegracam_device_unregister(priv->tc_dev);
		kobject_put(&priv->zed_sysfs.info_kobj);
		dev_err(dev, "%s: Video device id not found\n", __func__);
		return priv->zed_sysfs.video_id;
	}

	/* we create the sysfs info entry */
	if (kobject_init_and_add(&priv->zed_sysfs.info_kobj, &zed_info_kobj_type, &priv->zed_sysfs.parent_kobj, "zed_info")<0) {
		dev_err(dev, "%s: Failed to create sysfs entry\n", __func__);
		tegracam_v4l2subdev_unregister(priv->tc_dev);
		zedxhdr_eeprom_device_release(priv);
		tegracam_device_unregister(priv->tc_dev);
		kobject_put(&priv->zed_sysfs.info_kobj);
		return -ENOMEM;
	}

	/* we duplicate some information of priv in priv->sys_info*/
	duplicate_priv_info(priv);

	/* 
	 * This counter allows us to link all struct zedxhdr *priv together.
	 * It is needed for sensors that can't get access to the eeprom:
	 * we access the sensor that just got probed and retrieve its serial
	 * number to assign it to the current sensor (the one who got
	 * an eeprom access denial).
	 */
	priv->probe_counter = zedxhdr_probe_count;
	zedxhdr_array[priv->probe_counter] = priv;
	zedxhdr_probe_count++;

    dev_info(dev, "%s: success\n", __func__);

	return 0;
}

static void zedxhdr_shutdown(struct i2c_client *client){
	struct camera_common_data *s_data = NULL;
	struct zedxhdr *priv = NULL;
	int err = 0;
	int i = 0;
	unsigned long sn_ioctl = 0;
	unsigned long sn_eeprom = 0;
	u8 buff[4] = {};

	s_data = to_camera_common_data(&client->dev);
	priv = (struct zedxhdr *)s_data->priv;

	if(!priv->ioctl_updated){
		dev_dbg(&priv->i2c_client->dev,"IOCTL !updated\r\n");
		return;
	}

	priv->ioctl_updated = false;

	/* eeprom interface */
	err = zedxhdr_eeprom_device_init(priv);
	if (err)
		dev_info(&priv->i2c_client->dev, "Failed to allocate eeprom reg map: %d\n", err);

	if(priv->eeprom[1].regmap == NULL){
		dev_info(&priv->i2c_client->dev,"Regmap eeprom not set\r\n");
		err = zedxhdr_eeprom_device_release(priv);
		return;
	}

	err = dser_enable_gmsl_link(priv->channel,priv->zedx_id);
	if(err < 0){
		dev_info(&priv->i2c_client->dev,"Failed to enable gmsl link %d\r\n",err);
		err = zedxhdr_eeprom_device_release(priv);
		return;
	}

	msleep(100);

	err = regmap_bulk_read(priv->eeprom[0].regmap, 0, &buff, 4);
	if(err){
		dev_info(&priv->i2c_client->dev,"Failed to read eeprom to save calibrations\r\n");
		err = zedxhdr_eeprom_device_release(priv);
		if(err) dev_dbg(&priv->i2c_client->dev,"Failed to release eeprom %d\r\n",err);
		return;
	}

	for (i = 3; i >= 0; i--){
		sn_eeprom = (sn_eeprom << 8) | buff[i];
		sn_ioctl = (sn_ioctl << 8) | priv->eeprom_buf[i];
	}

	dev_dbg(&priv->i2c_client->dev,"Sn ioctl/eeprom: %ld %ld %ld\r\n",sn_ioctl,sn_eeprom, priv->zed_sysfs.serial_number);
	for (i = 0; i < 4; i++){
		dev_dbg(&priv->i2c_client->dev,"ioctl/eeprom: %02x/%02x\r\n",priv->eeprom_buf[i],buff[i]);
	}

	if(sn_eeprom != sn_ioctl){
		dev_info(&priv->i2c_client->dev,"Sn eeprom does not match\r\n");
		err = zedxhdr_eeprom_device_release(priv);
		if(err) dev_dbg(&priv->i2c_client->dev,"Failed to release eeprom %d\r\n",err);
		err = dser_open_all_gmsl_link(priv->channel);
		if(err) dev_dbg(&priv->i2c_client->dev,"Failed to open all gmsl %d\r\n",err);

		return;
	}

	for(i = 0;i<16;++i){ // eeprom can be written only 16 bytes at a time
		err = regmap_bulk_write(priv->eeprom[0].regmap,16 * i, &priv->eeprom_buf[16*i], 16); // We write only to the 2nd 256 bytes
		if(err)
			dev_dbg(&priv->i2c_client->dev,"Failed to write eeprom %d\r\n",err);

		msleep(20);
	}

	err = zedxhdr_eeprom_device_release(priv);
	if(err)
		dev_dbg(&priv->i2c_client->dev,"Failed to release eeprom %d\r\n",err);

	err = dser_open_all_gmsl_link(priv->channel);
	if(err)
		dev_dbg(&priv->i2c_client->dev,"Failed to open all gmsl %d\r\n",err);

	msleep(100);

	dev_dbg(&priv->i2c_client->dev,"Eeprom successfully written\r\n");
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 0, 0)
static int zedxhdr_remove(struct i2c_client *client)
#else
static void zedxhdr_remove(struct i2c_client *client)
#endif
{
	struct device *dev = &client->dev;
	struct camera_common_data *s_data = NULL;
	struct zedxhdr *priv = NULL;

	s_data = to_camera_common_data(&client->dev);
	priv = (struct zedxhdr *)s_data->priv;

	zedxhdr_shutdown(client);

	kobject_put(&priv->zed_sysfs.info_kobj);

	if (priv->tc_dev){
		tegracam_v4l2subdev_unregister(priv->tc_dev);
		tegracam_device_unregister(priv->tc_dev);
	}

	zedxhdr_eeprom_device_release(priv);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0)
	v4l2_ctrl_handler_free(&priv->ctrl_handler);
#endif

	zedxhdr_probe_count--;
	if (zedxhdr_probe_count < 0)
		dev_alert(&client->dev,"%s: zedxhdr_probe_count < 0\n", __func__);

	dev_dbg(dev, " ZED-X sensor successfully removed\n");

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 0, 0)
	return 0;
#endif
}

static const struct i2c_device_id zedxhdr_id[] = {
	{ "zedxhdr", 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, zedxhdr_id);

static struct i2c_driver zedxhdr_i2c_driver = {
	.driver = {
		.name = "zedxhdr",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(zedxhdr_of_match),
		/* Drivers are loaded synchronously by default,
		 * but let's not risk anything. We need this feature
		 * to give the right serial number to stereocams sensors. */
		.probe_type = PROBE_FORCE_SYNCHRONOUS,
	},
	.probe = zedxhdr_probe,
	.remove = zedxhdr_remove,
	.shutdown = zedxhdr_shutdown,
	.id_table = zedxhdr_id,
};

static int __init zedxhdr_init(void)
{
	return i2c_add_driver(&zedxhdr_i2c_driver);
}

static void __exit zedxhdr_exit(void)
{
	i2c_del_driver(&zedxhdr_i2c_driver);
}

module_init(zedxhdr_init);
module_exit(zedxhdr_exit);

MODULE_DESCRIPTION("Media Controller driver for ZED-X HDR");
MODULE_AUTHOR("STEREOLABS <support@stereolabs.com>");
MODULE_LICENSE("GPL v2");
MODULE_VERSION(DRV_STR_VERSION);
