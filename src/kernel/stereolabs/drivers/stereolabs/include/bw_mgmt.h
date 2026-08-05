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

#ifndef __BW_MGMT_H__
#define __BW_MGMT_H__

#include <linux/types.h>
#include <linux/device.h>
#include <linux/atomic.h>

/* __sl_serdes_import (weak on SL_SERDES_OPTIONAL builds), see sl_serdes.h */
#include "sl_serdes.h"

/* Bits-per-pixel for bandwidth calculations */
#define BPP      10  /* RAW10 sensors (zedx, zedone4k) */
#define BPP_HDR  16  /* HDR sensors (zedxhdr / ISX031) */

/* Maximum PHY outputs per deserializer chip (array ceiling, actual count in n_phy) */
#define MAX_PHY_PER_DESER 6

/* Bandwidth status codes */
#define BW_OK        0
#define BW_OVERFLOW  1

/**
 * dser_get_csi_group - Get CSI group/port for a camera
 * @channel: GMSL channel number
 * @zedx_id: Camera ID on the channel
 * @gmsl_port: GMSL port number
 *
 * Returns CSI physical port index (0-5), or -1 on error
 */
int __sl_serdes_import dser_get_csi_group(int channel, int zedx_id, int gmsl_port);

/**
 * sl_get_deser_priv - Get deserializer private data for a given channel
 * @channel: GMSL channel number
 *
 * Returns pointer to deserializer private data, or NULL if not found
 */
void * __sl_serdes_import sl_get_deser_priv(int channel);

/**
 * sl_update_camera_bw - Update camera's current bandwidth usage
 * @channel: GMSL channel number
 * @zedx_id: Camera ID on the channel
 * @cam_addr: Camera I2C address
 * @bps: Current bandwidth in bits per second (0 if not streaming)
 * @is_streaming: true if camera is actively streaming, false otherwise
 *
 * Updates the stored bandwidth usage for a camera. Called whenever
 * streaming state or FPS changes. Cameras calculate and report their own bps.
 *
 * Returns 0 on success, negative error code on failure
 */
int __sl_serdes_import sl_update_camera_bw(int channel, int zedx_id, u32 cam_addr, u64 bps, bool is_streaming);


/* Return the summed bandwidth (bits per second) for a CSI group. */
u64 __sl_serdes_import sl_get_bw_usage(void *priv, int csi_group);


/**
 * sl_get_bw_status - Get current bandwidth status for a CSI group
 * @dev: Device pointer for logging
 * @priv: Deserializer private data
 * @csi_group: CSI group index
 *
 * Returns BW_OK or BW_OVERFLOW
 */
int __sl_serdes_import sl_get_bw_status(struct device *dev, void *priv, int csi_group);

#endif /* __BW_MGMT_H__ */
