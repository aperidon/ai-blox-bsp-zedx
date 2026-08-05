/**
 * max9296_mode_tbls.h - Deserializer mode tables
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
#ifndef __DESER_MAXI2C_TABLES__
#define __DESER_MAXI2C_TABLES__

#include <linux/mutex.h>

/// Driver Version ///
#define DESER_DRIVER_VERSION_MAJOR 1
#define DESER_DRIVER_VERSION_MINOR 4
#define DESER_DRIVER_VERSION_PATCH 3


#define MAX9296_TABLE_END 0xff01

static const u8 EEPROM_I2C_ADDRS[] = { 0x55, 0x57, 0x52 };
#define EEPROM_KEY_BYTE_OFFSET     4
#define EEPROM_KEY_VALUE           0x11
#define EEPROM_MODEL_BYTE_OFFSET   5

#define EEPROM_ZEDX_MARKER1         0x02
#define EEPROM_ZEDX_MARKER2         0x03
#define EEPROM_ZEDX_MARKER3         0x04
#define EEPROM_ZEDXHDR_MARKER1      0x05
#define EEPROM_ZEDXHDR_MARKER2      0x06
#define EEPROM_ZEDXHDR_MARKER3      0x07
#define EEPROM_ZEDXNANO_MARKER     	0x08
#define EEPROM_ZEDONEGS_MARKER     	0x1E  /* 30 */
#define EEPROM_ZEDONE4K_MARKER     	0x1F  /* 31 (ONE UHD) */
#define EEPROM_ZEDONEHDR_MARKER    	0x20  /* 32 */
#define MAX_CSI_GROUPS 8  /* Supports port-indices 0-7; observed values: 0-4, 6 */

const int verbosity_level=0;

struct index_reg_8
{
	u16 addr;
	u16 val;
};


struct i2c_fingerprint
{
	u16 i2c_addr;
	u16 reg_addr;
	u16 val;
};

#define GMSL_LINKS_EN_REG 0x0006
#define GMSL_LINK_CTRL_REG 0x0010
#define GMSL_LINK_RATE_CTRL 0x0001
#define GMSL_PIPES_01_REG 0x00F0
#define GMSL_PIPES_23_REG 0x00F1
#define GMSL_LINKS_CC_REG 0x0003
#define GMSL_CC_X_OVR_REG 0x0007

#define GMSL_VID_ST_MAP 	0x044d
#define GMSL_12BIT_MODE   	0x2C
#define GMSL_10BIT_MODE   	0x2B
#define GMSL_8BIT_MODE    	0x2A
#define MAX9296_GMSL_6GBPS_MODE	0x02
#define MAX9296_GMSL_3GBPS_MODE	0x01
#define MAX9295_GMSL_6GBPS_MODE 0x08
#define MFP5_REG			0x2BF
#define MAX9296_FSYNC_CONF		0x03E0
#define VIDEO_LOCK_STATUS_REG 		0x1DC // Video lock status register are 0x1DC, 0x1FC, 0x21C, 0x23C for pipe 0,1,2,3

#define PIPES_XZ_MASK 0x20

#define NB_GMSL 2

#define ZED_ONE_SER_DFLT_ADDR 0x42
#define ZEDXHDR_SEN1_BASE_ADDR 0x2e
#define ZEDXHDR_SEN2_BASE_ADDR 0x3e

#define ZED_ONE_GS_SER_ADDR_A 	0x42
#define ZED_ONE_UHD_SER_ADDR_A 	0x44
#define ZED_ONE_GS_SER_ADDR_B 	(ZED_ONE_GS_SER_ADDR_A+1)
#define ZED_ONE_UHD_SER_ADDR_B 	(ZED_ONE_UHD_SER_ADDR_A+1)
#define NB_MAX_SERIALIZERS 16
#define MAX9296_NB_MFP 12
#define MAX9296_GPIO_ADDR 0x02B0

//values not used as registers in 9296
#define RIGHT_SENSOR_ADDR 0xff02
#define LEFT_SENSOR_ADDR 0xff03
#define SER_ADDR 0xff04

#define N_LANES 2
typedef struct serializer_devices{
	int zedx_id;
	int gmsl_index;
	int camera_model;
	u32 ser_addr;
	u32 cam_addr[2];
	bool is_second_ser_from_i2c;
	int vc_id[2];
	int phy_index;
	int dsr_pipe;
	int serial;
	u32 n_lanes;  /* CSI lane count from DT bus-width */
}serializer_devices;

typedef struct detected_sensor_slot {
	int zedx_id;
	u32 cam_addr;
	int serial;
	u64 current_bps;
	bool is_streaming;
	struct mutex bw_lock;
} detected_sensor_slot;

enum deser_mode
{
	MASTER_MODE,
	SLAVE_MODE
};

struct max9296
{
	bool intialized;
	enum deser_mode mode; // FSYNC role: MASTER_MODE (internal fsync) or SLAVE_MODE (external fsync)
	u8 fsync_conf; // Cached FSYNC config reg (0x03E0), holds board MFP routing
	struct i2c_client *i2c_client;
	struct regmap *regmap;
	u32 i2c_address; // MAX deser address from dts (required to update addr in tables)
	int channel; // channel id from dts
	int pwdn_gpio;
	int cam_id;
	int nb_ser_devices;
	serializer_devices ser_devices[NB_MAX_SERIALIZERS];  //struct that stores all devices declared in the dts
	detected_sensor_slot detected_sensors[2 * NB_MAX_SERIALIZERS];
	serializer_devices current_ser_device[NB_GMSL]; //struct that stores the data of the current connected serializer
	const char *cam_name;
	int gmsl_link; // this value is instantiated with the detection of the cameras, it is used to enable the gmsl right number of gmsl links
	int mfp_trig_info; // Mfp used for slave mode detection (-1 if not used)
	int mfp_trig_in; // Mfp used as trigger input (default MFP1)
	u8  phy_idx_bound;                                   /* max(phy_index)+1 across all DT serializers; use as array bound, not PHY count */
	u8  csi_rate_reg_val[MAX_PHY_PER_DESER];     /* PHY CSI rate register value, indexed by phy_index */
	u64 csi_rate_bps[MAX_PHY_PER_DESER];         /* PHY CSI capacity in bps, indexed by phy_index */
	int csi_group_to_phy_idx[MAX_CSI_GROUPS];    /* Maps csi_group (port-index) to phy_index for fast BW lookup */
};

typedef enum
{
	MAX9296_I2C_LINK1 = 0x00,
	MAX9296_I2C_LINK2,
	MAX9296_I2C_LINK1_LINK2,
}I2cIndex;

typedef void (*i2c_fingerprint_func)(struct i2c_fingerprint* table,int ser_addr, int left_sensor_addr, int right_sensor_addr,int gmsl_index);
void get_zedx_alt_fingerprint(struct i2c_fingerprint* table, int ser_addr, int left_sensor_addr, int right_sensor_addr, int gmsl_index);
void get_zedonegs_alt_fingerprint(struct i2c_fingerprint* table, int ser_addr, int left_sensor_addr, int right_sensor_addr,int gmsl_index);
void get_zedone4k_alt_fingerprint(struct i2c_fingerprint* table, int ser_addr, int left_sensor_addr, int right_sensor_addr,int gmsl_index);
void get_zedxhdr_alt_fingerprint(struct i2c_fingerprint* table, int ser_addr, int left_sensor_addr, int right_sensor_addr,int gmsl_index);
void get_zedonehdr_alt_fingerprint(struct i2c_fingerprint* table, int ser_addr, int left_sensor_addr, int right_sensor_addr,int gmsl_index);
void get_zedxnano_alt_fingerprint(struct i2c_fingerprint* table, int ser_addr, int left_sensor_addr, int right_sensor_addr, int gmsl_index);

// Link status registers
static struct index_reg_8 max9296_link_regs[] = {
	{0x0013,0x00},
	{MAX9296_TABLE_END, 0x00}
};

static struct i2c_fingerprint zedx_fingerprint[] = {
	{0x62, 0x000D, 0x95}, // Serializer device ID
	{0x10, 0x3000, 0x0A}, // Image sensor device ID1
	{0x10, 0x3001, 0x56}, // Image sensor device ID2
	{0x18, 0x3000, 0x0A}, // Image sensor device ID1
	{0x18, 0x3001, 0x56}, // Image sensor device ID2
	{MAX9296_TABLE_END, 0x00, 0x00}
};

/* NOTE: identical to zedx_fingerprint — ZEDX vs ZEDXNANO can ONLY be told apart via the
 * EEPROM marker (identify_via_eeprom). If the EEPROM is unreachable, the
 * fingerprint loop will match ZEDX first*/
static struct i2c_fingerprint zedxnano_fingerprint[] = {
	{0x62, 0x000D, 0x95}, // Serializer device ID
	{0x10, 0x3000, 0x0A}, // Image sensor device ID1
	{0x10, 0x3001, 0x56}, // Image sensor device ID2
	{0x18, 0x3000, 0x0A}, // Image sensor device ID1
	{0x18, 0x3001, 0x56}, // Image sensor device ID2
	{MAX9296_TABLE_END, 0x00, 0x00}
};

// static struct i2c_fingerprint zedx_alt_fingerprint[] = {
// 	{0x62, 0x0000, SER_ADDR},
// 	{SER_ADDR, 			0x000D, 0x95}, // Fix addr
// 	{0x10, 				0x301B, 0x50}, // Image sensor @ x10 unlock reg 
// 	{0x10, 				0x31FD, LEFT_SENSOR_ADDR}, // Image sensor @ x10 change addr 
// 	{LEFT_SENSOR_ADDR, 0x301B, 0x58}, // Image sensor new addr relock reg 
// 	{0x18, 0x301B, 0x50}, /* Image sensor @ x18 unlock reg  */
// 	// #ifndef CONFIG_TEGRA_327_ZEDX_ADDR_FIX
// 	// {0x18, 0x31FC, 0x50}, /* Image sensor @ x18 change addr */
// 	// {0x28, 0x301B, 0x58}, /* Image sensor @ x28 relock reg  */
// 	// #else
// 	{0x18, 				0x31FC, RIGHT_SENSOR_ADDR}, // Image sensor @ x18 change addr
// 	{RIGHT_SENSOR_ADDR,0x301B, 0x58}, // Image sensor new addr relock reg 
// 	// #endif
// 	{MAX9296_TABLE_END, 0x00, 0x00}
// };

void get_zedx_alt_fingerprint(struct i2c_fingerprint* table, 
	int ser_addr, int left_sensor_addr, int right_sensor_addr,
	int gmsl_index){
	int i = 0;

	if(gmsl_index == 0)
	{
		struct i2c_fingerprint routing_table[] = {
			{0x62, 0x0000, ser_addr*2},
			{0x10, 				0x301B, 0x50}, // Image sensor @ x10 unlock reg 
			{0x10, 				0x31FD, left_sensor_addr*2}, // Image sensor @ x10 change addr 
			{left_sensor_addr, 0x301B, 0x58}, // Image sensor new addr relock reg 
			{0x18, 0x301B, 0x50}, /* Image sensor @ x18 unlock reg  */
			{0x18, 				0x31FC, right_sensor_addr*2}, // Image sensor @ x18 change addr
			{right_sensor_addr,0x301B, 0x58}, // Image sensor new addr relock reg 
			{MAX9296_TABLE_END, 0x00, 0x00}
		};

		for (i = 0; i < sizeof(routing_table)/sizeof(routing_table[0]); i++){
			table[i] = routing_table[i];
		}
	}
	else{
		struct i2c_fingerprint routing_table[] = {
			{0x62, 0x008B, 0x10},
			{0x62, 0x0000, ser_addr*2},
			{0x10, 				0x301B, 0x50}, // Image sensor @ x10 unlock reg 
			{0x10, 				0x31FD, left_sensor_addr*2}, // Image sensor @ x10 change addr 
			{left_sensor_addr, 0x301B, 0x58}, // Image sensor new addr relock reg 
			{0x18, 0x301B, 0x50}, /* Image sensor @ x18 unlock reg  */
			{0x18, 				0x31FC, right_sensor_addr*2}, // Image sensor @ x18 change addr
			{right_sensor_addr,0x301B, 0x58}, // Image sensor new addr relock reg 
			{MAX9296_TABLE_END, 0x00, 0x00}
		};

		for (i = 0; i < sizeof(routing_table)/sizeof(routing_table[0]); i++){
			table[i] = routing_table[i];
		}
	}
}

void get_zedxnano_alt_fingerprint(struct i2c_fingerprint* table, 
	int ser_addr, int left_sensor_addr, int right_sensor_addr,
	int gmsl_index){
	int i = 0;

	if(gmsl_index == 0)
	{
		struct i2c_fingerprint routing_table[] = {
			{0x62, 0x0000, ser_addr*2},
			{0x10, 				0x301B, 0x50}, // Image sensor @ x10 unlock reg 
			{0x10, 				0x31FD, left_sensor_addr*2}, // Image sensor @ x10 change addr 
			{left_sensor_addr, 0x301B, 0x58}, // Image sensor new addr relock reg 
			{0x18, 0x301B, 0x50}, /* Image sensor @ x18 unlock reg  */
			{0x18, 				0x31FC, right_sensor_addr*2}, // Image sensor @ x18 change addr
			{right_sensor_addr,0x301B, 0x58}, // Image sensor new addr relock reg 
			{MAX9296_TABLE_END, 0x00, 0x00}
		};

		for (i = 0; i < sizeof(routing_table)/sizeof(routing_table[0]); i++){
			table[i] = routing_table[i];
		}
	}
	else{
		struct i2c_fingerprint routing_table[] = {
			{0x62, 0x008B, 0x10},
			{0x62, 0x0000, ser_addr*2},
			{0x10, 				0x301B, 0x50}, // Image sensor @ x10 unlock reg 
			{0x10, 				0x31FD, left_sensor_addr*2}, // Image sensor @ x10 change addr 
			{left_sensor_addr, 0x301B, 0x58}, // Image sensor new addr relock reg 
			{0x18, 0x301B, 0x50}, /* Image sensor @ x18 unlock reg  */
			{0x18, 				0x31FC, right_sensor_addr*2}, // Image sensor @ x18 change addr
			{right_sensor_addr,0x301B, 0x58}, // Image sensor new addr relock reg 
			{MAX9296_TABLE_END, 0x00, 0x00}
		};

		for (i = 0; i < sizeof(routing_table)/sizeof(routing_table[0]); i++){
			table[i] = routing_table[i];
		}
	}
}

static struct i2c_fingerprint zedx_sensor_reset[]= {
	// {SER_ADDR, 0x10, 0x91}, // Reset at address B to reset serializer to address A
	{0x62, 0x02D4, 0x60}, /* Set sensor MFP to push pull */
	{0x62, 0x02D3, 0x80}, /* Power off sensor MFP */
	{0x62, 0x02D3, 0x90}, /* Power on sensor@x18 MFP */
	{0x62, 0x02D7, 0x60}, /* Set sensor MFP to push pull */
	{0x62, 0x02D6, 0x80}, /* Power off sensor MFP */
	{0x62, 0x02D6, 0x90}, /* Power on sensor MFP@x10 */
	{MAX9296_TABLE_END, 0x00, 0x00},
};

static struct i2c_fingerprint zedxnano_sensor_reset[]= {
	// {SER_ADDR, 0x10, 0x91}, // Reset at address B to reset serializer to address A
	{0x62, 0x02D4, 0x60}, /* Set sensor MFP to push pull */
	{0x62, 0x02D3, 0x80}, /* Power off sensor MFP */
	{0x62, 0x02D3, 0x90}, /* Power on sensor@x18 MFP */
	{0x62, 0x02D7, 0x60}, /* Set sensor MFP to push pull */
	{0x62, 0x02D6, 0x80}, /* Power off sensor MFP */
	{0x62, 0x02D6, 0x90}, /* Power on sensor MFP@x10 */
	{MAX9296_TABLE_END, 0x00, 0x00},
};

static struct i2c_fingerprint zedonegs_fingerprint[] = {
	{0x42, 0x000D, 0x91}, /* Serializer device ID */
	{0x10, 0x3000, 0x0A}, // Image sensor device ID1
	{0x10, 0x3001, 0x56}, // Image sensor device ID2
	{MAX9296_TABLE_END, 0x00, 0x00}
};

static struct i2c_fingerprint zedonegs_sensor_reset[]= {
	// {ZED_ONE_GS_SER_ADDR_B, 0x10, 0x91}, // Reset at address B to reset serializer to address A
	{0x42, 0x02BF, 0x60}, /* Set sensor MFP to push pull */
	{0x42, 0x02BE, 0x80}, /* Power off sensor MFP */
	{0x42, 0x000D, 0x00}, /* Power on sensor MFP */
	{0x42, 0x000D, 0x00}, /* Power on sensor MFP */
	{0x42, 0x02BE, 0x90}, /* Power on sensor MFP */
	{0x42, 0x000D, 0x00}, /* Power on sensor MFP */
	{MAX9296_TABLE_END, 0x00, 0x00}
};

// static struct i2c_fingerprint zedonegs_alt_fingerprint[] = {
// 	{MAX9296_TABLE_END, 0x00, 0x00},
// };

void get_zedonegs_alt_fingerprint(struct i2c_fingerprint* table, 
	int ser_addr, int left_sensor_addr, int right_sensor_addr,
	int gmsl_index){
	int i = 0;

	if (gmsl_index == 0)
	{
		struct i2c_fingerprint routing_table[] = {
			// {ser_addr, 0x008B, 0x12},
			{0x42, 0x0000, ser_addr*2}, /* SER address becomes 0x43 */
			{0x10, 0x301B, 0x50}, /* Image sensor @ x10 unlock reg  */
			{0x10, 0x31FD, right_sensor_addr*2}, /* Image sensor address becomes 0x11 */ 
			{right_sensor_addr, 0x301B, 0x58}, /* Image sensor @ x11 relock reg  */
			{MAX9296_TABLE_END, 0x00, 0x00},
		};

		for (i = 0; i < sizeof(routing_table)/sizeof(routing_table[0]); i++){
			table[i] = routing_table[i];
		}
	}
	else{
		struct i2c_fingerprint routing_table[] = {
			{0x42, 0x008B, 0x14},
			{0x42, 0x0000, ser_addr*2}, /* SER address becomes 0x43 */
			{0x10, 0x301B, 0x50}, /* Image sensor @ x10 unlock reg  */
			{0x10, 0x31FD, right_sensor_addr*2}, /* Image sensor address becomes 0x11 */ 
			{right_sensor_addr, 0x301B, 0x58}, /* Image sensor @ x11 relock reg  */
			{MAX9296_TABLE_END, 0x00, 0x00},
		};

		for (i = 0; i < sizeof(routing_table)/sizeof(routing_table[0]); i++){
			table[i] = routing_table[i];
		}
	}
}

static struct i2c_fingerprint zedone4k_fingerprint[] = {
	{0x42, 0x000D, 0x91}, /* Serializer device ID */
	{0x1A, 0x3028, 0xCA}, /* VMAX_MSB register of the IMX678 */
	{0x1A, 0x302C, 0x4C}, /* HMAX_MSB register of the IMX678 */
	{MAX9296_TABLE_END, 0x00, 0x00}
};

static struct i2c_fingerprint zedone4k_sensor_reset[]= {
	// {ZED_ONE_UHD_SER_ADDR_B, 0x0010, 0x91}, // Reset at address B to reset serializer to address A
	{0x42, 0x02BF, 0x60}, /* Set sensor MFP to push pull */
	{0x42, 0x02BE, 0x80}, /* Power off sensor MFP */
	{0x42, 0x02BE, 0x90}, /* Power on sensor MFP */
	{MAX9296_TABLE_END, 0x00, 0x00}
};

// static struct i2c_fingerprint zedone4k_alt_fingerprint[] = {
// 	{0x42, 0x0000, 0x88}, /* SER address becomes 0x44  */
// 	{MAX9296_TABLE_END, 0x00, 0x00},
// };

void get_zedone4k_alt_fingerprint(struct i2c_fingerprint* table, 
	int ser_addr, int left_sensor_addr, int right_sensor_addr,
	int gmsl_index){
	int i = 0;

	if (gmsl_index == 0){
		struct i2c_fingerprint routing_table[] = {
			{0x42, 0x0000, ser_addr*2}, /* SER address becomes 0x44  */
			{MAX9296_TABLE_END, 0x00, 0x00},
		};

		for (i = 0; i < sizeof(routing_table)/sizeof(routing_table[0]); i++){
			table[i] = routing_table[i];
		}
	}
	else{
		struct i2c_fingerprint routing_table[] = {
			{0x42, 0x008B, 0x15},
			{0x42, 0x0000, ser_addr*2}, /* SER address becomes 0x44  */
			// {0x1a, 0xBFD0, 0x01},
			// {0x42, 0x02BF, 0x60}, /* Set sensor MFP to push pull */
			// {0x42, 0x02BE, 0x80}, /* Power off sensor MFP */
			// {0x42, 0x02BE, 0x90}, /* Power on sensor MFP */
			// {0x42, 0x0000, ser_addr*2}, /* SER address becomes 0x45  */
			{MAX9296_TABLE_END, 0x00, 0x00},
		};

		for (i = 0; i < sizeof(routing_table)/sizeof(routing_table[0]); i++){
			table[i] = routing_table[i];
		}
	}
}

static struct i2c_fingerprint zedxhdr_sensor_reset[]= {
	// {0x60, 0x02D3, 0x90}, /* Power on sensor MFP */
	// {0x60, 0x02D6, 0x90}, /* Power on sensor MFP */

  	{0x60, 0x0010, 0x91}, /* Serializer reset */
	
	{0x60, 0x02DC, 0x80}, /* FSYNC sensor 1 down */
	{0x60, 0x02D9, 0x80}, /* FSYNC sensor 2 down */
	{0x42, 0x000D, 0x0000}, // SLEEP 100 ms
	{0x42, 0x000D, 0x0000}, // SLEEP 100 ms
	{0x60, 0x02D6, 0x80}, /* Power off sensor MFP */
	{0x60, 0x02D3, 0x80}, /* Power off sensor MFP */
	// {0x42, 0x000D, 0x0000}, // SLEEP 100 ms
	// {0x42, 0x000D, 0x0000}, // SLEEP 100 ms
	// {0x60, 0x02D6, 0x90}, /* Power on sensor MFP */
	// {0x60, 0x02D3, 0x90}, /* Power on sensor MFP */
	{MAX9296_TABLE_END, 0x00, 0x00}
};

static struct i2c_fingerprint zedxhdr_fingerprint[] = {
	{0x60, 0x000D, 0x95}, /* Serializer device ID */
	//{0x1A, 0x8A54, 0x1A}, // Image sensor device i2c address 
	{MAX9296_TABLE_END, 0x00, 0x00}
};

// static struct i2c_fingerprint zedxhdr_alt_fingerprint[] = {
// 	{MAX9296_TABLE_END, 0x00, 0x00},
// };

void get_zedxhdr_alt_fingerprint(struct i2c_fingerprint* table,
	int ser_addr, int left_sensor_addr, int right_sensor_addr,
	int gmsl_index){
	int i = 0;

	if (gmsl_index == 0){
		struct i2c_fingerprint routing_table[] = {
			{0x60, 		0x0000, ser_addr*2}, /* SER address becomes 0x44  */
			{ser_addr, 	0x000D, 0x95},

			{ser_addr, 0x02D6, 0x90}, /* Power on sensor MFP */
			{0x1a, 	0x000D, 0x95},
			{0x1a, 	0x000D, 0x95},
			{0x1a, 0x8A54, right_sensor_addr}, // Image sensor @ x10 unlock reg
			{0x1a, 	0x000D, 0x95},
			{0x1a, 	0x000D, 0x95},
			// {0x1a, 0xFFFF, 0xF5}, // Image sensor @ x10 unlock reg
			{ser_addr, 0x02D6, 0x80}, /* Power off sensor MFP */

			{ser_addr, 0x02D3, 0x90}, /* Power on sensor MFP */
			{0x1a, 	0x000D, 0x95},
			{0x1a, 	0x000D, 0x95},
			{0x1a, 0x8A54, left_sensor_addr}, // Image sensor @ x10 unlock reg
			{0x1a, 	0x000D, 0x95},
			{0x1a, 	0x000D, 0x95},
			{ser_addr, 0x02D3, 0x80}, /* Power off sensor MFP */

			{0x1a, 	0x000D, 0x95},
			{0x1a, 	0x000D, 0x95},
			{ser_addr, 0x02DC, 0x90}, /* FSYNC sensor 1 up */
			{ser_addr, 0x02D9, 0x90}, /* FSYNC sensor 2 up */
			{0x42, 0x000D, 0x0000}, // SLEEP 100 ms
			{ser_addr, 0x02D6, 0x90}, /* Power on sensor MFP */
			{ser_addr, 0x02D3, 0x90}, /* Power on sensor MFP */
			{MAX9296_TABLE_END, 0x00, 0x00},
		};

		for (i = 0; i < sizeof(routing_table)/sizeof(routing_table[0]); i++){
			table[i] = routing_table[i];
		}
	}
	else{
		struct i2c_fingerprint routing_table[] = {
			{0x60, 0x008B, 0x16},
			{0x60, 		0x0000, ser_addr*2}, /* SER address becomes 0x44  */
			{ser_addr, 	0x000D, 0x95},
			{ser_addr, 	0x000D, 0x95},

			{ser_addr, 0x02D6, 0x90}, /* Power on sensor MFP */
			{0x1a, 	0x000D, 0x95},
			{0x1a, 	0x000D, 0x95},
			{0x1a, 0x8A54, right_sensor_addr}, // Image sensor @ x10 unlock reg
			{0x1a, 	0x000D, 0x95},
			{0x1a, 	0x000D, 0x95},
			// {0x1a, 0xFFFF, 0xF5}, // Image sensor @ x10 unlock reg
			{ser_addr, 0x02D6, 0x80}, /* Power off sensor MFP */

			{ser_addr, 0x02D3, 0x90}, /* Power on sensor MFP */
			{0x1a, 	0x000D, 0x95},
			{0x1a, 	0x000D, 0x95},
			{0x1a, 0x8A54, left_sensor_addr}, // Image sensor @ x10 unlock reg
			{0x1a, 	0x000D, 0x95},
			{0x1a, 	0x000D, 0x95},
			{ser_addr, 0x02D3, 0x80}, /* Power off sensor MFP */

			{0x1a, 	0x000D, 0x95},
			{0x1a, 	0x000D, 0x95},
			{ser_addr, 0x02DC, 0x90}, /* FSYNC sensor 1 up */
			{ser_addr, 0x02D9, 0x90}, /* FSYNC sensor 2 up */
			{0x42, 0x000D, 0x0000}, // SLEEP 100 ms
			{ser_addr, 0x02D6, 0x90}, /* Power on sensor MFP */
			{ser_addr, 0x02D3, 0x90}, /* Power on sensor MFP */
			{MAX9296_TABLE_END, 0x00, 0x00},
		};

		for (i = 0; i < sizeof(routing_table)/sizeof(routing_table[0]); i++){
			table[i] = routing_table[i];
		}
	}
}

static struct i2c_fingerprint zedonehdr_sensor_reset[]= {
	// #if IS_ENABLED(CONFIG_TEGRA_STREAMERBOX_ADDR_FIX)
	// {0x46, 0x0000, 0x84}, /* Reset address to default: 0x42 */
	// #else
	// {0x40, 0x0000, 0x84}, /* Reset address to default: 0x42 */
	// #endif
	// {0x42, 0x02BE, 0x90}, /* Power off sensor MFP */
	// {0x42, 0x000D, 0x0000}, // SLEEP 100 ms
	// {0x42, 0x000D, 0x0000}, // SLEEP 100 ms
  	
	// {0x42, 0x0010, 0x91}, /* Serializer reset */
	// {0x42, 0x000D, 0x0000}, // SLEEP 100 ms
	{0x42, 0x02BF, 0x60}, /* Set sensor MFP to push pull */
  	{0x42, 0x02D6, 0x80}, /* Fsync pin is pulled down */

	// {0x42, 0x02BE, 0x80}, /* Power off sensor MFP */
	// {0x42, 0x000D, 0x0000}, // SLEEP 100 ms
	// {0x42, 0x000D, 0x0000}, // SLEEP 100 ms
	// {0x42, 0x02BE, 0x90}, /* Power off sensor MFP */
	// {0x42, 0x02BE, 0x90}, /* Power on sensor MFP */
	{MAX9296_TABLE_END, 0x00, 0x00}
};

static struct i2c_fingerprint zedonehdr_fingerprint[] = {
	{0x42, 0x000D, 0x91}, /* Serializer device ID */
	// {0x1A, 0x8A54, 0x1A}, // Image sensor device i2c address 
	{MAX9296_TABLE_END, 0x00, 0x00}
};

// static struct i2c_fingerprint zedonehdr_alt_fingerprint[] = {
// 	#if IS_ENABLED(CONFIG_TEGRA_STREAMERBOX_ADDR_FIX)
// 	{0x42, 0x0000, 0x8C}, /* Serializer addr change */
// 	#else
// 	{0x42, 0x0000, 0x80}, /* Serializer addr change */
// 	#endif
// 	{MAX9296_TABLE_END, 0x00, 0x00},
// };

void get_zedonehdr_alt_fingerprint(struct i2c_fingerprint* table,
	int ser_addr, int left_sensor_addr, int right_sensor_addr,
	int gmsl_index){
	int i = 0;

	if (gmsl_index == 0){
		struct i2c_fingerprint routing_table[] = {
			{0x42, 		0x0000, ser_addr*2}, /* SER address becomes 0x44  */
			{ser_addr, 	0x02BF, 0x60}, /* Set sensor MFP to push pull */
			{ser_addr, 	0x02BE, 0x80}, /* Power off sensor MFP */
		  	{ser_addr,  0x02D6, 0x80}, /* Fsync pin is pulled low */
			{ser_addr, 	0x000D, 0x0000}, // SLEEP 100 ms
			{ser_addr, 	0x000D, 0x0000}, // SLEEP 100 ms
			{ser_addr, 	0x02BE, 0x90}, /* Power on sensor MFP */
			{ser_addr, 	0x000D, 0x0000}, // SLEEP 100 ms
			{ser_addr, 	0x000D, 0x0000}, // SLEEP 100 ms
			{0x1a, 0x8A54, right_sensor_addr}, // Image sensor @ x10 unlock reg
			{ser_addr, 	0x000D, 0x0000}, // SLEEP 100 ms
			{ser_addr, 	0x000D, 0x0000}, // SLEEP 100 ms
		  	{ser_addr,  0x02D6, 0x90}, /* Fsync pin is pulled high */
			{ser_addr, 	0x02BE, 0x80}, /* Power on sensor MFP */
			{ser_addr, 	0x000D, 0x0000}, // SLEEP 100 ms
			{ser_addr, 	0x000D, 0x0000}, // SLEEP 100 ms
			{ser_addr, 	0x02BE, 0x90}, /* Power on sensor MFP */
			{MAX9296_TABLE_END, 0x00, 0x00},
		};

		for (i = 0; i < sizeof(routing_table)/sizeof(routing_table[0]); i++){
			table[i] = routing_table[i];
		}
	}
	else{
		struct i2c_fingerprint routing_table[] = {
			{0x42, 0x008B, 0x17},
			{0x42, 		0x0000, ser_addr*2}, /* SER address becomes 0x44  */
			{ser_addr, 	0x02BF, 0x60}, /* Set sensor MFP to push pull */
			{ser_addr, 	0x02BE, 0x80}, /* Power off sensor MFP */
		  	{ser_addr,  0x02D6, 0x80}, /* Fsync pin is pulled low */
			{ser_addr, 	0x000D, 0x0000}, // SLEEP 100 ms
			{ser_addr, 	0x000D, 0x0000}, // SLEEP 100 ms
			{ser_addr, 	0x02BE, 0x90}, /* Power on sensor MFP */
			{ser_addr, 	0x000D, 0x0000}, // SLEEP 100 ms
			{ser_addr, 	0x000D, 0x0000}, // SLEEP 100 ms
			{0x1a, 0x8A54, right_sensor_addr}, // Image sensor @ x10 unlock reg
			{ser_addr, 	0x000D, 0x0000}, // SLEEP 100 ms
			{ser_addr, 	0x000D, 0x0000}, // SLEEP 100 ms
		  	{ser_addr,  0x02D6, 0x90}, /* Fsync pin is pulled high */
			{ser_addr, 	0x02BE, 0x80}, /* Power on sensor MFP */
			{ser_addr, 	0x000D, 0x0000}, // SLEEP 100 ms
			{ser_addr, 	0x000D, 0x0000}, // SLEEP 100 ms
			{ser_addr, 	0x02BE, 0x90}, /* Power on sensor MFP */
			{MAX9296_TABLE_END, 0x00, 0x00},
		};

		for (i = 0; i < sizeof(routing_table)/sizeof(routing_table[0]); i++){
			table[i] = routing_table[i];
		}
	}
}

static struct index_reg_8 max9296_7_fps[] = {
	{0x03E5, 0xD0}, // frame rate --> FSYN period for 25MHz oscillator (833 333)
	{0x03E6, 0xDC}, // frame rate
	{0x03E7, 0x32}, // frame rate
	{MAX9296_TABLE_END, 0x00}
};

// Set the FSYNC trigger FPS (mandatory to have Left/Right in sync)
// See MAX9296 documentation for FSYN register 
static struct index_reg_8 max9296_15_fps[] = {
	{0x03E5, 0x68}, // frame rate --> FSYN period for 25MHz oscillator (833 333)
	{0x03E6, 0x6e}, // frame rate
	{0x03E7, 0x19}, // frame rate
	{MAX9296_TABLE_END, 0x00}
};

static struct index_reg_8 max9296_25_fps[] = {
	{0x03E5, 0x40}, // frame rate --> FSYN period for 25MHz oscillator (833 333)
	{0x03E6, 0x42}, // frame rate
	{0x03E7, 0x0F}, // frame rate
	{MAX9296_TABLE_END, 0x00}
};

static struct index_reg_8 max9296_30_fps[] = {
	{0x03E5, 0x35}, // frame rate --> FSYN period for 25MHz oscillator (833 333)
	{0x03E6, 0xB7}, // frame rate
	{0x03E7, 0x0C}, // frame rate
	{MAX9296_TABLE_END, 0x00}
};

static struct index_reg_8 max9296_60_fps[] = {
	{ 0x03E5, 0x9A}, // frame rate --> FSYN period for 25MHz oscillator (416 666)
	{ 0x03E6, 0x5B}, // frame rate
	{ 0x03E7, 0x06}, // frame rate
	{ MAX9296_TABLE_END, 0x00}
};

static struct index_reg_8 max9296_120_fps[] = {
	{0x03E5, 0xCD}, // frame rate --> FSYN period for 25MHz oscillator (208 333)
	{0x03E6, 0x2D}, // frame rate
	{0x03E7, 0x03}, // frame rate
	{MAX9296_TABLE_END, 0x00}
};


static struct index_reg_8 max9296_slave_fps[] = {
	{ 0x03E0, 0x0C},
	{ 0x02B0, 0xC3},
	{ 0x02B1, 0xA8},
	{ 0x02B2, 0x80},
	{ MAX9296_TABLE_END, 0x00}
};

static struct index_reg_8 max9296_deser_reset[] = {
	{ 0x0010, 0x80}, // Apply full reset for changes for DESER ZEDX
	// { 0x0010, 0x23}, // Apply full reset for changes for DESER ZEDX
	{ MAX9296_TABLE_END, 0x00}
};

static struct index_reg_8 max9296_deser_i2c_table[] = {
	[MAX9296_I2C_LINK1] = { 0x0010, 0x21},
	[MAX9296_I2C_LINK2] = { 0x0010, 0x22},
	[MAX9296_I2C_LINK1_LINK2] = { 0x0010, 0x23},
};

static struct index_reg_8 max9296_zedonegs_sensor_reset[] = {

	// route data from serializer stream 0x00(X) 0x02(Z) -> 0x00(X) 0x01(Y) -> DPHY1 

	{ 0x0050, 0x01}, // Route data from stream 1 to pipe X
	{ 0x0051, 0x00}, // Route data from stream 0 to pipe Y
	{ 0x0052, 0x02}, // Route data from stream 2 to pipe Z - dflt mapping
	{ 0x0053, 0x03}, // Route data from stream 3 to pipe U - dflt mapping

	// { 0x161, 0x38}, // Set MIPI Phy Mode: 2x(1x4) mode

	{ 0x0330, 0x04}, // Set MIPI Phy Mode: 2x(1x4) mode
	// { 0x0332, 0xF0}, // All MIPI Phy powered - dflt mapping160

	// { 0x031D, 0x2A}, // PHY clock rate -  1000MBPS + disable fine tune

	{ 0x040A, 0x00}, // lane count - 0 lanes striping on controller 0 (Port A slave in 2x1x4 mode).
	{ 0x044A, 0x40}, // From the datasheet, 2 datalanes //lane count - 4 lanes striping on controller 1 (Port A master in 2x1x4 mode).
	{ 0x048A, 0x40}, // From the datasheet, 2 datalanes //lane count - 4 lanes striping on controller 1 (Port A master in 2x1x4 mode).
	{ 0x04cA, 0x00}, // From the datasheet, 2 datalanes //lane count - 4 lanes striping on controller 1 (Port A master in 2x1x4 mode).

	{ 0x0333, 0x4E}, // MIPI Phy 0&1 lane maps - dflt mapping
	{ 0x0334, 0xE4}, // MIPI Phy 3&2 lane maps - dflt mapping

	//{ 0x1d00, 0xf4}, // PHY clock rate -  1000MBPS + disable fine tune
	{ 0x031D, 0x2a},
	{ 0x0320, 0x2a}, // PHY clock rate -  1000MBPS + disable fine tune
	{ 0x0323, 0x2a}, // PHY clock rate -  1000MBPS + disable fine tune
	{ 0x0326, 0x2a}, // PHY clock rate -  1000MBPS + disable fine tune
	// When using the 9296 in 2x4 (it is a dual deserializer),
	// some lane stripping is necessary to output everything on port MIPI 0
	// This means that the MIPI output is 1x2 after lane stripping.
	// Since the pipe Y seems connected natively to PHY 1 and the pipe Z to PHY 2,
	// and since we have selected VC 0 on pipe Y and VC 2 on pipe Z, we need
	// to remap those signals on port MIPI 0.
	// VC 0 on pipe Y has an output that defaults to MIPI PHY 1 on port 0 in 1x4 mode
	// (It should be noted that MIPI PHY 0 and 3 are disabled in 2x4 mode)
	// VC 2 on pipe Z has an output that defaults to MIPI PHY 2 on port 1 in 1x4 mode
	// that is why we remap it to MIPI port 0 below.

	{ 0x0005, 0x00}, // Disable lock output, disable errb

	{0x02b3,0x83}, 	// MFP1 - 1M resistor + enable gmsl tx + tx output driver disabled
	{0x02b4,0x10}, 	// MFP1 - TX ID = 0x10
	{0x02bc,0x04}, 	// MFP4 - enable gmsl rx
	{0x02be,0x11}, 	// MFP4 - Rx ID = 0x11
	{0x02bf,0x04}, 	// MFP5 - enable gmsl rx
	{0x02c1,0x12}, 	// MFP5 - Rx ID = 0x11
	{0x0003, 0x40}, // disable Uart 1 + disable access to link B serializer control channel

	{0x03EF,0xC0},	// FSYNC signal = gmsl 2 type + FS_USE_XTAL = 1 + AUTO_FS_LINKS = 0 + FS_LINK_[3:0] = 0
	{0x03E2,0x00},	// Turn off auto master link selection
	{0x03EA,0x00},	// OVLP window = 0
	{0x03EB,0x00},	// OVLP window = 0

	//FSYNC --> overwrite when fps set
	{0x03E5,0x9A}, // 60Hz FSYNC LVal of period
	{0x03E6,0x5B}, // Mval of period
	{0x03E7,0x06}, // Hval of period
	{0x03F1,0x40}, // FSYNC TX ID = 0x01
	{0x03E0,0x04}, // Enable manual frame sync, output on GPIO --> drive slave devices

	{MAX9296_TABLE_END, 0x00}
};

static struct index_reg_8 default_pipe_conf[] = {
	// route data from serializer 1 stream 0x00(X) 0x02(Z) -> 0x00(X) 0x01(Y) -> DPHY1 
	// route data from serializer 2 stream 0x01(Y) 0x03(U) -> 0x03(U) 0x02(Z) -> DPHY2

	{ 0x0330, 0x04}, // Set MIPI Phy Mode: 2x(1x4) mode
	{ 0x0332, 0xF0}, // All MIPI Phy powered - dflt mapping

	{ 0x0333, 0x4E}, // MIPI Phy 0&1 lane maps - dflt mapping
	{ 0x0334, 0xE4}, // MIPI Phy 3&2 lane maps - dflt mapping

	// // X=0x00, Y=0x01, Z=0x02, U=0x03
	{ 0x0050, 0x00}, // Route data from stream 0(X) to pipe X
	{ 0x0051, 0x01}, // Route data from stream 2(Z) to pipe Y
	{ 0x0052, 0x02}, // Route data from stream 3(U) to pipe Z
	{ 0x0053, 0x03}, // Route data from stream 1(Y) to pipe U

	{ 0x040A, 0x00}, // lane count - 0 lanes striping on controller 0 (Port A slave in 2x1x4 mode).
	{ 0x044A, 0x40}, // From the datasheet, 2 datalanes //lane count - 4 lanes striping on controller 1 (Port A master in 2x1x4 mode).
	{ 0x048A, 0x40}, // From the datasheet, 2 datalanes //lane count - 4 lanes striping on controller 2 (Port B master in 2x1x4 mode).
	{ 0x04CA, 0x00}, // lane count - 0 lanes striping on controller 3 (Port B slave in 2x1x4 mode).

	//{ 0x443, 0x80}, // Enable Deskew 

	{MAX9296_TABLE_END, 0x00}
};

static struct index_reg_8 default_misc_conf[] = {
	{ 0x0005, 0x00}, // Disable lock output, disable errb

	// MFP0 - Fsync Out
	{0x02b0,0x01},// Disable output drivers
	{0x02b1,0x00},// MFP1 GPIO TX output driver enabled
	// MFP1
	{0x02b3,0x01}, // Disable output drivers
	{0x02b4,0x00}, // Open drain output driver type
	// MFP4
	{0x02bc,0x04},
	{0x02be,0x11},
	// MFP5
	{0x02bf,0x01}, // Disable output drivers
	{0x02c0,0xA0}, // Enable pull-down

	{0x0003, 0x40}, // Disable UART1

	{0x03EF,0xC0},   // AUTO_FS_LINKS = 0, FS_USE_XTAL = 1, FS_LINK_[3:0] = 0
	{0x03E2,0x00},   // Turn off auto master link selection
	{0x03EA,0x00},   // OVLP window = 0
	{0x03EB,0x00},   // OVLP window = 0

	//FSYNC > overwrite when fps set
	//		> ununsed when slave mode (mfp5 == HIGH)
	{0x03E5,0x9A}, // 60Hz FSYNC LVal of period
	{0x03E6,0x5B}, // Mval of period
	{0x03E7,0x06}, // Hval of period
	{0x03F1,0x80}, // FSYNC TX ID = 0x10
	{0x03E0,0x24}, // Internal Frame sync mode, manual frame sync, output on MFP0

	{MAX9296_TABLE_END, 0x00},
};

static int get_max9296_slave_mode_table(int mfp_trig_in, struct index_reg_8* table, const size_t size){
	int err = 0;
	int mfp_trig_out = 0x00; // MFP0 is default trigger output

	struct index_reg_8 slave_mode_table[] = {
		// mfp_trig_out = fsync_out (default MFP0)
		{ MAX9296_GPIO_ADDR+(3*mfp_trig_out), 0x01}, // Disable output drivers
		{ MAX9296_GPIO_ADDR+(3*mfp_trig_out)+1, 0x00}, // Open drain output driver type
		// mfp_trig_in = fsync_in (default MFP1)
		{ MAX9296_GPIO_ADDR+(3*mfp_trig_in), 0xc3}, // Output driver disabled, gmsl transmission enabled 
		{ MAX9296_GPIO_ADDR+(3*mfp_trig_in)+1, 0x30}, // gmsl transmission address = 0x10

		{0x03E0,0x08}, // External Fsync mode
		{MAX9296_TABLE_END, 0x00},
	};

	if(mfp_trig_in < 0 || mfp_trig_in > MAX9296_NB_MFP){
		return -EINVAL;
	}

	if(size < sizeof(slave_mode_table)){
		return -ENOMEM;
	}

	memcpy(table, slave_mode_table, sizeof(slave_mode_table));

	return err;
}

static struct index_reg_8 max9296_test2[] = {
	// route data from serializer 1 stream 0x00(X) 0x02(Z) -> 0x00(X) 0x01(Y) -> DPHY1 
	// route data from serializer 2 stream 0x01(Y) 0x03(U) -> 0x03(U) 0x02(Z) -> DPHY2

	{ 0x0330, 0x04}, // Set MIPI Phy Mode: 2x(1x4) mode
	{ 0x0332, 0xF0}, // All MIPI Phy powered - dflt mapping

	{ 0x0333, 0x4E}, // MIPI Phy 0&1 lane maps - dflt mapping
	{ 0x0334, 0xE4}, // MIPI Phy 3&2 lane maps - dflt mapping

	// // X=0x00, Y=0x01, Z=0x02, U=0x03
	{ 0x0050, 0x00}, // Route data from stream 0(X) to pipe X
	{ 0x0051, 0x01}, // Route data from stream 2(Z) to pipe Y
	{ 0x0052, 0x02}, // Route data from stream 3(U) to pipe Z
	{ 0x0053, 0x03}, // Route data from stream 1(Y) to pipe U

	// { 0x0309, 0x02},   // FRONTTOP5 - Select the virtual channel 2 for pipe X
	// { 0x030b, 0x02},   // FRONTTOP3 - Select the virtual channel 0 for pipe Y
	{ 0x030d, 0x04},   // FRONTTOP5 - Select the virtual channel 2 for pipe Z
	{ 0x030f, 0x08},   // FRONTTOP5 - Select the virtual channel 2 for pipe U

	{ 0x031D, 0x30}, // PHY clock rate -  1600MBPS + disable fine tune
	{ 0x0320, 0x30}, // PHY clock rate -  1600MBPS + disable fine tune
	{ 0x0323, 0x30}, // PHY clock rate -  1600MBPS + disable fine tune
	{ 0x0326, 0x30}, // PHY clock rate -  1600MBPS + disable fine tune

	{ 0x040A, 0x00}, // lane count - 0 lanes striping on controller 0 (Port A slave in 2x1x4 mode).
	{ 0x044A, 0x40}, // From the datasheet, 2 datalanes //lane count - 4 lanes striping on controller 1 (Port A master in 2x1x4 mode).
	{ 0x048A, 0x40}, // From the datasheet, 2 datalanes //lane count - 4 lanes striping on controller 2 (Port B master in 2x1x4 mode).
	{ 0x04CA, 0x00}, // lane count - 0 lanes striping on controller 3 (Port B slave in 2x1x4 mode).

	// Route pipe X to DPHY1
	// { 0x040b, 0x07}, // enable 4 mappings for pipe X

	// { 0x040d, 0x2b}, // map raw10,vc0
	// { 0x040e, 0x2b}, // map raw10,vc0

	// { 0x040f, 0x00}, // map raw10,vc0
	// { 0x0410, 0x00}, // map raw10,vc0
	
	// { 0x0411, 0x01}, // map raw10,vc0
	// { 0x0412, 0x01}, // map raw10,vc0

	// { 0x042d, 0x15}, // Map pipe X to DPHY 2

	// // Route pipe Y to DPHY1
	// { 0x044b, 0x07}, // Enable mapping registers 0 to 2 for MIPI PHY 1
	// // This phy is connected to pipe Y stream 0
	// { 0x044d, 0x2b}, // When the stream source is 0 with raw 10 data type
	// { 0x044e, 0x6b}, // mark it as virtual channel 0 with raw 10 data type
	// // and send it through MIPI port x.
	// { 0x044f, 0x00}, // Frame start with id 0 on PHY 1...
	// { 0x0450, 0x40}, // Is sent as frame start with id 1 through MIPI port x.
	// { 0x0451, 0x01}, // Frame end with id 0 on PHY 1...
	// { 0x0452, 0x41}, // is sent as frame end with id 1 through MIPI port x.
	// { 0x046d, 0x15}, // Map mapping registers 0 to 2 to MIPI PHY 1 : x=1
	// // Now pipe Y is on MIPI port 0, with VC id 1

	// // 0x15, 0x2a,0x55, 0x

	// Route pipe Z to DPHY2
	{ 0x048b, 0x07}, // Enable mapping registers 0 to 2 for MIPI PHY 1
	// This phy is connected to pipe Y stream 0
	{ 0x048d, 0x2b}, // When the stream source is 0 with raw 10 data type
	{ 0x048e, 0xab}, // mark it as virtual channel 0 with raw 10 data type
	// and send it through MIPI port x.
	{ 0x048f, 0x80}, // Frame start with id 0 on PHY 1...	
	{ 0x0490, 0x80}, // Is sent as frame start with id 1 through MIPI port x.
	
	{ 0x0491, 0x81}, // Frame end with id 0 on PHY 1...
	{ 0x0492, 0x81}, // is sent as frame end with id 1 through MIPI port x.
	
	{ 0x04ad, 0x2a}, // Map mapping registers 0 to 2 to MIPI PHY 1 : x=1
	// Now pipe Z is on MIPI port 0, with VC id 1

	// Route pipe U to DPHY2
	{ 0x04cb, 0x07}, // enable 4 mappings for pipe X

	{ 0x04cd, 0x2b}, // map raw10,vc0
	{ 0x04ce, 0xeb}, // map raw10,vc0

	{ 0x04cf, 0xc0}, // map raw10,vc0
	{ 0x04d0, 0xc0}, // map raw10,vc0
	
	{ 0x04d1, 0xc1}, // map raw10,vc0
	{ 0x04d2, 0xc1}, // map raw10,vc0

	{ 0x04ed, 0x2a}, // Map pipe X to DPHY 1
	// Now pipe X is on MIPI port 0, with VC id 0


	{ 0x0005, 0x00}, // Disable lock output, disable errb

	{0x02b3,0x83},// MFP1 GPIO TX output driver disabled
	{0x02b4,0x10},// TX address = 0x10
	{0x02bc,0x04},
	{0x02be,0x11},
	{0x02bf,0x04},
	{0x02c1,0x12},
	{0x0003, 0x40}, // Disable UART1

	{0x03EF,0xC0},   // AUTO_FS_LINKS = 0, FS_USE_XTAL = 1, FS_LINK_[3:0] = 0
	{0x03E2,0x00},   // Turn off auto master link selection
	{0x03EA,0x00},   // OVLP window = 0
	{0x03EB,0x00},   // OVLP window = 0

	//FSYNC --> overwrite when fps set
	{0x03E5, 0x35}, // frame rate --> FSYN period for 25MHz oscillator (833 333)
	{0x03E6, 0xB7}, // frame rate
	{0x03E7, 0x0C}, // frame rate
	{0x03F1,0x40},    // 
	{0x03E0,0x04},    // Enable manual frame sync, output on GPIO --> drive slave devices

	{MAX9296_TABLE_END, 0x00}
};

static struct index_reg_8 max9296_test[] = {
	// route data from serializer 1 stream 0x00(X) 0x02(Z) -> 0x00(X) 0x01(Y) -> DPHY1 
	// route data from serializer 2 stream 0x01(Y) 0x03(U) -> 0x03(U) 0x02(Z) -> DPHY2

	{ 0x0330, 0x04}, // Set MIPI Phy Mode: 2x(1x4) mode
	{ 0x0332, 0xF0}, // All MIPI Phy powered - dflt mapping

	{ 0x0333, 0x4E}, // MIPI Phy 0&1 lane maps - dflt mapping
	{ 0x0334, 0xE4}, // MIPI Phy 3&2 lane maps - dflt mapping

	// // X=0x00, Y=0x01, Z=0x02, U=0x03
	{ 0x0050, 0x00}, // Route data from stream 0(X) to pipe X
	{ 0x0051, 0x01}, // Route data from stream 2(Z) to pipe Y
	{ 0x0052, 0x02}, // Route data from stream 3(U) to pipe Z
	{ 0x0053, 0x03}, // Route data from stream 1(Y) to pipe U

	// X=0x00, Y=0x01, Z=0x02, U=0x03
	// { 0x0050, 0x00}, // Route data from stream 0(X) to pipe X
	// { 0x0051, 0x02}, // Route data from stream 2(Z) to pipe Y
	// { 0x0052, 0x01}, // Route data from stream 3(U) to pipe Z
	// { 0x0053, 0x03}, // Route data from stream 1(Y) to pipe U

	// { 0x0309, 0x02},   // FRONTTOP5 - Select the virtual channel 2 for pipe X
	{ 0x030b, 0x02},   // FRONTTOP3 - Select the virtual channel 0 for pipe Y
	// { 0x030d, 0x02},   // FRONTTOP5 - Select the virtual channel 2 for pipe Z
	// { 0x030f, 0x02},   // FRONTTOP5 - Select the virtual channel 2 for pipe U

	{ 0x031D, 0x30}, // PHY clock rate -  1600MBPS + disable fine tune
	{ 0x0320, 0x30}, // PHY clock rate -  1600MBPS + disable fine tune
	{ 0x0323, 0x2a}, // PHY clock rate -  1600MBPS + disable fine tune
	{ 0x0326, 0x2a}, // PHY clock rate -  1600MBPS + disable fine tune

	{ 0x040A, 0x00}, // lane count - 0 lanes striping on controller 0 (Port A slave in 2x1x4 mode).
	{ 0x044A, 0x40}, // From the datasheet, 2 datalanes //lane count - 4 lanes striping on controller 1 (Port A master in 2x1x4 mode).
	{ 0x048A, 0x40}, // From the datasheet, 2 datalanes //lane count - 4 lanes striping on controller 2 (Port B master in 2x1x4 mode).
	{ 0x04CA, 0x00}, // lane count - 0 lanes striping on controller 3 (Port B slave in 2x1x4 mode).

	// When using the 9296 in 2x4 (it is a dual deserializer),
	// some lane stripping is necessary to output everything on port MIPI 0
	// This means that the MIPI output is 1x2 after lane stripping.
	// Since the pipe Y seems connected natively to PHY 1 and the pipe Z to PHY 2,
	// and since we have selected VC 0 on pipe Y and VC 2 on pipe Z, we need
	// to remap those signals on port MIPI 0.
	// VC 0 on pipe Y has an output that defaults to MIPI PHY 1 on port 0 in 1x4 mode
	// (It should be noted that MIPI PHY 0 and 3 are disabled in 2x4 mode)
	// VC 2 on pipe Z has an output that defaults to MIPI PHY 2 on port 1 in 1x4 mode
	// that is why we remap it to MIPI port 0 below.

	////////////////////////////////////////// SERIALIZER 1

	// Route pipe Y to DPHY1
	{ 0x044b, 0x07}, // Enable mapping registers 0 to 2 for MIPI PHY 1
	// This phy is connected to pipe Y stream 0
	{ 0x044d, 0x2b}, // When the stream source is 0 with raw 10 data type
	{ 0x044e, 0x6b}, // mark it as virtual channel 0 with raw 10 data type
	// and send it through MIPI port x.
	{ 0x044f, 0x40}, // Frame start with id 0 on PHY 1...
	{ 0x0450, 0x40}, // Is sent as frame start with id 1 through MIPI port x.
	{ 0x0451, 0x41}, // Frame end with id 0 on PHY 1...
	{ 0x0452, 0x41}, // is sent as frame end with id 1 through MIPI port x.
	{ 0x046d, 0x15}, // Map mapping registers 0 to 2 to MIPI PHY 1 : x=1
	// { 0x046d, 0x15}, // Map mapping registers 0 to 2 to MIPI PHY 1 : x=1
	// Now pipe Y is on MIPI port 0, with VC id 1

	// 0x15, 0x2a,0x55, 0x

	// Route pipe X to DPHY1
	{ 0x040b, 0x07}, // enable 4 mappings for pipe X

	{ 0x040d, 0x2b}, // map raw10,vc0
	{ 0x040e, 0x2b}, // map raw10,vc0

	{ 0x040f, 0x00}, // map raw10,vc0
	{ 0x0410, 0x00}, // map raw10,vc0
	
	{ 0x0411, 0x01}, // map raw10,vc0
	{ 0x0412, 0x01}, // map raw10,vc0

	{ 0x042d, 0x15}, // Map pipe X to DPHY 2
	// { 0x042d, 0x15}, // Map pipe X to DPHY 1
	// Now pipe X is on MIPI port 0, with VC id 0

	////////////////////////////////////////// SERIALIZER 2

	// // Route pipe Z to DPHY2
	// { 0x048b, 0x07}, // Enable mapping registers 0 to 2 for MIPI PHY 1
	// // This phy is connected to pipe Y stream 0
	// { 0x048d, 0x2b}, // When the stream source is 0 with raw 10 data type
	// { 0x048e, 0x2b}, // mark it as virtual channel 0 with raw 10 data type
	// // and send it through MIPI port x.
	// { 0x048f, 0x00}, // Frame start with id 0 on PHY 1...	
	// { 0x0490, 0x00}, // Is sent as frame start with id 1 through MIPI port x.
	
	// { 0x0491, 0x01}, // Frame end with id 0 on PHY 1...
	// { 0x0492, 0x01}, // is sent as frame end with id 1 through MIPI port x.
	
	// { 0x04ad, 0x2a}, // Map mapping registers 0 to 2 to MIPI PHY 1 : x=1
	// // Now pipe Z is on MIPI port 0, with VC id 1

	// Route pipe U to DPHY2
	// { 0x04cb, 0x07}, // enable 4 mappings for pipe X

	// { 0x04cd, 0x2b}, // map raw10,vc0
	// { 0x04ce, 0x6b}, // map raw10,vc0

	// { 0x04cf, 0x40}, // map raw10,vc0
	// { 0x04d0, 0x40}, // map raw10,vc0
	
	// { 0x04d1, 0x41}, // map raw10,vc0
	// { 0x04d2, 0x41}, // map raw10,vc0

	// // { 0x042d, 0x2a}, // Map pipe X to DPHY 2
	// { 0x04ed, 0x2a}, // Map pipe X to DPHY 1
	// Now pipe X is on MIPI port 0, with VC id 0

	//////////////////////////////////////////

	// { 0x048b, 0x07}, // Enable mapping registers 0 to 2 for MIPI PHY 2
	// // This phy is connected to pipe Z stream 2
	// { 0x048d, 0x2b}, // When the stream source is 0 with raw 10 data type
	// { 0x048e, 0x2b}, // mark it as virtual channel 1 with raw 10 data type
	// // and send it through MIPI port x.
	// { 0x048f, 0x00}, // Frame start with id 1 on PHY 2...
	// { 0x0490, 0x00}, // Is sent as frame start with id 1 through MIPI portx.
	
	// { 0x0491, 0x01}, // Frame end with id 1 on PHY 2...
	// { 0x0492, 0x01}, // is sent as frame end with id 1 through MIPI port x.
	
	// { 0x04ad, 0x15}, // Map mapping registers 0 to 2 to MIPI PHY 1 : x=1
	// // Now pipe Z is on MIPI port 0, with VC id 1

	{ 0x0005, 0x00}, // Disable lock output, disable errb

	{0x02b3,0x83},// MFP1 GPIO TX output driver disabled
	{0x02b4,0x10},// TX address = 0x10
	{0x02bc,0x04},
	{0x02be,0x11},
	{0x02bf,0x04},
	{0x02c1,0x12},
	{0x0003, 0x40}, // Disable UART1

	{0x03EF,0xC0},   // AUTO_FS_LINKS = 0, FS_USE_XTAL = 1, FS_LINK_[3:0] = 0
	{0x03E2,0x00},   // Turn off auto master link selection
	{0x03EA,0x00},   // OVLP window = 0
	{0x03EB,0x00},   // OVLP window = 0

	//FSYNC --> overwrite when fps set
	{0x03E5,0x9A},    // 60Hz FSYNC LVal of period
	{0x03E6,0x5B},    // Mval of period
	{0x03E7,0x06},    // Hval of period
	{0x03F1,0x40},    // 
	{0x03E0,0x04},    // Enable manual frame sync, output on GPIO --> drive slave devices

	{MAX9296_TABLE_END, 0x00}
};

static struct index_reg_8 max9296_zedx_sensor_reset[] = {

	// route data from serializer 1 stream 0x00(X) 0x02(Z) -> 0x00(X) 0x01(Y) -> DPHY1 
	// route data from serializer 2 stream 0x01(Y) 0x03(U) -> 0x03(U) 0x02(Z) -> DPHY2

	{ 0x0330, 0x04}, // Set MIPI Phy Mode: 2x(1x4) mode
	{ 0x0332, 0xF0}, // All MIPI Phy powered - dflt mapping

	{ 0x0333, 0x4E}, // MIPI Phy 0&1 lane maps - dflt mapping
	{ 0x0334, 0xE4}, // MIPI Phy 3&2 lane maps - dflt mapping

	// // X=0x00, Y=0x01, Z=0x02, U=0x03
	{ 0x0050, 0x00}, // Route data from stream 0(X) to pipe X
	{ 0x0051, 0x01}, // Route data from stream 2(Z) to pipe Y
	{ 0x0052, 0x02}, // Route data from stream 3(U) to pipe Z
	{ 0x0053, 0x03}, // Route data from stream 1(Y) to pipe U

	// X=0x00, Y=0x01, Z=0x02, U=0x03
	// { 0x0050, 0x00}, // Route data from stream 0(X) to pipe X
	// { 0x0051, 0x02}, // Route data from stream 2(Z) to pipe Y
	// { 0x0052, 0x01}, // Route data from stream 3(U) to pipe Z
	// { 0x0053, 0x03}, // Route data from stream 1(Y) to pipe U

	// { 0x0309, 0x02},   // FRONTTOP5 - Select the virtual channel 2 for pipe X
	{ 0x030b, 0x02},   // FRONTTOP3 - Select the virtual channel 0 for pipe Y
	// { 0x030d, 0x02},   // FRONTTOP5 - Select the virtual channel 2 for pipe Z
	{ 0x030f, 0x02},   // FRONTTOP5 - Select the virtual channel 2 for pipe U

	{ 0x031D, 0x30}, // PHY clock rate -  1600MBPS + disable fine tune
	{ 0x0320, 0x30}, // PHY clock rate -  1600MBPS + disable fine tune
	{ 0x0323, 0x30}, // PHY clock rate -  1600MBPS + disable fine tune
	{ 0x0326, 0x30}, // PHY clock rate -  1600MBPS + disable fine tune

	{ 0x040A, 0x00}, // lane count - 0 lanes striping on controller 0 (Port A slave in 2x1x4 mode).
	{ 0x044A, 0x40}, // From the datasheet, 2 datalanes //lane count - 4 lanes striping on controller 1 (Port A master in 2x1x4 mode).
	{ 0x048A, 0x40}, // From the datasheet, 2 datalanes //lane count - 4 lanes striping on controller 2 (Port B master in 2x1x4 mode).
	{ 0x04CA, 0x00}, // lane count - 0 lanes striping on controller 3 (Port B slave in 2x1x4 mode).

	// When using the 9296 in 2x4 (it is a dual deserializer),
	// some lane stripping is necessary to output everything on port MIPI 0
	// This means that the MIPI output is 1x2 after lane stripping.
	// Since the pipe Y seems connected natively to PHY 1 and the pipe Z to PHY 2,
	// and since we have selected VC 0 on pipe Y and VC 2 on pipe Z, we need
	// to remap those signals on port MIPI 0.
	// VC 0 on pipe Y has an output that defaults to MIPI PHY 1 on port 0 in 1x4 mode
	// (It should be noted that MIPI PHY 0 and 3 are disabled in 2x4 mode)
	// VC 2 on pipe Z has an output that defaults to MIPI PHY 2 on port 1 in 1x4 mode
	// that is why we remap it to MIPI port 0 below.


	////////////////////////////////////////// SERIALIZER 1

	// Route pipe Y to DPHY1
	{ 0x044b, 0x07}, // Enable mapping registers 0 to 2 for MIPI PHY 1
	// This phy is connected to pipe Y stream 0
	{ 0x044d, 0x2b}, // When the stream source is 0 with raw 10 data type
	{ 0x044e, 0x6b}, // mark it as virtual channel 0 with raw 10 data type
	// and send it through MIPI port x.
	{ 0x044f, 0x40}, // Frame start with id 0 on PHY 1...
	{ 0x0450, 0x40}, // Is sent as frame start with id 1 through MIPI port x.
	{ 0x0451, 0x41}, // Frame end with id 0 on PHY 1...
	{ 0x0452, 0x41}, // is sent as frame end with id 1 through MIPI port x.
	{ 0x046d, 0x15}, // Map mapping registers 0 to 2 to MIPI PHY 1 : x=1
	// { 0x046d, 0x15}, // Map mapping registers 0 to 2 to MIPI PHY 1 : x=1
	// Now pipe Y is on MIPI port 0, with VC id 1

	// Route pipe X to DPHY1
	{ 0x040b, 0x07}, // enable 4 mappings for pipe X

	{ 0x040d, 0x2b}, // map raw10,vc0
	{ 0x040e, 0x2b}, // map raw10,vc0

	{ 0x040f, 0x00}, // map raw10,vc0
	{ 0x0410, 0x00}, // map raw10,vc0
	
	{ 0x0411, 0x01}, // map raw10,vc0
	{ 0x0412, 0x01}, // map raw10,vc0

	{ 0x042d, 0x15}, // Map pipe X to DPHY 2
	// { 0x042d, 0x15}, // Map pipe X to DPHY 1
	// Now pipe X is on MIPI port 0, with VC id 0

	////////////////////////////////////////// SERIALIZER 2

	// Route pipe Z to DPHY2
	{ 0x048b, 0x07}, // Enable mapping registers 0 to 2 for MIPI PHY 1
	// This phy is connected to pipe Y stream 0
	{ 0x048d, 0x2b}, // When the stream source is 0 with raw 10 data type
	{ 0x048e, 0x2b}, // mark it as virtual channel 0 with raw 10 data type
	// and send it through MIPI port x.
	{ 0x048f, 0x00}, // Frame start with id 0 on PHY 1...	
	{ 0x0490, 0x00}, // Is sent as frame start with id 1 through MIPI port x.
	
	{ 0x0491, 0x01}, // Frame end with id 0 on PHY 1...
	{ 0x0492, 0x01}, // is sent as frame end with id 1 through MIPI port x.
	
	{ 0x04ad, 0x2a}, // Map mapping registers 0 to 2 to MIPI PHY 1 : x=1
	// Now pipe Z is on MIPI port 0, with VC id 1

	// Route pipe U to DPHY2
	{ 0x04cb, 0x07}, // enable 4 mappings for pipe X

	{ 0x04cd, 0x2b}, // map raw10,vc0
	{ 0x04ce, 0x6b}, // map raw10,vc0

	{ 0x04cf, 0x40}, // map raw10,vc0
	{ 0x04d0, 0x40}, // map raw10,vc0
	
	{ 0x04d1, 0x41}, // map raw10,vc0
	{ 0x04d2, 0x41}, // map raw10,vc0

	// { 0x042d, 0x2a}, // Map pipe X to DPHY 2
	{ 0x04ed, 0x2a}, // Map pipe X to DPHY 1
	// Now pipe X is on MIPI port 0, with VC id 0

	//////////////////////////////////////////

	// { 0x048b, 0x07}, // Enable mapping registers 0 to 2 for MIPI PHY 2
	// // This phy is connected to pipe Z stream 2
	// { 0x048d, 0x2b}, // When the stream source is 0 with raw 10 data type
	// { 0x048e, 0x2b}, // mark it as virtual channel 1 with raw 10 data type
	// // and send it through MIPI port x.
	// { 0x048f, 0x00}, // Frame start with id 1 on PHY 2...
	// { 0x0490, 0x00}, // Is sent as frame start with id 1 through MIPI portx.
	
	// { 0x0491, 0x01}, // Frame end with id 1 on PHY 2...
	// { 0x0492, 0x01}, // is sent as frame end with id 1 through MIPI port x.
	
	// { 0x04ad, 0x15}, // Map mapping registers 0 to 2 to MIPI PHY 1 : x=1
	// // Now pipe Z is on MIPI port 0, with VC id 1

	{ 0x0005, 0x00}, // Disable lock output, disable errb

	{0x02b3,0x83},// MFP1 GPIO TX output driver disabled
	{0x02b4,0x10},// TX address = 0x10
	{0x02bc,0x04},
	{0x02be,0x11},
	{0x02bf,0x04},
	{0x02c1,0x12},
	{0x0003, 0x40}, // Disable UART1

	{0x03EF,0xC0},   // AUTO_FS_LINKS = 0, FS_USE_XTAL = 1, FS_LINK_[3:0] = 0
	{0x03E2,0x00},   // Turn off auto master link selection
	{0x03EA,0x00},   // OVLP window = 0
	{0x03EB,0x00},   // OVLP window = 0

	//FSYNC --> overwrite when fps set
	{0x03E5,0x9A},    // 60Hz FSYNC LVal of period
	{0x03E6,0x5B},    // Mval of period
	{0x03E7,0x06},    // Hval of period
	{0x03F1,0x40},    // 
	{0x03E0,0x04},    // Enable manual frame sync, output on GPIO --> drive slave devices

	{MAX9296_TABLE_END, 0x00}
};

// static struct index_reg_8 max9296_zedx_sensor_reset2[] = {

// 	// route data from serializer stream 0x01(Y) 0x03(U) -> 0x00(Z) 0x01(Y) -> DPHY1 

// 	{ 0x0330, 0x04}, // Set MIPI Phy Mode: 2x(1x4) mode
// 	{ 0x0332, 0xF0}, // All MIPI Phy powered - dflt mapping

// 	{ 0x0333, 0x4E}, // MIPI Phy 0&1 lane maps - dflt mapping
// 	{ 0x0334, 0xE4}, // MIPI Phy 3&2 lane maps - dflt mapping

// 	// X=0x00, Y=0x01, Z=0x02,U=0x03
// 	{ 0x0050, 0x00}, // Route data from stream 0(X) to pipe X
// 	{ 0x0051, 0x02}, // Route data from stream 2(Z) to pipe Y
// 	{ 0x0052, 0x03}, // Route data from stream 3(U) to pipe Z
// 	{ 0x0053, 0x01}, // Route data from stream 1(Y) to pipe U

// 	{0x030b, 0x02},   // FRONTTOP3 - Select the virtual channel 0 for pipe Y - dflt mapping
// 	// { 0x030d, 0x02},   // FRONTTOP5 - Select the virtual channel 2 for pipe Z

// 	{ 0x031D, 0x30}, // PHY clock rate -  1600MBPS + disable fine tune
// 	{ 0x0320, 0x30}, // PHY clock rate -  1600MBPS + disable fine tune
// 	// { 0x0323, 0x30}, // PHY clock rate -  1600MBPS + disable fine tune
// 	// { 0x0326, 0x30}, // PHY clock rate -  1600MBPS + disable fine tune

// 	{ 0x040A, 0x00}, // lane count - 0 lanes striping on controller 0 (Port A slave in 2x1x4 mode).
// 	{ 0x044A, 0x40}, // From the datasheet, 2 datalanes //lane count - 4 lanes striping on controller 1 (Port A master in 2x1x4 mode).
// 	{ 0x048A, 0x40}, // From the datasheet, 2 datalanes //lane count - 4 lanes striping on controller 2 (Port B master in 2x1x4 mode).
// 	{ 0x04CA, 0x00}, // lane count - 0 lanes striping on controller 3 (Port B slave in 2x1x4 mode).

// 	// When using the 9296 in 2x4 (it is a dual deserializer),
// 	// some lane stripping is necessary to output everything on port MIPI 0
// 	// This means that the MIPI output is 1x2 after lane stripping.
// 	// Since the pipe Y seems connected natively to PHY 1 and the pipe Z to PHY 2,
// 	// and since we have selected VC 0 on pipe Y and VC 2 on pipe Z, we need
// 	// to remap those signals on port MIPI 0.
// 	// VC 0 on pipe Y has an output that defaults to MIPI PHY 1 on port 0 in 1x4 mode
// 	// (It should be noted that MIPI PHY 0 and 3 are disabled in 2x4 mode)
// 	// VC 2 on pipe Z has an output that defaults to MIPI PHY 2 on port 1 in 1x4 mode
// 	// that is why we remap it to MIPI port 0 below.

// 	// Route pipe Y to DPHY1
// 	{ 0x044b, 0x07}, // Enable mapping registers 0 to 2 for MIPI PHY 1
// 	// This phy is connected to pipe Y stream 0
// 	{ 0x044d, 0x2b}, // When the stream source is 0 with raw 10 data type
// 	{ 0x044e, 0x6b}, // mark it as virtual channel 0 with raw 10 data type
// 	// and send it through MIPI port x.
// 	{ 0x044f, 0x40}, // Frame start with id 0 on PHY 1...
// 	{ 0x0450, 0x40}, // Is sent as frame start with id 1 through MIPI port x.
// 	{ 0x0451, 0x41}, // Frame end with id 0 on PHY 1...
// 	{ 0x0452, 0x41}, // is sent as frame end with id 1 through MIPI port x.
// 	{ 0x046d, 0x2a}, // Map mapping registers 0 to 2 to MIPI PHY 1 : x=1
// 	// Now pipe Y is on MIPI port 0, with VC id 1

// 	// Route pipe X to DPHY1
// 	{ 0x040b, 0x07}, // enable 4 mappings for pipe X

// 	{ 0x040d, 0x2b}, // map raw10,vc0
// 	{ 0x040e, 0x2b}, // map raw10,vc0

// 	{ 0x040f, 0x00}, // map raw10,vc0
// 	{ 0x0410, 0x00}, // map raw10,vc0
	
// 	{ 0x0411, 0x01}, // map raw10,vc0
// 	{ 0x0412, 0x01}, // map raw10,vc0

// 	// { 0x042d, 0x2a}, // Map pipe X to DPHY 2
// 	{ 0x042d, 0x2a}, // Map pipe X to DPHY 1
// 	// Now pipe X is on MIPI port 0, with VC id 0

// 	// { 0x048b, 0x07}, // Enable mapping registers 0 to 2 for MIPI PHY 2
// 	// // This phy is connected to pipe Z stream 2
// 	// { 0x048d, 0x2b}, // When the stream source is 0 with raw 10 data type
// 	// { 0x048e, 0x2b}, // mark it as virtual channel 1 with raw 10 data type
// 	// // and send it through MIPI port x.
// 	// { 0x048f, 0x00}, // Frame start with id 1 on PHY 2...
// 	// { 0x0490, 0x00}, // Is sent as frame start with id 1 through MIPI portx.
	
// 	// { 0x0491, 0x01}, // Frame end with id 1 on PHY 2...
// 	// { 0x0492, 0x01}, // is sent as frame end with id 1 through MIPI port x.
	
// 	// { 0x04ad, 0x15}, // Map mapping registers 0 to 2 to MIPI PHY 1 : x=1
// 	// // Now pipe Z is on MIPI port 0, with VC id 1

// 	{ 0x0005, 0x00}, // Disable lock output, disable errb

// 	{0x02b3,0x83},// MFP1 GPIO TX output driver disabled
// 	{0x02b4,0x10},// TX address = 0x10
// 	{0x02bc,0x04},
// 	{0x02be,0x11},
// 	{0x02bf,0x04},
// 	{0x02c1,0x12},
// 	{0x0003, 0x40}, // Disable UART1

// 	{0x03EF,0xC0},   // AUTO_FS_LINKS = 0, FS_USE_XTAL = 1, FS_LINK_[3:0] = 0
// 	{0x03E2,0x00},   // Turn off auto master link selection
// 	{0x03EA,0x00},   // OVLP window = 0
// 	{0x03EB,0x00},   // OVLP window = 0

// 	//FSYNC --> overwrite when fps set
// 	{0x03E5,0x9A},    // 60Hz FSYNC LVal of period
// 	{0x03E6,0x5B},    // Mval of period
// 	{0x03E7,0x06},    // Hval of period
// 	{0x03F1,0x40},    // 
// 	{0x03E0,0x04},    // Enable manual frame sync, output on GPIO --> drive slave devices

// 	{MAX9296_TABLE_END, 0x00}
// };

static struct index_reg_8 max9296_zedone4k_sensor_reset[] = {

	// Full reset, registers and paths
	{ 0x0330, 0x04}, // Set MIPI Phy Mode: 2x(1x4) mode
	{ 0x0332, 0xF0}, // All MIPI Phy powered - dflt mapping

	{ 0x0333, 0x4E}, // MIPI Phy 0&1 lane maps - dflt mapping
	{ 0x0334, 0xE4}, // MIPI Phy 3&2 lane maps - dflt mapping

	{ 0x0050, 0x00}, // Route data from stream 1 to pipe X
	{ 0x0051, 0x01}, // Route data from stream 0 to pipe Y
	{ 0x0052, 0x02}, // Route data from stream 2 to pipe Z - dflt mapping
	{ 0x0053, 0x03}, // Route data from stream 3 to pipe U - dflt mapping


	{ 0x031D, 0x00}, // PHY clock rate -  1600MBPS + disable fine tune
	{ 0x0320, 0x2f}, // PHY clock rate -  1600MBPS + disable fine tune

	{ 0x040A, 0x00}, // lane count - 0 lanes striping on controller 0 (Port A slave in 2x1x4 mode).
	{ 0x044A, 0x40}, // From the datasheet, 2 datalanes //lane count - 4 lanes striping on controller 1 (Port A master in 2x1x4 mode).

	// When using the 9296 in 2x4 (it is a dual deserializer),
	// some lane stripping is necessary to output everything on port MIPI 0
	// This means that the MIPI output is 1x2 after lane stripping.
	// Since the pipe Y seems connected natively to PHY 1 and the pipe Z to PHY 2,
	// and since we have selected VC 0 on pipe Y and VC 2 on pipe Z, we need
	// to remap those signals on port MIPI 0.
	// VC 0 on pipe Y has an output that defaults to MIPI PHY 1 on port 0 in 1x4 mode
	// (It should be noted that MIPI PHY 0 and 3 are disabled in 2x4 mode)
	// VC 2 on pipe Z has an output that defaults to MIPI PHY 2 on port 1 in 1x4 mode
	// that is why we remap it to MIPI port 0 below.

	{ 0x044b, 0x07}, // Enable mapping registers 0 to 2 for MIPI PHY 1
	// This phy is connected to pipe Y stream 0
	{ 0x044d, 0x2b}, // When the stream source is 0 with raw 10 data type
	{ 0x044e, 0x2b}, // mark it as virtual channel 0 with raw 10 data type
	// and send it through MIPI port x.
	{ 0x044f, 0x00}, // Frame start with id 0 on PHY 1...
	{ 0x0450, 0x00}, // Is sent as frame start with id 1 through MIPI port x.
	{ 0x0451, 0x01}, // Frame end with id 0 on PHY 1...
	{ 0x0452, 0x01}, // is sent as frame end with id 1 through MIPI port x.
	{ 0x046d, 0x15}, // Map mapping registers 0 to 2 to MIPI PHY 1 : x=1
	// Now pipe Y is on MIPI port 0, with VC id 0

	{ 0x0005, 0x00}, // Disable lock output, disable errb

	{0x02b3,0x83},// MFP1 GPIO TX output driver disabled
	{0x02b4,0x10},// TX address = 0x10
	{0x02bc,0x04},
	{0x02be,0x11},
	{0x02bf,0x04},
	{0x02c1,0x12},
	{0x0003, 0x40}, // Disable UART1

	{0x03EF,0xC0},   // AUTO_FS_LINKS = 0, FS_USE_XTAL = 1, FS_LINK_[3:0] = 0
	{0x03E2,0x00},   // Turn off auto master link selection

	{0x03EA,0x00},   // OVLP window = 0
	{0x03EB,0x00},   // OVLP window = 0

	//FSYNC --> overwrite when fps set
	{0x03E5,0x9A},    // 60Hz FSYNC LVal of period
	{0x03E6,0x5B},    // Mval of period
	{0x03E7,0x06},    // Hval of period

	{0x03F1,0x40},    // 
	{0x03E0,0x04},    // Enable manual frame sync, output on GPIO --> drive slave devices

	{MAX9296_TABLE_END, 0x00}
};

typedef enum
{
	ZEDX = 0,
	ZEDONEGS,
	ZEDONE4K,
	ZEDONEHDR,
	ZEDXHDR,
	ZEDXNANO,
	/* Don't add a camera type below N_CAM_TYPE, it will not be parsed*/
	N_CAM_TYPE,
}CamType;

static const char *camera_names[] = {
	[ZEDX] = "zedx",
	[ZEDONEGS] = "zedonegs",
	[ZEDONE4K] = "zedone4k",
	[ZEDONEHDR] = "zedonehdr",
	[ZEDXHDR] = "zedxhdr",
	[ZEDXNANO] = "zedxnano",
};

static struct index_reg_8 *reset_table[] = {
	// [ZEDX] = max9296_zedx_sensor_reset,
	[ZEDX] = max9296_test,
	[ZEDONEGS] = max9296_zedonegs_sensor_reset,
	[ZEDONE4K] = max9296_zedone4k_sensor_reset,
};

static struct i2c_fingerprint *reset_fingerprint_table[] = {
	[ZEDX] = zedx_sensor_reset,
	[ZEDONEGS] = zedonegs_sensor_reset,
	[ZEDONE4K] = zedone4k_sensor_reset,
	[ZEDONEHDR] = zedonehdr_sensor_reset,
	[ZEDXHDR] = zedxhdr_sensor_reset,
	[ZEDXNANO] = zedxnano_sensor_reset,
};

// static struct i2c_fingerprint_func *fingerprint_alt_table_func[] = {
// 	[ZEDX] = zedx_alt_fingerprint,
// 	[ZEDONEGS] = zedonegs_alt_fingerprint,
// 	[ZEDONE4K] = zedone4k_alt_fingerprint,
// 	[ZEDONEHDR] = zedonehdr_alt_fingerprint,
// 	[ZEDXHDR] = zedxhdr_alt_fingerprint,
// };

static i2c_fingerprint_func get_fingerprint_alt_table[] = {
	[ZEDX] = get_zedx_alt_fingerprint,
	[ZEDONEGS] = get_zedonegs_alt_fingerprint,
	[ZEDONE4K] = get_zedone4k_alt_fingerprint,
	[ZEDONEHDR] = get_zedonehdr_alt_fingerprint,
	[ZEDXHDR] = get_zedxhdr_alt_fingerprint,
	[ZEDXNANO] = get_zedxnano_alt_fingerprint,
};

static struct i2c_fingerprint *fingerprint_table[] = {
	[ZEDX] = zedx_fingerprint,
	[ZEDONEGS] = zedonegs_fingerprint,
	[ZEDONE4K] = zedone4k_fingerprint,
	[ZEDONEHDR] = zedonehdr_fingerprint,
	[ZEDXHDR] = zedxhdr_fingerprint,
	[ZEDXNANO] = zedxnano_fingerprint,
};

static int speed_table[] = {
	[ZEDX] = 0x30,
	[ZEDONEGS] = 0x30,
	[ZEDONE4K] = 0x32,
	[ZEDONEHDR] = 0x2f,
	[ZEDXHDR] = 0x32,
	[ZEDXNANO] = 0x30,
};

enum
{
	MAX9296_LINK_REGS,
	MAX9296_7_FPS,
	MAX9296_15_FPS,
	MAX9296_25_FPS,
	MAX9296_30_FPS,
	MAX9296_60_FPS,
	MAX9296_120_FPS,
	MAX9296_SLAVE_FPS,
};

static __maybe_unused struct index_reg_8 *mode_table[] = {
	[MAX9296_LINK_REGS] = max9296_link_regs,
	[MAX9296_7_FPS] = max9296_7_fps,
	[MAX9296_15_FPS] = max9296_15_fps,
	[MAX9296_25_FPS] = max9296_25_fps,
	[MAX9296_30_FPS] = max9296_30_fps,
	[MAX9296_60_FPS] = max9296_60_fps,
	[MAX9296_120_FPS] = max9296_120_fps,
	[MAX9296_SLAVE_FPS] = max9296_slave_fps,
};

#endif /* __DESER_I2C_TABLES__ */
