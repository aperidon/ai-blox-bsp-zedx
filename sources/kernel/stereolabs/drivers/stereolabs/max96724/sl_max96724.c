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

 #define DEBUG  1

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

#define STRINGIFY0(s)                                                       
#define STRINGIFY(s) STRINGIFY0(s)                                              
#define DRV_STR_VERSION STRINGIFY(DESER_DRIVER_VERSION_MAJOR)"."STRINGIFY(DESER_DRIVER_VERSION_MINOR)"."STRINGIFY(DESER_DRIVER_VERSION_PATCH)"" 

//int write_reg_Ser(int slaveAddr, int channel, u16 addr, u8 val);
int set_bitrate_Dser(int channel, u32 i2c_bus, u8 val);
u32 fps_set_Dser(int channel, s64 val);
int dser_get_gmsl_port(int channel, int zedx_id);
int dser_enable_gmsl_link(int channel, int zedx_id);
int dser_open_all_gmsl_link(int channel);
bool isSecondCamFromI2C(int channel, int zedx_id);
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
    u32 dser_port; /*Dser CSI connection */
    u32 dser_cc_port;
    u8 gmsl_link; /* GMSL link connected to the device */
    u32 cam_addr;
    u32 ser_addr;
    u32 zedx_id;
    bool is_second_cam_from_i2c;
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
    struct i2c_client *i2c_client;
    struct regmap *regmap;
    struct list_head sensor_list;
    u32 channel; // channel id from dts
    char csi_port;
    u32 n_lanes;
    int reset_gpio;
    int pwr_gpio;
    int pwdn_gpio;
    s8 port_to_i2c[N_GMSL_PORTS];
    u8 i2c_trans[2*N_GMSL_PORTS][2];
    u8 sensor_list_sz;
    u8 avail_pipe;
    u8 n_cam;
	u8 avail_i2c_bus[N_DSER_I2C_BUS];
    struct serializer_devices ser_devices[N_MAX_TOTAL_SER];
    struct sensor detected_sensors[2*N_GMSL_PORTS];
};

// static void print_sensor_info(struct max96724 *priv, struct sensor *sp)
// {
//     struct i2c_client *client = priv->i2c_client;
// 	u8 i;

//     dev_info(&client->dev, "%s: following comes from dts\n", __func__);
//     dev_info(&client->dev, "%s: camera %s\n", __func__, sp->camera);
//     dev_info(&client->dev, "%s: model id %d\n", __func__, sp->model);
//     dev_info(&client->dev, "%s: dts id %d\n", __func__, sp->cam_dts_id);
//     dev_info(&client->dev, "%s: n_lanes %d\n", __func__, sp->n_lanes);
//     dev_info(&client->dev, "%s: serial port %d\n", __func__, sp->serial);
//     dev_info(&client->dev, "%s: vc-id %d\n", __func__, sp->vc_id);
//     dev_info(&client->dev, "%s: i2c bus %d\n", __func__, sp->i2c_bus);
//     dev_info(&client->dev, "%s: SER ADDR %x\n", __func__, sp->ser_addr);
//     dev_info(&client->dev, "%s: CAM ADDR %x\n", __func__, sp->cam_addr);


//     if (sp->detection_id < 0)
//         return;

//     dev_info(&client->dev, "%s: following comes from port parsing\n", __func__);
//     dev_info(&client->dev, "%s: detection id  %d\n", __func__, sp->detection_id);

//     for (i = 0; i < N_SER_PIPES; i++)
//     {
//         dev_info(&client->dev, "%s: pipe %d = %d\n", __func__, i, sp->pipes[i]);
//     }

//     return;

// }

/**
 * global_priv - Array of pointers to deserializer device structures
 *
 * Array of pointers to deserializer device structures representing connected devices.
 */
struct max96724 *global_priv[4];

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
    if(!strcmp(sp->camera,"zedonepro") || !strcmp(sp->camera,"zedone4k") || !strcmp(sp->camera,"zedonegs"))
        return true; 
    else
        return false;
}

static inline int apply_alternate_mapping(struct max96724 *priv, struct sensor *sp,
		struct i2c_fingerprint *alt_i2c)
{
	struct i2c_client *client = priv->i2c_client;
	int deser_addr = client->addr;
	int err;
	u8 j = 0;
    u32 val;
    u32 ser_addr = sp->ser_addr;
    u32 sen_1_addr = 0, sen_2_addr = 0;

    sen_1_addr = sp->cam_addr;
    //if camera mono, sen_2_addr won't be used so it doesn't matter if it takes a wrong addr
    if(!isCameraMono(sp))
    sp = list_entry(sp->list.next, struct sensor, list);
    sen_2_addr = sp->cam_addr;

    dev_dbg(&client->dev, "%s: Apply new addr : Serializer -> @0x%x / sen 0 -> @0x%x / sen 1 -> @0x%x \n",
					__func__, ser_addr, sen_1_addr,sen_2_addr);

	while (alt_i2c[j].i2c_addr!=MAX96724_TABLE_END)
	{
        if(alt_i2c[j].i2c_addr==SLEEP)
        {
            msleep(100);
            j++;
            continue;
        }
        switch(alt_i2c[j].val)
        {
            case SER_ADDR:
                val = ser_addr<<1;
            break;
            case RIGHT_SENSOR_ADDR:
                if(!strcmp(sp->camera,"zedxpro") || !strcmp(sp->camera,"zedonepro"))
                    val = sen_1_addr;
                else
                    val = sen_1_addr<<1;
            break;
            case LEFT_SENSOR_ADDR:
                if(!strcmp(sp->camera,"zedxpro") || !strcmp(sp->camera,"zedonepro"))
                    val = sen_2_addr;
                else
                    val = sen_2_addr<<1;   
            break;
            default:
                val = alt_i2c[j].val;
            break;
        }

        switch(alt_i2c[j].i2c_addr)
        {
            case SER_ADDR:
                client->addr = ser_addr;
            break;
            case RIGHT_SENSOR_ADDR:
                client->addr = sen_1_addr;
            break;
            case LEFT_SENSOR_ADDR:
                client->addr = sen_2_addr;
            break;
            default:
                client->addr = alt_i2c[j].i2c_addr;
            break;
        }

        err = regmap_write(priv->regmap, alt_i2c[j].reg_addr, val);

		dev_dbg(&client->dev, "%s: I2C device @0x%x, read returns %d\n",
					__func__, client->addr, err);

		dev_dbg(&client->dev, "%s: value written in 0x%x (if any) : 0x%x",
					__func__,alt_i2c[j].reg_addr, val);

		if (err)
		{
			dev_err(&client->dev, "%s: Cannot write 0x%x in 0x%x\n",
					__func__,val, alt_i2c[j].reg_addr);
			client->addr = deser_addr;

			return -1;
		}

		j++;
	}

	client->addr = deser_addr;

	return 0;
}

static inline void zedxep_patch(struct max96724 *priv,
		struct i2c_fingerprint *fingerprint)
{
	struct i2c_client *client = priv->i2c_client;
	int deser_addr = client->addr;
	unsigned int val = 0;

	client->addr = fingerprint[0].i2c_addr;

	regmap_read(priv->regmap, fingerprint[1].reg_addr, &val);

	if (val == fingerprint[1].val)
		regmap_write(priv->regmap, fingerprint[0].reg_addr, 
				fingerprint[0].val);

	client->addr = deser_addr;
}

static inline void model_reset(struct max96724 *priv, u8 model)
{
	struct i2c_fingerprint *fingerprint = reset_table[model];
	struct i2c_client *client = priv->i2c_client;
	int deser_addr=client->addr;
	u8 j=0;
	int err;

	/* If we need to reset the serializer because we changed its address */
	if (fingerprint[j].reg_addr == 0x0000)
	{

		while (j < 2)
		{
			client->addr = fingerprint[j].i2c_addr;

			/* when j=0, the first write might fail
			 * if we just plugged in the sensor:
			 * hence, we just check the last err */
			err = regmap_write(priv->regmap, fingerprint[j].reg_addr  , fingerprint[j].val);

			msleep(6);

			j++;

		}
		
		/* In case of success, the serializer is reset and
		 * retrieves its original address: we sleep a total of 100ms */
		if ( err==0 )
			msleep(94);

	}

	/* We can parse the reset table for the current sensor */
	while (fingerprint[j].i2c_addr!=MAX96724_TABLE_END)
	{
		client->addr = fingerprint[j].i2c_addr;

        if(fingerprint[j].i2c_addr == SLEEP )
        {
            msleep(100);
            j++;
            continue;
        }

		err = regmap_write(priv->regmap, fingerprint[j].reg_addr  , fingerprint[j].val);


		dev_dbg(&client->dev, "%s: I2C device @0x%x, read returns %d\n",
				__func__, fingerprint[j].i2c_addr, err);

		dev_dbg(&client->dev, "%s: value wrote in 0x%x (if any) : 0x%x",
				__func__,fingerprint[j].reg_addr, fingerprint[j].val);

		msleep(6);

		j++;
	}

	client->addr = deser_addr;

	return;

}

static inline int check_model(struct max96724 *priv, u8 i)
{
	struct i2c_client *client = priv->i2c_client;
	struct i2c_fingerprint *fingerprint = fingerprint_table[i];
	unsigned int val = 0;
	int deser_addr=client->addr;
	int err;
	u8 j = 0;

	/* ZED X EP Patch */
	if (i == ZEDX)
	{
		zedxep_patch(priv, fingerprint);

		/* skip the first entry for ZED X fingerprint */
		j = 1;
	}

	/* We reset the alternative i2c_mapping, if any */
	model_reset(priv, i);

	/* We check the i2c fingerprint correspondance with this model */
	while (fingerprint[j].i2c_addr!=MAX96724_TABLE_END)
	{

		client->addr = fingerprint[j].i2c_addr;

		msleep(10);

		err = regmap_read(priv->regmap, fingerprint[j].reg_addr  , &val);

		dev_dbg(&client->dev, "%s: I2C device @0x%x, read returns %d\n",
				__func__, fingerprint[j].i2c_addr, err);

		dev_dbg(&client->dev, "%s: value read in 0x%x (if any) : 0x%x",
				__func__,fingerprint[j].reg_addr, val);

		if (fingerprint[j].val != val || err)
		{
			client->addr = deser_addr;

			return -1;
		}

		j++;
	}

	client->addr = deser_addr;


	return 0;
}

static inline int sl_max96724_get_camera_model(struct max96724 *priv)
{
	struct i2c_client *client = priv->i2c_client;
	int err;
	u8 i;

	for (i = 0; i<N_CAM_TYPE; i++)
	{

		err = check_model(priv, i);

		if (err)
			continue;

		dev_info(&client->dev, "%s: %s camera connected to this port",
				__func__, camera_names[i]);
		
		return i;
	}
    
    return -1;
}

static int sl_max96724_i2c_setup(struct max96724 *priv)
{
    int err;
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

    err = regmap_write(priv->regmap, GMSL_LINKS_CC_REG, val_cc);

    err = regmap_write(priv->regmap, GMSL_LINKS_EN_REG, val_port);

    msleep(SLEEP_TIME);

    return 0;
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
            break;
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
		val = val | (u8)((offset + (sp->dser_port%4)) << 2*i);

	ret = regmap_write(priv->regmap, addr, val);
    dev_dbg(&i2c_client->dev,"%s: (0x%x ; 0x%x)\n",__func__,addr,val);

	if (ret)
		dev_warn(&i2c_client->dev, "%s: fail write in csi reg of pipe %d\n",
				__func__, dser_pipe);

	offset = 0;

	/* Set the data rate for this CSI Link, there is one cam per I2C so should be all right */
	addr = csi_data_rate_reg.addr + 0x03 * sp->dser_port + offset;

	ret = regmap_write(priv->regmap, addr, csi_data_rate_reg.val);
    dev_dbg(&i2c_client->dev,"%s: (0x%x ; 0x%x)\n",__func__,addr,csi_data_rate_reg.val);

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
	s8 cam_id = -1;

    for(i=0; i < priv->avail_pipe; i++)
        cam_pipping = cam_pipping<<1;

    dev_dbg(&client->dev, "%s: n_cam = %d ->  cam_pipping = 0x%x\n",
                     __func__, priv->n_cam, cam_pipping);

    /* For each serializer pipe */
    for (i=0; i<N_SER_PIPES; i++)
    {
        if (priv->avail_pipe >= N_DSER_PIPES)
        {
            dev_warn(&client->dev, "%s: no more pipes available\n",
                    __func__);
            return 0;
        }

        if ((sp->cam_dts_id != cam_id) && cam_id >= 0)
        {
            dev_dbg(&client->dev, "%s: no more sensor for this camera\n",
                    __func__);
            //return 0;
            break;
        }

        if (sp->model != model)
        {
            dev_warn(&client->dev, "%s: Montrouge, we got a problem\n",
                    __func__);
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

        /**
         * get next sensor of this camera, since we filled the list in the right order,
         * we just need to get next element */
        //sp = list_entry(sp->list.next, struct sensor, list);

        if (sp->list.next != &priv->sensor_list) {
            sp = list_entry(sp->list.next, struct sensor, list);
        } else {
            dev_warn(&client->dev,"%s: No more sensor",__func__);
            break;
        }

    }

	dev_info(&client->dev, "%s: camera pipeline operational\n", __func__);

    return 0;
}

static int filter_by_gmsl_entry(struct max96724 *priv, int id, int gmsl_index)
{
    dev_dbg(&priv->i2c_client->dev, "%s: id: %d (eq GMSL %d) / Looking for gmsl %d \n",
                __func__,id,id % N_GMSL_PORTS,gmsl_index);
    if(id % N_GMSL_PORTS == gmsl_index)
        return 0;
    else
        return 1;
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
    int deser_addr=client->addr; //init addr
    struct sensor *sp;
    struct list_head *pos;
    unsigned int link = 0, ret = 0;
    int tab_id = MAX96724_LINK_REGS;
    int err;
    int model;
    bool cam_found, config_supported;
    u8 i,j;
    int cam_gmsl_id;

    priv->n_cam = 0;

    priv->avail_pipe = 0;

	dev_dbg(&client->dev, "%s: client addr = 0x%x\n",
			__func__, client->addr);   

    for (i = 0; i < N_GMSL_PORTS; i++)
    {
        err = regmap_write(priv->regmap, GMSL_LINKS_EN_REG, 0xF0|(1<<i));

        msleep(SLEEP_TIME);

        if (err || verbosity_level)
            dev_dbg(&client->dev, "%s: write addr = 0x%x, val = 0x%x, err %d\n",
                    __func__, GMSL_LINKS_EN_REG, 0xF0|(1<<i), err);

        err = regmap_read(priv->regmap, mode_table[tab_id][i].addr, &link);

        if (err && verbosity_level)
        {
            dev_dbg(&client->dev, "%s: write addr = 0x%x, val = 0x%x, err %d\n",
                    __func__, mode_table[tab_id][i].addr, link, err);
            return -1;
        }

        /* Bit mask to get the essential information: is link i connected?*/
        link = (link & 0x08) >> 3;

        if (!link)
		{
			dev_info(&client->dev, "%s: No camera connected to GMSL port %d\n",
					__func__, i);
			continue;
		}
        
        /* increments the number of connected links accordingly */
        ret += link;
        
        dev_info(&client->dev, "%s: Camera connected to GMSL port %d\n",
				__func__, i);

        for (j = 0; j < N_MAX_TOTAL_SER; j++){
            struct i2c_fingerprint reset_table[] = {
                {priv->ser_devices[j].ser_addr, 0x0010, 0x91}, /* Reset every possible serializer */
            };

            if(!priv->ser_devices[j].ser_addr)
                continue;
            
            client->addr = reset_table[0].i2c_addr;
            err = regmap_write(priv->regmap, reset_table[0].reg_addr, reset_table[0].val);
            dev_dbg(&client->dev, "%s: reset ser %s %d %d %x %x %02x\n",
                    __func__,camera_names[priv->ser_devices[j].camera_model],priv->ser_devices[j].zedx_id, err,client->addr,reset_table[0].reg_addr,reset_table[0].val);

            msleep(6);
        }
        msleep(200);

        client->addr = deser_addr;        

        /* read the camera fingerprint and return its ID */
        model = sl_max96724_get_camera_model(priv);
        
        if (model<0)
        {
            dev_warn(&client->dev, "%s: Camera model unknown\n", __func__);
            continue;
        }

        cam_found = false;
        cam_gmsl_id=0;
        /* for each sensor from the dts */
        list_for_each(pos, &priv->sensor_list)
        {
            sp = list_entry(pos, struct sensor, list);
            
            err = 0;
            if (sp == NULL)
            {
                return -1;
            }

            /* if not the right model or this model has been assigned already
             * we continue looking for an available camera */
            if (!(sp->model == model))
                continue;

            /* We select the i entry in DT of the detected model to associate to GMSL i */
            /* if cam_gmsl_id isn't the same as i, we increment it and pass to the next camera */
            if(filter_by_gmsl_entry(priv, cam_gmsl_id, i))
            {
                cam_gmsl_id++;
                config_supported=false;
                /* If camera Stereo, skip second sensor */
                if(!isCameraMono(sp))
                    pos = pos->next;
                continue;
            }
            
            dev_info(&client->dev, "%s: found %s %d linked to GMSL %d  \n",
                     __func__,camera_names[model], sp->zedx_id, i);

			/* GMSL port i will be connected to the i2c bus priv->avail_i2c_bus[model] */
			priv->port_to_i2c[i] = sp->dser_cc_port;
            sp->gmsl_link = i;

            /* we found the right sensor to initialize a camera */
            cam_found = true;
            config_supported = true;
		/* Next camera with the same model will be connected to another bus */
            priv->avail_i2c_bus[sp->dser_cc_port]++;
            break;

        }

        if (!cam_found)
        {
            if(!config_supported)
            {
                dev_warn(&client->dev, "%s: Camera plugged in GMSL #%d wrongly placed. Check user guide for camera placement info  \n", __func__, i);
            }
            else
            {
                dev_warn(&client->dev, "%s: Known camera connected, but entry not found in DTS.\n",
                        __func__);
                dev_warn(&client->dev, "%s: Do you have the right DTS?\n",
                        __func__);
            }

			continue;
        }

        sp->is_second_cam_from_i2c = false;
        sl_max96724_pipes_setup(priv, sp, model, i);

        priv->detected_sensors[priv->n_cam] = *sp;
        priv->n_cam++;

        dev_dbg(&client->dev, "%s: csi route : gmsl port %d -> pipes [%d %d %d %d] (vc %d)-> dser port %d -> %d jetson serial",__func__, sp->gmsl_link, 
            sp->pipes[0],sp->pipes[1],sp->pipes[2],sp->pipes[3],sp->vc_id,sp->dser_port, sp->serial);

        err = apply_alternate_mapping(priv, sp , fingerprint_alt_table[sp->model]);

    }

    /* enable build the correct i2c_map */
    sl_max96724_i2c_setup(priv);

    err = regmap_write(priv->regmap, GMSL_PIPES_ENABLE,
                       0xFF >> (N_DSER_PIPES - (priv->avail_pipe)));

    return ret;
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

static int slow_reset_Dser(struct max96724 *priv)
{
    int err;
    int channel = priv->channel;

    if (channel > 3 || channel < 0 || global_priv[channel] == NULL)
        return -1;

    if(priv->n_lanes == 2)
    {
        err = sl_max96724_write_table(global_priv[channel], mode_table[MAX96724_INIT]);
        dev_err(&priv->i2c_client->dev, "%s: Setup Deser as 2 lanes \n", __func__);
    }
       
    if(priv->n_lanes == 4)
    {
        err = sl_max96724_write_table(global_priv[channel], mode_table[MAX96724_INIT_2x4]);
        dev_err(&priv->i2c_client->dev, "%s: Setup Deser as 4 lanes \n", __func__);
    }
        
    if (err)
        return -1;

    if (global_priv[channel]->csi_port == 'a')
	    return 0;

    //pas forcément utile car overwritten while mapping, peut être à enlever
	err = sl_max96724_write_table(global_priv[channel], mode_table[MAX96724_CSI_B]);

    return 0;
}

int dser_get_gmsl_port(int channel, int zedx_id){
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

int dser_enable_gmsl_link(int channel, int zedx_id){
	int err = -1;
	struct list_head *pos;
	struct sensor *sp;

    dev_dbg(&global_priv[channel]->i2c_client->dev,
        "%s: dser_enable_gmsl_link: %d \n",
        __func__, zedx_id);

    list_for_each(pos, &global_priv[channel]->sensor_list){
		sp = list_entry(pos, struct sensor, list);
		if (sp == NULL){
			return err;
		}
      
        if (sp->zedx_id != zedx_id)
			continue;

        dev_dbg(&global_priv[channel]->i2c_client->dev,
            "%s: MATCH: %d \n",
            __func__, sp->zedx_id);

        if(sp->gmsl_link == -1)
        {
        dev_info(&global_priv[channel]->i2c_client->dev,
            "%s: sp->gmsl_link == -1\n",__func__);
            return err;
        }

        err = write_reg_Dser(channel, GMSL_LINKS_EN_REG, 
            0xF0 | (1<<sp->gmsl_link));
        if (err)
            return -1;

        dev_info(&global_priv[channel]->i2c_client->dev,
            "%s: gmsl id: %d \n",
            __func__, sp->gmsl_link);

        return sp->gmsl_link + (channel * N_GMSL_PORTS);
    }

    return -1;
}
EXPORT_SYMBOL(dser_enable_gmsl_link);

int dser_open_all_gmsl_link(int channel){
    int err = -1;
    dev_dbg(&global_priv[channel]->i2c_client->dev,
        "%s: dser_open_all_gmsl_link\n",
        __func__);
        
    err = write_reg_Dser(channel, GMSL_LINKS_EN_REG, 
            0xFF);
    return err;
}
EXPORT_SYMBOL(dser_open_all_gmsl_link);

int set_bitrate_Dser(int channel, u32 i2c_bus, u8 val)
{
	struct sensor *sp;
	struct list_head *pos;
	int err = -1;
	u8 reg, i;
	u16 addr;

	if (channel > 3 || channel < 0 || global_priv[channel] == NULL)
		return -1;

	if (val == 10)
	{
		reg = GMSL_10BIT_MODE;
	}

	if (val == 12)
	{
		reg = GMSL_12BIT_MODE;
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
		if (!(sp->i2c_bus == i2c_bus) || sp->detection_id < 0)
			continue;

		for (i = 0; i<N_SER_PIPES; i++)
		{
			if (sp->pipes[i]<0) 
				continue;

			addr = GMSL_VID_ST_MAP+0x40*sp->pipes[i];

			err = write_reg_Dser(channel, addr, reg);

			addr = addr+1;

			reg = reg | (sp->vc_id<<6);

			err = write_reg_Dser(channel, addr, reg);

		}
	}

	return err;
}
EXPORT_SYMBOL(set_bitrate_Dser);

int getCamPipeIndex(int channel, int zedx_id)
{
    struct sensor *sp;
    u8 i,j = 0;


    for( i=0; i < global_priv[channel]->n_cam; i++)
    {
        sp = &global_priv[channel]->detected_sensors[i];
        printk("%s: compare input id : %d and zedx_id %d\n",__func__,zedx_id,sp->zedx_id);
        if( zedx_id == sp->zedx_id)
        {
            printk("%s: pipes [%d %d %d %d]",__func__,sp->pipes[0],sp->pipes[1],sp->pipes[2],sp->pipes[3]);

            for( j=0; j<N_SER_PIPES; j++)
            {
                if(sp->pipes[j] >= 0)
                    return j;
            }
            break;
        }
    }
    return -1;
}
EXPORT_SYMBOL(getCamPipeIndex);


bool isSecondCamFromI2C(int channel, int zedx_id)
{
    struct sensor *sp;
    u8 i = 0;

    for( i=0; i < global_priv[channel]->n_cam; i++)
    {
        sp = &global_priv[channel]->detected_sensors[i];
        if( zedx_id == sp->zedx_id)
        {
            return sp->is_second_cam_from_i2c;
        }
    }
    return false;
}
EXPORT_SYMBOL(isSecondCamFromI2C);




u32 fps_set_Dser(int channel, s64 val)
{
    int err = -1;
    int tab_id = -1;

    if (channel > 3 || channel < 0 || global_priv[channel] == NULL)
        return -1;

    if (val == 7000000)
    {
        tab_id = MAX96724_7_FPS;
    }

    // 15fps full resolution
    if (val == 15000000)
    {
        tab_id = MAX96724_15_FPS;
    }
    
    // 30fps full resolution
    if ((val > 24000000) && (val < 26000000)) 
    {
        tab_id = MAX96724_25_FPS;
    }

    // 30fps full resolution
    if ((val > 29000000) && (val < 31000000)) 
    {
        tab_id = MAX96724_30_FPS;
    }

    // 60fps full resolution
    if ((val > 59000000) && (val < 61000000)) 
    {
        tab_id = MAX96724_60_FPS;
    }

    // 120fps full resolution
    if (val == 120000000)
    {
        tab_id = MAX96724_120_FPS;
    }

    // It is not really an error to ask for a wrong fps value
	if (tab_id == -1)
	{
		dev_dbg(&global_priv[channel]->i2c_client->dev,
				"%s: %lld is not a supported value. [30,60,120]*10^6 are supported.\n",
				__func__, val);
		return 0xEEEE;
	}

    err = sl_max96724_write_table(global_priv[channel], mode_table[tab_id]);

    if (err)
        return 0xFFFF;

    return 0;
}
EXPORT_SYMBOL(fps_set_Dser);


static int sl_max96724_parse_serializer_node(struct max96724 *priv,
		struct device_node *ser_node, s8 cam_id)
{
	int err = 0;
    struct i2c_client *i2c_client = priv->i2c_client;
	struct device_node *cam_node;
	struct device_node *mux_node;
	struct device_node *ports_node, *port_node, *endpoint_node;
    struct sensor *sp;
    int n_sensors;
	const char *str;
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

        // todo: get i2c, serial, vc-id and n_lanes
        cam_node = of_parse_phandle(ser_node, "camera-sensors", i);
        if (cam_node==NULL)
        {  
            dev_warn(&i2c_client->dev, "%s: no cam node in ser node...\n", __func__);
            continue;
        }

        of_property_read_u32(cam_node, "zedx-id", &sp->zedx_id);
        // dev_dbg(&i2c_client->dev, "%s: ZEDX ID = %d",__func__, sp->zedx_id);

        of_property_read_u32(cam_node, "dser-port", &sp->dser_port);

        of_property_read_u32(cam_node, "dser-cc-port", &sp->dser_cc_port);

        of_property_read_u32(cam_node, "reg", &sp->cam_addr);
        // dev_dbg(&i2c_client->dev, "%s: CAM ADDR = %x",__func__, sp->cam_addr);

        of_property_read_u32(ser_node, "reg", &sp->ser_addr);
        // dev_dbg(&i2c_client->dev, "%s: SER ADDR = %x",__func__, sp->ser_addr);

        priv->ser_devices[cam_id].ser_addr = sp->ser_addr;

        mux_node = of_get_parent(cam_node);
        if (mux_node==NULL)
        {  
            dev_warn(&i2c_client->dev, "%s: no parent node ?\n", __func__);
            of_node_put(cam_node);
            continue;
        }
        of_property_read_u32(mux_node, "reg", &sp->i2c_bus);
		// dev_dbg(&i2c_client->dev, "%s: associated bus = %d\n", __func__, sp->i2c_bus);
        
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
        if (port_node==NULL)
        {  
            dev_warn(&i2c_client->dev, "%s: no endpoint node amongst children...\n", __func__);
            of_node_put(cam_node);
            of_node_put(ports_node);
            of_node_put(port_node);
            continue;
        }

        of_property_read_u32(endpoint_node, "vc-id", &sp->vc_id);
        of_property_read_u32(endpoint_node, "bus-width", &sp->n_lanes);
        of_property_read_u32(endpoint_node, "port-index", &sp->serial);


		// dev_dbg(&i2c_client->dev, "%s: associated vc-id = %d\n", __func__, sp->vc_id);
		// dev_dbg(&i2c_client->dev, "%s: associated n_lanes = %d\n", __func__, sp->n_lanes);
		// dev_dbg(&i2c_client->dev, "%s: associated mipi port = %d\n", __func__, sp->serial);
		// dev_dbg(&i2c_client->dev, "%s: associated output deser port = %d\n", __func__, sp->dser_port);
		// dev_dbg(&i2c_client->dev, "%s: associated cc deser port = %d\n", __func__, sp->dser_cc_port);

        of_node_put(endpoint_node);
        of_node_put(port_node);
        of_node_put(ports_node);
        
        of_property_read_string(ser_node, "camera_model", &sp->camera);
        of_property_read_string(ser_node, "zedx-id", &str);

        err = kstrtoint(str,10,&sp->zedx_id);

        for (j=0; j<N_CAM_TYPE; j++)
        {
            if (strcmp(sp->camera, camera_names[j])==0)
                sp->model = j;
        }
        
        sp->cam_dts_id = cam_id;
        
		// if (DEBUG > 0)
		// 	print_sensor_info(priv, sp);

        of_node_put(cam_node);
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

	if (err)
	    dev_err(&i2c_client->dev,
			    "%s: channel not found --> Requires 'channel' entry in DTS\n",
			    __func__);


	priv->channel = str[0]-'a';

	if (priv->channel < 0 ||  priv->channel > 3 )
	    return -EINVAL;
    
    dev_info(&i2c_client->dev, "%s: Dser channel %c\n", __func__, str[0]);

	err = of_property_read_string(np, "opt-csi-port", &str);
	if (err)
	{
		dev_info(&i2c_client->dev, "%s: Using CSI port %c\n", __func__, 'a');
		priv->csi_port = 'a';
	}
	else if (str[0] == 'b' || str[0] == 'a')
	{
		dev_info(&i2c_client->dev, "%s: Using CSI port %c\n", __func__, str[0]);
		priv->csi_port = str[0];
	}

	err = of_property_read_string(np, "n_lanes", &str);
    priv->n_lanes = str[0] - '0';

	global_priv[priv->channel] = priv;

    n_serializers = of_count_phandle_with_args(np, "camera-serializers", NULL);

    dev_info(&i2c_client->dev, "%s: Number of declared cameras with this dts %d\n",
     __func__, n_serializers);

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

static int sl_max96724_probe(struct i2c_client *client,
        const struct i2c_device_id *id)
{
    struct device *dev = &client->dev;
    struct max96724 *priv;
    int err = 0;
    u8 i = 0;
    unsigned int pipe_sync_val;
    dev_info(dev, "%s: enter\n", __func__);

    priv = devm_kzalloc(dev, sizeof(*priv), GFP_KERNEL);

    INIT_LIST_HEAD(&priv->sensor_list);
    //priv->avail_i2c_bus = devm_kzalloc(dev, N_CAM_TYPE*sizeof(*priv->avail_i2c_bus), GFP_KERNEL);
    memset(priv->avail_i2c_bus, 0, sizeof(priv->avail_i2c_bus));
    /* list pointer,  camera list and list structure in stuct camera*/
    for (i = 0; i<N_GMSL_PORTS; i++)
    {
    	priv->port_to_i2c[i] = -1;
    	priv->i2c_trans[2*i][0] = 0;
    	priv->i2c_trans[2*i][1] = 0;
    	priv->i2c_trans[2*i+1][0] = 0;
    	priv->i2c_trans[2*i+1][1] = 0;
    }

    priv->avail_pipe = 0;

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

    slow_reset_Dser(priv);

	sl_max96724_gmsl_pipeline_setup(priv);

    /* MAX96724 needs the exact number of used pipes for SYNC */
    pipe_sync_val = 0xC0 | ((1 << priv->avail_pipe) - 1);
    err = write_reg_Dser(priv->channel, 0x04AF, pipe_sync_val);
    dev_info(dev,"%s: set SYNC 0x04AF to 0x%x", __func__, pipe_sync_val);

    /*set daymode by fault*/
    dev_info(dev, "%s: success\n", __func__);
    return err;
}

static int sl_max96724_remove(struct i2c_client *client)
{
    dev_info(&client->dev, "%s: success\n", __func__);

    //
    //  Everything is automatically deallocated.
    //

    return 0;
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
