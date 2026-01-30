/**
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 */

#include <linux/types.h>
#include <linux/kobject.h>
#include "isx031.h"
#include "isx031_mode_tbls.h"

#ifndef __ZEDXHDR_SYSFS_H__
#define __ZEDXHDR_SYSFS_H__
#if 0
/**
 * Function that prints the content of a register of the ISX031.
 * We consider that the sysfs file has been written with the
 * register address first.
 */
static ssize_t sysfs_regs_show(struct kobject *kobj,
		struct attribute *attr, char *buf)
{
	struct zedxhdr *priv = container_of(kobj, struct zedxhdr, debug_sysfs);
	struct tegracam_device *tc_dev = priv->tc_dev;
	struct device *dev = tc_dev->dev;
	

	dev_info(dev, "%s: User read requested \n", __func__);
	zedxhdr_read_reg(priv->s_data,(u16)priv->register_,(u8*)&priv->value_);
	return sprintf(buf, "0x%x\n", priv->value_);
}

/**
 * Function that updates the register pointer of the ISX031
 * or writes a register of the ISX031:
 * If 1 hex is given, it is consider as a register address to point to
 * If 2 hexs are sent, we write a register at the given address
 */
static ssize_t sysfs_regs_store(struct kobject *kobj,
		struct attribute *attr,const char *buf, size_t count)
{
	struct zedxhdr *priv = container_of(kobj, struct zedxhdr, debug_sysfs);
	struct tegracam_device *tc_dev = priv->tc_dev;
	struct device *dev = tc_dev->dev;
	char* buf_t,*tmp;
	int i = 0;

	dev_info(dev, "%s: User write requested, ascii representation = %s \n",
			__func__, buf);

    tmp = kstrdup(buf, GFP_KERNEL);
    if (!tmp) {
        return -ENOMEM;
    }
	buf_t = strsep(&tmp, " ");

    while (buf_t != NULL) {
		if(i==0)
			sscanf(buf_t,"%x",&(priv->register_));
		else if(i==1)
			sscanf(buf_t,"%x",&(priv->value_));
		else 
			dev_info(dev, "%s: too much parameter \n",
			__func__);		
		buf_t = strsep(&tmp, " ");
		i++;
	}
	
	kfree(tmp);

	if(i==2)
		zedxhdr_write_reg(priv->s_data,(u16)priv->register_,(u8)priv->value_);

	return count;
}

/**
 * All the struct necessary to declare a new entry in
 * the targeted sysfs directory:
 * /sys/class/video4linux/videoX/zed_debug/sensor_regs
 */
static struct sysfs_ops zedxhdr_sensor_sysfs_ops = {
    .show = sysfs_regs_show,
    .store = sysfs_regs_store,
};

static struct attribute zedxhdr_sensor_flash_attribute = {
    .name = "sensor_regs",
    .mode = 0666, /* all users can set the test mode, without issues */
};

static struct attribute *zedxhdr_sensor_attrs[] = {
    &zedxhdr_sensor_flash_attribute,
    NULL,
};

/**
 * This kobj_type is used in the probe function of the driver
 * to create the sysfs entry
 */
static struct kobj_type zedxhdr_sensor_kobj_type = {
	.sysfs_ops = &zedxhdr_sensor_sysfs_ops,
	.default_attrs = zedxhdr_sensor_attrs,
};

#endif
#endif