# SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: BSD-3-Clause

KERNEL_HEADERS ?= /lib/modules/$(shell uname -r)/build
KERNEL_OUTPUT ?= ${KERNEL_HEADERS}

MAKEFILE_DIR := $(abspath $(shell dirname $(lastword $(MAKEFILE_LIST))))
NVIDIA_HEADERS ?= ${MAKEFILE_DIR}/out/nvidia-linux-header
NVIDIA_CONFTEST ?= ${MAKEFILE_DIR}/out/nvidia-conftest
SL_DRIVERS ?= ${MAKEFILE_DIR}/stereolabs/

ifneq ($(words $(subst :, ,$(MAKEFILE_DIR))), 1)
$(error source directory cannot contain spaces or colons)
endif

NPROC ?= $(shell nproc)

ifeq ("$(wildcard $(KERNEL_OUTPUT))","")
$(error kernel headers/output directory "$(KERNEL_OUTPUT)" does not exist!)
endif

.PHONY : help modules modules_install clean conftest nvidia-headers hwpm nvidia-oot nvgpu

# help is default target!
help:
	@echo   "================================================================================"
	@echo   "Usage:"
	@echo   "   make modules          # to build NVIDIA OOT and display drivers"
	@echo   "   make modules_oot      # to build SL OOT"
	@echo   "   make dtbs             # to build NVIDIA DTBs"
	@echo   "   make modules_install  # to install drivers to the INSTALL_MOD_PATH"
	@echo   "   make clean            # to make clean driver sources"
	@echo   "================================================================================"

modules: hwpm nvidia-oot stereolabs nvgpu nvidia-display
modules_oot: stereolabs
dtbs: nvidia-dtbs
modules_install: hwpm nvidia-oot nvgpu nvidia-display-install
clean: hwpm nvidia-oot nvgpu nvidia-display-clean nvidia-dtbs-clean


conftest:
ifeq ($(MAKECMDGOALS), modules)
	@echo   "================================================================================"
	@echo   "make $(MAKECMDGOALS) - conftest ..."
	@echo   "================================================================================"
	mkdir -p $(NVIDIA_CONFTEST)/nvidia;
	cp -av $(MAKEFILE_DIR)/nvidia-oot/scripts/conftest/* $(NVIDIA_CONFTEST)/nvidia/;
	$(MAKE) -j $(NPROC) ARCH=arm64 \
		src=$(NVIDIA_CONFTEST)/nvidia obj=$(NVIDIA_CONFTEST)/nvidia \
		CC=$(CROSS_COMPILE)gcc LD=$(CROSS_COMPILE)ld \
		NV_KERNEL_SOURCES=$(KERNEL_HEADERS) \
		NV_KERNEL_OUTPUT=$(KERNEL_OUTPUT) \
		-f $(NVIDIA_CONFTEST)/nvidia/Makefile
endif

hwpm: conftest
ifneq ($(MAKECMDGOALS), modules_oot)
	@if [ ! -d "$(MAKEFILE_DIR)/hwpm" ] ; then \
		echo "Directory hwpm is not found, exiting.."; \
		false; \
	fi
	@echo   "================================================================================"
	@echo   "make $(MAKECMDGOALS) - hwpm ..."
	@echo   "================================================================================"
	$(MAKE) -j $(NPROC) ARCH=arm64 \
		-C $(KERNEL_OUTPUT) \
		M=$(MAKEFILE_DIR)/hwpm/drivers/tegra/hwpm \
		CONFIG_TEGRA_OOT_MODULE=m \
		srctree.hwpm=$(MAKEFILE_DIR)/hwpm \
		srctree.nvconftest=$(NVIDIA_CONFTEST) \
		$(MAKECMDGOALS)
endif

nvidia-oot: conftest hwpm
ifneq ($(MAKECMDGOALS), modules_oot)
	@if [ ! -d "$(MAKEFILE_DIR)/nvidia-oot" ] ; then \
		echo "Directory nvidia-oot is not found, exiting.."; \
		false; \
	fi
	@echo   "================================================================================"
	@echo   "make $(MAKECMDGOALS) - nvidia-oot ..."
	@echo   "================================================================================"
	$(MAKE) -j $(NPROC) ARCH=arm64 \
		-C $(KERNEL_OUTPUT) \
		M=$(MAKEFILE_DIR)/nvidia-oot \
		CONFIG_TEGRA_OOT_MODULE=m \
		srctree.nvidia-oot=$(MAKEFILE_DIR)/nvidia-oot \
		srctree.hwpm=$(MAKEFILE_DIR)/hwpm \
		srctree.nvconftest=$(NVIDIA_CONFTEST) \
		KBUILD_EXTRA_SYMBOLS=$(MAKEFILE_DIR)/hwpm/drivers/tegra/hwpm/Module.symvers \
		$(MAKECMDGOALS)
endif

stereolabs: nvidia-headers
ifneq ($(MAKECMDGOALS), modules_oot)
	@if [ ! -d "$(MAKEFILE_DIR)/stereolabs" ] ; then \
		echo "Directory stereolabs is not found, exiting.."; \
		false; \
	fi
	@echo   "================================================================================"
	@echo   "make $(MAKECMDGOALS) - stereolabs ..."
	@echo   "================================================================================"
	$(MAKE) -j $(NPROC) ARCH=arm64 \
		-C $(KERNEL_OUTPUT) -C $(NVIDIA_HEADERS) \
		M=$(MAKEFILE_DIR)/stereolabs \
		CONFIG_TEGRA_OOT_MODULE=m \
		srctree.sl_drivers=$(SL_DRIVERS) \
		KBUILD_EXTRA_SYMBOLS=$(MAKEFILE_DIR)/hwpm/drivers/tegra/hwpm/Module.symvers \
		$(MAKECMDGOALS)
else
	@if [ ! -d "$(MAKEFILE_DIR)/stereolabs" ] ; then \
		echo "Directory stereolabs is not found, exiting.."; \
		false; \
	fi
	@if [ ! -d "$(NVIDIA_HEADERS)" ] ; then \
		echo "Kernel headers not found, exiting.. You must compile all the sources at least once!"; \
		false; \
	fi
	@echo   "================================================================================"
	@echo   "make $(MAKECMDGOALS) - stereolabs ..."
	@echo   "================================================================================"
	$(MAKE) -j $(NPROC) ARCH=arm64 \
		-C $(KERNEL_HEADERS) -C $(NVIDIA_HEADERS) \
		M=$(MAKEFILE_DIR)/stereolabs \
		CONFIG_TEGRA_OOT_MODULE=m \
		srctree.sl_drivers=$(SL_DRIVERS) \
		KBUILD_EXTRA_SYMBOLS=$(MAKEFILE_DIR)/hwpm/drivers/tegra/hwpm/Module.symvers \
		modules
endif

nvgpu: conftest nvidia-oot
	if [ ! -d "$(MAKEFILE_DIR)/nvgpu" ] ; then \
		echo "Directory nvgpu is not found, exiting.."; \
		false; \
	fi
	@echo   "================================================================================"
	@echo   "make $(MAKECMDGOALS) - nvgpu ..."
	@echo   "================================================================================"
	$(MAKE) -j $(NPROC) ARCH=arm64 \
		-C $(KERNEL_OUTPUT) \
		M=$(MAKEFILE_DIR)/nvgpu/drivers/gpu/nvgpu \
		CONFIG_TEGRA_OOT_MODULE=m \
		srctree.nvidia=$(MAKEFILE_DIR)/nvidia-oot \
		srctree.nvidia-oot=$(MAKEFILE_DIR)/nvidia-oot \
		srctree.nvconftest=$(NVIDIA_CONFTEST) \
		KBUILD_EXTRA_SYMBOLS=$(MAKEFILE_DIR)/nvidia-oot/Module.symvers \
		$(MAKECMDGOALS)


define display-cmd
	$(MAKE) -j $(NPROC) ARCH=arm64 TARGET_ARCH=aarch64 \
		-C $(MAKEFILE_DIR)/nvdisplay \
		LOCALVERSION=$(version) \
		NV_VERBOSE=0 \
		KERNELRELEASE="" \
		SYSSRC=$(NVIDIA_HEADERS) \
		SYSOUT=$(NVIDIA_HEADERS) \
		SYSSRCHOST1X=$(MAKEFILE_DIR)/nvidia-oot/drivers/gpu/host1x/include \
		CC=$(CROSS_COMPILE)gcc \
		LD=$(CROSS_COMPILE)ld.bfd \
		AR=$(CROSS_COMPILE)ar \
		CXX=$(CROSS_COMPILE)g++ \
		OBJCOPY=$(CROSS_COMPILE)objcopy
endef


nvidia-headers: nvidia-oot
ifneq ($(MAKECMDGOALS), modules_oot)
	mkdir -p $(NVIDIA_HEADERS)
	cp -LR $(KERNEL_HEADERS)/* $(NVIDIA_HEADERS)
	if [ "$(KERNEL_HEADERS)" != "$(KERNEL_OUTPUT)" ] ; then \
		cp -LR $(KERNEL_OUTPUT)/include/* $(NVIDIA_HEADERS)/include/ ; \
		cp -LR $(KERNEL_OUTPUT)/arch/arm64/include/* $(NVIDIA_HEADERS)/arch/arm64/include/ ; \
		cp -LR $(KERNEL_OUTPUT)/scripts/* $(NVIDIA_HEADERS)/scripts/ ; \
		cp $(KERNEL_OUTPUT)/System.map $(NVIDIA_HEADERS)/ || true ; \
	fi
	cp -LR $(MAKEFILE_DIR)/nvidia-oot/include/* $(NVIDIA_HEADERS)/include/
	cat $(KERNEL_OUTPUT)/Module.symvers $(MAKEFILE_DIR)/nvidia-oot/Module.symvers > \
		$(NVIDIA_HEADERS)/Module.symvers
endif


nvidia-display: nvidia-headers
	@if [ ! -d "$(MAKEFILE_DIR)/nvdisplay" ] ; then \
		echo "Directory nvdisplay is not found, exiting.."; \
		false; \
	fi
	@echo   "================================================================================"
	@echo   "make $(MAKECMDGOALS) - nvidia-display ..."
	@echo   "================================================================================"
	$(display-cmd) modules
	@echo   "================================================================================"
	@echo   "Display driver compiled successfully."
	@echo   "================================================================================"


nvidia-display-install:
	@echo   "================================================================================"
	@echo   "make $(MAKECMDGOALS) - nvidia-display ..."
	@echo   "================================================================================"
	$(MAKE) -j $(NPROC) -C $(NVIDIA_HEADERS) \
		M=$(MAKEFILE_DIR)/nvdisplay/kernel-open modules_install

nvidia-display-clean:
	@echo   "================================================================================"
	@echo   "make $(MAKECMDGOALS) - nvidia-display ..."
	@echo   "================================================================================"
	if [ -d $(NVIDIA_HEADERS) ]; then \
		$(display-cmd) clean && \
		rm -fr $(NVIDIA_HEADERS); \
	fi

nvidia-dtbs:
	@if [ ! -d "$(MAKEFILE_DIR)/kernel-devicetree" ] ; then \
		echo "Directory kernel-devicetree is not found, exiting.."; \
		false; \
	fi
	@echo   "================================================================================"
	@echo   "make nvidia-dtbs ..."
	@echo   "================================================================================"
	TEGRA_TOP=$(MAKEFILE_DIR) \
	srctree=$(KERNEL_HEADERS) \
	objtree=$(KERNEL_OUTPUT) \
	oottree=$(MAKEFILE_DIR)/kernel-devicetree \
	HOSTCC=gcc \
	$(MAKE) -j $(NPROC) -f $(MAKEFILE_DIR)/kernel-devicetree/scripts/Makefile.build \
		obj=$(MAKEFILE_DIR)/kernel-devicetree/generic-dts \
		dtbs
	@echo   "================================================================================"
	@echo   "DTBs compiled successfully."
	@echo   "================================================================================"

nvidia-dtbs-clean:
	@echo   "================================================================================"
	@echo   "make $(MAKECMDGOALS) - nvidia-dtbs ..."
	@echo   "================================================================================"
	rm -fr $(MAKEFILE_DIR)/kernel-devicetree/generic-dts/dtbs

