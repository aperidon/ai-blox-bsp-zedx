# AI-Blox BSP ZEDX

AI-Blox BSP ZEDX

L4T Kernel source for AI-Blox box and ZED-X compatibility


## Compatibility
Compatible with :
- AGX Orin L4T36.4.0


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

