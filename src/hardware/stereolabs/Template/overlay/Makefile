# SPDX-License-Identifier: GPL-2.0-only
# SPDX-FileCopyrightText: Copyright (c) 2023, NVIDIA CORPORATION & AFFILIATES.  All rights reserved.

DTC_FLAGS += -@
#DTC_CPP_FLAGS += -DBUILDOVERLAY

old-dtb := $(dtb-y)
old-dtbo := $(dtbo-y)
dtb-y :=
dtbo-y :=
makefile-path := t23x/nv-public/overlay

# Orin AGX
dtbo-y += tegra234-p3737-camera-zedlink-duo-sl-overlay.dtbo
dtbo-y += tegra234-p3737-camera-zedlink-quad-sl-overlay.dtbo

# Orin Nano
## Devkit
dtbo-y += tegra234-p3768-camera-zedlink-duo-sl-overlay.dtbo
dtbo-y += tegra234-p3768-camera-zedlink-mono-sl-overlay.dtbo

## Zedbox
dtbo-y += tegra234-p3768-camera-zedbox-mono-sl-overlay.dtbo
dtbo-y += tegra234-p3768-camera-zedbox-duo-sl-overlay.dtbo
dtbo-y += tegra234-p3768-camera-zedbox-mini-sl-overlay.dtbo

ifneq ($(dtb-y),)
dtb-y := $(addprefix $(makefile-path)/,$(dtb-y))
endif
ifneq ($(dtbo-y),)
dtbo-y := $(addprefix $(makefile-path)/,$(dtbo-y))
endif

dtb-y += $(old-dtb)
dtbo-y += $(old-dtbo)
