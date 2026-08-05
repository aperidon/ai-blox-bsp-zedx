/*
 * imx678_.h - imx678 sensor header
 *
 * Copyright (C) 2023, Leopardimaging Inc.
 * Based on Copyright (c) 2021-2022, NVIDIA CORPORATION.  All rights reserved.
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

#ifndef __IMX678_H__
#define __IMX678_H__

/* imx678 - sensor parameters */

#define IMX678_MIN_FRAME_LENGTH		        (256)

/* imx678 sensor register address */

#define IMX678_FRAME_LENGTH_ADDR_MSB		0x6140
#define IMX678_FRAME_LENGTH_ADDR_LSB		0x6141

#define MAX_RADIAL_COEFFICIENTS 6
#define MAX_TANGENTIAL_COEFFICIENTS 2
#define MAX_FISHEYE_COEFFICIENTS 6

#define CAMERA_MAX_SN_LENGTH            32
#define MAX_RLS_COLOR_CHANNELS          4
#define MAX_RLS_BREAKPOINTS             6

#define IMX678_DRIVER_VERSION_MAJOR 1
#define IMX678_DRIVER_VERSION_MINOR 4
#define IMX678_DRIVER_VERSION_PATCH 3

#define N_IMX678 16

int verbosity_level = 0;

struct v4l2_ctrl *fm_find_v4l2_ctrl(struct tegracam_device *tc_dev, int ctrl_id);

#endif /* __IMX678_H__ */
