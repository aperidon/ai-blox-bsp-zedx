/*
 * max96724.c - max96724 IO Expander driver
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

// #define DEBUG  1

#ifndef __SL_DESERIALIZER__
#define __SL_DESERIALIZER__
#define __MAX96724__
#else
#error Error, a deserializer is already compiled. Fix the defconfig and use only one deserializer.
#endif

#include <linux/seq_file.h>
#include <media/camera_common.h>
#include <linux/module.h>
#include "sl_max96724_mode_tbls.h"
#include <linux/seq_file.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/types.h>
#include "../include/bw_mgmt.h"

#define STRINGIFY0(s)
#define STRINGIFY(s) STRINGIFY0(s)
#define DRV_STR_VERSION STRINGIFY(DESER_DRIVER_VERSION_MAJOR)"."STRINGIFY(DESER_DRIVER_VERSION_MINOR)"."STRINGIFY(DESER_DRIVER_VERSION_PATCH)""

#define MAX_CSI_GROUPS 8  /* Supports port-indices 0-7; observed values: 0-4, 6 */

//int write_reg_Ser(int slaveAddr, int channel, u16 addr, u8 val);
int set_bitrate_Dser(int channel, u32 zedx_id, u8 val);
u32 fps_set_Dser(int channel, s64 val);
int fsync_set_Dser(int channel, bool on);
int dser_get_gmsl_port(int channel, int zedx_id);
int dser_enable_gmsl_link(int channel, int zedx_id);
int dser_open_all_gmsl_link(int channel);
int isSecondCamFromI2C(int channel, int zedx_id);
int dser_read_video_lock(int channel, int zedx_id);
int dser_read_link_lock(int channel, int zedx_id);
int getCamPipeIndex(int channel, int zedx_id);

struct sensor
{
    struct list_head list;
    int detection_id; /* ID of the camera when connected to the deser */
    s8 pipes[N_SER_PIPES]; /* table position gives ser pipe, [ X Y Z U ],
                            * table content gives deser pipe [ 0 1 -1 -1] */
    const char *camera;
    s8 cam_dts_id; /* ID of the camera when detected in dts */
    int model; /* Camera model ID */
    u32 vc_id; /* CSI Virtual channel ID */
    u32 n_lanes; /* Jetson number of CSI lanes */
    u32 serial; /* Jetson CSI connection */
    u32 i2c_bus; /* i2c bus of the device from dts */
    int gmsl_link; /* GMSL link connected to the device */
    u32 cam_addr;
    u32 ser_addr;
    u32 zedx_id;
    int phy_index;
    int i2c_cc;
    bool is_second_cam_from_i2c;
    int dsr_pipe;
    int phy_rate;
    u64 current_bps;
    bool is_streaming;
    struct mutex bw_lock;
};

/**
 * struct sl_max9295 - ZED-X private data.
 * @i2c_client: struct i2c_client * - i2c adapter of the sl_max9295.
 * @id: struct i2c_device_id * - i2c adapter id.
 * ZED-X private data for this driver. One structure is initialized by camera sensor,
 * so two per ZED-X.
 */
typedef struct serializer_devices{
	int zedx_id;
	int camera_model;
    u32 ser_addr;
}serializer_devices;


/**
 * struct max96724 - Deserializer device structure
 * @i2c_client: I2C client structure for communication with the device
 * @regmap: Register map for the device
 * @channel: Channel ID from device tree
 * @reset_gpio: GPIO number for device reset pin
 * @pwr_gpio: GPIO number for device power pin
 * @pwdn_gpio: GPIO number for device power down pin
 * @port_to_i2c: Mapping of GMSL ports to I2C buses (0,1, and 2)
 *
 * Deserializer device structure containing device-specific information.
 */
struct max96724
{
    bool initialized;
    enum deser_mode mode; // FSYNC role: MASTER_MODE (internal fsync) or SLAVE_MODE (external fsync)
    u8 fsync_conf; // Cached FSYNC config reg (0x04A0), holds board MFP routing
    struct i2c_client *i2c_client;
    struct regmap *regmap;
    struct list_head sensor_list;
    u32 channel; // channel id from dts
    u32 n_lanes;
    int reset_gpio;
    int pwr_gpio;
    int pwdn_gpio;
    s8 port_to_i2c[N_GMSL_PORTS];
    u8 avail_pipe;
    u8 n_cam;
    int n_serializers;
    struct serializer_devices ser_devices[N_MAX_TOTAL_SER];
    struct sensor detected_sensors[2*N_GMSL_PORTS];

    u8  phy_idx_bound;                                    /* max(phy_index)+1 across all DT sensors; use as array bound, not PHY count */
    u8  csi_rate_reg_val[MAX_PHY_PER_DESER];      /* PHY CSI rate register value, indexed by phy_index */
    u64 csi_rate_bps[MAX_PHY_PER_DESER];          /* PHY CSI capacity in bps, indexed by phy_index */
    int csi_group_to_phy_idx[MAX_CSI_GROUPS];     /* Maps csi_group (port-index) to phy_index for fast lookup */
	int mfp_trig_in; // Mfp used as trigger input (default MFP10)
	int mfp_trig_out; // Mfp used as trigger output (default MFP10)
    int mfp_trig_info; // Mfp used for HW sync mode control (-1 if not used)
};

/* Convert CSI data-rate register value to bits/sec capacity.
 * csi_tx_dpll_predef_freq table defines mapping from regval to Mbps for
 * each lane. Total bps = Mbps * 1,000,000 * n_lanes
 * regval: register value. Mask lower 5 bits [4:0] and multiply by 100 to get Mbps.
 * lanes: number of CSI lanes
 * Returns: capacity in bits/second
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

/**
 * global_priv - Array of pointers to deserializer device structures
 *
 * Array of pointers to deserializer device structures representing connected devices.
 */
struct max96724 *global_priv[4]; /* channels a-d; every accessor range-checks 0-3 */
static int sync_mode = 0;
module_param(sync_mode, int, 0);

/**
 * sl_get_deser_priv - Get deserializer private data for bandwidth management
 * @channel: GMSL channel number (0-3)
 *
 * Returns pointer to deserializer private data, or NULL if not found
 */
void *sl_get_deser_priv(int channel)
{
	if (channel < 0 || channel > 3 || !global_priv[channel])
		return NULL;
	return (void *)global_priv[channel];
}
EXPORT_SYMBOL(sl_get_deser_priv);

/**
 * sl_update_camera_bw - Update individual camera bandwidth
 * @channel: GMSL channel number
 * @zedx_id: Camera ID to update
 * @cam_addr: Camera I2C address
 * @bps: Current bandwidth in bits per second (0 if not streaming)
 * @is_streaming: Streaming status
 *
 * Called by cameras to update their bandwidth usage.
 * Each camera is responsible for calculating and reporting its own bps.
 *
 * Returns 0 on success, negative error code on failure
 */
int sl_update_camera_bw(int channel, int zedx_id, u32 cam_addr, u64 bps, bool is_streaming)
{
	struct max96724 *deser_priv;
	struct sensor *detected_sen = NULL;
	struct device *dev;
	int i;

	if (channel < 0 || channel > 3 || !global_priv[channel])
		return -EINVAL;

	deser_priv = global_priv[channel];
	dev = &deser_priv->i2c_client->dev;

	/* Find the sensor by (zedx_id, cam_addr) in detected_sensors array */
	for (i = 0; i < 2*N_GMSL_PORTS; i++) {
		if (deser_priv->detected_sensors[i].zedx_id == zedx_id &&
		    deser_priv->detected_sensors[i].cam_addr == cam_addr) {
			detected_sen = &deser_priv->detected_sensors[i];
			break;
		}
	}

	if (!detected_sen) {
		return -ENODEV;
	}

	/* Update sensor bandwidth with mutex protection */
	mutex_lock(&detected_sen->bw_lock);
	detected_sen->current_bps = bps;
	detected_sen->is_streaming = is_streaming;
	mutex_unlock(&detected_sen->bw_lock);

	/* BW status is only checked when sysfs is read, not on every update */
	return 0;
}
EXPORT_SYMBOL(sl_update_camera_bw);


/**
 * sl_get_bw_usage - Sum bandwidth for a CSI group and log per-sensor state
 * @priv: deserializer private data (struct max96724 *)
 * @csi_group: CSI group index
 *
 * Iterate BW sensors and log their current streaming state and
 * bandwidth (for debug purposes). Then sum current_bps for cameras that
 * are streaming and belong to the requested csi_group.
 * Returns total bits per second (0 on error or no active cameras).
 */
u64 sl_get_bw_usage(void *priv, int csi_group)
{
    struct max96724 *deser_priv = (struct max96724 *)priv;
    u64 sum_bps = 0ULL;
    int i;
    struct sensor *detected_sen;

    if (!deser_priv)
        return 0ULL;

    for (i = 0; i < 2*N_GMSL_PORTS; i++) {
        detected_sen = &deser_priv->detected_sensors[i];
        
        /* Skip empty slots */
        if (detected_sen->cam_addr == 0)
            continue;
        
        mutex_lock(&detected_sen->bw_lock);
        if (detected_sen->is_streaming && (detected_sen->serial == (u32)csi_group))
            sum_bps += detected_sen->current_bps;

        mutex_unlock(&detected_sen->bw_lock);
    }

    return sum_bps;
}
EXPORT_SYMBOL(sl_get_bw_usage);
/**
 * sl_get_phy_index_from_csi_group - Fast lookup of csi_group to PHY index
 * @priv: Deserializer private data (struct max96724 *)
 * @csi_group: CSI group index (serial/port-index)
 *
 * Returns PHY index from pre-computed lookup table built during probe.
 * Returns PHY index on success, negative error code on failure.
 */
static int sl_get_phy_index_from_csi_group(struct max96724 *priv, int csi_group)
{
	int phy_idx;

	if (!priv || csi_group < 0 || csi_group >= MAX_CSI_GROUPS)
		return -EINVAL;

	phy_idx = priv->csi_group_to_phy_idx[csi_group];
	return (phy_idx >= 0) ? phy_idx : -ENOENT;
}

/**
 * sl_get_csi_capacity - Get CSI capacity for a group
 * @priv: Deserializer private data (struct max96724 *)
 * @csi_group: CSI group index (serial/port-index)
 *
 * Resolves csi_group to PHY index using DT-populated detected_sensors and
 * returns the cached CSI data-rate capacity in bits per second for that PHY.
 *
 * Returns capacity in bps (0 if no sensor/PHY mapping was found)
 */
static u64 sl_get_csi_capacity(struct max96724 *priv, int csi_group)
{
	u64 capacity_bps;
	int phy_idx;

	if (!priv || csi_group < 0)
		return 0ULL;

	phy_idx = sl_get_phy_index_from_csi_group(priv, csi_group);
	if (phy_idx < 0)
		return 0ULL;

	capacity_bps = priv->csi_rate_bps[phy_idx];
	dev_dbg(&priv->i2c_client->dev,
		"sl_get_csi_capacity: csi_group=%d => phy_idx=%d, capacity=%llu Mbps\n",
		csi_group, phy_idx, capacity_bps / 1000000);
	return capacity_bps;
}

/**
 * sl_get_bw_status - Get bandwidth status for a CSI group
 * @dev: Device pointer
 * @priv: Deserializer private data
 * @csi_group: CSI group index
 *
 * Compares summed camera bandwidth against per-PHY CSI capacity.
 * Returns BW_OVERFLOW if usage exceeds 90% of capacity, else BW_OK.
 * Logs capacity, threshold, used bandwidth, and status.
 *
 * Returns BW_OK (0) or BW_OVERFLOW (1)
 */
int sl_get_bw_status(struct device *dev, void *priv, int csi_group)
{
	struct max96724 *deser_priv;
	u64 summed_bps;
	u64 capacity_bps;
	u64 threshold_bps;
	int phy_idx;

	deser_priv = (struct max96724 *)priv;
	if (!deser_priv || !dev)
		return -1; /* BW_ERR: no device */

	/* Get current bandwidth usage for this CSI group */
	summed_bps = sl_get_bw_usage(priv, csi_group);

	/* Get capacity for this CSI group's PHY */
	capacity_bps = sl_get_csi_capacity(deser_priv, csi_group);

	/* Fallback: if no PHY capacity cached, use conservative estimate (1.6 Gbps/lane * 2 lanes), that's the default for the ZED X */
	if (capacity_bps == 0ULL)
		capacity_bps = 3200000000ULL;

	/* Calculate 90% threshold using integer math */
	threshold_bps = (capacity_bps * 90ULL) / 100ULL;

	phy_idx = sl_get_phy_index_from_csi_group(deser_priv, csi_group);
	dev_dbg(dev, "BW check: csi_group=%d => phy_idx=%d, summed=%llu Mbps, threshold=%llu Mbps (90%%), capacity=%llu Mbps [%s]\n",
		csi_group, phy_idx, summed_bps / 1000000, threshold_bps / 1000000, capacity_bps / 1000000,
		(summed_bps > threshold_bps) ? "OVERFLOW" : "OK");

	return (summed_bps > threshold_bps) ? BW_OVERFLOW : BW_OK;
}
EXPORT_SYMBOL(sl_get_bw_status);

/**
 * dser_get_csi_group - Get CSI group index for a camera
 * @channel: GMSL channel number
 * @zedx_id: Camera ID
 * @gmsl_port: GMSL port number
 *
 * Returns the CSI group (serial) for the specified camera.
 * CSI group determines which Jetson CSI controller handles this camera.
 *
 * Returns CSI group index on success, negative error code on failure
 */
int dser_get_csi_group(int channel, int zedx_id, int gmsl_port)
{
	struct max96724 *priv;
	int i;
	int csi_group;

	priv = sl_get_deser_priv(channel);
	if (!priv)
		return -ENODEV;

	/* Find sensor by zedx_id */
	for (i = 0; i < 2 * N_GMSL_PORTS; i++) {
		if (priv->detected_sensors[i].zedx_id == zedx_id) {
			csi_group = priv->detected_sensors[i].serial;
			dev_dbg(&priv->i2c_client->dev,
				"dser_get_csi_group: zedx_id=%d found at sensor_idx=%d, csi_group=%d (phy_idx=%d), gmsl_port=%d\n",
				zedx_id, i, csi_group, priv->detected_sensors[i].phy_index, gmsl_port);
			return csi_group;
		}
	}

	dev_dbg(&priv->i2c_client->dev,
		"dser_get_csi_group: zedx_id=%d NOT FOUND, channel=%d gmsl_port=%d\n",
		zedx_id, channel, gmsl_port);
	return -ENOENT;
}
EXPORT_SYMBOL(dser_get_csi_group);

/**
 * write_reg_Dser - Write value to register on MAX96724 deserializer device
 * @channel: Channel ID of the device
 * @addr: Address of the register to write to
 * @val: Value to write to the register
 *
 * This function writes a value to a register on a MAX deserializer device.
 * If the device is connected and the write operation is successful, it returns 0.
 * Otherwise, it returns -1 and logs an error message.
 */
static int write_reg_Dser(int channel, u16 addr, u8 val)
{
    struct i2c_client *i2c_client = NULL;
    int err;

    if (channel > 3 || channel < 0 || global_priv[channel] == NULL)
        return -1;

    i2c_client = global_priv[channel]->i2c_client;

    err = regmap_write(global_priv[channel]->regmap, addr, val);

    if (err)
    {
        dev_err(&i2c_client->dev, "%s: addr = 0x%x, val = 0x%x\n",
                __func__, addr, val);
        return -1;
    }
    return 0;
}

static bool isCameraMono (struct sensor *sp)
{
    if(!strcmp(sp->camera,"zedonehdr") || !strcmp(sp->camera,"zedone4k") || !strcmp(sp->camera,"zedonegs"))
        return true; 
    else
        return false;
}

static inline int update_ISX_nor_flash(struct max96724 *priv, int addr, int force_update){
	int j = 0;
	int err = 0;
	struct i2c_client *client = priv->i2c_client;
    unsigned int register_val = 0;

    struct i2c_fingerprint update_flash_table[] = {
			{0x1a, 0x8A54, addr}, // Image sensor @ x10 unlock reg
			{0x1a, 0xFFFF, 0xF4}, // Image sensor @ x10 unlock reg
			{0x1a, 0xFFFF, 0xF7}, // Image sensor @ x10 unlock reg
			{0x1a, 0x8000, 0x04}, // Image sensor @ x10 unlock reg
			{0x1a, 0x8001, 0x19}, // Image sensor @ x10 unlock reg
			{0x1a, 0x8005, 0x5a}, // Image sensor @ x10 unlock reg
            {SLEEP, 	0x00, 	0x00},
			{0x1a, 0xFFFF, 0xF5}, // Image sensor @ x10 unlock reg
			{MAX96724_TABLE_END, 0x00, 0x00},
	};

    // Check if the ISX NOR flash already contains the right I2C address
    if(!force_update){
        err = regmap_read(priv->regmap, 0x8A54, &register_val);
        msleep(12);

        if(register_val == addr && !err){
            dev_dbg(&client->dev, "%s: ISX NOR flash already contains the right I2C address\n", __func__);
            return err;
        }
    }

    // Update the ISX NOR flash with the new I2C address
    dev_dbg(&client->dev, "%s: Update ISX NOR flash with new I2C address 0x%x\n", __func__, addr);
	while (update_flash_table[j].i2c_addr!=MAX96724_TABLE_END){
		client->addr = update_flash_table[j].i2c_addr;

        if(update_flash_table[j].i2c_addr==SLEEP){
            msleep(100);
            j++;
            continue;
        }

		err = regmap_write(priv->regmap, update_flash_table[j].reg_addr, update_flash_table[j].val);
		if(err)
			return err;

		msleep(12);
		j++;
	}

    dev_dbg(&client->dev, "%s: ISX NOR flash updated with new I2C address 0x%x\n", __func__, addr);

	return err;
}

static inline int apply_alternate_mapping(struct max96724 *priv, struct sensor *sp){
	struct i2c_client *client = priv->i2c_client;
	int deser_addr = client->addr;
	struct i2c_fingerprint alt_i2c[60];
	int err = 0;
	u8 j = 0, update_isx_address = 0, force_update = 0;
    u32 ser_addr = sp->ser_addr , sen_1_addr = sp->cam_addr, sen_2_addr = sp->cam_addr;

    if(!isCameraMono(sp))
    sp = list_entry(sp->list.next, struct sensor, list);
    sen_2_addr = sp->cam_addr;

    dev_dbg(&client->dev, "%s: Apply new addr : Serializer -> @0x%x / sen 0 -> @0x%x / sen 1 -> @0x%x \n",
					__func__, ser_addr, sen_1_addr,sen_2_addr);

    get_fingerprint_alt_table[sp->model](alt_i2c, sizeof(alt_i2c),
        sp->ser_addr, sen_2_addr, sen_1_addr);

    while (alt_i2c[j].i2c_addr!=MAX96724_TABLE_END){
		client->addr = alt_i2c[j].i2c_addr;

        if(alt_i2c[j].i2c_addr==SLEEP){
            msleep(100);
            j++;
            continue;
        }

        // For ISX only: we need to check whether the address needs to be updated to prevent unnecessary writes to the NOR flash
        update_isx_address = alt_i2c[j].reg_addr == 0x8A54 && 
			(sp->model == ZEDXHDR || sp->model == ZEDONEHDR);
        if(update_isx_address){
            err = update_ISX_nor_flash(priv, alt_i2c[j].val, force_update);
            if(err) {
	            client->addr = deser_addr;
                dev_err(&client->dev, "%s: ISX NOR flash update failed\n", __func__);
                return err;
            }

            j++;
            continue;
        }

        err = regmap_write(priv->regmap, alt_i2c[j].reg_addr, alt_i2c[j].val);

		dev_dbg(&client->dev, "%s: I2C device @0x%x, read returns %d\n",
					__func__, client->addr, err);

		dev_dbg(&client->dev, "%s: value written in 0x%x (if any) : 0x%x",
					__func__,alt_i2c[j].reg_addr, alt_i2c[j].val);

		if (err)
		{
			dev_err(&client->dev, "%s: Cannot write 0x%x in 0x%x\n",
					__func__,alt_i2c[j].val, alt_i2c[j].reg_addr);
			client->addr = deser_addr;

			return -1;
		}

		j++;
	}

	client->addr = deser_addr;

	return 0;
}


static inline int model_reset(struct max96724 *priv, u8 model)
{
	struct i2c_fingerprint *fingerprint = reset_table[model];
	struct i2c_client *client = priv->i2c_client;
	int deser_addr = client->addr;
	int err = 0;
	u8 j = 0;

	if (!fingerprint)
		return 0;

	while (fingerprint[j].i2c_addr != MAX96724_TABLE_END) {
		if (fingerprint[j].i2c_addr == SLEEP) {
			msleep(100);
			j++;
			continue;
		}

		client->addr = fingerprint[j].i2c_addr;
		err = regmap_write(priv->regmap, fingerprint[j].reg_addr,
				   fingerprint[j].val);
		dev_dbg(&client->dev,
			"%s: write @0x%x reg 0x%x = 0x%x, err %d\n",
			__func__, fingerprint[j].i2c_addr,
			fingerprint[j].reg_addr, fingerprint[j].val, err);

		if (err) {
			client->addr = deser_addr;
			return err;
		}

		msleep(6);
		j++;
	}

	client->addr = deser_addr;
	return 0;
}

/* Reset every possible serializer */
static inline void reset_all_serializers(struct max96724 *priv)
{
    int j;
    int err = 0;
	struct i2c_client *client = priv->i2c_client;
    int deser_addr = client->addr;

    for (j = 0; j < N_MAX_TOTAL_SER; j++){
        struct i2c_fingerprint reset_table[] = {
            {priv->ser_devices[j].ser_addr, 0x0010, 0x91}, 
        };

        if(!priv->ser_devices[j].ser_addr)
            continue;
        
        client->addr = reset_table[0].i2c_addr;

        err = regmap_write(priv->regmap, reset_table[0].reg_addr, reset_table[0].val);
        
        if(err == 0 ){
            dev_dbg(&client->dev, "%s: %d %x %x %02x\n",
                __func__, err,client->addr,reset_table[0].reg_addr,reset_table[0].val);
            msleep(100);
            break;
        }
        msleep(6);
    }

    client->addr = deser_addr;
}

/* Tries each populated model's reset table in order and stops at the first
 * one that completes without an i2c error — that model is assumed to be the
 * camera actually present. Earlier models that NACK are tolerated because
 * their target i2c addresses are empty on this board. */
static inline void reset_all_models(struct max96724 *priv)
{
	u8 model;
	int err;

	for (model = 0; model < N_CAM_TYPE; model++) {
		if (!reset_table[model])
			continue;

		err = model_reset(priv, model);
		if (err == 0){
            dev_dbg(&priv->i2c_client->dev,
                "%s: model_reset(%s) succeeded\n",
                __func__, camera_names[model]);
            break;
        }

        dev_dbg(&priv->i2c_client->dev,
            "%s: model_reset(%s) returned %d, ignored\n",
            __func__, camera_names[model], err);
	}
}

/* Reads the camera EEPROM and returns the matching CamType.
 *
 * Tries each candidate address in EEPROM_I2C_ADDRS — the EEPROM lives at
 * different i2c addresses across camera variants.
 *
 * Returns -ENODEV when no address yields a valid EEPROM */
static int identify_via_eeprom(struct max96724 *priv)
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

/* Walks fingerprint_table[model] and returns 0 if every register matches.
 * Returns -ENODEV on mismatch or read NACK — both mean "this candidate isn't
 * the camera connected here." */
static inline int match_fingerprint(struct max96724 *priv, u8 model)
{
	struct i2c_fingerprint *fingerprint = fingerprint_table[model];
	struct i2c_client *client = priv->i2c_client;
	int deser_addr = client->addr;
	unsigned int val = 0;
	int err;
	u8 j = 0;

	if (!fingerprint)
		return -ENODEV;

	while (fingerprint[j].i2c_addr != MAX96724_TABLE_END) {
		client->addr = fingerprint[j].i2c_addr;

		err = regmap_read(priv->regmap, fingerprint[j].reg_addr, &val);
		msleep(10);

		dev_dbg(&client->dev,
			"%s: %s @0x%x reg 0x%x: read 0x%x (want 0x%x), err %d\n",
			__func__, camera_names[model],
			fingerprint[j].i2c_addr, fingerprint[j].reg_addr,
			val, fingerprint[j].val, err);

		if (err || fingerprint[j].val != val) {
			client->addr = deser_addr;
			return -ENODEV;
		}

		j++;
	}

	client->addr = deser_addr;


	return 0;
}

// Reset serializer at 3Gbps and set it to 6Gpbs gmsl speed to match other camera speed
// Necessary for one HDR
static inline int configure_3Gbps_cameras_to_6Gbps(struct max96724 *priv, int link){
	int err = 0;
	struct i2c_client *client = priv->i2c_client;
    int deser_addr = client->addr; //init addr
    const int gmsl_6gbps_mode = 0x22;
    int reg_gmsl_ctrl_addr = (link == 0 || link == 1) ? 0x10 : 0x11;
    int gmsl_3gbps_mode = (link == 0 || link == 2) ? 0x21 : 0x12;

	// Set deserializer at 3Gpbs gmsl speed
	client->addr = deser_addr;
	err = regmap_write(priv->regmap, reg_gmsl_ctrl_addr, gmsl_3gbps_mode);

	dev_dbg(&client->dev, "%s: Switching deserializer mode to 3Gbps: %d %x %x %02x\n",
		__func__, err,client->addr, reg_gmsl_ctrl_addr, gmsl_3gbps_mode);
	msleep(SLEEP_TIME);

    reset_all_serializers(priv);
	
    // Set serializer at 6Gbps gmsl speed
	client->addr = ZED_ONE_SER_DFLT_ADDR;
	err = regmap_write(priv->regmap, MAX9295_GMSL_LINK_RATE_CTRL, MAX9295_GMSL_6GBPS_MODE);

	dev_dbg(&client->dev, "%s: Switching serializer mode to 6Gbps: %d %x %x %02x\n",
		__func__, err,client->addr,MAX9295_GMSL_LINK_RATE_CTRL,MAX9295_GMSL_6GBPS_MODE);

	// Set deserializer at 6 Gbps gmsl speed
	client->addr = deser_addr;
	err = regmap_write(priv->regmap, reg_gmsl_ctrl_addr, gmsl_6gbps_mode);

	dev_dbg(&client->dev, "%s: Switching deserializer mode to 6Gbps: %d %x %x %02x\n",
		__func__, err,client->addr,reg_gmsl_ctrl_addr,gmsl_6gbps_mode);
	msleep(SLEEP_TIME);

	return err;
}

static inline int sl_max96724_get_camera_model(struct max96724 *priv)
{
	struct i2c_client *client = priv->i2c_client;
	int model;

	model = identify_via_eeprom(priv);
	if (model >= 0 && model < N_CAM_TYPE && reset_table[model]) {
		if (model_reset(priv, model)) {
			dev_warn(&client->dev,
				"%s: eeprom-id %s but model_reset failed\n",
				__func__, camera_names[model]);
			return -EIO;
		}
		dev_info(&client->dev,
			"%s: %s camera connected to this port (eeprom)\n",
			__func__, camera_names[model]);

		return model;
	}

	/* Fallback: reset every known model, then probe by fingerprint. */
    dev_dbg(&client->dev,
        "%s: camera model not identified via eeprom, trying fingerprint\n",
        __func__);
    reset_all_models(priv);

	for (model = 0; model < N_CAM_TYPE; model++) {
		if (!fingerprint_table[model])
			continue;
		if (match_fingerprint(priv, model) == 0) {
			dev_info(&client->dev,
				"%s: %s camera connected to this port (fingerprint)\n",
				__func__, camera_names[model]);
			return model;
		}
	}

	return -ENODEV;
}

static int sl_max96724_i2c_setup(struct max96724 *priv)
{
    int err = 0;
    u8 val_cx, i, val_cc, val_port;
    s8 i2c_cc;
    
    // Default value to disable all GMSL2 port 
    val_port = 0xF0;

    // Default value to disable any control channel crossover 
    val_cx = 0x00;

    // Default control channel for all gmsl port is i2c0
    val_cc = 0xAA;

    for (i=0; i<N_GMSL_PORTS; i++)
    {
        i2c_cc = priv->port_to_i2c[i]; 
        dev_info(&priv->i2c_client->dev, "set GMSL %d to i2c-cc %d", i, i2c_cc);

        // if nothing is connected to the port, don't activate it
        // and make the CC configuration default
        /* TODO: support i2c_cc = 2 */
        if (i2c_cc<0 || i2c_cc>1)
            i2c_cc = 0;
        else
            val_port = val_port | 1<<i;

        val_cx = val_cx | (i2c_cc<<(4+i));

        val_cc = val_cc & (0xFF ^ (1<<i2c_cc)<<(2*i));

        val_cc = val_cc | ((2>>i2c_cc)<<(2*i));
        
    }
    
    err = regmap_write(priv->regmap, GMSL_LINKS_EN_REG, 0xF0);
    msleep(SLEEP_TIME);

    err = regmap_write(priv->regmap, GMSL_CC_X_OVR_REG, val_cx);
    dev_dbg(&priv->i2c_client->dev,"%s: set i2c cc val_cx: 0x%x 0x%x",__func__,GMSL_CC_X_OVR_REG, val_cx);

    err = regmap_write(priv->regmap, GMSL_LINKS_CC_REG, val_cc);
    dev_dbg(&priv->i2c_client->dev,"%s: set i2c cc val_cc: 0x%x 0x%x",__func__,GMSL_LINKS_CC_REG, val_cc);

    err = regmap_write(priv->regmap, GMSL_LINKS_EN_REG, val_port);
    dev_dbg(&priv->i2c_client->dev,"%s: set i2c cc val_port: 0x%x 0x%x",__func__,GMSL_LINKS_EN_REG, val_port);

    msleep(SLEEP_TIME);

    dev_dbg(&priv->i2c_client->dev,"%s: set GMSL to i2c [ %d, %d, %d, %d ]",
        __func__, priv->port_to_i2c[0], priv->port_to_i2c[1], priv->port_to_i2c[2], priv->port_to_i2c[3]);

    return err;
}

static int setup_sensor_pipe(struct max96724 *priv, struct sensor *sp, u8 ser_pipe)
{
    struct i2c_client *i2c_client = priv->i2c_client;
    struct index_reg_8 *table = pipeline_table[sp->model];
	struct index_reg_8 csi_map_reg = table[0];
	struct index_reg_8 csi_data_rate_reg = table[1];
    int i = 2, j = 0;
    int ret = 0;
    int retry = 5;
    u16 addr;
    u8 val, n_map_out, offset = 0;
    s8 dser_pipe = sp->pipes[ser_pipe];
    u16 vc_id = 0;

    /* We start at index 1 since index 0 corresponds to MIPI output mapping */
    /* While we haven't reach the end of the table */
    while (table[i].addr != MAX96724_TABLE_END)
    {

		/* number of register mapping to output on current pipe
		 * only valid after the OUTPU_STREAM entry in the table */
		n_map_out++;

        /* While we haven't tried to write the register 'retry' times */
        for (j = 0; j < retry; j++)
        {
            /* For the output stream, we need to give the vc_id */
            if (table[i].addr == OUTPUT_STREAM)
            {
                vc_id = sp->vc_id << 6;
				/* number of register mapping to output on current pipe */
				n_map_out = 0;
                continue;
            }

            /* Address the right pipe in the in the deserializer */
            addr = table[i].addr + 0x40 * dser_pipe;

            /* Append the vc_id if necessary */
            val = (u8)table[i].val | vc_id;

            /* We try to write in the register and retry if necessary */
            ret = regmap_write(priv->regmap, addr, val);

            dev_dbg(&i2c_client->dev,"%s: (0x%x ; 0x%x)\n",__func__,addr,val);

            if (ret)
            {
                dev_warn(&i2c_client->dev, "write_reg_pipe: try %d\n", j);
                msleep(4);
                // if we have retried 'retry' number of time we exit
                if (j == retry-1)
                    return -1;
                //else, we retry
                continue;
            }
        }

		i++;
    }

	/* Set the CSI serial output for each register map */
	addr = csi_map_reg.addr + 0x40 * dser_pipe;

	/* This is the serial port of the jetson % number of MIPI phy of the
	 * deserializer : it works in 4x2 and 2x4 on ZED Link Dual and Quad.
	 * The modulo is there because we the second Deserializer of the Quad
	 * is connected to serial ports 4 to 8 of the Jetson module.*/
	val = 0;

	for (i=0; i<n_map_out; i++)
		val = val | (u8)((offset + (sp->phy_index%4)) << 2*i);

	ret = regmap_write(priv->regmap, addr, val);

    dev_dbg(&i2c_client->dev,"%s: (0x%x ; 0x%x)\n",__func__,addr,val);

	if (ret)
		dev_warn(&i2c_client->dev, "%s: fail write in csi reg of pipe %d\n",
				__func__, dser_pipe);

    sp->dsr_pipe=dser_pipe;
    
    /* Set the data rate for this CSI Link, there is one cam per I2C so should be all right */
	addr = csi_data_rate_reg.addr + 0x03 * sp->phy_index;

    if(sp->phy_rate > 0){
        csi_data_rate_reg.val = sp->phy_rate;
    }

	ret = regmap_write(priv->regmap, addr, csi_data_rate_reg.val);

	/* Update per-PHY CSI rate cache (programming is per opt-csi-port/PHY). */
    if (sp->phy_index >= 0 && sp->phy_index < MAX_PHY_PER_DESER) {
        priv->csi_rate_reg_val[sp->phy_index] = csi_data_rate_reg.val;
        priv->csi_rate_bps[sp->phy_index] =
            csi_tx_regval_to_bps((u8)csi_data_rate_reg.val, sp->n_lanes);
        dev_dbg(&i2c_client->dev,
                "PHY%d CSI capacity set: regval=0x%02x (%d Mbps), lanes=%d => capacity=%llu Mbps\n",
                sp->phy_index, priv->csi_rate_reg_val[sp->phy_index],
                ((csi_data_rate_reg.val & 0x1f) * 100),
                sp->n_lanes,
                priv->csi_rate_bps[sp->phy_index] / 1000000);
    }
    dev_dbg(&i2c_client->dev, "set pipping : opt-csi-port %d / vc-id %d / csi-rate=%x\n", sp->phy_index, sp->vc_id, csi_data_rate_reg.val);
    return 0;
}

static int sl_max96724_pipes_setup(struct max96724 *priv, struct sensor *sp,
                         int model, u8 link)
{
    struct i2c_client *client = priv->i2c_client;
    int err;
    unsigned int ival;
    u8 val, offset;
    u16 pipe_reg;
    u8 i;
    u8 cam_pipping = cam_pipes[model];
    u8 n_en;		/* PAR-16: pipe-enable mask width */
	s8 cam_id = -1;

    //first camera of one i2c use X Y pipes 
    //second cam use Z U pipes
    //uses i2c_bus as conflicting values because is_second_cam used for IMU and pipping
    //comes from limitation from max9296
    // for(i=0; i < priv->n_cam; i++)
    // {
    //     if(sp->i2c_bus == priv->detected_sensors[i].i2c_bus)
    //     {
    //        // sp->is_second_cam_from_i2c = true;
    //         cam_pipping = cam_pipping<<2;
    //     }
    // }

    dev_dbg(&client->dev, "%s: n_cam = %d ->  cam_pipping = 0x%x\n",
                     __func__, priv->n_cam, cam_pipping);

    /* For each serializer pipe */
    for (i=0; i<N_SER_PIPES; i++)
    {
        if (priv->avail_pipe >= N_DSER_PIPES)
        {
            dev_warn(&client->dev, "%s: no more pipes available\n",
                    __func__);
            break;
        }

		if ((sp->cam_dts_id != cam_id) && cam_id >= 0)
		{
            dev_dbg(&client->dev, "%s: no more sensor for this camera\n",
                     __func__);
			break;
		}

        if (sp->model != model && !(sp->model == ZEDX && model == ZEDXNANO)){
            dev_info(&client->dev, "%s: sensor model %d doesn't match with camera model %d\n",
                     __func__, sp->model, model);
            return 0;
        }

		cam_id = sp->cam_dts_id;

        /* if serializer's pipe i is not used (X Y Z U), continue the loop */
        if ( !(1&(cam_pipping>>i)) )
            continue;

        /**
         * pipe x-u gets assign to pipe 0-3 of the deserializer
         * first, we find an available deserializer pipe */
        pipe_reg = GMSL_PIPES_01_REG + (priv->avail_pipe>>1);

        err = regmap_read(priv->regmap, pipe_reg, &ival);
        if (err)
            return -1;

        /**
         * GMSL_PIPES_AB_REG structure is 0bGGxxHHyy
         * where A and B denote deserializer pipes A and B,
         * G and H denote gmsl deserializer ports A (0b00) to D (0b11)
         * and x and y denote serializer pipes X (0b0) to U (0b11) */
        offset = 4*(priv->avail_pipe%2);

        /* then we only write the concerned pipe, and keep the other one */
        val = (ival & (0xF0>>offset)) | i<<(offset) | link<<(2+offset);

        dev_dbg(&client->dev,"%s: Pipe %d : (0x%x ; 0x%x)",__func__, priv->avail_pipe, pipe_reg, val);

        err = regmap_write(priv->regmap, pipe_reg, val);
        if (err)
            return -1;
        
        sp->pipes[i] = priv->avail_pipe;
        
        sp->detection_id = priv->n_cam;
        
        /**
         * For the current pipe, we state the CSI
         * packets we want to forward, with the desired
         * virtual channel id.
         * eg: RAW12, Frame Start, Frame End */
        setup_sensor_pipe(priv, sp, i);
        
        dev_dbg(&client->dev, "%s: used pipes : %d\n", __func__, priv->avail_pipe);

        priv->avail_pipe++;

        /* PAR-16: enable every pipe we have actually allocated, here rather than
         * only once at the end of sl_max96724_gmsl_pipeline_setup(). That trailing
         * write was never reaching the device (measured: zero writes to 0x00F4, and
         * the register still held the init-table value 0x01), so all but pipe 0
         * stayed disabled and could not deliver frames even when mapped and locked.
         *
         * Bits [3:0] enable pipes 0..3. Bit 4 (STREAM_SELECT_ALL) must stay CLEAR:
         * it makes every pipe ingest ALL active serializer streams, which is invisible
         * with one sensor (duplicate selection) but line-interleaves both sensors into
         * both pipes the moment two stream concurrently - both outputs turn to noise
         * with zero CSI/CRC errors (measured on REX-52N, hot A/B 0x13 vs 0x03).
         * The earlier "MUST stay set" note dated from the broken pipe-stream mask era
         * (pre-f71a34309, when per-pipe selection pointed at streams the serializer
         * never sent); with the mask fixed, per-pipe selection in 0x00F0 is correct
         * and bit 4 is pure poison for concurrency. */
        n_en = (priv->avail_pipe < N_DSER_PIPES) ? priv->avail_pipe
                                                  : N_DSER_PIPES;
        err = regmap_write(priv->regmap, GMSL_PIPES_ENABLE,
                           (u8)((1u << n_en) - 1u));
        if (err)
            return -1;

        /**
         * get next sensor of this camera, since we filled the list in the right order,
         * we just need to get next element */
        if (sp->list.next != &priv->sensor_list) {
            sp = list_entry(sp->list.next, struct sensor, list);
        } else {
            dev_warn(&client->dev,"%s: No more sensor",__func__);
            break;
        }
    }

	dev_info(&client->dev, "%s: camera pipeline operational\n", __func__);

	/* Log per-PHY CSI rate cache */
	{
		u8 i;
		for (i = 0; i < priv->phy_idx_bound; i++) {
			dev_dbg(&client->dev,
				"PHY%d CSI cache: regval=0x%02x, bps=%llu\n",
				i, priv->csi_rate_reg_val[i], priv->csi_rate_bps[i]);
		}
	}

    return 0;
}

/* Enables GMSL port `i` on the deserializer and reports whether a link is
 * locked. Tries 6Gbps first; falls back to a 3Gbps probe if the 6Gbps probe
 * fails. The deserializer is restored to 6Gbps mode regardless — any
 * matching serializer will be brought up to 6Gbps later by
 * configure_3Gbps_cameras_to_6Gbps(). */
static bool gmsl_link_present(struct max96724 *priv, u8 i, int tab_id)
{
	struct i2c_client *client = priv->i2c_client;
	unsigned int link = 0;
	int reg_gmsl_ctrl_addr;
	int gmsl_3gbps_mode;
	int err;

	err = regmap_write(priv->regmap, GMSL_LINKS_EN_REG, 0xF0 | (1 << i));
	if (err)
		return false;
	msleep(SLEEP_TIME);

	err = regmap_read(priv->regmap, mode_table[tab_id][i].addr, &link);
	if (err) {
		dev_dbg(&client->dev,
			"%s: link status read failed for port %d (err %d)\n",
			__func__, i, err);
		return false;
	}

	if ((link & 0x08) >> 3)
		return true;

	/* No link at 6Gbps — probe at 3Gbps. */
	reg_gmsl_ctrl_addr = (i == 0 || i == 1) ? 0x10 : 0x11;
	gmsl_3gbps_mode    = (i == 0 || i == 2) ? 0x21 : 0x12;

	err = regmap_write(priv->regmap, reg_gmsl_ctrl_addr, gmsl_3gbps_mode);
	if (err)
		return false;
	msleep(150);

	err = regmap_read(priv->regmap, mode_table[tab_id][i].addr, &link);
	if (err)
		link = 0;
	msleep(6);

	link = (link & 0x08) >> 3;

	/* Restore deserializer to 6Gbps; configure_3Gbps_cameras_to_6Gbps()
	 * brings the serializer side up to match. */
	if (regmap_write(priv->regmap, reg_gmsl_ctrl_addr, 0x22))
		dev_warn(&client->dev,
			"%s: failed to restore 6Gbps on port %d\n",
			__func__, i);
	msleep(150);

	return link != 0;
}

/*
 * Poll the GMSL CC lock bit for @port after a GMSL_LINKS_EN_REG write.
 *
 * Bit 3 of the per-link status register is set only when both the forward PHY
 * and the reverse control channel are locked — I2C is usable once asserted:
 *   Link A: CTRL3  (0x1A) bit 3 LOCKED
 *   Link B: CTRL12 (0x0A) bit 3 LOCKED_B
 *   Link C: CTRL13 (0x0B) bit 3 LOCKED_C
 *   Link D: CTRL14 (0x0C) bit 3 LOCKED_D
 *
 * Checked at 20 ms, 70 ms, and 150 ms. Datasheet typical settling is 20 ms,
 * worst case 100 ms — the 150 ms ceiling gives margin above that. Polling exits
 * at the first passing check, so the typical path costs only 20 ms.
 *
 * Returns 0 when locked, -ETIMEDOUT if the bit is still clear at 170 ms.
 */
static int sl_max96724_poll_cc_lock(struct max96724 *priv, u8 port, int tab_id)
{
	struct i2c_client *client = priv->i2c_client;
	static const unsigned int delays_ms[] = { 20, 50, 80 };
	unsigned int elapsed = 0;
	unsigned int status = 0;
	size_t i;
	int err;

	for (i = 0; i < ARRAY_SIZE(delays_ms); i++) {
		msleep(delays_ms[i]);
		elapsed += delays_ms[i];
		err = regmap_read(priv->regmap, mode_table[tab_id][port].addr, &status);
		if (!err && (status & 0x08)) {
			dev_dbg(&client->dev, "%s: port %u CC locked at %u ms\n",
					__func__, port, elapsed);
			return 0;
		}
		dev_dbg(&client->dev,
			"%s: port %u not locked at %u ms (reg=0x%02x err=%d)\n",
			__func__, port, elapsed, status, err);
	}

	dev_dbg(&client->dev, "%s: port %u CC not locked after %u ms\n",
		 __func__, port, elapsed);
	return -ETIMEDOUT;
}

/**
 * links_check_Dser - Check the links connected to a MAX deserializer device
 * @channel: Channel ID of the device
 * @links: Pointer to an array to store the connected links
 *
 * This function checks which links are connected to a MAX deserializer device
 * on the specified @channel and stores the connected link IDs in the array
 * pointed to by @links. If no links are connected or an error occurs, it returns
 * -1. Otherwise, it returns the number of connected links and populates @links
 * with their IDs.
 */
static int sl_max96724_gmsl_pipeline_setup(struct max96724 *priv)
{
    struct i2c_client *client = priv->i2c_client;
    struct sensor *sp;
    struct list_head *pos;
    int tab_id = MAX96724_LINK_REGS;
    int err = 0;
    int model;
    bool cam_found, config_supported;
    int cam_model_count;
    u8 i;
    int active_gmsl = 0;
    bool link_active[N_GMSL_PORTS] = { false };
    
    priv->n_cam = 0;
    priv->avail_pipe = 0;

	dev_dbg(&client->dev, "%s: client addr = 0x%x\n",
			__func__, client->addr);

    for (i = 0; i < N_GMSL_PORTS; i++)
    {
        priv->port_to_i2c[i] = -1;
        link_active[i] = gmsl_link_present(priv, i, tab_id);
        if (link_active[i])
            active_gmsl++;
    }

    dev_info(&client->dev,"%s: Active GMSL ports : %d [%d %d %d %d]",__func__, active_gmsl, link_active[0], link_active[1], link_active[2], link_active[3]);

    for (i = 0; i < N_GMSL_PORTS; i++)
    {
        dev_dbg(&client->dev, "%s: port %u is %s\n", __func__, i, link_active[i] ? "active" : "inactive");  
        if (!link_active[i])
            continue;

        err = regmap_write(priv->regmap, GMSL_LINKS_EN_REG, 0xF0 | (1 << i));
        if (err)
            return -1;
        msleep(SLEEP_TIME);

        /* If 6Gbps serializer, needs to be reset as well */
        reset_all_serializers(priv);

        if (sl_max96724_poll_cc_lock(priv, i, tab_id)) {
            dev_dbg(&client->dev,
                     "%s: GMSL port %d: 6Gbps link not locked, switch to 3Gbps\n",
                     __func__, i);
            /* Configure 3Gpbs serializer (one hdr) to 6Gbps*/
            configure_3Gbps_cameras_to_6Gbps(priv, i);

            msleep(SLEEP_TIME);

            /* configure_3Gbps soft-resets the serializer, which drops and
            * re-establishes the GMSL link — wait for CC lock again. */
            if (sl_max96724_poll_cc_lock(priv, i, tab_id)) {
                dev_warn(&client->dev,
                        "%s: GMSL port %d: link lost after configure_3Gbps, skipping\n",
                        __func__, i);
                continue;
            }

            msleep(SLEEP_TIME);

        }
        msleep(SLEEP_TIME);


        /* read the camera fingerprint and return its ID */
        model = sl_max96724_get_camera_model(priv);
        if (model < 0)
        {
            dev_warn(&client->dev, "%s: Camera model unknown\n", __func__);
            continue;
        }

        cam_found = false;
        cam_model_count=0;
        /* for each sensor from the dts */
        list_for_each(pos, &priv->sensor_list)
        {
            sp = list_entry(pos, struct sensor, list);
            
            if (sp == NULL)
                return -1;

            /* if not the right model or this model has been assigned already
             * we continue looking for an available camera
             * ZEDXNANO model is using ZEDX nodes in the DTS */

            if (sp->model != model && !(sp->model == ZEDX && model == ZEDXNANO))
                continue;

            /* For 2 or less cameras, we keep the same behavior so that each get 
             * its own csi port. Dummies are not taken into account */
            // if(active_gmsl <= 2)
            // {


                if(sp->detection_id >= 0)
                    continue;


                /* first camera probed will take the first available entry 
                 * second camera probed will take first entry with a different csi port */
                // if(priv->n_cam > 0)
                //     if(sp->serial == priv->detected_sensors[0].serial)
                //         continue;

                if(sp->zedx_id == -1)
                    continue;
            //}
            /* More than 2 cameras configuration, associate right DT entry to the right GMSL
             * Allows bandwidth optimization base on the plug-in order 
             * example : 1st ZED X entry in camera-serializers associated to GMSL #0 
             * while 3nd ZED X entry associated to GMSL #2 */
            // else
            // {
            //     if(cam_model_count != i)
            //     {
            //         cam_model_count++;
            //         config_supported=false;
            //         /* If camera Stereo, skip second sensor */
            //         if(!isCameraMono(sp))
            //             pos = pos->next;
            //         continue;
            //     }

            //     if(sp->zedx_id == -1)
            //     {
            //         config_supported=false;
            //         break;
            //     }
            // }

			/* GMSL port i will be connected to the i2c bus priv->avail_i2c_bus[model] */
			priv->port_to_i2c[i] = sp->i2c_cc;
            sp->gmsl_link = i;
            sp->dsr_pipe = -1;
            
            /* we found the right sensor to initialize a camera */
            cam_found = true;
            config_supported = true;

            break;

        }

        if (!cam_found)
        {
            if(!config_supported)
            {
                int cleanup_idx;
                dev_err(&client->dev, "%s: Camera plugged in GMSL #%d wrongly placed. Check user guide for camera placement info  \n", __func__, i);
                /* Cleanup any already-initialized mutexes */
                for (cleanup_idx = 0; cleanup_idx < 2*N_GMSL_PORTS; cleanup_idx++)
                    if (priv->detected_sensors[cleanup_idx].cam_addr != 0)
                        mutex_destroy(&priv->detected_sensors[cleanup_idx].bw_lock);
                return -EINVAL;
            }
            else
            {
                int cleanup_idx;
                dev_warn(&client->dev, "%s: Known camera connected, but entry not found in DTS.\n",
                        __func__);
                dev_warn(&client->dev, "%s: Do you have the right DTS?\n",
                        __func__);
                /* Cleanup any already-initialized mutexes */
                for (cleanup_idx = 0; cleanup_idx < 2*N_GMSL_PORTS; cleanup_idx++)
                    if (priv->detected_sensors[cleanup_idx].cam_addr != 0)
                        mutex_destroy(&priv->detected_sensors[cleanup_idx].bw_lock);
                return -EINVAL;
            }

			continue;
        }

        sp->is_second_cam_from_i2c = false;
        sl_max96724_pipes_setup(priv, sp, model, i);

        priv->detected_sensors[priv->n_cam] = *sp;
        priv->detected_sensors[priv->n_cam].current_bps = 0;
        priv->detected_sensors[priv->n_cam].is_streaming = false;
        mutex_init(&priv->detected_sensors[priv->n_cam].bw_lock);
        
        /* If stereo camera, save the second sensor to upper half of array */
        if (!isCameraMono(sp) && pos->next != &priv->sensor_list)
        {
            struct sensor *sp_second = list_entry(pos->next, struct sensor, list);
            if (sp_second->model == model && sp_second->cam_dts_id == sp->cam_dts_id)
            {
                priv->detected_sensors[priv->n_cam + N_GMSL_PORTS] = *sp_second;
                priv->detected_sensors[priv->n_cam + N_GMSL_PORTS].current_bps = 0;
                priv->detected_sensors[priv->n_cam + N_GMSL_PORTS].is_streaming = false;
                mutex_init(&priv->detected_sensors[priv->n_cam + N_GMSL_PORTS].bw_lock);
            }
        }
        
        priv->n_cam++;

        dev_info(&client->dev, "%s: GMSL #%d : Link Camera %s (id: %d) to port-index %d",__func__,i,sp->camera,sp->zedx_id,sp->serial);

        err = apply_alternate_mapping(priv, sp);
        if(err)
        {
            dev_err(&client->dev, "%s: Failed to apply alternate mapping %d", __func__, err);
            return err;
        }
    }

    /* enable build the correct i2c_map */
    sl_max96724_i2c_setup(priv);
    if(err)
    {
        dev_err(&client->dev, "%s: Failed to map i2c Control Channels %d", __func__, err);
        return err;
    }

    /* Debug: Print detected_sensors array */
    #ifdef DEBUG
    dev_dbg(&client->dev, "=== detected_sensors [0..%d] (first sensors) ===\n", priv->n_cam - 1);
    for (i = 0; i < priv->n_cam; i++) {
        dev_dbg(&client->dev, "[%d] %s zedx_id=%d model=%d gmsl=%d phy=%d serial=%d cam_addr=%d is_second=%d\n",
                i,
                priv->detected_sensors[i].camera,
                priv->detected_sensors[i].zedx_id,
                priv->detected_sensors[i].model,
                priv->detected_sensors[i].gmsl_link,
                priv->detected_sensors[i].phy_index,
                priv->detected_sensors[i].serial,
                priv->detected_sensors[i].cam_addr,
                priv->detected_sensors[i].is_second_cam_from_i2c);
    }
    dev_dbg(&client->dev, "=== detected_sensors [%d..%d] (second sensors) ===\n", N_GMSL_PORTS, N_GMSL_PORTS + priv->n_cam - 1);
    for (i = N_GMSL_PORTS; i < N_GMSL_PORTS + priv->n_cam; i++) {
        if (priv->detected_sensors[i].cam_addr != 0) {
            dev_dbg(&client->dev, "[%d] %s zedx_id=%d model=%d gmsl=%d phy=%d serial=%d cam_addr=%d\n",
                     i,
                     priv->detected_sensors[i].camera,
                     priv->detected_sensors[i].zedx_id,
                     priv->detected_sensors[i].model,
                     priv->detected_sensors[i].gmsl_link,
                     priv->detected_sensors[i].phy_index,
                     priv->detected_sensors[i].serial,
                     priv->detected_sensors[i].cam_addr);
        }
    }
    #endif
    return err;
}

static int sl_max96724_write_table(struct max96724 *priv,
        const struct index_reg_8 table[])
{
    struct i2c_client *i2c_client = priv->i2c_client;
    int i = 0, j = 0;
    int ret = 0;
    int retry = 5;

    // While we haven't reach the end of the table
    while (table[i].addr != MAX96724_TABLE_END)
    {
        // While we haven't tried to write the register 'retry' times
        for (j = 0; j < retry; j++)
        {
            // We try to write the register. 
            ret = write_reg_Dser(priv->channel, table[i].addr, (u8)table[i].val);
            // if the return value is bad
            if (ret && ((table[i].addr != 0x0000) || (table[i].addr != 0x0013)))
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
            if (0x0013 == table[i].addr || 0x0000 == table[i].addr ||
                    0x0018 == table[i].addr || 0x0006 == table[i].addr)
                msleep(100);
            else if (0x0003 == table[i].addr || 0x0007 == table[i].addr) 
                msleep(30);
            break;
        }
        i++;
    }
    return 0;
}

// configuration for slave/master mode
// sync_mode == 0:
//  > Master mode: Internal Fsync + output Fsync on MFP sync (default=MFP10)
// sync_mode == 1:
//  > Master mode deser 1: Internal Fsync + output Fsync on MFP sync (default=MFP10)
//  > Slave mode deser 2: External Fsync + input Fsync on MFP sync (default=MFP10)
// sync_mode == 2:
//  > Slave mode deser 1: External Fsync + input Fsync on MFP sync (default=MFP10)
//  > Slave mode deser 2: External Fsync + input Fsync on MFP sync (default=MFP10)
static int sl_max96724_configure_sync_mode(struct max96724 *priv){
	int err = 0;
	int mfp_trig_info_addr = 0;
    int hw_rqst_slave_mode = 0; // 0: Master mode, 1: Slave mode

    if(sync_mode < 0 || sync_mode > 2){
        sync_mode = 0; // default to master mode
    }

    switch (priv->mfp_trig_out) {
        case 10:
            err = regmap_write(priv->regmap, MAX96724_FSYNC_CONF, 0x24); // MFP10 as output
            break;
        case 2:
            err = regmap_write(priv->regmap, MAX96724_FSYNC_CONF, 0x04); // MFP2 as output
            break;
        default:
            dev_err(&priv->i2c_client->dev, "%s: Invalid mfp_trig_out value (%d) - Defaulting to MFP10 \n",
                __func__, priv->mfp_trig_out);
            err = regmap_write(priv->regmap, MAX96724_FSYNC_CONF, 0x24); // Default to MFP10 as output
    }
    if(err < 0){
        dev_err(&priv->i2c_client->dev, "%s: Failed to configure mfp_trig_out (%d) \n",
            __func__, err);
        return err;
    }

    dev_dbg(&priv->i2c_client->dev, "%s: mfp_trig_out configured to MFP%d \n",
                __func__, priv->mfp_trig_out);

	// Check if the mfp-trig-info has been configured in the device tree
	// This MFP is used to automatically configure the slave mode without having to set sync_mode when loading the driver
    if(priv->mfp_trig_info != -1){
		// MFP0 is 0x2B0 and each GPIO is offset by 3 register
        int offset_link = (priv->mfp_trig_info > 4) ? (priv->mfp_trig_info > 9) ? (priv->mfp_trig_info > 14) ? 
            MAX96724_GPIOA_ADDR+3 : MAX96724_GPIOA_ADDR+2 : MAX96724_GPIOA_ADDR+1 : MAX96724_GPIOA_ADDR;

        mfp_trig_info_addr = offset_link + (3*priv->mfp_trig_info);
		err = regmap_read(priv->regmap, mfp_trig_info_addr, &hw_rqst_slave_mode);

		if(err < 0){ // Retry reading the register
			msleep(6);
			err = regmap_read(priv->regmap, mfp_trig_info_addr, &hw_rqst_slave_mode);
			if(err < 0){
				dev_err(&priv->i2c_client->dev, "%s: Failed to read mfp trig info state (%d) - Defaulting to master mode \n",
                    __func__, err);
				return 0;
			}
		}
		msleep(6);

		// MFP value is written in bit 3, if MFP is HIGH then the board mode = slave
		hw_rqst_slave_mode = (hw_rqst_slave_mode >> 3) & 0x01;
	}

    // Master mode
    if(sync_mode == 0 && !hw_rqst_slave_mode){
		dev_dbg(&priv->i2c_client->dev, "Sync mode configured - Deser set as Master\n");
        return err;
    }

    // Checking for board mode in Master/Slave configuration
    // Sync mode should be set to 1 and deser should be the first
    // hw_config should be ==0 not to force slave mode
    if((sync_mode == 1 && priv->channel == 0) && !hw_rqst_slave_mode){ 
		dev_dbg(&priv->i2c_client->dev, "Sync mode configured - Deser set as Master\n");
        return err;
    }

    // At this stage we should only have slave mode request
    // e.g: trig_info requested slave mode, sync_mode was set to 1 or 2
	if(hw_rqst_slave_mode || sync_mode > 0) {
		struct index_reg_8 slave_mode_table[10] = {0};
		err = get_max96724_slave_mode_table(priv->mfp_trig_in, slave_mode_table, sizeof(slave_mode_table));
		if(err){
			dev_err(&priv->i2c_client->dev, "%s: slave mode table failed to initialize: %d\n",
				__func__, err);
			return err;
		}
		err = sl_max96724_write_table(priv, slave_mode_table);
		dev_info(&priv->i2c_client->dev, "Sync mode configured\n");
	}

	return err;
}

static int slow_reset_Dser(int channel)
{
    int err;

    if (channel > 3 || channel < 0 || global_priv[channel] == NULL)
        return -1;

    if(global_priv[channel]->n_lanes == 2)
    {
        err = sl_max96724_write_table(global_priv[channel], mode_table[MAX96724_INIT]);
        dev_info(&global_priv[channel]->i2c_client->dev, "%s: Setup Deser as 2 lanes \n", __func__);
    }
       
    if(global_priv[channel]->n_lanes == 4)
    {
        err = sl_max96724_write_table(global_priv[channel], mode_table[MAX96724_INIT_2x4]);
        dev_info(&global_priv[channel]->i2c_client->dev, "%s: Setup Deser as 4 lanes \n", __func__);
    }
        
    if (err)
        return -1;

    dev_dbg(&global_priv[channel]->i2c_client->dev, "%s: Setup Deser as %d lanes \n", __func__, global_priv[channel]->n_lanes);
    
    err = sl_max96724_configure_sync_mode(global_priv[channel]);
	    if(err) return -1;

	/* Cache the FSYNC config register once, fsync_set_Dser then toggles 
	 * only the gate bit off this shadow */
    {
        unsigned int v;
        if (!regmap_read(global_priv[channel]->regmap, MAX96724_FSYNC_CONF, &v))
            global_priv[channel]->fsync_conf = (u8)v;
        else /* read failed: fall back to a mode-consistent enabled value */
            global_priv[channel]->fsync_conf =
                (global_priv[channel]->mode == SLAVE_MODE) ? 0x08 : 0x24;
    }

    return 0;
}
int dser_get_gmsl_port(int channel, int zedx_id){
    int err = -1;
    struct list_head *pos;
	struct sensor *sp;

    if (global_priv[channel]->initialized == 0)
        return err;

    list_for_each(pos, &global_priv[channel]->sensor_list){
		sp = list_entry(pos, struct sensor, list);
        if (sp == NULL){
			return err;
		}

        if (sp->zedx_id != zedx_id)
			continue;
        
        if(sp->gmsl_link == -1)
        {
            dev_info(&global_priv[channel]->i2c_client->dev,
                "%s: sp->gmsl_link == -1\n",__func__);
            return err;
        }

        return sp->gmsl_link + (channel * N_GMSL_PORTS);
    }

    return -1;
}
EXPORT_SYMBOL(dser_get_gmsl_port);

static int get_video_pipe(int channel, int zedx_id){
    int err = -1;
    struct list_head *pos;
    struct sensor *sp;

    list_for_each(pos, &global_priv[channel]->sensor_list){
        sp = list_entry(pos, struct sensor, list);
        if (sp == NULL){
            return err;
        }

        if (sp->zedx_id != zedx_id)
            continue;

        if(sp->dsr_pipe == -1)
        {
            dev_err(&global_priv[channel]->i2c_client->dev,
                "%s: Invalid video pipe value\n",__func__);
            return err;
        }

        return sp->dsr_pipe;
    }

    return -1;
}

int dser_read_link_lock(int channel, int zedx_id){
    int err = -1;
    int val = 0;
    int gmsl_link = -1;

    if (channel > 3 || channel < 0 || global_priv[channel] == NULL)
        return err;

    gmsl_link = dser_get_gmsl_port(channel, zedx_id);
    gmsl_link = gmsl_link - (channel * N_GMSL_PORTS); // (channel * N_GMSL_PORTS) is added in get gmsl port to take into account board with multiple deser

    if(gmsl_link < 0 || gmsl_link >= N_GMSL_PORTS)
        return err;

    err = regmap_read(global_priv[channel]->regmap, max96724_link_regs[gmsl_link].addr, &val);

    if(err)
        return err;

    val = (val >> 3) & 0x01;

    return val;
}
EXPORT_SYMBOL(dser_read_link_lock);

int dser_read_video_lock(int channel, int zedx_id){
    int err = -1;
    int val = 0;
    int dsr_pipe = -1;

    if (channel > 3 || channel < 0 || global_priv[channel] == NULL)
        return err;

    dsr_pipe = get_video_pipe(channel, zedx_id);

    if(dsr_pipe < 0 || dsr_pipe >= N_DSER_PIPES)
        return err;

    err = regmap_read(global_priv[channel]->regmap, (VIDEO_LOCK_STATUS_REG + 0x20*dsr_pipe), &val);
    if(err)
        return err;

    val = val & 0x01;

    return val;
}
EXPORT_SYMBOL(dser_read_video_lock);

int dser_enable_gmsl_link(int channel, int zedx_id){
	int err = -1;
	struct list_head *pos;
	struct sensor *sp;
    int i=0;

    if (global_priv[channel]->initialized == 0)
        return err;

    for( i=0; i<2; i++)
    {
        if (global_priv[i]->initialized == 0)
            continue;
        err = write_reg_Dser(i, GMSL_LINKS_EN_REG, 
            0xF0);
        if (err)
            return -1;
        msleep(100);
    }
    
    list_for_each(pos, &global_priv[channel]->sensor_list){
		sp = list_entry(pos, struct sensor, list);
		if (sp == NULL){
			return err;
		}
      
        if (sp->zedx_id != zedx_id)
			continue;

        if(sp->gmsl_link == -1)
        {
        dev_info(&global_priv[channel]->i2c_client->dev,
            "%s: sp->gmsl_link == -1\n",__func__);
            return err;
        }

        err = write_reg_Dser(channel, GMSL_LINKS_EN_REG, 
            0xF0 | (1<<sp->gmsl_link));

        if (err){
            msleep(4);
            err = write_reg_Dser(channel, GMSL_LINKS_EN_REG, 
            0xF0 | (1<<sp->gmsl_link));
        }

        if (err)
            return -1;

        dev_dbg(&global_priv[channel]->i2c_client->dev,
            "%s: open GMSL link %d for zedx-id %d\n",
            __func__, sp->gmsl_link, zedx_id);

        return sp->gmsl_link + (channel * N_GMSL_PORTS);
    }

    return -1;
}
EXPORT_SYMBOL(dser_enable_gmsl_link);

int dser_open_all_gmsl_link(int channel){
    int i, j, err = -1;
    u8 val;

    for(i = 0; i < 2 ; i++)
    {
        if (global_priv[channel]->initialized == 0)
                return err;
                
        for( j=0; j<N_GMSL_PORTS; j++)
        {
            val = 0xF0 | ((1 << (j+1)) - 1);
            err = write_reg_Dser(i, GMSL_LINKS_EN_REG, 
                            val);
            dev_dbg(&global_priv[i]->i2c_client->dev,
            "%s: open GMSL link %d -> 0x%x\n",
            __func__, j, val);
            msleep(50);
        }
    }

   

    return err;
}
EXPORT_SYMBOL(dser_open_all_gmsl_link);

int set_bitrate_Dser(int channel, u32 zedx_id, u8 val)
{
	struct sensor *sp;
	struct list_head *pos;
	int err = -1;
	u8 reg, i;
	u16 addr;

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

	list_for_each(pos, &global_priv[channel]->sensor_list)
	{
		sp = list_entry(pos, struct sensor, list);
		if (sp == NULL)
		{
			return -1;
		}

		/* if not the right bus or this model was not detected
		 * we continue looking for an available camera */
		if (!(sp->zedx_id == zedx_id) || sp->detection_id < 0)
			continue;

		for (i = 0; i<N_SER_PIPES; i++)
		{
			if (sp->pipes[i]<0) 
				continue;

			addr = GMSL_VID_ST_MAP+0x40*sp->pipes[i];

			/* MAP_SRC must stay VC0 (the serializer transmits VC0);
			 * only MAP_DST carries the vc retag */
			err = write_reg_Dser(channel, addr, reg);

			addr = addr+1;

			err = write_reg_Dser(channel, addr, reg | (sp->vc_id<<6));

		}
	}

	return err;
}
EXPORT_SYMBOL(set_bitrate_Dser);

int isSecondCamFromI2C(int channel, int zedx_id)
{
    struct sensor *sp;
    u8 i = 0;

    if (global_priv[channel]->initialized == 0)
        return -1;

    for( i=0; i < global_priv[channel]->n_cam; i++)
    {
        sp = &global_priv[channel]->detected_sensors[i];
        if( zedx_id == sp->zedx_id)
        {
            return sp->is_second_cam_from_i2c;
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
        {0x04A5, 0x34}, /* frame rate byte 0 */
        {0x04A6, 0xB7}, /* frame rate byte 1 */
        {0x04A7, 0x0C}, /* frame rate byte 2 */
        {MAX96724_TABLE_END, 0x00}
    };

    if (channel > 3 || channel < 0 || global_priv[channel] == NULL)
        return -1;

    dev_dbg(&global_priv[channel]->i2c_client->dev,
        "%s: Requested FPS = %lld\n",
        __func__, val);

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

    err = sl_max96724_write_table(global_priv[channel], fps_arr);

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

    err = write_reg_Dser(channel, MAX96724_FSYNC_CONF, val);
    if (!err)
        global_priv[channel]->fsync_conf = val;

    return err;
}
EXPORT_SYMBOL(fsync_set_Dser);

static int sl_max96724_parse_serializer_node(struct max96724 *priv,
		struct device_node *ser_node, s8 cam_id)
{
	int err = 0;
    struct i2c_client *i2c_client = priv->i2c_client;
	struct device_node *cam_node;
	struct device_node *mux_node;
	struct device_node *ports_node, *port_node, *endpoint_node;
    struct sensor *sp;
	const char *str;
    int n_sensors;
    u8 i,j;

	if (!ser_node)
		return -1;

    n_sensors = of_count_phandle_with_args(ser_node, "camera-sensors", NULL);

    for (i = 0; i<n_sensors ; i++)
    {
        
        sp = devm_kzalloc(&(priv->i2c_client)->dev, sizeof(*sp), GFP_KERNEL);

        list_add_tail(&sp->list, &priv->sensor_list);
        
        for (j = 0; j<N_SER_PIPES; j++)
        {
            sp->pipes[j] = -1;
        }

        sp->detection_id = -1;
        sp->gmsl_link = -1;
        sp->phy_rate = -1;

        of_property_read_string(ser_node, "camera_model", &sp->camera);
        for (j=0; j<N_CAM_TYPE; j++)
        {
            if (strcmp(sp->camera, camera_names[j])==0)
                sp->model = j;
        }

        dev_dbg(&i2c_client->dev, "%s: Add %s (model : %d)",__func__, sp->camera, sp->model );

        /* Parse zedx-id */
        err = of_property_read_string(ser_node, "zedx-id", &str);
        if(err){
            dev_err(&i2c_client->dev, "%s: 'zedx-id' missing in serializer node %s (if it is a dummy, set zedx_id = -1)",__func__,ser_node->full_name);
            return -EINVAL;
        }

        err = kstrtoint(str,10,&sp->zedx_id);
        if(err)
        {
            dev_err(&i2c_client->dev, "%s: zedx-id conversion to int failed", __func__);
            return -1;
        }
        /* if cam is dummy (declared with id = -1 in DTS) */
        if(sp->zedx_id == -1)
            continue;

        /* Sensor's info */
        cam_node = of_parse_phandle(ser_node, "camera-sensors", i);
        if (cam_node==NULL)
        {  
            dev_warn(&i2c_client->dev, "%s: no cam node in ser node...\n", __func__);
            continue;
        }
        of_property_read_u32(cam_node, "reg", &sp->cam_addr);
        dev_dbg(&i2c_client->dev, "%s: CAM ADDR = %x",__func__, sp->cam_addr);

        of_property_read_u32(ser_node, "reg", &sp->ser_addr);
        dev_dbg(&i2c_client->dev, "%s: SER ADDR = %x",__func__, sp->ser_addr);

        priv->ser_devices[cam_id].ser_addr = sp->ser_addr;
        priv->ser_devices[cam_id].camera_model = sp->model;

        err = of_property_read_u32(cam_node, "reg", &sp->cam_addr);
        if(err){
            of_node_put(cam_node);
            dev_err(&i2c_client->dev, "%s: 'reg' missing in camera node %s",__func__,cam_node->full_name);
            return -EINVAL;
        }

        /* porting info */
        mux_node = of_get_parent(cam_node);
        if (mux_node==NULL)
        {  
            dev_warn(&i2c_client->dev, "%s: no parent node ?\n", __func__);
            of_node_put(cam_node);
            continue;
        }
        of_property_read_u32(mux_node, "reg", &sp->i2c_bus);
        
        of_node_put(mux_node);
        
        ports_node = of_get_child_by_name(cam_node, "ports");
        
        if (ports_node==NULL)
        {  
            dev_warn(&i2c_client->dev, "%s: no ports node amongst children...\n", __func__);
            of_node_put(cam_node);
            continue;
        }
        port_node = of_get_child_by_name(ports_node, "port");
        if (port_node==NULL)
        {  
            dev_warn(&i2c_client->dev, "%s: no port node amongst children...\n", __func__);
            of_node_put(cam_node);
            of_node_put(ports_node);
            continue;
        }
        endpoint_node = of_get_child_by_name(port_node, "endpoint");
        if (endpoint_node==NULL)
        {  
            dev_warn(&i2c_client->dev, "%s: no endpoint node amongst children...\n", __func__);
            of_node_put(cam_node);
            of_node_put(ports_node);
            of_node_put(port_node);
            continue;
        }

        err = of_property_read_u32(endpoint_node, "vc-id", &sp->vc_id);
        if(err)
        {
            of_node_put(cam_node);
            of_node_put(ports_node);
            of_node_put(port_node);
            of_node_put(endpoint_node);
            dev_err(&i2c_client->dev,"%s: Sensor %d is missing vc-id entry",__func__,sp->zedx_id);
            return -EINVAL;
        }
        if(sp->vc_id<0 || sp->vc_id>3)
        {
            of_node_put(cam_node);
            of_node_put(ports_node);
            of_node_put(port_node);
            of_node_put(endpoint_node);
            dev_err(&i2c_client->dev,"%s: Sensor %d VC-ID out of range [0,1,2,3]",__func__,sp->zedx_id);
            return -EINVAL;
        }

        err = of_property_read_u32(endpoint_node, "bus-width", &sp->n_lanes);
        if(err)
        {
            of_node_put(cam_node);
            of_node_put(ports_node);
            of_node_put(port_node);
            of_node_put(endpoint_node);
            dev_err(&i2c_client->dev,"%s: Sensor %d is missing bus-width entry",__func__,sp->zedx_id);
            return -EINVAL;
        }
        if(sp->n_lanes != 2 && sp->n_lanes != 4)
        {
            of_node_put(cam_node);
            of_node_put(ports_node);
            of_node_put(port_node);
            of_node_put(endpoint_node);
            dev_err(&i2c_client->dev,"%s: Sensor %d bus-width out of range (2 or 4 lanes only)",__func__,sp->zedx_id);
            return -EINVAL;
        }

        err = of_property_read_u32(endpoint_node, "port-index", &sp->serial);
        if(err)
        {
            of_node_put(cam_node);
            of_node_put(ports_node);
            of_node_put(port_node);
            of_node_put(endpoint_node);
            dev_err(&i2c_client->dev,"%s: Sensor %d is missing port-index entry",__func__,sp->zedx_id);
            return -EINVAL;
        }

        // /* Parse optional csi-rate, e.g 1600Mhz should be: opt-csi-rate = <1600> */
        // of_property_read_u32(endpoint_node, "opt-csi-rate", &sp->phy_rate);
        of_property_read_u32(endpoint_node, "opt-csi-rate", &sp->phy_rate);
        if(sp->phy_rate > 0){
            sp->phy_rate /= 100;
            sp->phy_rate = sp->phy_rate | (1 << 5); // disable software override
        }

        err = of_property_read_string(endpoint_node, "opt-csi-port", &str);
        if(err)
        {
            of_node_put(cam_node);
            of_node_put(ports_node);
            of_node_put(port_node);
            of_node_put(endpoint_node);
            dev_err(&i2c_client->dev,"%s: Sensor %d is missing opt-csi-port entry",__func__,sp->zedx_id);
            return -EINVAL;
        }
        if(sp->n_lanes == 2)
        {
            sp->phy_index = str[0] - 'c';
            if(sp->phy_index < 0 || sp->phy_index > 3)
            {
                of_node_put(cam_node);
                of_node_put(ports_node);
                of_node_put(port_node);
                of_node_put(endpoint_node);
                dev_err(&i2c_client->dev, "%s: Sensor %d opt-csi-port out of range ! For 2 lanes configuration : c,d,e,f",__func__,sp->zedx_id);
                return -EINVAL;
            }
        }
        else
        {
            sp->phy_index = str[0] - 'a';
            if(sp->phy_index < 0 || sp->phy_index > 1)
            {
                of_node_put(cam_node);
                of_node_put(ports_node);
                of_node_put(port_node);
                of_node_put(endpoint_node);
                dev_err(&i2c_client->dev, "%s: Sensor %d opt-csi-port out of range ! For 4 lanes configuration : a,b",__func__,sp->zedx_id);
                return -EINVAL;
            }
        }

        of_property_read_u32(endpoint_node, "i2c-cc", &sp->i2c_cc);

        priv->n_lanes = sp->n_lanes;

		dev_dbg(&i2c_client->dev, "%s: associated vc-id = %d\n", __func__, sp->vc_id);
		dev_dbg(&i2c_client->dev, "%s: associated n_lanes = %d\n", __func__, sp->n_lanes);
		dev_dbg(&i2c_client->dev, "%s: associated mipi port = %d\n", __func__, sp->serial);
		dev_dbg(&i2c_client->dev, "%s: associated opt-csi-port = %d\n", __func__, sp->phy_index);
		dev_dbg(&i2c_client->dev, "%s: associated i2c control channel = %d\n", __func__, sp->i2c_cc);
        
        of_node_put(cam_node);
        of_node_put(ports_node);
        of_node_put(port_node);
        of_node_put(endpoint_node);
        
        sp->cam_dts_id = cam_id;

        dev_dbg(&i2c_client->dev, "%s: Parse camera %d (%s) -> SER addr : %d | CAM addr : %d", 
            __func__, sp->cam_dts_id, sp->camera, sp->ser_addr, sp->cam_addr);
        dev_dbg(&i2c_client->dev, "%s: (i2c cc %d -> i2c_bus %d) | (phy %d | vc-id %d) -> serial port %d", 
            __func__, sp->i2c_cc, sp->i2c_bus, sp->phy_index ,sp->vc_id, sp->serial);
        dev_dbg(&i2c_client->dev, "%s: -------------------------------------------------------------", 
            __func__);

    }

	return 0;
}

static int sl_max96724_parse_dt(struct max96724 *priv)
{
	int err = 0;
	struct i2c_client *i2c_client = priv->i2c_client;
	struct device_node *np = i2c_client->dev.of_node;
	struct device_node *ser_np;
	//struct of_phandle_args args;
	const char *str;
    int n_serializers;
	s8 i;

	err = of_property_read_string(np, "channel", &str);
	priv->channel = str[0]-'a';
	if (err) {
        dev_err(&i2c_client->dev, "%s: Channel not found --> Requires 'channel' entry in DTS\n", __func__);
        return -EINVAL;
    }
	if (priv->channel < 0 ||  priv->channel > 3 ) {
        dev_err(&i2c_client->dev, "%s: Channel value must be : a,b,c,d \n", __func__);
	    return -EINVAL;
    }

	priv->mfp_trig_info = -1;
	err = of_property_read_u32(np, "mfp-trig-info", &priv->mfp_trig_info);
	if (err || (priv->mfp_trig_info < 0 || priv->mfp_trig_info > MAX96724_NB_MFP)){
		dev_dbg(&i2c_client->dev,
				"%s: 'mfp-trig-info' not found or invalid, assuming no trigger info\n", __func__);
		priv->mfp_trig_info = -1;
	}

    priv->mfp_trig_in = -1;
    err = of_property_read_u32(np, "mfp-trig-in", &priv->mfp_trig_in);
    if (err || priv->mfp_trig_in < 0 || priv->mfp_trig_in > MAX96724_NB_MFP){
        dev_dbg(&i2c_client->dev, 
            "%s: 'mfp-trig-in' not found or invalid, defaulting to MFP10\n", __func__);
        priv->mfp_trig_in = 10;
    }

	priv->mfp_trig_out = -1;
	err = of_property_read_u32(np, "mfp-trig-out", &priv->mfp_trig_out);
	if (err || (priv->mfp_trig_out != 2 && priv->mfp_trig_out != 10)){
		dev_dbg(&i2c_client->dev,
				"%s: 'mfp-trig-out' not found or unsupported, defaulting to MFP10\n", __func__);
		priv->mfp_trig_out = 10;
	}

	global_priv[priv->channel] = priv;

    n_serializers = of_count_phandle_with_args(np, "camera-serializers", NULL);

    dev_dbg(&i2c_client->dev, "%s: Number of declared cameras with this deserializer : %d\n",
     __func__, n_serializers);
    
     if(n_serializers % 4 != 0)
        dev_warn(&i2c_client->dev,"%s: Camera not declared for all GMSL link",__func__);

    if(n_serializers % 4 != 0)
        dev_warn(&i2c_client->dev,"%s: Camera not declared for all GMSL link. If you don't want a camera at a certain port, set a dummy instead",__func__);

    /* retrieve all information for each port */
	for ( i = 0 ; i < n_serializers ; i++ )
	{

		ser_np = of_parse_phandle(np, "camera-serializers" , i);

		if ( ser_np == NULL )
		{
			dev_info(&i2c_client->dev,
					"%s: Issue getting node pointer to serializer %d\n", __func__, i);
			continue;
		}
		err = sl_max96724_parse_serializer_node(priv, ser_np , i);

		if (err)
			dev_info(&i2c_client->dev,
					"%s: Issue parsing serializer node %d\n", __func__, i);
	
		of_node_put(ser_np);

	}

	return 0;
}


static int sl_max96724_parse_gpios(struct max96724 *priv)
{
    struct i2c_client *i2c_client = priv->i2c_client;
    struct device_node *node = i2c_client->dev.of_node;
    int gpio = 0;
	gpio = of_get_named_gpio(node, "reset-gpio", 0);

	if (gpio > 0)
	{
		priv->reset_gpio = gpio;
		gpio_direction_output(priv->reset_gpio, 1);
    }
	else
	{
        dev_dbg(&i2c_client->dev, "%s: No reset GPIO in the dts.\n", __func__);
    }

	gpio = of_get_named_gpio(node, "pwdn-gpio", 0);

	if (gpio > 0)
	{
		priv->pwdn_gpio = gpio;
		gpio_direction_output(priv->pwdn_gpio, 1);
    }
	else
	{
        dev_dbg(&i2c_client->dev, "%s: No pwdn GPIO in the dts.\n", __func__);
    }

	gpio = of_get_named_gpio(node, "pwr-gpio", 0);
	if (gpio > 0)
	{
		priv->pwr_gpio = gpio;
		gpio_direction_output(priv->pwr_gpio, 1);
    }
	else
	{
        dev_dbg(&i2c_client->dev, "%s: No pwr GPIO in the dts.\n", __func__);
    }

    return 0;
	
}

static struct regmap_config sl_max96724_regmap_config = {
    .reg_bits = 16,
    .val_bits = 8,
    .cache_type = REGCACHE_NONE, //No cache for proper reset
};

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 0, 0)
static int sl_max96724_probe(struct i2c_client *client,
        const struct i2c_device_id *id)
#else
static int sl_max96724_probe(struct i2c_client *client)
#endif
{
    struct device *dev = &client->dev;
    struct max96724 *priv;
    int err = 0;
    unsigned int pipe_sync_val;
    
	dev_info(dev, "Driver Version : v%d.%d.%d\n",DESER_DRIVER_VERSION_MAJOR,DESER_DRIVER_VERSION_MINOR,DESER_DRIVER_VERSION_PATCH);

    priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);

    INIT_LIST_HEAD(&priv->sensor_list);

    priv->avail_pipe = 0;
    priv->mode = MASTER_MODE;
    priv->fsync_conf = 0x24; // master-enabled default; reseeded from HW after init
    priv->initialized = 0;
    priv->i2c_client = client;
    priv->regmap = devm_regmap_init_i2c(priv->i2c_client,
            &sl_max96724_regmap_config);
    if (IS_ERR(priv->regmap))
    {
        dev_err(dev,
                "regmap init failed: %ld\n", PTR_ERR(priv->regmap));
        return -ENODEV;
    }

    err = sl_max96724_parse_gpios(priv);

    err = sl_max96724_parse_dt(priv);
    if(err)
    {
        dev_warn(dev, "%s: Deser initialization failed",__func__);
        return -EINVAL;
    }    

    slow_reset_Dser(priv->channel);

	err = sl_max96724_gmsl_pipeline_setup(priv);
    if(err)
    {
        int cleanup_idx;
        dev_warn(dev, "%s: Deser initialization failed",__func__);
        /* Cleanup any mutexes initialized during pipeline setup */
        for (cleanup_idx = 0; cleanup_idx < 2*N_GMSL_PORTS; cleanup_idx++)
            if (priv->detected_sensors[cleanup_idx].cam_addr != 0)
                mutex_destroy(&priv->detected_sensors[cleanup_idx].bw_lock);
        return -EINVAL;
    }

    /* Build csi_group to phy_index lookup table for fast BW lookups */
    {
        int i, csi_group, phy_idx;
        for (i = 0; i < MAX_CSI_GROUPS; i++)
            priv->csi_group_to_phy_idx[i] = -1;

        for (i = 0; i < 2 * N_GMSL_PORTS; i++) {
            if (priv->detected_sensors[i].cam_addr == 0)
                continue;

            csi_group = (int)priv->detected_sensors[i].serial;
            phy_idx = priv->detected_sensors[i].phy_index;

            if (csi_group >= 0 && csi_group < MAX_CSI_GROUPS &&
                phy_idx >= 0 && phy_idx < MAX_PHY_PER_DESER) {
                priv->csi_group_to_phy_idx[csi_group] = phy_idx;
                if ((u8)(phy_idx + 1) > priv->phy_idx_bound)
                    priv->phy_idx_bound = (u8)(phy_idx + 1);
                dev_dbg(dev, "CSI lookup: csi_group %d => phy_idx %d\n",
                    csi_group, phy_idx);
            }
        }
        if (priv->phy_idx_bound == 0) {
            if (priv->n_cam > 0)
                dev_warn(dev, "%s: phy_idx_bound=0 after building CSI lookup table with %d camera(s) — check phy_index/csi_group validity in DTS\n",
                    __func__, priv->n_cam);
            priv->phy_idx_bound = 1;
        }
    }

    if(priv->n_cam == 0)
        dev_info(dev, "%s: No Camera connected to this deserializer",__func__);

    /* MAX96724 needs the exact number of used pipes for SYNC */
    pipe_sync_val = 0xC0 | ((1 << priv->avail_pipe) - 1);
    err = write_reg_Dser(priv->channel, 0x04AF, pipe_sync_val);
    dev_info(dev,"%s: set SYNC 0x04AF to 0x%x", __func__, pipe_sync_val);

    err = write_reg_Dser(priv->channel, 0x0435, 0x04);
    dev_info(dev,"%s: set FrameBlock 0x0435 to 0x%x", __func__, 0x04);

    /*set daymode by fault*/
    dev_info(dev, "%s: success\n", __func__);
    priv->initialized = 1;
    i2c_set_clientdata(client, priv);
    return err;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 0, 0)
static int sl_max96724_remove(struct i2c_client *client)
#else
static void sl_max96724_remove(struct i2c_client *client)
#endif
{
    struct max96724 *priv = i2c_get_clientdata(client);
    int i;

    if (priv) {
        for (i = 0; i < 2*N_GMSL_PORTS; i++) {
            if (priv->detected_sensors[i].cam_addr != 0)
                mutex_destroy(&priv->detected_sensors[i].bw_lock);
        }
        global_priv[priv->channel] = NULL;
    }

    dev_info(&client->dev, "%s: success\n", __func__);

    //
    //  Everything is automatically deallocated.
    //

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 0, 0)
	return 0;
#endif
}

static const struct i2c_device_id max96724_id[] = {
    {"sl_max96724", 0},
    {},
};

const struct of_device_id max96724_of_match[] = {
    {
        .compatible = "stereolabs,sl_max96724",
    },
    {},
};

MODULE_DEVICE_TABLE(i2c, max96724_id);

static struct i2c_driver max96724_i2c_driver = {
    .driver = {
        .name = "sl_max96724",
        .owner = THIS_MODULE,
    },
    .probe = sl_max96724_probe,
    .remove = sl_max96724_remove,
    .id_table = max96724_id,
};

static int __init sl_max96724_init(void)
{
    return i2c_add_driver(&max96724_i2c_driver);
}

static void __exit sl_max96724_exit(void)
{
    i2c_del_driver(&max96724_i2c_driver);
}

module_init(sl_max96724_init);
module_exit(sl_max96724_exit);

MODULE_DESCRIPTION("IO Expander driver max96724");
MODULE_AUTHOR("STEREOLABS <support@stereolabs.com>");
MODULE_LICENSE("GPL v2");
MODULE_VERSION(DRV_STR_VERSION);
