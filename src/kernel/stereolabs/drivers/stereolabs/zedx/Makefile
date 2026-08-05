subdir-ccflags-y += -Werror

ccflags-y += -I$(srctree)/drivers/media/platform/tegra
ccflags-y += -I$(srctree)/drivers/video/tegra/camera

obj-$(CONFIG_SL_VIDEO_ZEDX) += sl_zedx.o

# MIPI (zedlink-mipi) packages must load this driver without the GMSL
# serdes modules: import their symbols as weak references (see
# ../include/sl_serdes.h). Scoped to this driver only — the other sensor
# drivers have no is_mipi guards and must keep strong references.
ifeq ($(CONFIG_SL_ZEDLINK_MIPI),y)
ccflags-y += -DSL_SERDES_OPTIONAL
endif
