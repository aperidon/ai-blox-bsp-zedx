/*
 * max9296.c - max9296 IO Expander driver
 *
 * Copyright (c) 2016-2022, NVIDIA CORPORATION.  All rights reserved.
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
// #define DEBUG 1

#ifndef __SL_DESERIALIZER__
#define __SL_DESERIALIZER__
#define __MAX9296__
#else
#error Error, a deserializer is already compiled. Fix the defconfig and use only one deserializer.
#endif

#include <linux/seq_file.h>
#include <media/camera_common.h>
#include <linux/module.h>
#include "../include/bw_mgmt.h"
#include "sl_max9296_mode_tbls.h"
#include <linux/seq_file.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>

#define STRINGIFY0(s)
#define STRINGIFY(s) STRINGIFY0(s)
#define DRV_STR_VERSION STRINGIFY(DESER_DRIVER_VERSION_MAJOR)"."STRINGIFY(DESER_DRIVER_VERSION_MINOR)"."STRINGIFY(DESER_DRIVER_VERSION_PATCH)""

u32 fps_set_Dser(int channel, s64 val);
int fsync_set_Dser(int channel, bool on);
int isSecondCamFromI2C(int channel, int zedx_id);
int get_dser_addr(int channel);
int set_bitrate_Dser(int channel, u32 zedx_id, u8 val);
int dser_get_gmsl_port(int channel, int zedx_id);
int dser_enable_gmsl_link(int channel, int zedx_id);
int dser_open_all_gmsl_link(int channel);
int update_ISX_nor_flash(struct max9296 *priv, int addr);
int dser_read_video_lock(int channel, int zedx_id);
int dser_read_link_lock(int channel, int zedx_id);

struct max9296 *global_priv[4];
static int sync_mode = 0;
module_param(sync_mode, int, 0);

#define TOTAL_DETECTED_SENSORS (2 * NB_MAX_SERIALIZERS)

static bool isCameraStereo(const serializer_devices *ser)
{
	if (!ser)
		return false;

	/* Stereo camera families on MAX9296. */
	if (ser->camera_model == ZEDX || ser->camera_model == ZEDXHDR || ser->camera_model == ZEDXNANO)
		return true;

	return false;
}

/**
 * csi_tx_regval_to_bps - Convert CSI PHY rate register value to bits per second
 * @regval: Register value (typically 0x2a for 1000 Mbps, 0x30 for 1600 Mbps)
 * @lanes: Number of CSI lanes (typically 4 for MAX9296)
 *
 * Matches MAX96712 encoding: rate_code = regval & 0x1f, mbps = rate_code * 100
 * For MAX9296: 0x2a & 0x1f = 10 → 1000 Mbps
 *            0x30 & 0x1f = 16 → 1600 Mbps
 */
static u64 csi_tx_regval_to_bps(u8 regval, u32 lanes)
{
	u64 mbps;
	u8 rate_code;
	u64 bps;

	rate_code = regval & 0x1f;  /* Mask lower 5 bits [4:0] */
	mbps = (u64)rate_code * 100ULL;

	bps = (mbps * 1000000ULL) * lanes;
	return bps;
}

static int write_reg_Dser(struct max9296 *priv, int slaveAddr, int channel, u16 addr, u8 val)
{
	struct i2c_client *i2c_client = NULL;
	int bak = 0;
	int err;

	if (!priv)
		return -1;

    if (priv->channel < 0 || priv->channel > 3)
		return err;

	i2c_client = global_priv[channel]->i2c_client;
	bak = i2c_client->addr;

	i2c_client->addr = slaveAddr;
	err = regmap_write(priv->regmap, addr, val);

	i2c_client->addr = bak;
	if (err)
	{
		dev_err(&i2c_client->dev, "%s: addr = 0x%x, val = 0x%x\n",
				__func__, addr, val);
		return -1;
	}
	return 0;
}

// static int sl_max9296_read_reg(struct max9296 *priv, u16 addr,
//         unsigned int *val)
// {
//     struct i2c_client *i2c_client = priv->i2c_client;
//     int err;

//     err = regmap_read(priv->regmap, addr, val);
//     if (err)
//         dev_err(&i2c_client->dev, "%s:i2c read failed, 0x%x = %x\n",
//                 __func__, addr, *val);

//     return err;
// }

static int sl_max9296_write_table(struct max9296 *priv,
		const struct index_reg_8 table[])
{
	struct i2c_client *i2c_client = priv->i2c_client;
	int i = 0, j = 0;
	int ret = 0;
	int retry = 5;

	// While we haven't reach the end of the table
	while (table[i].addr != MAX9296_TABLE_END)
	{
		// While we haven't tried to write the register 'retry' times
		for (j = 0; j < retry; j++)
		{
			// We try to write the register. 0 means i2c master
			ret = write_reg_Dser(priv,priv->i2c_address, priv->channel, table[i].addr, (u8)table[i].val);
			// dev_err(&i2c_client->dev, "%s: write_reg_Dser: 0x%x, %d 0x%x, 0x%x %d\n",__func__,
					// priv->i2c_address,priv->channel,table[i].addr, table[i].val , ret);
			// if the return value is bad
			if (ret && (table[i].addr != 0x0010))
			{
				dev_warn(&i2c_client->dev, "write_reg_Dser: try %d\n", j);
				msleep(4);
				// if we have retried 'retry' number of time we exit
				if (j == retry-1)
					return -1;
				//else, we retry
				continue;
			}
			// If we write a reset register
			if ( 0x0010 == table[i].addr)
				msleep(100);

			/* Write went well, we can go on */
			break;
		}
		i++;
	}
	return 0;
}

static int sl_max9296_reset(struct max9296 *priv){
	int err;

	if (!priv)
		return -1;

	err = sl_max9296_write_table(priv, max9296_deser_reset);

	if (err)
		return -1;

	return 0;
}

static int sl_max9296_set_PHY_speed(struct max9296 *priv, int phy, u32 n_lanes){
	int err = 0;
	int speed = speed_table[priv->cam_id];
	int phy_idx = phy - 1;

	struct index_reg_8 spd_arr[] = {
		{ 0x031D + (phy*3), speed}, // PHY clock rate -  1600MBPS + disable fine tune
		{MAX9296_TABLE_END, 0x00},
	};

	err = sl_max9296_write_table(priv, spd_arr);

	/* Cache PHY capacity indexed by PHY index (serializers sharing a PHY share capacity) */
	if (!err && phy_idx >= 0 && phy_idx < MAX_PHY_PER_DESER) {
		priv->csi_rate_reg_val[phy_idx] = (u8)speed;
		priv->csi_rate_bps[phy_idx] = csi_tx_regval_to_bps((u8)speed, n_lanes);
		dev_dbg(&priv->i2c_client->dev,
			"PHY%d CSI capacity set: regval=0x%02x lanes=%u => capacity=%llu Mbps\n",
			phy_idx, (u8)speed, n_lanes, priv->csi_rate_bps[phy_idx] / 1000000);
	}

	return err;
}

// configuration for multiple board synchronization
// trig_info == HIGH/NC > mfp_trig_info == LOW > Master mode: Internal Fsync + output Fsync 
// trig_info == LOW > mfp_trig_info == HIGH > Slave mode: External Fsync
// sync_mode == 0 > Master mode: Internal Fsync + output Fsync on MFP0
// sync_mode == 1:
//  > Master mode deser 1: Internal Fsync + output Fsync MFP0
//  > Slave mode deser 2: External Fsync + input Fsync on mfp_trig_in
// sync_mode == 2 > Slave mode: External Fsync + input Fsync on mfp_trig_in
static int sl_max9296_configure_sync_mode(struct max9296 *priv){
	int err = 0;
	unsigned int hw_rqst_slave_mode = 0; // 0: Master mode, 1: Slave mode
	int mfp_trig_info_addr = 0;

	// Check if the mfp-trig-info has been configured in the device tree
	// This MFP is used to automatically configure the slave mode without having to set sync_mode when loading the driver
	if(priv->mfp_trig_info != -1){
		// MFP0 is 0x2B0 and each GPIO is offset by 3 register
		mfp_trig_info_addr = MAX9296_GPIO_ADDR + (3*priv->mfp_trig_info);

		err = regmap_read(priv->regmap, mfp_trig_info_addr, &hw_rqst_slave_mode);

		if(err < 0) // Retry reading the register
		{	
			msleep(10);
			err = regmap_read(priv->regmap, mfp_trig_info_addr, &hw_rqst_slave_mode);
			if(err < 0){
				dev_err(&priv->i2c_client->dev, "%s: Failed to read mfp trig info state (%d) - Defaulting to master mode \n",
					__func__, err);
				return 0;
			}
		}
		msleep(10);
		
		// MFP value is written in bit 3, if MFP is HIGH then the board mode = slave (1)
		hw_rqst_slave_mode = (hw_rqst_slave_mode >> 3) & 0x01;
	}

	// Master/Slave mode, first deser(channel) is configured as master
	// If board mode == 1 this is always a slave mode
	// If sync_mode == 1 we are in a master/slave configuration so channel 0 is configured as master
    if(!hw_rqst_slave_mode && sync_mode == 1 && priv->channel == 0){ 
        return err;
    }

	if(hw_rqst_slave_mode || sync_mode > 0) {
		struct index_reg_8 slave_mode_table[10] = {0};
		err = get_max9296_slave_mode_table(priv->mfp_trig_in, slave_mode_table, sizeof(slave_mode_table));
		if(err){
			dev_err(&priv->i2c_client->dev, "%s: slave mode table failed to initialize: %d\n",
				__func__, err);
			return err;
		}
		err = sl_max9296_write_table(priv, slave_mode_table);
		if (err) {
			dev_err(&priv->i2c_client->dev, "%s: failed to write slave mode table: %d\n",
				__func__, err);
			return err;
		}
		priv->mode = SLAVE_MODE;
		dev_info(&priv->i2c_client->dev, "Sync mode configured\n");
	}

	return err;
}

static int sl_max9296_set_pipe_routing(struct max9296 *priv,CamType cam_type,int pipe,int phy,int vc_id){
	int i = 0;
	int err = 0;
	int data_type = (cam_type == ZEDONEHDR 
					|| cam_type == ZEDXHDR)?0x1e:(cam_type == ZEDONE4K)?
						0x00:0x2b;
	
	struct index_reg_8 routing_table[] = {
		{ 0x0309 + (pipe*2), 1<<vc_id},   // FRONTTOP3 - Select the virtual channel 0 for pipe Y
		{ 0x040b + (pipe*0x40) , 0x07}, // Enable mapping registers 0 to 2 for MIPI PHY 1
		// This phy is connected to pipe Y stream 0
		{ 0x040d + (pipe*0x40), data_type}, // When the stream source is 0 with raw 10 data type
		{ 0x040e + (pipe*0x40), data_type + (vc_id << 6)}, // mark it as virtual channel 0 with raw 10 data type
		// and send it through MIPI port x.
		{ 0x040f + (pipe*0x40), 0x00 + (vc_id << 6)}, // Frame start with id 0 on PHY 1...
		{ 0x0410 + (pipe*0x40), 0x00 + (vc_id << 6)}, // Is sent as frame start with id 1 through MIPI port x.
		{ 0x0411 + (pipe*0x40), 0x01 + (vc_id << 6)}, // Frame end with id 0 on PHY 1...
		{ 0x0412 + (pipe*0x40), 0x01 + (vc_id << 6)}, // is sent as frame end with id 1 through MIPI port x.
		{ 0x042d + (pipe*0x40), (phy*0x15)}, // Map mapping registers 0 to 2 to MIPI PHY 1 : x=1
		{MAX9296_TABLE_END, 0x00},
	};

	dev_dbg(&priv->i2c_client->dev, "%s cam_type:%d pipe table:",
		__func__,(int)cam_type);

	for(i = 0;i<sizeof(routing_table)/sizeof(routing_table[0]);i++){
		dev_dbg(&priv->i2c_client->dev, "%s: 0x%x 0x%x 0x%x",
			__func__,priv->i2c_client->addr,routing_table[i].addr,routing_table[i].val);
	}

	err = sl_max9296_write_table(priv, routing_table);

	return err;
}

static int sl_max9296_gmsl_pipeline_init(struct max9296 *priv, I2cIndex i2c_index)
{
	int err = 0;
	bool test = false;

	// mono port > gmsl 1 identique - gmsl2 : 	phy = 0x15, 
    // 											vc_id = 2 pour mono
	// 													2/3 pour stereo 
	// dual port > gmsl1 identique - gmsl2 : 	phy = 0x2a
	// 										 	vc_id = 2 pour mono
	//													2/3 pour stereo

	// bool dual_port = false;
	int phy = 1;
	int pipe = (i2c_index==0)?0:2;
	int vc_id1 = priv->current_ser_device[i2c_index].vc_id[0];
	int vc_id2 = priv->current_ser_device[i2c_index].vc_id[1];

	// if(i2c_index == 0)
	// {
	// 	vc_id1 = 0;
	// 	vc_id2 = 1*((CamType)priv->cam_id == ZEDX);
	// }
	// else
	// {
	// 	vc_id1 = 2;
	// 	vc_id2 = 3*((CamType)priv->cam_id == ZEDX);
	// }

	// if(dual_port)
	// {
	// 	phy = (i2c_index==0)?1:2;
	// }
	if(priv->current_ser_device[i2c_index].phy_index == 0)
		phy = 1;
	else if(priv->current_ser_device[i2c_index].phy_index == 1)
		phy = 2;

	if(test)
	{
		err = sl_max9296_write_table(priv, max9296_test2);
		return err;
	}

	if (priv->cam_id < 0)
	{
		dev_info(&priv->i2c_client->dev, "%s ERROR cam id < 0",
			__func__);
		return -EINVAL;
	}

	(void)reset_table;
	(void)max9296_zedx_sensor_reset;

	err = sl_max9296_write_table(priv, default_pipe_conf);
	if(err) return err;

	err = sl_max9296_set_PHY_speed(priv, phy, priv->current_ser_device[i2c_index].n_lanes);
	if(err) return err;

	sl_max9296_set_pipe_routing(priv, priv->cam_id, pipe, phy, vc_id1);
	if(err) return err;

	sl_max9296_set_pipe_routing(priv, priv->cam_id, pipe+1, phy, vc_id2);
	if(err) return err;

	err = sl_max9296_write_table(priv, default_misc_conf);
	if(err) return err;

	msleep(10);

	err = sl_max9296_configure_sync_mode(priv);
	if(err) return err;

	/* Cache the FSYNC config register once, fsync_set_Dser then toggles 
	 * only the gate bit off this shadow */
	{
		unsigned int v;
		if (!regmap_read(priv->regmap, MAX9296_FSYNC_CONF, &v))
			priv->fsync_conf = (u8)v;
		else /* read failed: fall back to a mode-consistent enabled value */
			priv->fsync_conf = (priv->mode == SLAVE_MODE) ? 0x08 : 0x24;
	}

	priv->current_ser_device[i2c_index].dsr_pipe = pipe;

	return err;
}

int get_dser_addr(int channel){
	// dev_info(&global_priv[channel]->i2c_client->dev,"%s\n", __func__);
	return global_priv[channel]->i2c_address;
}
EXPORT_SYMBOL(get_dser_addr);

int dser_open_all_gmsl_link(int channel){
	int err = 0;
	u16 reg_val = max9296_deser_i2c_table[global_priv[channel]->gmsl_link].val;
	// dev_info(&global_priv[channel]->i2c_client->dev,"%s: Enable all gmsl link %d\n", __func__,reg_val);
	
	err = regmap_write(global_priv[channel]->regmap, GMSL_LINK_CTRL_REG, reg_val);
	if(err){
		msleep(4);
		err = regmap_write(global_priv[channel]->regmap, GMSL_LINK_CTRL_REG, reg_val);
	}

	return err;
}
EXPORT_SYMBOL(dser_open_all_gmsl_link);

static int get_video_pipe(int channel, int zedx_id){
	int i = 0;
	int err = -1;
	int nb_devices = global_priv[channel]->nb_ser_devices;

	for(i = 0; i < nb_devices; i++){
		if(global_priv[channel]->ser_devices[i].zedx_id == zedx_id)
		{
			if(global_priv[channel]->ser_devices[i].dsr_pipe == -1)
				return err;

			return global_priv[channel]->ser_devices[i].dsr_pipe;
		}
	}

	return err;
}

int dser_read_link_lock(int channel, int zedx_id){
    return -255; // link lock not supported
}
EXPORT_SYMBOL(dser_read_link_lock);

int dser_read_video_lock(int channel, int zedx_id){
    int err = -1;
    int val = 0;
    int dsr_pipe = -1;
	const int NB_DSER_PIPES = 4;

    if (channel < 0 || channel > 3)
        return err;

    dsr_pipe = get_video_pipe(channel, zedx_id);

    if(dsr_pipe < 0 || dsr_pipe >= NB_DSER_PIPES)
		return err;

	if(global_priv[channel]->regmap == NULL)
		return err;

    err = regmap_read(global_priv[channel]->regmap, (VIDEO_LOCK_STATUS_REG + 0x20*dsr_pipe), &val);
    if(err)
        return err;

    val = val & 0x01;

    return val;
}
EXPORT_SYMBOL(dser_read_video_lock);

int set_bitrate_Dser(int channel, u32 zedx_id, u8 val)
{
	int err = -1;
	u8 reg;

	if (channel > 3 || channel < 0 || global_priv[channel] == NULL)
		return -1;

	switch (val) {
		case 8:  reg = GMSL_8BIT_MODE;  break;
		case 10: reg = GMSL_10BIT_MODE; break;
		case 12: reg = GMSL_12BIT_MODE; break;
		default:
			dev_info(&global_priv[channel]->i2c_client->dev,
				"%s: %d is not a supported value. Must be 8, 10 or 12.\n",
				__func__, val);
			return -1;
	}

	err = regmap_write(global_priv[channel]->regmap, GMSL_VID_ST_MAP, reg);

	err = regmap_write(global_priv[channel]->regmap, GMSL_VID_ST_MAP+1, reg);

	return err;
}
EXPORT_SYMBOL(set_bitrate_Dser);

int dser_get_gmsl_port(int channel, int zedx_id){
	int i = 0;
	int err = -1;
	int nb_devices = global_priv[channel]->nb_ser_devices;

	if (global_priv[channel]->intialized == 0)
        return err;
	
	for(i = 0; i < nb_devices; i++){
		if(global_priv[channel]->ser_devices[i].zedx_id == zedx_id)
		{
			if(global_priv[channel]->ser_devices[i].gmsl_index == -1)
				return err;

			return global_priv[channel]->ser_devices[i].gmsl_index;
		}
	}

	return err;
}
EXPORT_SYMBOL(dser_get_gmsl_port);

int dser_enable_gmsl_link(int channel, int zedx_id){
	int i = 0;
	int err = -1;
	int nb_devices = global_priv[channel]->nb_ser_devices;

	if (global_priv[channel]->intialized == 0)
        return err;
	
	for(i = 0; i < nb_devices; i++){
		if(global_priv[channel]->ser_devices[i].zedx_id == zedx_id)
		{
			if(global_priv[channel]->ser_devices[i].gmsl_index == -1)
				return err;

			if(global_priv[channel]->regmap == NULL)
				return err;

			err = regmap_write(global_priv[channel]->regmap, GMSL_LINK_CTRL_REG, (0x20+(global_priv[channel]->ser_devices[i].gmsl_index+1)));
			if(err){
				msleep(4);
				err = regmap_write(global_priv[channel]->regmap, GMSL_LINK_CTRL_REG, (0x20+(global_priv[channel]->ser_devices[i].gmsl_index+1)));
			}

			dev_dbg(&global_priv[channel]->i2c_client->dev,
			 	"%s: open GMSL link %d for zedx-id %d\n",
            __func__, global_priv[channel]->ser_devices[i].gmsl_index, global_priv[channel]->ser_devices[i].zedx_id);
			
			if(err < 0) 
				return err;
			else 
				return global_priv[channel]->ser_devices[i].gmsl_index;
		}
	}

	return err;
}
EXPORT_SYMBOL(dser_enable_gmsl_link);

int isSecondCamFromI2C(int channel, int zedx_id){
	u8 i = 0;

	if (global_priv[channel]->intialized == 0)
        return -1;

    for(i=0; i < global_priv[channel]->nb_ser_devices; i++)
    {
        if(global_priv[channel]->ser_devices[i].zedx_id == zedx_id){
			return (global_priv[channel]->ser_devices[i].gmsl_index > 0);
		}
    }

    return -1;
}
EXPORT_SYMBOL(isSecondCamFromI2C);

u32 fps_set_Dser(int channel, s64 val)
{
	int err = -1;
	s64 fps_reg_val;
	const int deser_clk_freq = 25000000; /* 25 MHz */

	struct index_reg_8 fps_arr[] = {
		{0x03E5, 0x35}, /* frame rate byte 0 (LSB) */
		{0x03E6, 0xB7}, /* frame rate byte 1 */
		{0x03E7, 0x0C}, /* frame rate byte 2 (MSB) */
		{MAX9296_TABLE_END, 0x00}
	};

	if (channel > 3 || channel < 0 || global_priv[channel] == NULL)
		return -1;

	dev_dbg(&global_priv[channel]->i2c_client->dev,
		"%s: Requested FPS = %lld\n", __func__, val);

	if (val < 2000000) {
		dev_dbg(&global_priv[channel]->i2c_client->dev,
			"%s: %lld is not a supported value. Must be > 2,000,000.\n",
			__func__, val);
		return 0xEEEE;
	}

	/* Round to nearest fps */
	val = (val + 500000) / 1000000;

	fps_reg_val = deser_clk_freq / val;

	dev_dbg(&global_priv[channel]->i2c_client->dev,
		"%s: Setting FPS to %lld reg value = %lld\n",
		__func__, val, fps_reg_val);

	fps_arr[2].val = (u8)((fps_reg_val >> 16) & 0xFF);
	fps_arr[1].val = (u8)((fps_reg_val >> 8) & 0xFF);
	fps_arr[0].val = (u8)(fps_reg_val & 0xFF);

	err = sl_max9296_write_table(global_priv[channel], fps_arr);

	return err;
}
EXPORT_SYMBOL(fps_set_Dser);

/* Enable/Disable the FSYNC generation */
int fsync_set_Dser(int channel, bool on)
{
	u8 val, gate;
	int err;

	if (channel > 3 || channel < 0 || global_priv[channel] == NULL)
		return -1;

	/* Toggle only the gate bit off the cached config so the FSYNC mode and MFP
	 * output routing bits are preserved */
	gate = (global_priv[channel]->mode == SLAVE_MODE) ? 0x04 : 0x08;
	val = global_priv[channel]->fsync_conf;

	if (on)
		val &= ~gate;
	else
		val |= gate;

	err = write_reg_Dser(global_priv[channel], global_priv[channel]->i2c_address,
		channel, MAX9296_FSYNC_CONF, val);
	if (!err)
		global_priv[channel]->fsync_conf = val;

	return err;
}
EXPORT_SYMBOL(fsync_set_Dser);

static int sl_max9296_parse_serializer_node(struct max9296 *priv,
		struct device_node *ser_node, s8 ser_index)
{
	const char *str = "a";
	const char *zedx_id;
    struct i2c_client *i2c_client = priv->i2c_client;
	struct device_node *cam_np;
	struct device_node *ports_node;
	struct device_node *port_node;
	struct device_node *endpoint_node;
    u8 j, i;
	int err;
	int n_sensors = 0;

	if (!ser_node)
		return -1;

	err = of_property_read_string(ser_node, "camera_model", &priv->cam_name);
	if (err){
		dev_err(&i2c_client->dev,
				"%s: 'camera_model' missing in serializer node %s",__func__,ser_node->full_name);
		return -EINVAL;
	}

	for (j=0; j<N_CAM_TYPE; j++)
	{
		if (strcmp(priv->cam_name, camera_names[j])==0){
			priv->ser_devices[ser_index].camera_model = j;
			break;
		}
	}

	/* Parse zedx-id */
	err = of_property_read_string(ser_node, "zedx-id", &zedx_id);
	if(err){
		dev_err(&i2c_client->dev, "%s: 'zedx-id' missing in serializer node %s (if it is a dummy, set zedx_id = -1)",__func__,ser_node->full_name);
		return -EINVAL;
	}

	err = kstrtoint(zedx_id,10,&priv->ser_devices[ser_index].zedx_id);
	if(err)
	{
		dev_err(&i2c_client->dev, "%s: zedx-id conversion to int failed", __func__);
		return -1;
	}

	if(priv->ser_devices[ser_index].zedx_id == -1)
	{
		dev_dbg(&i2c_client->dev, "%s: Parse Dummy %d (%s)", 
        __func__, priv->ser_devices[ser_index].zedx_id, priv->cam_name);
		return 0;
	}

	err = of_property_read_u32(ser_node, "reg", &priv->ser_devices[ser_index].ser_addr);
	if (err){
		dev_err(&i2c_client->dev,
				"%s: 'reg' missing in serializer node %s",__func__,ser_node->full_name);
		return -EINVAL;
	}

	n_sensors = of_count_phandle_with_args(ser_node, "camera-sensors", NULL);

	if(priv->ser_devices[ser_index].zedx_id >= 0)
	{
		for(i=0; i<n_sensors; i++)
		{
			cam_np = of_parse_phandle(ser_node, "camera-sensors" , i);

			if ( cam_np == NULL ){
				dev_info(&i2c_client->dev,
						"%s: Issue getting node pointer to camera %d in serializer id %d\n", __func__, i, priv->ser_devices[ser_index].zedx_id);
				return -EINVAL;
			}

			err = of_property_read_u32(cam_np, "reg", &priv->ser_devices[ser_index].cam_addr[i]);
			if (err){
				of_node_put(cam_np);
				dev_err(&i2c_client->dev,
						"%s: 'reg' missing in camera node %s",__func__,cam_np->full_name);
				return -EINVAL;
			}

			ports_node = of_get_child_by_name(cam_np, "ports");
			if ( ports_node == NULL ){
				of_node_put(cam_np);
				dev_info(&i2c_client->dev,
						"%s: Issue getting node pointer to ports of camera node %s\n", __func__, cam_np->full_name);
				return -EINVAL;	
			}
			port_node = of_get_child_by_name(ports_node, "port");
			if (port_node==NULL){
				dev_info(&i2c_client->dev,
						"%s: Issue getting node pointer to port of camera node %s\n", __func__, cam_np->full_name);
				of_node_put(cam_np);
				of_node_put(ports_node);
				return -EINVAL;	
			}

			endpoint_node = of_get_child_by_name(port_node, "endpoint");
			if (endpoint_node==NULL){
				dev_warn(&i2c_client->dev, "%s: no endpoint node amongst children...\n", __func__);
				of_node_put(cam_np);
				of_node_put(ports_node);
				of_node_put(port_node);
				return -EINVAL;	
			}

			err = of_property_read_u32(endpoint_node, "vc-id", &priv->ser_devices[ser_index].vc_id[i]);
			if (err){
				dev_err(&i2c_client->dev,
						"%s: 'vc-id' missing in camera node %s", __func__, cam_np->full_name);
				of_node_put(cam_np);
				of_node_put(ports_node);
				of_node_put(port_node);
				of_node_put(endpoint_node);
				return -EINVAL;
			}

			err = of_property_read_u32(endpoint_node, "port-index", &priv->ser_devices[ser_index].serial);
			if (err)
			{
				dev_err(&i2c_client->dev,
						"%s: 'port-index' missing in camera node %s", __func__, cam_np->full_name);
				of_node_put(cam_np);
				of_node_put(ports_node);
				of_node_put(port_node);
				of_node_put(endpoint_node);
				return -EINVAL;
			}

			err = of_property_read_string(endpoint_node, "opt-csi-port", &str);
			if (err){
				of_node_put(cam_np);
				of_node_put(ports_node);
				of_node_put(port_node);
				of_node_put(endpoint_node);
				dev_err(&i2c_client->dev,
						"%s: 'opt-csi-port' missing in camera node %s",__func__,cam_np->full_name);
				return -EINVAL;
			}
			priv->ser_devices[ser_index].phy_index = str[0] - 'a';

			err = of_property_read_u32(endpoint_node, "bus-width", &priv->ser_devices[ser_index].n_lanes);
			if (err) {
				of_node_put(cam_np);
				of_node_put(ports_node);
				of_node_put(port_node);
				of_node_put(endpoint_node);
				dev_err(&i2c_client->dev,
					"%s: 'bus-width' missing in camera node %s", __func__, cam_np->full_name);
				return -EINVAL;
			}
			if (priv->ser_devices[ser_index].n_lanes != 2 && priv->ser_devices[ser_index].n_lanes != 4) {
				of_node_put(cam_np);
				of_node_put(ports_node);
				of_node_put(port_node);
				of_node_put(endpoint_node);
				dev_err(&i2c_client->dev,
					"%s: bus-width=%u invalid for camera %s (2 or 4 only)", __func__,
					priv->ser_devices[ser_index].n_lanes, cam_np->full_name);
				return -EINVAL;
			}

			of_node_put(cam_np);
			of_node_put(ports_node);
			of_node_put(port_node);
			of_node_put(endpoint_node);
		}
	}
	else
	{
		dev_warn(&i2c_client->dev, "%s: %s zedx-id invalid %d", 
        __func__, priv->cam_name, priv->ser_devices[ser_index].zedx_id);
		return 0;
	}

	dev_dbg(&i2c_client->dev, "%s: Parse camera %d (%s) -> SER addr : %d | CAM addr : %d/%d",
        __func__, priv->ser_devices[ser_index].zedx_id, priv->cam_name, priv->ser_devices[ser_index].ser_addr,
		priv->ser_devices[ser_index].cam_addr[0], priv->ser_devices[ser_index].cam_addr[1]);

    dev_dbg(&i2c_client->dev, "%s: (phy %d | vc-id %d/%d) -> serial port %d | lanes %d",
        __func__, priv->ser_devices[ser_index].phy_index, priv->ser_devices[ser_index].vc_id[0], priv->ser_devices[ser_index].vc_id[1], priv->ser_devices[ser_index].serial, priv->ser_devices[ser_index].n_lanes);
	dev_dbg(&i2c_client->dev, "%s: -------------------------------------------------------------", 
        __func__);

	return 0;
}

static int sl_max9296_parse_dt(struct max9296 *priv)
{
	struct i2c_client *i2c_client = priv->i2c_client;
	struct device_node *ser_np, *np = i2c_client->dev.of_node;
	const char *str;
	int err = 0;
	int n_serializers;
	u8 i;

	if (!np)
		return -EINVAL;

	priv->pwdn_gpio = of_get_named_gpio(np, "pwdn-gpio", 0);
	if (priv->pwdn_gpio > 0) {
		gpio_direction_output(priv->pwdn_gpio, 0);
		gpio_direction_output(priv->pwdn_gpio, 1);
		gpio_set_value(priv->pwdn_gpio, 1);
	}
	else{
		dev_dbg(&i2c_client->dev,
				"%s: no GPIOs given in the device tree\n",__func__);
	}

	err = of_property_read_string(np, "channel", &str);

	if (err)
		dev_err(&i2c_client->dev,
				"%s: channel not found, DTS misses the 'channel'\n",
				__func__);

	priv->channel = str[0] - 'a';

	if (priv->channel < 0 || priv->channel > 4){
		dev_err(&i2c_client->dev,
			"%s: channel value incorrect in DTS: %d\n",
			__func__,priv->channel);
			return -EINVAL;
	}

	err = of_property_read_u32(np, "reg", &priv->i2c_address);
	if (err)
		dev_err(&i2c_client->dev,
				"%s: i2c address not found, DTS misses the 'reg' entry\n",
				__func__);

	if (err < 0)
		return -EINVAL;
		
	priv->mfp_trig_info = -1;
	err = of_property_read_u32(np, "mfp-trig-info", &priv->mfp_trig_info);
	if (err || (priv->mfp_trig_info < 0 || priv->mfp_trig_info > MAX9296_NB_MFP)){
		dev_dbg(&i2c_client->dev,
				"%s: 'mfp-trig-info' not found or invalid, assuming no trigger info\n", __func__);
		priv->mfp_trig_info = -1;
	}

	priv->mfp_trig_in = -1;
	err = of_property_read_u32(np, "mfp-trig-in", &priv->mfp_trig_in);
	if (err || priv->mfp_trig_in < 0 || priv->mfp_trig_in > MAX9296_NB_MFP){
		dev_dbg(&i2c_client->dev,
				"%s: 'mfp-trig-in' not found or invalid, defaulting to MFP1\n", __func__);
		priv->mfp_trig_in = 1;
	}

    n_serializers = of_count_phandle_with_args(np, "camera-serializers", NULL);

    dev_dbg(&i2c_client->dev, "%s: Number of declared cameras with this dts %d\n",
     __func__, n_serializers);

	priv->nb_ser_devices = n_serializers;

    /* retrieve all information for each port */
	for ( i = 0 ; i < n_serializers ; i++ )
	{
		priv->ser_devices[i].gmsl_index = -1;

		ser_np = of_parse_phandle(np, "camera-serializers" , i);

		if ( ser_np == NULL )
		{
			dev_info(&i2c_client->dev,
					"%s: Issue getting node pointer to serializer %d\n", __func__, i);
			continue;
		}
		err = sl_max9296_parse_serializer_node(priv, ser_np , i);

		if (err)
			dev_info(&i2c_client->dev,
					"%s: Issue parsing serializer node %d\n", __func__, i);
	
		of_node_put(ser_np);
	}

	for (i = 0; i < TOTAL_DETECTED_SENSORS; i++)
	{
		priv->detected_sensors[i].zedx_id = -1;
		priv->detected_sensors[i].serial = -1;
		mutex_init(&priv->detected_sensors[i].bw_lock);
	}

	for (i = 0; i < n_serializers; i++)
	{
		int primary_idx = i;
		int secondary_idx = i + NB_MAX_SERIALIZERS;

		priv->detected_sensors[primary_idx].zedx_id = priv->ser_devices[i].zedx_id;
		priv->detected_sensors[primary_idx].cam_addr = priv->ser_devices[i].cam_addr[0];
		priv->detected_sensors[primary_idx].serial = priv->ser_devices[i].serial;

		if (isCameraStereo(&priv->ser_devices[i]) && priv->ser_devices[i].cam_addr[1] != 0) {
			priv->detected_sensors[secondary_idx].zedx_id = priv->ser_devices[i].zedx_id;
			priv->detected_sensors[secondary_idx].cam_addr = priv->ser_devices[i].cam_addr[1];
			priv->detected_sensors[secondary_idx].serial = priv->ser_devices[i].serial;
		}
	}

	global_priv[priv->channel] = priv;

	return 0;
}

static int sl_max9296_enable_I2C(struct max9296 *priv, I2cIndex i2c_index){
	int err = 0;
	struct i2c_client *client = priv->i2c_client;

	if(i2c_index < MAX9296_I2C_LINK1 || i2c_index > MAX9296_I2C_LINK1_LINK2 )
		return -1;

	err = write_reg_Dser(priv,priv->i2c_address, priv->channel, max9296_deser_i2c_table[i2c_index].addr, max9296_deser_i2c_table[i2c_index].val);
	if(err) return err;

	dev_dbg(&client->dev, "%s: opened i2c: %d success\n", __func__, i2c_index);

	msleep(100);

	return err;
}

static struct regmap_config max9296_regmap_config = {
    .reg_bits = 16,
    .val_bits = 8,
    .cache_type = REGCACHE_NONE, //No cache for proper reset
};

// Reset 3Gbps serializer and set to 6Gpbs gmsl speed to match other camera speed
// Necessary for one HDR
static inline int configure_3Gbps_cameras_to_6Gbps(struct max9296 *priv){
	int i = 0;
	int err = 0;
	struct i2c_client *client = priv->i2c_client;

	// Set deserializer to 3Gpbs gmsl speed
	client->addr = priv->i2c_address;
	err = regmap_write(priv->regmap, GMSL_LINK_RATE_CTRL, MAX9296_GMSL_3GBPS_MODE);

	msleep(200);

	// Reset every possible 3Gbps serializer
	for (i = 0; i < priv->nb_ser_devices; i++){
		struct i2c_fingerprint reset_table[] = {
			{priv->ser_devices[i].ser_addr, 0x0010, 0x91}, /* Reset every possible serializer */
		};

		client->addr = reset_table[0].i2c_addr;
		err = regmap_write(priv->regmap, reset_table[0].reg_addr, reset_table[0].val);

		if(err ==0 ){
			msleep(94);
			break;
		}

		msleep(6);
	}

	// Set serializer to 6Gbps gmsl speed
	client->addr = ZED_ONE_SER_DFLT_ADDR;
	err = regmap_write(priv->regmap, GMSL_LINK_RATE_CTRL, MAX9295_GMSL_6GBPS_MODE);

	// Set deserializer to 6 Gbps gmsl speed
	client->addr = priv->i2c_address;
	err = regmap_write(priv->regmap, GMSL_LINK_RATE_CTRL, MAX9296_GMSL_6GBPS_MODE);

	msleep(200);
	
	return err;
}

/*
	Reset_Sensors reset all i2c Addr to the "factory settings" to make sure there isn't any 
	issues while checking/modifying the fingerprints.
*/
static void sl_max9296_sensor_reset(struct max9296 *priv){
	struct i2c_client *client = priv->i2c_client;
	int i = 0;
	int err = 0;
	int j = 0;
	int deser_addr=client->addr;
	struct i2c_fingerprint *fingerprint_res;

	/* Reset every serializer declared in DT */
	for (i = 0; i < priv->nb_ser_devices; i++){
		struct i2c_fingerprint reset_table[] = {
			{priv->ser_devices[i].ser_addr, 0x0010, 0x91}, 
		};

		client->addr = reset_table[0].i2c_addr;
		err = regmap_write(priv->regmap, reset_table[0].reg_addr, reset_table[0].val);
		if(err ==0){
			msleep(94);
			break;
		}

		msleep(6);
	}

	/* Configure 3Gpbs serializer (one hdr) to 6Gbps */
	configure_3Gbps_cameras_to_6Gbps(priv);

	/* Reset every possible sensors */
	for( i = 0; i < N_CAM_TYPE; i ++) //For every camera listed
	{
		fingerprint_res = reset_fingerprint_table[i]; 
		while (fingerprint_res[j].i2c_addr!=MAX9296_TABLE_END){
			client->addr = fingerprint_res[j].i2c_addr;

			if (fingerprint_res[j].reg_addr == 0x000D){
				msleep(94);
				j++;
				continue;
			}

			err = regmap_write(priv->regmap, fingerprint_res[j].reg_addr, fingerprint_res[j].val); //set regmap res_values

			if (fingerprint_res[j].reg_addr == 0x0010 && err == 0)
				msleep(94);

			dev_dbg(&client->dev, "%s: I2C device @0x%x (reg 0x%x), returns %d\n",
				__func__, fingerprint_res[j].i2c_addr,fingerprint_res[j].reg_addr, err);

			if(err)
				break;

			msleep(6);

			j++;
		}
		j=0;
	}

	client->addr = deser_addr;
}

/* Reads the camera EEPROM and returns the matching CamType.
 *
 * Tries each candidate address in EEPROM_I2C_ADDRS — the EEPROM lives at
 * different i2c addresses across camera variants.
 *
 * Returns -ENODEV when no address yields a valid EEPROM */
static int identify_via_eeprom(struct max9296 *priv)
{
	struct i2c_client *client = priv->i2c_client;
	struct device *dev = &client->dev;
	struct i2c_msg msgs[2];
	u8 reg_buf[1] = { 0x00 };
	u8 data[EEPROM_MODEL_BYTE_OFFSET + 1];
	bool found = false;
	u8 marker;
	int ret;
	size_t i;

	msgs[0].flags = 0;
	msgs[0].len   = sizeof(reg_buf);
	msgs[0].buf   = reg_buf;

	msgs[1].flags = I2C_M_RD;
	msgs[1].len   = sizeof(data);
	msgs[1].buf   = data;

	for (i = 0; i < ARRAY_SIZE(EEPROM_I2C_ADDRS); i++) {
		msgs[0].addr = EEPROM_I2C_ADDRS[i];
		msgs[1].addr = EEPROM_I2C_ADDRS[i];

		ret = i2c_transfer(client->adapter, msgs, 2);
		if (ret != 2) {
			dev_dbg(dev, "%s: eeprom @0x%02x not reachable (%d)\n",
				__func__, EEPROM_I2C_ADDRS[i], ret);
			continue;
		}

		if (data[EEPROM_KEY_BYTE_OFFSET] != EEPROM_KEY_VALUE) {
			dev_dbg(dev,
				"%s: device @0x%02x acked but key byte 0x%02x != 0x%02x, ignoring\n",
				__func__, EEPROM_I2C_ADDRS[i],
				data[EEPROM_KEY_BYTE_OFFSET], EEPROM_KEY_VALUE);
			continue;
		}

		dev_dbg(dev, "%s: eeprom found @0x%02x\n",
			__func__, EEPROM_I2C_ADDRS[i]);
		found = true;
		break;
	}

	if (!found)
		return -ENODEV;

	marker = data[EEPROM_MODEL_BYTE_OFFSET];
	dev_dbg(dev, "%s: eeprom[0..5]: %02x %02x %02x %02x %02x %02x\n",
		__func__, data[0], data[1], data[2], data[3], data[4], data[5]);

	switch (marker) {
	case EEPROM_ZEDX_MARKER1:        return ZEDX;
	case EEPROM_ZEDX_MARKER2:        return ZEDX;
	case EEPROM_ZEDX_MARKER3:        return ZEDX;
	case EEPROM_ZEDXHDR_MARKER1:     return ZEDXHDR;
	case EEPROM_ZEDXHDR_MARKER2:     return ZEDXHDR;
	case EEPROM_ZEDXHDR_MARKER3:     return ZEDXHDR;
	case EEPROM_ZEDXNANO_MARKER:    return ZEDXNANO;
	case EEPROM_ZEDONEGS_MARKER:    return ZEDONEGS;
	case EEPROM_ZEDONE4K_MARKER:    return ZEDONE4K;
	case EEPROM_ZEDONEHDR_MARKER:   return ZEDONEHDR;
	default:
		dev_dbg(dev, "%s: unknown eeprom marker 0x%02x\n",
			__func__, marker);
		return -ENODEV;
	}
}

/*
	Index Cam returns the index of the camera from the N_CAM_TYPE array to
	identify the fingerprints that have to be used.
*/
static int sl_max9296_get_cam(struct max9296 *priv){
	int i = 0, j = 0;
	unsigned int val = 0;
	int err;
	int index = -1;
	struct i2c_client *client = priv->i2c_client;
	int deser_addr=client->addr; //init addr
	struct i2c_fingerprint *fingerprint;

	sl_max9296_sensor_reset(priv); //reset sensors addr to "factory settings"

	/* Try EEPROM-based identification first — needed to distinguish
	 * ZEDX from ZEDXNANO since their fingerprints are identical. */
	index = identify_via_eeprom(priv);
	if (index >= 0 && index < N_CAM_TYPE) {
		dev_info(&client->dev, "%s: %s camera identified via eeprom",
			__func__, camera_names[index]);
		return index;
	}
	index = -1;

	dev_dbg(&client->dev,
		"%s: camera model not identified via eeprom, trying fingerprint\n",
		__func__);

	for( i=0; i < N_CAM_TYPE; i++) { // for every camera in N_CAM_TYPE

		dev_dbg(&client->dev, "%s: Check if cam model is %s",__func__, camera_names[i]);

		fingerprint = fingerprint_table[i];

		while (fingerprint[j].i2c_addr != MAX9296_TABLE_END) { // for every i2c addr listed in fingerprint
			client->addr = fingerprint[j].i2c_addr; 
			// Add stability delay for address change
			msleep(10);
			err = regmap_read(priv->regmap, fingerprint[j].reg_addr, &val);
			
			// If read fails, try once more with longer delay
			if (err) {
				msleep(10);
				err = regmap_read(priv->regmap, fingerprint[j].reg_addr, &val);
			}

			dev_dbg(&client->dev, "%s: I2C device @0x%x (reg 0x%x), read returns %d\n",
				__func__, fingerprint[j].i2c_addr, fingerprint[j].reg_addr, err);


			if(err) break;
			
			// Compare fingerprint values
			if (fingerprint[j].val != val)
				break;
			j++;
		}

		client->addr = deser_addr;

		if(fingerprint[j].i2c_addr == MAX9296_TABLE_END){	// if the whole finger print has been read (means that this is the right device)
			index = i;										// save index value and break loop
			break;
		}

		j = 0;	
	}

	return index;
};

/*
	update i2c addr to match hardware configuration
*/
static inline int sl_max9296_probe_camera(struct max9296 *priv, int gmsl_index){
	int j = 0; 
	bool first_ser = true;
	int selected_idx = -1;

	priv->cam_id = sl_max9296_get_cam(priv);

	if(priv->cam_id < 0){
        return -EINVAL;
	}
	
	// match detected serializer with the list from dtb
	// first serializer detected in the dtb gets first serializer detected
	// on I2C. This means that if only 1 camera is present, it will be
	// set accordingly to the first serializer declared(i2c address, dphy..)
	// this is used later by sl_zedx to enable only the gmsl link of a specific 
	// camera with dser_enable_gmsl_link()
	for(j = 0; j < priv->nb_ser_devices; j++){
		bool available = (priv->ser_devices[j].gmsl_index == -1);
		/* ZEDXNANO uses ZEDX nodes in the DTS — accept that pairing too. */
		bool model_match = (priv->cam_id == priv->ser_devices[j].camera_model) ||
			(priv->cam_id == ZEDXNANO && priv->ser_devices[j].camera_model == ZEDX);
		// dev_info(&client->dev, "%s: Serializer match - Id: %d Model: %d - gmsl_index: %d\n", __func__,
			// priv->cam_id, priv->ser_devices[j].camera_model, priv->ser_devices[j].gmsl_index);
		if(model_match && gmsl_index == 1 && first_ser){
			selected_idx = j;
			first_ser = false;
			// dev_info(&client->dev, "%s: FIRST SER\n", __func__);
			continue; // continue to get next serializer, initialize in case there is only one
		}

		if(model_match && available){
			selected_idx = j;
			priv->ser_devices[j].gmsl_index = gmsl_index;
			break;
		}

		// if first_ser is false a device has been detected - useful when only one device is declared in the DT (zed one uhd)
		if(j == priv->nb_ser_devices-1 && (first_ser == true)){ 
			return -1;
		}
	}

	if (selected_idx < 0)
		return -1;

	priv->current_ser_device[gmsl_index] = priv->ser_devices[selected_idx];

	return 0;
}

int update_ISX_nor_flash(struct max9296 *priv, int addr){
	int j = 0;
	int err = 0;
	struct i2c_client *client = priv->i2c_client;

	struct i2c_fingerprint update_rom_table[] = {
			{0x1a, 0x8A54, addr}, // Image sensor @ x10 unlock reg
			{0x1a, 0xFFFF, 0xF4}, // Image sensor @ x10 unlock reg
			{0x1a, 0xFFFF, 0xF7}, // Image sensor @ x10 unlock reg
			{0x1a, 0x8000, 0x04}, // Image sensor @ x10 unlock reg
			{0x1a, 0x8001, 0x19}, // Image sensor @ x10 unlock reg
			{0x1a, 0x8005, 0x5a}, // Image sensor @ x10 unlock reg
			{0x1a, 0xFFFF, 0xF5}, // Image sensor @ x10 unlock reg
			// {0x1a, 0x000D, 0x00}, // Image sensor @ x10 unlock reg
			// {0x1a, 0x000D, 0x00}, // Image sensor @ x10 unlock reg
			{MAX9296_TABLE_END, 0x00, 0x00},
	};

	while (update_rom_table[j].i2c_addr!=MAX9296_TABLE_END){
		client->addr = update_rom_table[j].i2c_addr;

		if(update_rom_table[j].reg_addr == 0x000D){
			msleep(100);
			j++;
			continue;
		}

		err = regmap_write(priv->regmap, update_rom_table[j].reg_addr, update_rom_table[j].val);
		if(err)
			return err;

		msleep(12);
		j++;
	}

	return err;
}

static int sl_max9296_translate_I2C(struct max9296 *priv, int gmsl_index){
	struct i2c_client *client = priv->i2c_client;
	struct i2c_fingerprint fingerprint_update[60];
	int deser_addr=client->addr;
	u8 j = 0;
	int err;
	unsigned int val = 0;
	u32 ser_addr = priv->current_ser_device[gmsl_index].ser_addr;
	u32 sen_1_addr = priv->current_ser_device[gmsl_index].cam_addr[0];
	u32 sen_2_addr = priv->current_ser_device[gmsl_index].cam_addr[1];
	// int cam_model = priv->current_ser_device[gmsl_index].camera_model;

	//dev_info(&client->dev, "%s: ser: 0x%x sen1: 0x%x sen2: 0x%x gmsl index: %d\n", __func__, ser_addr,sen_1_addr,sen_2_addr,gmsl_index);
	get_fingerprint_alt_table[priv->cam_id](fingerprint_update,ser_addr,sen_2_addr,sen_1_addr,gmsl_index);

	while (fingerprint_update[j].i2c_addr!=MAX9296_TABLE_END){
		client->addr = fingerprint_update[j].i2c_addr;


		if(fingerprint_update[j].reg_addr == 0x000D){
			msleep(100);
			// dev_info(&client->dev, "%s: SLEEP\n", __func__);
			j++;
			continue;
		}

		// For ISX only: we check whether the current address set in reg 8A54 is already properly set
		// if that is the case we skip to the next sensor, else we update the nor flash
		if(fingerprint_update[j].reg_addr == 0x8A54 && 
			(priv->cam_id == ZEDXHDR || priv->cam_id == ZEDONEHDR)){

			err = regmap_read(priv->regmap, fingerprint_update[j].reg_addr, &val);
			msleep(12);

			if (err)
				dev_err(&client->dev, "%s:i2c read failed, 0x%x = %x\n", __func__, fingerprint_update[j].reg_addr, val);

			if(err | (val != fingerprint_update[j].val)){
				dev_dbg(&client->dev, "%s:Update ISX NOR flash required\n", __func__);

				err = update_ISX_nor_flash(priv, fingerprint_update[j].val);
				if(err) {
					client->addr = deser_addr;
					dev_err(&client->dev, "%s: ISX NOR flash update failed\n", __func__);
					return err;
				}
			}
			else
				dev_dbg(&client->dev, "%s:Update ISX NOR flash skipped\n", __func__);

			j++;
			continue;
		}

		err = regmap_write(priv->regmap, fingerprint_update[j].reg_addr, fingerprint_update[j].val);

		dev_dbg(&client->dev, "%s: value written in 0x%x : 0x%x",
					__func__,fingerprint_update[j].reg_addr, val);

		if (err) break;

		msleep(12);
		j++;
	}
	client->addr = deser_addr;

	return 0;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 0, 0)
static int sl_max9296_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
#else
static int sl_max9296_probe(struct i2c_client *client)    
#endif
{
	struct device *dev = &client->dev;
    struct max9296 *priv;
    int err = 0;

	int gmsl_index = 0;
	u8 gmsl_used = 0;

	dev_info(dev, "Driver Version : v%d.%d.%d\n",DESER_DRIVER_VERSION_MAJOR,DESER_DRIVER_VERSION_MINOR,DESER_DRIVER_VERSION_PATCH);

    priv = devm_kzalloc(&client->dev, sizeof(struct max9296), GFP_KERNEL);
	priv->intialized = 0;
    priv->mode = MASTER_MODE;
    priv->fsync_conf = 0x24; // master-enabled default, overwritten if Slave mode is required
    priv->i2c_client = client;
    priv->regmap = devm_regmap_init_i2c(priv->i2c_client,
            &max9296_regmap_config);
    if (IS_ERR(priv->regmap)){
        dev_err(&client->dev,
                "regmap init failed: %ld\n", PTR_ERR(priv->regmap));
        return -ENODEV;
    }

	err = sl_max9296_parse_dt(priv);
    if(err)
    {
        dev_warn(dev, "%s: Deser initialization failed",__func__);
        return -EINVAL;
    } 

	dev_dbg(&client->dev, "%s: client addr = 0x%x\n",
			__func__, client->addr);

	err = sl_max9296_reset(priv);
	if (err) return err;

	for(gmsl_index = 0 ; gmsl_index < NB_GMSL;gmsl_index++)
	{
		err = sl_max9296_enable_I2C(priv,gmsl_index);
		if (err) return err; 

		err = sl_max9296_probe_camera(priv, gmsl_index);
        if (err) {
			dev_info(&client->dev, "No camera found on gmsl %d\n", gmsl_index);
			continue;
		}
		else {
			dev_warn(&client->dev, "%s: Camera connected to GMSL port %d", __func__,
					gmsl_index);
			dev_info(&client->dev, "%s: %s camera connected to this port",__func__,
					camera_names[priv->cam_id]);
		}

		if(priv->current_ser_device[gmsl_index].zedx_id == -1){
			dev_err(&client->dev, "%s: Camera plugged in GMSL #%d wrongly placed. Check user guide for camera placement info  \n", __func__, gmsl_index);
			return -EINVAL;
		}

		err = sl_max9296_translate_I2C(priv,gmsl_index);
		if (err == 0) {
			gmsl_used |= (1 << gmsl_index);
			dev_dbg(&client->dev, "Configuration success on gmsl %d\n", gmsl_index);
		}
		else {
			dev_dbg(&client->dev, "Configuration failure on gmsl %d\n", gmsl_index);
			return err;
		}

		err = sl_max9296_gmsl_pipeline_init(priv,gmsl_index);
		if(err)return err;
		else {
			dev_info(&client->dev, "%s: camera pipeline operational",__func__);
		}

		i2c_set_clientdata(client, priv);
	}

	if(gmsl_used == 0){
		dev_err(&client->dev, "%s: No camera found on any gmsl\n", __func__);
		return -1;
	}

	gmsl_used--;

	err = sl_max9296_enable_I2C(priv,gmsl_used);
	if (err) return err;

	/* Build csi_group to phy_index lookup table and derive phy_idx_bound from DT */
	{
		int i, csi_group, phy_idx;
		for (i = 0; i < MAX_CSI_GROUPS; i++)
			priv->csi_group_to_phy_idx[i] = -1;

		priv->phy_idx_bound = 0;
		for (i = 0; i < priv->nb_ser_devices; i++) {
			csi_group = (int)priv->ser_devices[i].serial;
			phy_idx = priv->ser_devices[i].phy_index;
			if (csi_group >= 0 && csi_group < MAX_CSI_GROUPS &&
			    phy_idx >= 0 && phy_idx < MAX_PHY_PER_DESER) {
				priv->csi_group_to_phy_idx[csi_group] = phy_idx;
				if ((u8)(phy_idx + 1) > priv->phy_idx_bound)
					priv->phy_idx_bound = (u8)(phy_idx + 1);
				dev_dbg(&client->dev, "CSI lookup: csi_group %d => phy_idx %d\n",
					csi_group, phy_idx);
			} else {
				dev_dbg(&client->dev,
					"CSI lookup: ser %d skipped (csi_group=%d, phy_idx=%d out of range) — BW check will use fallback capacity\n",
					i, csi_group, phy_idx);
			}
		}
		if (priv->phy_idx_bound == 0)
			priv->phy_idx_bound = 1;
		dev_dbg(&client->dev, "phy_idx_bound=%d (derived from DT phy_index values)\n", priv->phy_idx_bound);
	}

	priv->gmsl_link = gmsl_used;
	priv->intialized = 1;

	global_priv[priv->channel] = priv;

	dev_info(&client->dev, "%s: success \n",__func__);
	
	return err;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 0, 0)
static int sl_max9296_remove(struct i2c_client *client)
#else
static void sl_max9296_remove(struct i2c_client *client)
#endif
{
    struct max9296 *priv = i2c_get_clientdata(client);
    int i;

    if (priv) {
		for (i = 0; i < TOTAL_DETECTED_SENSORS; i++)
			mutex_destroy(&priv->detected_sensors[i].bw_lock);
        global_priv[priv->channel] = NULL;
    }

    dev_info(&client->dev, "%s: success\n", __func__);

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 0, 0)
    return 0;
#endif
}

static const struct i2c_device_id max9296_id[] = {
    {"sl_max9296", 0},
    {},
};

const struct of_device_id max9296_of_match[] = {
    {
        .compatible = "stereolabs,sl_max9296",
    },
    {},
};

MODULE_DEVICE_TABLE(i2c, max9296_id);

static struct i2c_driver max9296_i2c_driver = {
    .driver = {
        .name = "sl_max9296",
        .owner = THIS_MODULE,
    },
    .probe = sl_max9296_probe,
    .remove = sl_max9296_remove,
    .id_table = max9296_id,
};

/* Bandwidth management functions for MAX9296 */

/**
 * sl_get_deser_priv - Get deserializer private data for a given channel
 * @channel: GMSL channel number
 *
 * Returns pointer to deserializer private data, or NULL if not found.
 */
void *sl_get_deser_priv(int channel)
{
	if (channel < 0 || channel >= 4)
		return NULL;

	if (!global_priv[channel] || global_priv[channel]->i2c_client == NULL)
		return NULL;

	return global_priv[channel];
}
EXPORT_SYMBOL(sl_get_deser_priv);

/**
 * sl_update_camera_bw - Update camera bandwidth usage
 * @channel: GMSL channel number
 * @zedx_id: Camera ID to update
 * @cam_addr: Camera I2C address
 * @bps: Current bandwidth in bits per second
 * @is_streaming: Streaming status
 *
 * Updates the stored bandwidth for a specific sensor identified by
 * (zedx_id, cam_addr). Searches ser_devices[] for matching entry.
 *
 * Returns 0 on success, negative error code on failure.
 */
int sl_update_camera_bw(int channel, int zedx_id, u32 cam_addr, u64 bps, bool is_streaming)
{
	struct max9296 *priv;
	int i;
	detected_sensor_slot *detected_sen;

	if (channel < 0 || channel >= 4)
		return -EINVAL;

	priv = global_priv[channel];
	if (!priv || priv->i2c_client == NULL)
		return -ENODEV;

	/* Search detected_sensors for matching zedx_id and cam_addr */
	for (i = 0; i < TOTAL_DETECTED_SENSORS; i++) {
		detected_sen = &priv->detected_sensors[i];

		if (detected_sen->cam_addr == 0)
			continue;

		if (detected_sen->zedx_id != zedx_id || detected_sen->cam_addr != cam_addr)
			continue;

		mutex_lock(&detected_sen->bw_lock);
		detected_sen->current_bps = bps;
		detected_sen->is_streaming = is_streaming;
		mutex_unlock(&detected_sen->bw_lock);

		/* BW status is only checked when sysfs is read, not on every update */
		return 0;
	}

	return -ENODEV;
}
EXPORT_SYMBOL(sl_update_camera_bw);

/**
 * dser_get_csi_group - Get CSI group/port for a camera
 * @channel: GMSL channel number
 * @zedx_id: Camera ID
 * @gmsl_port: GMSL port number (unused, for API compat)
 *
 * Returns the serial (CSI port index) for the given camera,
 * which serves as the CSI group identifier for BW accounting.
 */
int dser_get_csi_group(int channel, int zedx_id, int gmsl_port)
{
	struct max9296 *priv;
	int i;

	if (channel < 0 || channel >= 4)
		return -EINVAL;

	priv = global_priv[channel];
	if (!priv || priv->i2c_client == NULL)
		return -ENODEV;

	for (i = 0; i < priv->nb_ser_devices; i++) {
		if (priv->ser_devices[i].zedx_id == zedx_id)
			return priv->ser_devices[i].serial;
	}

	return -ENODEV;
}
EXPORT_SYMBOL(dser_get_csi_group);

/**
 * sl_get_bw_usage - Get summed bandwidth for a CSI group
 * @priv: Deserializer private data (struct max9296 *)
 * @csi_group: CSI group index (matches serial field)
 *
 * Sums current_bps for all streaming sensors in ser_devices[]
 * that belong to the requested CSI group (by serial).
 *
 * Returns total bits per second.
 */
u64 sl_get_bw_usage(void *priv, int csi_group)
{
	struct max9296 *deser_priv = (struct max9296 *)priv;
	u64 sum_bps = 0ULL;
	int i, count = 0;
	detected_sensor_slot *detected_sen;

	if (!deser_priv)
		return 0ULL;

	for (i = 0; i < TOTAL_DETECTED_SENSORS; i++) {
		detected_sen = &deser_priv->detected_sensors[i];

		/* cam_addr and serial are immutable after init — skip without lock */
		if (detected_sen->cam_addr == 0)
			continue;

		if (detected_sen->serial != csi_group)
			continue;

		mutex_lock(&detected_sen->bw_lock);
		if (detected_sen->is_streaming) {
			if (deser_priv->i2c_client)
				dev_dbg(&deser_priv->i2c_client->dev,
					"  [%d] zedx_id=%d cam=0x%x serial=%d => %llu Mbps\n",
					count, detected_sen->zedx_id, detected_sen->cam_addr,
					detected_sen->serial, detected_sen->current_bps / 1000000);
			sum_bps += detected_sen->current_bps;
			count++;
		}
		mutex_unlock(&detected_sen->bw_lock);
	}

	if (deser_priv->i2c_client)
		dev_dbg(&deser_priv->i2c_client->dev,
			"BW usage: csi_group=%d => %d sensor(s) streaming, total=%llu Mbps\n",
			csi_group, count, sum_bps / 1000000);

	return sum_bps;
}
EXPORT_SYMBOL(sl_get_bw_usage);

static u64 sl_get_csi_capacity(struct max9296 *priv, int csi_group)
{
	int phy_idx;
	u64 capacity_bps;

	if (csi_group < 0 || csi_group >= MAX_CSI_GROUPS)
		return 0ULL;
	phy_idx = priv->csi_group_to_phy_idx[csi_group];
	if (phy_idx < 0 || phy_idx >= priv->phy_idx_bound) {
		dev_dbg(&priv->i2c_client->dev,
			"sl_get_csi_capacity: csi_group=%d => no phy mapping (phy_idx=%d)\n",
			csi_group, phy_idx);
		return 0ULL;
	}
	capacity_bps = priv->csi_rate_bps[phy_idx];
	dev_dbg(&priv->i2c_client->dev,
		"sl_get_csi_capacity: csi_group=%d => phy_idx=%d, capacity=%llu Mbps\n",
		csi_group, phy_idx, capacity_bps / 1000000);
	return capacity_bps;
}

/**
 * sl_get_bw_status - Get bandwidth status for a CSI group
 * @dev: Device pointer for logging
 * @priv: Deserializer private data
 * @csi_group: CSI group index
 *
 * Serializers sharing the same PHY share a single capacity entry indexed by
 * phy_index, mirroring MAX96712 behavior.
 *
 * Returns BW_OK (0) or BW_OVERFLOW (1).
 */
int sl_get_bw_status(struct device *dev, void *priv, int csi_group)
{
	struct max9296 *deser_priv = (struct max9296 *)priv;
	u64 summed_bps;
	u64 capacity_bps;
	int result;

	if (!deser_priv || !dev)
		return -1;

	dev_dbg(dev, "BW check start: csi_group=%d — streaming sensors:\n", csi_group);

	capacity_bps = sl_get_csi_capacity(deser_priv, csi_group);

	if (capacity_bps == 0ULL) {
		dev_dbg(dev, "BW check: csi_group=%d no PHY capacity cached, using fallback 3200 Mbps\n",
			csi_group);
		capacity_bps = 3200000000ULL;  /* 1.6 Gbps/lane * 2 lanes = 3.2 Gbps */
	}

	summed_bps = sl_get_bw_usage(priv, csi_group);

	{
		u64 threshold_bps = (capacity_bps * 90ULL) / 100ULL;
		result = (summed_bps > threshold_bps) ? BW_OVERFLOW : BW_OK;
		dev_dbg(dev, "BW check: csi_group=%d summed=%llu Mbps, threshold=%llu Mbps (90%%), capacity=%llu Mbps [%s]\n",
			csi_group, summed_bps / 1000000, threshold_bps / 1000000, capacity_bps / 1000000,
			result == BW_OK ? "OK" : "OVERFLOW");
	}

	return result;
}
EXPORT_SYMBOL(sl_get_bw_status);

static int __init sl_max9296_init(void)
{
    return i2c_add_driver(&max9296_i2c_driver);
}

static void __exit sl_max9296_exit(void)
{
    i2c_del_driver(&max9296_i2c_driver);
}

module_init(sl_max9296_init);
module_exit(sl_max9296_exit);

MODULE_DESCRIPTION("IO Expander driver max9296");
MODULE_AUTHOR("STEREOLABS <support@stereolabs.com>");
MODULE_LICENSE("GPL v2");
MODULE_VERSION(DRV_STR_VERSION);
