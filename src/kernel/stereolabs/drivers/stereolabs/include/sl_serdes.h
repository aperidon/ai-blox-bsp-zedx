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

#ifndef __SL_SERDES_H__
#define __SL_SERDES_H__

#include <linux/types.h>

/*
 * Cross-module API exported by the GMSL serializer (sl_max9295) and
 * deserializer (sl_max9296 / sl_max96712) modules. Sensor drivers must
 * include this header instead of re-declaring the functions.
 *
 * Builds that must load without the serdes modules (MIPI-attached
 * sensors) define SL_SERDES_OPTIONAL — set by CONFIG_SL_ZEDLINK_MIPI=y
 * in the top-level stereolabs Makefile (zedlink-mipi packages only): the
 * symbols below then become weak references that resolve to NULL when
 * the module is absent, and every call site must be guarded (e.g. by an
 * is_mipi check). All other builds keep strong references, so modprobe
 * enforces the serdes-before-sensor load order.
 */
#ifdef SL_SERDES_OPTIONAL
#define __sl_serdes_import __attribute__((weak))
#else
#define __sl_serdes_import
#endif

/* Deserializer API (sl_max9296 / sl_max96712) */
u32 __sl_serdes_import fps_set_Dser(int channel, s64 val);
int __sl_serdes_import fsync_set_Dser(int channel, bool on);
int __sl_serdes_import dser_enable_gmsl_link(int channel, int zedx_id);
int __sl_serdes_import dser_open_all_gmsl_link(int channel);
int __sl_serdes_import dser_get_gmsl_port(int channel, int zedx_id);
int __sl_serdes_import dser_read_link_lock(int channel, int zedx_id);
int __sl_serdes_import dser_read_video_lock(int channel, int zedx_id);
int __sl_serdes_import set_bitrate_Dser(int channel, u32 zedx_id, u8 val);

/* Serializer API (sl_max9295) */
int __sl_serdes_import set_bitrate_Ser(int zedx_id, u8 val);
int __sl_serdes_import fsync_set_Ser(int zedx_id, bool on);
int __sl_serdes_import ser_get_acc_addr(int zedx_id);
int __sl_serdes_import ser_get_gyro_addr(int zedx_id);
int __sl_serdes_import ser_read_video_conf(int zedx_id);

#endif /* __SL_SERDES_H__ */
