# SPDX-License-Identifier: GPL-2.0
# Copyright (c) 2022-2024, NVIDIA CORPORATION.  All rights reserved.

LINUXINCLUDE += -I$(srctree.nvconftest)
LINUXINCLUDE += -I$(srctree.nvidia-oot)/include

subdir-ccflags-y += -Werror
subdir-ccflags-y += -Wmissing-prototypes

ifeq ($(CONFIG_TEGRA_VIRTUALIZATION),y)
subdir-ccflags-y += -DCONFIG_TEGRA_VIRTUALIZATION
endif

ifeq ($(CONFIG_TEGRA_SYSTEM_TYPE_ACK),y)
subdir-ccflags-y += -DCONFIG_TEGRA_SYSTEM_TYPE_ACK
subdir-ccflags-y += -Wno-sometimes-uninitialized
subdir-ccflags-y += -Wno-parentheses-equality
subdir-ccflags-y += -Wno-enum-conversion
subdir-ccflags-y += -Wno-implicit-fallthrough
endif

export CONFIG_SL_DESER_MAX96724=m
export CONFIG_SL_SER_MAX9295=m
export CONFIG_SL_VIDEO_ZEDX=m
export CONFIG_SL_VIDEO_ZEDONE4K=m
export CONFIG_SL_VIDEO_ZEDXHDR=m
export CONFIG_TEGRA_STREAMERBOX_ADDR_FIX=n

ifeq ($(CONFIG_SL_DESER_MAX96712),m)
subdir-ccflags-y += -DCONFIG_SL_DESER_MAX96712
else ifeq ($(CONFIG_SL_DESER_MAX96724),m)
subdir-ccflags-y += -DCONFIG_SL_DESER_MAX96724
else
subdir-ccflags-y += -DCONFIG_SL_DESER_MAX9296
endif

ifeq ($(CONFIG_TEGRA_STREAMERBOX_ADDR_FIX),m)
subdir-ccflags-y += -DCONFIG_TEGRA_STREAMERBOX_ADDR_FIX
endif

obj-m += drivers/
