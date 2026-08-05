## Release Notes

##### v1.4.3
- Add new v4l2 controls gate_fps and mode preventing unwanted dephasing in acquisition

##### v1.4.2
- Add new v4l2 control for offline calibration
- Add bandwidth error flag to sysfs
- Fix IMU stalling for |delta| seconds after a backward `clock_settime()` on `CLOCK_REALTIME` (NTP/PTP step or manual date change): the `bmi_spsc` hrtimer was bound to `CLOCK_REALTIME` while its absolute deadline was computed from `ktime_get_ns()` (monotonic); rebind the timer to `CLOCK_MONOTONIC` so the two domains match and the sample cadence is immune to wall-clock jumps.

##### v1.4.1
- Add compatibility with JP6.2.2
- Add compatibility with JP7.1
- Fix an issue preventing the usage of an external trigger
- Fix an issue that could reintroduce a faulty ISP library
- Fix a compatibility issue in the IMU driver for JP7
- Fix an issue leading to high noise level in HDR mode for the ZED ONE UHD 

##### v1.4.0
- Improved jitter and drops through kernel driver for the IMU.
- Add configurable parameters through devicetree : 
    - Sensor's control channel
    - Sensor's output csi port
    - Deserializer external trigger MFP
- Devicetree refactoring 
- Add link lock value to sysfs
- Add status value to sysfs
- Rewrite ISX Nor Flash only if necessary
- Dmesg refactoring
- Rework camera piping on MAX96712 to enable more configurations
- Update mipi-csi speed on MAX96712, enable 1 ZED X and 1 ZED X One GS streaming HD1200@60fps on same opt csi port
- Add ZED X HDR cropping modes
- Fix exposure out of scope values
- Set ZEDLink Quad both i2c's clock-frequencie at 400kHz
- Remove QT dependency
- Add Nvidia's 36.4 patches

##### v1.3.2
- Update Debian Control files for future Jetpack updates compatibility

##### v1.3.1
- Support RT-Kernel
- Multi-deserializers synchronization
- Multi ZED Box Mini synchronization
- Support ZED X & ZED X One 4K on ZED Box Mini
- Support 2 ZEDLink Mono on Orin Nano Devkit
- Fix bug opening 960x600
- Fix error writing eeprom 9296
- Fix Xavier support on L4T35.2
- Support L4T36.4.4
- Improved stability
- Add optional VIC settings
- Add support for 3Gbps one hdr

##### v1.3.0
- Dynamic I2C address allocation
- Dynamic video pipe configuration 
- Support ZED X HDR, ZED X One HDR
- Support ZED Box Mini
- Update camera metadata sysfs
- Add ioctl control for eeprom
- Add CSI/VI/ISP clock boosting at daemon start

##### v1.2.2
- Support JP6.2
- Add compatibility for Xavier platforms
- Fix ZED X detection error on ZED Box NX8 and Nano devkit on JP 6.0 (36.3) with capture card mono
- Fix HDR on ZED One 4K
- Added a camera metadata sysfs file on host that is retrieved by the capture software instead of direct I2C

##### v1.2.1
- Adding support for ZED X Pro on all platforms
- Adding support for JP6.1

##### v1.2.0
- Sysfs entries for several functions.
- Simplification of device tree management for JP5 and JP4.
- Fully separating SL modules and Nvidia OOT modules for JP6 compilation.
- Lessen driver kernel logs.

##### v1.1.1
- Get the serial number from the eeprom in the driver

##### v1.1.0
- Add support for overlays
- Switch MAX9296 serializer to 2 lanes for the ZED X One 4K
- Add support for multiple simultaneous camera model
- Add support for multiple cameras in one dtbo
- Add support for HDR ZED One 4K: 15FPS QHD+ and 30FPS 1200p
- Add support for 60fps for 1200p/1080p and 25 FPS 4K
- Add support for ZED Box in R36.3

##### v1.0.5
- Add support for L4T R36.3
- Minor reorganization of the repository
- Change compilation order of the build.sh script

##### v1.0.4
- Fix ZED X One 4K exposure

##### v1.0.3
- integrate ZED X HDR

##### v1.0.2
- integrate ZED Box with rev4.1 board

##### v1.0.1
- Patch remove HMAX modif in binning mode

##### v1.0.0
- Internal Rework : Separate Deser and Ser tables for more flexibility in DTS 
- Add support of ZED-XOne GS (ar0234) / ZED-XOne 4K (imx678)
- Add support of ZEDLink-MONO (for ZED-X, ZED-X One GS/4K)
- Add support of ZEDLink-QUAD (for 4 ZED-X or 4 ZED-X One GS/4K)

## Note: next release will be a major version (1.0.0) with significant changes
##### v0.6.5
- Modify ISP configuration for better Exposure/Gain ratio. This will limit motion blur when camera is vibrating
- Add module version in modinfo output. 

##### v0.6.4
- Fix exposure and gain computation functions.
- Prepare ground for manual trigger possibility.

##### v0.6.3
- Activate chroma denoising in ISP
- Change ratio Exposure/gain in ISP to give more space for gain adjustment.

##### v0.6.2
- Integration of Jetpack 6.0 Developer Preview : internal only

##### v0.6.1
- Integrate ZEDBox in 35.4

##### v0.6.0: minor release
- Change the repository structure to facilitate support of multiple Jetpack versions 
- Add patching mechanism to support external carrier boards
- Add patching mechanism to keep track of recurent internal changes for L4T BSP.
- Change naming of max96712 and max9296 modules : switch to sl_max convention

##### v0.5.9
- Add support for Jetpack 5.1.2

##### v0.5.8
- Add support for new carrier/module combo (devkit+nx, devkit+nano)

##### v0.5.7
- Add Module version in modinfo parameters to know the exact version
- Add sync calls in deb script calls to avoid file corruptions

##### v0.5.6
- Improve/Fix for extlinux.conf backup when installing driver. This could lead to corrupted exlinux.conf files
- Add preload/postload capabilities in ZED-X daemon. Using a custom /etc/systemd/system/zed_x_daemon.preload and/or /etc/systemd/system/zed_x_daemon.postload in a shell format allows to create custom commands
to be called before and after driver is loaded.

##### v0.5.5
- Add i2c deselect idle option in dts to improve support of multiple MAX9296 deserializer

##### v0.5.4
- Add 15FPS support in all sensor modes (HD1200,HD1080,SVGA). Requires ZED SDK >= 4.0.5

##### v0.5.3
- Fix MAX9296 detection (through i2c)
- Fix gpio activation for ZED-X power up
- Reduce ZED-X daemon dependencies to minimum required (libqt5core5a)
- Add NVIDIA Orin NX / Orin Nano for compatible jetpack 
- Add Seeed Studio J401 support through specific version

 
##### v0.5.2
- Fix L4T35.3 wrong dts

##### v0.5.1
- Add ZED-X Daemon in deb installer (removed from ZED SDK)
- Add support for L4T35.3
- Add rootfs support (through extlinux) on other devices than eMMC (SSD for example)


##### v0.5.0: minor release
- Fix MAX9296 Sync issue and FPS
- Small fixes on driver/dts
- Update build script to handle multiple configuration automatically
- Move DTS to stereolabs folder for clarification
- Add DTS for AGX Xavier, ZEDBox, NX DevKit
- Fix extlinux that choose the right dtb when deb is installed
- Add support for L4T 35.2
