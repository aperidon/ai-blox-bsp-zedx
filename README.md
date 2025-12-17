# AI-Blox BSP ZEDX

AI-Blox BSP ZEDX

L4T Kernel source for AI-Blox box and ZED-X compatibility


## Compatibility
Compatible with :
- AGX Orin L4T36.4.0

## AI-Blox kernel modification required 

In order for the camera to work on the AIBP0042, few modifications has to be implemented on hardware and firmware side : 
- SCL / SDA mix-up on the deserializer (fix procedure described by Frédéric)
- HDMI serializer address : Change address by modifying the resistance that drive ADDR0, ADDR1, ADDR2 pins of the MAX96751. We switched it so that the serializer appears at 0x64 on i2c-1 instead of 0x42
- Device tree to match up HDMI serializer new address (0x42 -> 0x64)


## Build L4T with GMSL support

```
$ sh prepare.sh
$ sh package.sh
```

Note : Prepare must only do once. It is used to install the cross compiler<br/>


## DEB output
package.sh will generate automatically the deb file associated with the kernel/drivers changes. It will be created under the name 
`ai-blox-zedx_1.3.0-MAX96724-L4T36.4.0_arm64.deb` at the root of the repository. <br/>
This is recommended to use the deb package for driver installation as it install the zed_x_daemon used by the ZED SDK to control sanity of cameras<br/>

## DEB Dependencies : 
`$ sudo apt install libqt5core5a zstd`

## ZED SDK Compatibility
The driver is compatible with the latest ZED SDK official release available here : <br/>
https://www.stereolabs.com/developers/release#82af3640d775

## Demo 

After plugging in the camera, you can run `sudo systemctl restart zed_x_daemon.service` to restart the driver. This procedure is done automatically at boot
You can then use various tools to display the camera streams : 
- `argus_camera` : Select multi-session in the top-right corner drop menu to display all streams at once
- `ZED_Media_Server` : Will display all streams from the ZED SDK (might need you to enter board pwd)