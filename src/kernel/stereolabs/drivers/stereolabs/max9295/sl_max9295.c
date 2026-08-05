/*
 * sl_max9295.c - Stereolabs ar0234 sensor driver
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

// #define DEBUG 1
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <media/camera_common.h>

#include <linux/seq_file.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>

#include "max9295_mode_tbls.h"
#include "../include/sl_serdes.h"

#define STRINGIFY0(s)
#define STRINGIFY(s) STRINGIFY0(s)
#define DRV_STR_VERSION STRINGIFY(MAX9295_DRIVER_VERSION_MAJOR)"."STRINGIFY(MAX9295_DRIVER_VERSION_MINOR)"."STRINGIFY(MAX9295_DRIVER_VERSION_PATCH)""
#define MAX_NB_SER 16

/**
 * Structure definitions
 */

/**
 * struct of_device_id* - Link the driver to the dts.
 * @compatible: String that includes the name of the
 *				company and the name of the device.
 *
 * To add support for your ZED-X, the dts compatibility
 * must match "stereolabs,sl_max9295".
 */
const struct of_device_id sl_max9295_of_match[] = {
	{.compatible = "stereolabs,sl_max9295"},
	{},
};

/**
 * macro MODULE_DEVICE_TABLE - Allow hotplug of the device
 * Exposes the vendor/device id for the compiler.
 */
MODULE_DEVICE_TABLE(of, sl_max9295_of_match);

struct bmi_device{
	int gyro_addr;
	int acc_addr;
};

struct sl_max9295
{
	struct i2c_client *i2c_client;
	const struct i2c_device_id *id;
	struct regmap *ser_regmap;
	int zedx_id;
	int channel;
	int camera_model;
	struct bmi_device bmi_array[2]; // bmi-array preset in the DT
	int acc_addr;
	int gyro_addr;
	struct index_reg_8 *conf_table;
};

int isValidConf(struct sl_max9295 *priv);
int fsync_set_Ser(int zedx_id, bool on);
extern int isSecondCamFromI2C(int channel, int zedx_id);

static int index_serializer = 0;

struct sl_max9295 *ser_global_priv[MAX_NB_SER];

/**
 * const struct regmap_config - Serializer register mapping configuration.
 * @reg_bits: uint 32 - 16 bits register addresses.
 * @val_bits: uint 32 - 8 bits register contents.
 * @cache_type: enum - Supported cache type.
 *
 * Register configuration of the MAX9295 serializer.
 */
static const struct regmap_config serializer_regmap_config = {
	.reg_bits = 16,
	.val_bits = 8,
	.cache_type = REGCACHE_NONE,
};


int set_bitrate_Ser(int zedx_id, u8 val) {
	int err = -1;
	u8 reg;
	int i =0;
	int index = -1;

	if(IS_ERR_OR_NULL(ser_global_priv))
		return -1;

	for(i=0;i<MAX_NB_SER;i++){
		if(ser_global_priv[i] == NULL)
			continue;

		if(ser_global_priv[i]->zedx_id == zedx_id){
			index = i;
			break;
		}
	}

	if (index < 0 || ser_global_priv[index] == NULL)
		return -1;

	switch (val) {
		case 8: reg = GMSL_8BIT_MODE;
			break;
		case 10: reg = GMSL_10BIT_MODE;
			break;
		case 12: reg = GMSL_12BIT_MODE;
			break;
		default:
			dev_info(&ser_global_priv[index]->i2c_client->dev,
				"%s: Id: %d, unsupported bitrate value %d\n", __func__, zedx_id, val);
			return -1;
	};

	if(!isSecondCamFromI2C(ser_global_priv[index]->channel, ser_global_priv[index]->zedx_id))
		err = regmap_write(ser_global_priv[index]->ser_regmap, GMSL_VID_PIPE_Y, reg);
	else
		err = regmap_write(ser_global_priv[index]->ser_regmap, GMSL_VID_PIPE_Z, reg);

	return err;
}
EXPORT_SYMBOL(set_bitrate_Ser);

/* Enable/Disable the FSYNC (shutter-trigger) forwarding to the sensor. */
int fsync_set_Ser(int zedx_id, bool on)
{
	static const u16 mfps_stereo[] = {
		MAX9295_MFP9_GPIO_A, MAX9295_MFP10_GPIO_A
	};
	static const u16 mfps_mono[] = { MAX9295_MFP8_GPIO_A };

	const u16 *mfps;
	size_t n_mfp, k;
	int i, index = -1;
	int err = 0;
	u8 val;

	if (IS_ERR_OR_NULL(ser_global_priv))
		return -1;

	for (i = 0; i < MAX_NB_SER; i++) {
		if (ser_global_priv[i] == NULL)
			continue;

		if (ser_global_priv[i]->zedx_id == zedx_id) {
			index = i;
			break;
		}
	}

	if (index < 0 || ser_global_priv[index] == NULL)
		return -1;

	switch (ser_global_priv[index]->camera_model) {
	case ZEDX:
	case ZEDXHDR:
	case ZEDXNANO:
		mfps  = mfps_stereo;
		n_mfp = ARRAY_SIZE(mfps_stereo);
		break;
	default: /* mono: ZEDONEGS, ZEDONE4K, ZEDONEHDR */
		mfps  = mfps_mono;
		n_mfp = ARRAY_SIZE(mfps_mono);
		break;
	}

	/* Toggle only the gate bit off the known shutter-trigger config so the
	 * GMSL RX routing is preserved */
	val = MAX9295_FSYNC_GPIO_RX;
	if (!on)
		val |= MAX9295_GPIO_OUT_DIS;

	for (k = 0; k < n_mfp; k++) {
		int ret;

		dev_dbg(&ser_global_priv[index]->i2c_client->dev,
			"%s: FSYNC %s, writing 0x%02X to 0x%04X\n",
			__func__, on ? "ON" : "OFF", val, mfps[k]);

		ret = regmap_write(ser_global_priv[index]->ser_regmap, mfps[k], val);
		if (ret)
			err = ret;
	}

	return err;
}
EXPORT_SYMBOL(fsync_set_Ser);

int ser_get_acc_addr(int zedx_id){
	int i = 0;
	int err = -1;
	
	for(i = 0; i < MAX_NB_SER; i++){
		if(ser_global_priv[i] == NULL)
			return err;

		if(ser_global_priv[i]->zedx_id != zedx_id)
			continue;

		if(ser_global_priv[i]->acc_addr == -1){
			dev_info(&ser_global_priv[i]->i2c_client->dev,
				"%s: Id: %d acc addr not initialized\n", __func__, zedx_id);
			return err;
		}

		dev_dbg(&ser_global_priv[i]->i2c_client->dev,
			"%s: Id: %d acc addr: %x\n",
			__func__,
			ser_global_priv[i]->zedx_id,
			ser_global_priv[i]->acc_addr);

		return ser_global_priv[i]->acc_addr;
	}

	return err;
}
EXPORT_SYMBOL(ser_get_acc_addr);

int ser_get_gyro_addr(int zedx_id){
	int i = 0;
	int err = -1;
	
	for(i = 0; i < MAX_NB_SER; i++){
		if(ser_global_priv[i] == NULL)
			return err;

		if(ser_global_priv[i]->zedx_id != zedx_id)
			continue;

		if(ser_global_priv[i]->gyro_addr == -1){
			dev_info(&ser_global_priv[i]->i2c_client->dev,
				"%s: Id: %d gyro addr not initialized\n", __func__, zedx_id);
			return err;
		}

		dev_dbg(&ser_global_priv[i]->i2c_client->dev,
			"%s: Id: %d gyro addr: %x\n",
			__func__,
			ser_global_priv[i]->zedx_id,
			ser_global_priv[i]->gyro_addr);

		return ser_global_priv[i]->gyro_addr;
	}

	return err;
}
EXPORT_SYMBOL(ser_get_gyro_addr);

int ser_read_video_conf(int zedx_id){
	int i = 0;
	int err = -1;
	
	for(i = 0; i < MAX_NB_SER; i++){
		if(ser_global_priv[i] == NULL)
			return err;

		if(ser_global_priv[i]->zedx_id != zedx_id)
			continue;

		return isValidConf(ser_global_priv[i]);
	}

	return err;
}
EXPORT_SYMBOL(ser_read_video_conf);

/**
 * ser_read_reg() - Read in the registers of the MAX9295D serializer.
 * @priv: Private data, it contains the regmap setup.
 * @addr: Address of the register to read.
 * @val: Pointer to store the value of the register.
 *
 * Read 8 bits in a register of the MAX9295D serializer and store the
 * read value at the address of the val pointer.
 *
 * Context: Non critical function, can sleep.
 * Return: 0 in case of success and a negative errno in case of error.
 */
static inline int ser_read_reg(struct sl_max9295 *priv, u16 addr, u8* val)
{
	int err;
	struct i2c_client *client = priv->i2c_client;
	struct device *dev = &client->dev;
	u32 reg_val = 0;

	if (val == NULL)
		dev_err(dev, "%s: u8 *val is not initialized...\n", __func__);

	err = regmap_read(priv->ser_regmap, addr, &reg_val);
	*val = (u8) reg_val & 0xFF;

	if (err && verbosity_level)
		dev_warn(dev, "%s:i2c read failed: dev. 0x%x, reg. 0x%x\n",
				__func__, client->addr, addr);

	return err;
}

// Check if the current configuration matches the expected one
// Return 1 if configuration is valid, 0 if not, negative errno in case of error
int isValidConf(struct sl_max9295 *priv){
	// We verify only some critical registers to speed up the process
	// and avoid read errors due to i2c traffic.
	// In 4K mode, 0x0316/0x0318 are overridden at runtime, so skip them.
	static const u16 check_regs_default[] = { 0x0314, 0x0316, 0x0318, 0x031A };
	static const u16 check_regs_4k[]      = { 0x0314, 0x031A };

	const u16 *check_regs;
	size_t n_check, j;
	u32 reg_val = 0;
	int err = 0;
	int i = 0;

	if (priv->camera_model == ZEDONE4K) {
		check_regs = check_regs_4k;
		n_check    = ARRAY_SIZE(check_regs_4k);
	} else {
		check_regs = check_regs_default;
		n_check    = ARRAY_SIZE(check_regs_default);
	}

	while(priv->conf_table[i].addr != SERIALIZER_TABLE_END){
		// Check only critical registers
		int retry = 0;
		bool to_check = false;

		for (j = 0; j < n_check; j++) {
			if (priv->conf_table[i].addr == check_regs[j]) {
				to_check = true;
				break;
			}
		}

		if (!to_check) {
			i++;
			continue;
		}

		for(retry = 0; retry < 5; retry++){
			err = regmap_read(priv->ser_regmap, priv->conf_table[i].addr, &reg_val);
			dev_dbg(&priv->i2c_client->dev,
				"%s: device: 0x%x addr: 0x%x read attempt %d - err %d\n",
				__func__,
				priv->i2c_client->addr,
				priv->conf_table[i].addr,
				retry,
				err);

			if (err){
				msleep(4);
				continue;
			}
			break;
		}

		dev_dbg(&priv->i2c_client->dev,
			"%s: device: 0x%x addr: 0x%x read: 0x%x expected: 0x%x - err %d\n",
			__func__,
			priv->i2c_client->addr,
			priv->conf_table[i].addr,
			reg_val,
			(u8)(priv->conf_table[i].val & 0xFF),
			err);

		if(err == -EREMOTEIO)
			return 0;

		if (err)
			return err;

		if ((u8)(reg_val & 0xFF) != (u8)(priv->conf_table[i].val & 0xFF))
			return 0;

		i++;
	}

	return 1;
}

/**
 * ser_write_reg() - Write in the registers of the MAX9295D serializer.
 * @priv: Private data, to change the i2c_client address, and the regmap setup.
 * @addr: Address of the register to write.
 * @val: Value to write in the register
 *
 * Change the driver adapter i2c address then write 8 bits in a register
 * of the MAX9295D serializer.
 *
 * Context: Non critical function, can sleep.
 * Return: 0 in case of success and a negative errno in case of error.
 */
static int ser_write_reg(struct sl_max9295 *priv, u16 addr, u8 val)
{
	int err;
	struct i2c_client *client = priv->i2c_client;
	struct device *dev = &client->dev;

	err = regmap_write(priv->ser_regmap, addr, val);

	if (err && verbosity_level)
		dev_err(dev, "%s:i2c write failed: dev. 0x%x, reg. 0x%x, val. 0x%x\n",
				__func__, client->addr, addr, val);

	return err;
}

/**
 * ser_write_table() - Write in the registers.
 * @priv: Driver data.
 * @table: Table of registers to write in the deserializer.
 *
 * Write a table of 16 bits registers sequentially to the serializer
 * or deserializer. The timings are important.
 * For each register, it tries two times before returning with an error.
 *
 * Context: Non critical function, can sleep.
 * Return: 0 in case of success and a negative errno in case of error.
 */
static int ser_write_table(struct sl_max9295 *priv,
						   const struct index_reg_8 table[])
{
	struct i2c_client *client = priv->i2c_client;
	struct device *dev = &client->dev;
	int i = 0, j = 0;
	int ret = 0;
	int retry = 5;

	// While we haven't reach the end of the table
	while (table[i].source != 0x00)
	{

		for (j = 0; j < retry; j++)
		{
			if (table[i].addr == 0x000D)
			{
				msleep(94);			
				break;
			}

			ret = ser_write_reg(priv, table[i].addr, (u8)table[i].val);
			// dev_err(dev, "%s: write_reg: 0x%x, 0x%x %d\n", __func__, table[i].addr, table[i].val, ret);
			if (ret && (table[i].addr != 0x0000))
			{
				retry--;
				if (j == retry - 1)
					return -1;
				dev_dbg(dev, "%s: try %d\n", __func__, retry);
				msleep(4); /*if we struggle to write, we wait a bit*/
				continue;
			}
			/* Reset register. Datasheet states a minimum sleep time of 20 ms,
			 * and a maximum sleep time of 100ms */
			if (0x0010 == table[i].addr)
				msleep(100);
			break;
		}
		i++;
	}

	return 0;
}

static int translate_imu(struct sl_max9295 *priv, u8 gyro_base_addr, u8 acc_base_addr){
	int index = isSecondCamFromI2C(priv->channel, priv->zedx_id);
	int err = 0;
	u8 gyro_addr = (index == 0)?priv->bmi_array[0].gyro_addr:priv->bmi_array[1].gyro_addr;
	u8 acc_addr = (index == 0)?priv->bmi_array[0].acc_addr:priv->bmi_array[1].acc_addr;

	struct index_reg_8 i2c_translate_table[] = {
		{MAX9295D_ADDRESS_BASE, 0x0042, gyro_addr*2}, // When the ser receive at x58... eeprom i2c map eeporm have two i2c address
		{MAX9295D_ADDRESS_BASE, 0x0043, gyro_base_addr*2}, // it remaps at x54
		{MAX9295D_ADDRESS_BASE, 0x0044, acc_addr*2}, // When the ser receive at x59...
		{MAX9295D_ADDRESS_BASE, 0x0045, acc_base_addr*2}, // it remaps at x55
		{0x00, SERIALIZER_TABLE_END, 0x00},
	};

	err = ser_write_table(priv, i2c_translate_table);

	priv->gyro_addr = gyro_addr;
	priv->acc_addr = acc_addr;

	return err;
}

static int identify_model(struct sl_max9295 *priv){
	struct device *dev = &priv->i2c_client->dev;
	struct i2c_client *client = priv->i2c_client;
	struct i2c_msg msgs[2];
	u8 reg_buf[1] = { 0x00 }; /* start read at EEPROM offset 0 (1-byte addressing) */
	u8 data[EEPROM_MODEL_BYTE_OFFSET + 1];
	int ret;

	msgs[0].addr  = EEPROM_I2C_ADDR;
	msgs[0].flags = 0;
	msgs[0].len   = sizeof(reg_buf);
	msgs[0].buf   = reg_buf;

	msgs[1].addr  = EEPROM_I2C_ADDR;
	msgs[1].flags = I2C_M_RD;
	msgs[1].len   = sizeof(data);
	msgs[1].buf   = data;

	ret = i2c_transfer(client->adapter, msgs, 2);
	if (ret != 2) {
		dev_err(dev, "%s: eeprom i2c read @0x%02x failed (%d)\n",
			__func__, EEPROM_I2C_ADDR, ret);
		return ret < 0 ? ret : -EIO;
	}

	dev_dbg(dev, "%s: eeprom[0..5]: %02x %02x %02x %02x %02x %02x\n",
		__func__, data[0], data[1], data[2], data[3], data[4], data[5]);

	if (data[EEPROM_MODEL_BYTE_OFFSET] == EEPROM_ZEDXNANO_MARKER)
		return ZEDXNANO;

	return ZEDX;
}

static int probe_serializer(struct sl_max9295 *priv) {
	struct device *dev = &priv->i2c_client->dev;
	struct device_node *np = priv->i2c_client->dev.of_node;
	int err = 0;
	const char *str;
	struct index_reg_8** table = mode_table_A;
	int second_cam = isSecondCamFromI2C(priv->channel, priv->zedx_id);
	int acc_addr = 0;
	int gyro_addr = 0;
	int idx = 0;

	err = of_property_read_string(np, "camera_model", &str);
	if (err){
		dev_err(dev, "%s: camera-model not found in dts\n", __func__);
        return -1;
    }

	if (strcmp(str, "zedx") == 0) {
		priv->camera_model = identify_model(priv);
		if(priv->camera_model < 0){
			dev_err(dev, "%s: Camera model identification failed\n", __func__);
			return priv->camera_model;
		}
		idx = (priv->camera_model == ZEDXNANO) ? ZEDXNANO_9295D_SER : AR0234_9295D_SER;
		gyro_addr = ZED_STEREO_GYRO_BASE_ADDR;
		acc_addr  = ZED_STEREO_ACC_BASE_ADDR;
	}
	else if (strcmp(str, "zedone4k")==0){
		idx = IMX678_9295A_SER;
		gyro_addr = ZED_MONO_GYRO_BASE_ADDR;
		acc_addr = ZED_MONO_ACC_BASE_ADDR;
		priv->camera_model = ZEDONE4K;
	}
	else if (strcmp(str, "zedonegs")==0){
		idx = AR0234_9295A_SER;
		gyro_addr = ZED_MONO_GYRO_BASE_ADDR;
		acc_addr = ZED_MONO_ACC_BASE_ADDR;
		priv->camera_model = ZEDONEGS;
	}
	else if (strcmp(str, "zedonehdr")==0){
		idx = ISX031_9295A_SER;
		gyro_addr = ZED_MONO_GYRO_BASE_ADDR;
		acc_addr = ZED_MONO_ACC_BASE_ADDR;
		priv->camera_model = ZEDONEHDR;
	}
	else if (strcmp(str, "zedxhdr")==0){
		idx = ISX031_9295D_SER;
		gyro_addr = ZED_STEREO_GYRO_BASE_ADDR;
		acc_addr = ZED_STEREO_ACC_BASE_ADDR;
		priv->camera_model = ZEDXHDR;
	}
	else{
		dev_err(dev, "%s: Camera model unrecognized\n",
				__func__);
        return -1;
	}
	table = second_cam ? mode_table_B : mode_table_A;
	priv->conf_table = table[idx];
	err = ser_write_table(priv, priv->conf_table);	

	if (err){
		dev_info(dev, "%s: Serializer for %s detect error\n", __func__, camera_names[priv->camera_model]);
		return err;
	}

	dev_info(dev, "%s: Serializer pipeline operational",__func__);

	err = translate_imu(priv, gyro_addr,acc_addr);

	if (err){
		dev_err(dev, "%s: IMU addr translation failed\n",__func__);
		return err;
	}

	dev->driver_data = priv;

	if(index_serializer >= MAX_NB_SER){
		dev_err(dev, "%s: Index_serializer value out of range\n",__func__);
		return -ENOMEM;
	}

	dev_info(dev, "%s: Serializer for %s detect success\n", __func__, camera_names[priv->camera_model]);

	return 0;
}

/**
 * sl_max9295_probe() - Initialize the max9295 camera.
 * @client: i2c adapter structure to fill.
 * @id: i2c device id.
 *
 * This functions is called when loading the module. Its main role
 * is to setup the serializer and the deserializer of the max9295. In
 * addition, it links a bunch of device driver structures, register
 * a tegracam device and a v4l2 device.
 *
 * Context: Can sleep.
 * Return: 0 in case of success, and a negative errno otherwise.
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 0, 0)
static int sl_max9295_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
#else
static int sl_max9295_probe(struct i2c_client *client)
#endif
{
	struct device *dev = &client->dev;
	struct device_node *node = dev->of_node;
	struct device_node* bmi;
	struct sl_max9295 *priv;
	const char *str;
	int n_imu = 0;
	int i = 0;
	int err;
	u8 val;


	if (!IS_ENABLED(CONFIG_OF) || !node)
		return -EINVAL;

	priv = devm_kzalloc(dev, sizeof(struct sl_max9295), GFP_KERNEL);

	if (!priv)
	{
		dev_err(dev, "unable to allocate memory!\n");
		return -ENOMEM;
	}

	priv->i2c_client = client;

	priv->ser_regmap = devm_regmap_init_i2c(priv->i2c_client, &serializer_regmap_config);

	if (IS_ERR(priv->ser_regmap))
	{
		dev_err(dev,
			"%s: regmap init failed: %ld\n", __func__, PTR_ERR(priv->ser_regmap));
		return -1;
	}

	err = ser_read_reg(priv, MAX9295_ID_REG, &val);
	if(err)
	{
		return -ENODEV;
	}

	dev_info(dev, "Driver Version : v%d.%d.%d\n",MAX9295_DRIVER_VERSION_MAJOR,MAX9295_DRIVER_VERSION_MINOR,MAX9295_DRIVER_VERSION_PATCH);

	err = of_property_read_string(node, "channel", &str);
	if(err){
		dev_err(dev, "%s: Channel %d doesn't exist\n", __func__, priv->channel);
		return err;
	}

	priv->channel = str[0] - 'a';

	if (priv->channel < 0 || priv->channel > 3)
	{
		dev_err(dev, "%s: Channel %d wrong value\n", __func__, priv->channel);
		return -EINVAL;
	}

	if (verbosity_level>=1)
		dev_dbg(dev, "%s: channel: %d\n", __func__, priv->channel);

	err = of_property_read_string(node, "zedx-id", &str);
	if (err)
	{
		dev_err(dev, "%s: zedx-id not found in dts\n",__func__);
		return -EINVAL;
	}

	err = kstrtoint(str, 10, &priv->zedx_id);
	if (err)
	{
		dev_err(dev, "%s: zedx-id is missing\n", __func__);
		return -EINVAL;
	}

	//If dummy entry, don't continue probe - no verbose
	if (priv->zedx_id < 0)
		return -ENODEV;

	n_imu = of_count_phandle_with_args(node, "imu", NULL);

	if(n_imu <= 0){
		dev_err(dev, "%s: IMU missing in serializer id %d", __func__, priv->zedx_id);
		return -EINVAL; 
	}

	for(i=0;i<n_imu;i++){
		bmi = of_parse_phandle(node, "imu" , i);
		if ( bmi == NULL ){
			dev_info(dev,"%s: Issue getting node pointer to imu %d\n", __func__, i);
			return -EINVAL;
		}

		err = of_property_read_u32(bmi, "reg" , &priv->bmi_array[i].gyro_addr);
		if(err){
			of_node_put(bmi);
			dev_err(dev, "%s: gyro_addr not found in dts\n",__func__);
			return err;
		}

		err = of_property_read_u32(bmi, "accel_i2c_addr" , &priv->bmi_array[i].acc_addr);
		if(err){
			of_node_put(bmi);
			dev_err(dev, "%s: accel_i2c_addr not found in dts\n",__func__);
			return err;
		}

		of_node_put(bmi);
	}

	err = probe_serializer(priv);
	if (err)
		return err;

	ser_global_priv[index_serializer++] = priv;

	return 0;
}

/**
 * sl_max9295_remove() - Remove the max9295 camera.
 * @client: i2c adapter structure to remove.
 *
 * This functions is called when discharging the module. Its main role
 * is to unregister the v4l2 and tegracam devices. Otherwise, the memory
 * is freed automatically thanks to the devmkzalloc() used in the probing
 * function.
 *
 * Context: Can sleep.
 * Return: 0 in case of success.
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 0, 0)
static int sl_max9295_remove(struct i2c_client *client)
#else
static void sl_max9295_remove(struct i2c_client *client)
#endif
{
	struct device *dev = &client->dev;

	dev_info(dev, "ZED-X serializer successfully removed\n");

#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 0, 0)
	return 0;
#endif
}

/**
 * struct i2c_device_id - Name and ID of the i2c device.
 *  The id should be the same as the one in the dts module part.
 */
static const struct i2c_device_id sl_max9295_id[] = {
	{"sl_max9295", 0},
	{}};

MODULE_DEVICE_TABLE(i2c, sl_max9295_id);

/**
 * struct i2c_driver - i2c driver functions
 * Here the name should be the same as the first part of the V4L2 devname
 * and the first part of the badge in the dts.
 * For more infos, please refer to nvidia's documentation
 */
static struct i2c_driver sl_max9295_i2c_driver = {
	.driver = {
		.name = "sl_max9295",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(sl_max9295_of_match),
	},
	.probe = sl_max9295_probe,
	.remove = sl_max9295_remove,
	.id_table = sl_max9295_id,
};

/* Register the i2c driver in the kernel */
module_i2c_driver(sl_max9295_i2c_driver);

MODULE_DESCRIPTION("Media Controller driver for Stereolabs ZED-X");
MODULE_AUTHOR("STEREOLABS <support@stereolabs.com>");
MODULE_LICENSE("GPL v2");
MODULE_VERSION(DRV_STR_VERSION);
