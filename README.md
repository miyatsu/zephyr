***
# Requirement analysis #
**Be aware**: This file is not the original README file and created by **Shenzhen Trylong Intelligence Technology Co., Ltd.**
**Note**: The original README file is **README.rst**.
## Porting zephyr to board **ITC PolyU HK** ##
### **ITC PolyU HK** board spec ###
* Nordic Semiconductor nRF51822_QFAA SoC
	* CPU: ARM Cortex-M0
	* RAM: 16KB SRAM
	* ROM: 256KB NOR Flash
* MPU6050
* etc.
## Support device firmware upgrade over the air ##
- We can not ensure that the device we released is 100% perfect. Once a firmware bug is detected, we can use OTA technology to upgrade the firmware without recall the product.
- This chapter's README.md should be find in *[here](https://github.com/miyatsu/mcuboot/boot/zephyr/README.md "Bootloader")*.
***
# Porting zephyr #
Zephyr source code tree (some directory or file are ommited).
```
.
|--arch		/* CPU arch spec related */
|--boards	/* Onboard interface definition */
|  `--arm
|     |--nrf51_pca10028	/* Nordic nRF51 series default board name */
|     `--nrf52_pca10040	/* Nordic nRF52 series default board name */
|--doc		/* Develop documentation */
|--drivers	/* Hardware drivers like I2C, SPI, Flash etc. */
|--dts		/* DTS file, on board resource definition */
|  `--arm
|     |--nordic
|     |  |--nrf51822.dtsi
|     |  |--nrf52832.dtsi
|     |  `--nrf52840.dtsi
|     |--nrf51_pca10028.dts
|     |--nrf51_pca10028.fixup
|     `--Makefile
|--ext		/* Extra kernel feature like file system etc. */
|--include	/* Header files */
|--kernel	/* Zephyr kernel */
|--lib		/* Minimal C standard lib */
|--samples	/* Sample code of all project features */
|  |--basic
|  |  `--blinky
|  `--blutooth
|     `--peripheral_dis
|--scripts	/* CONFIG_* auto generation */
|  `--extract_dts_includes.py	/* Parse dts file, generate CONFIG_* definition */
|--subsys	/* Netword stack implementation */
`--tests	/* Functionality auto test scripts */
```
The source code of Zephyr indicates that the Zephyr currently support both Nordic Semiconductor nRF51 and nRF52 series SoC.

According to the support board list, we can find board **nRF51-PCA10028** at *[here](https://www.zephyrproject.org/doc/boards/arm/nrf51_pca10028/doc/nrf51_pca10028.html "nRF51-PCA10028")*. Then we have successful running the sample code in **$(ZEPHYR_BASE)/samples/basic/blinky** on board **ITC PolyU HK** by following instruction of *[Set Up the Development Environment](https://www.zephyrproject.org/doc/getting_started/getting_started.html "Getting Started Guide").*

## Add an new board ##
The source file has a good organizational structure, all board related file are all in **$(ZEPHYR_BASE)/boards** and the board resource definition are in **$(ZEPHYR_BASE)/dts**. The commit log can be find at *[https://github.com/miyatsu/zephyr/commit/0712a88104c670b6147f44a95b0a690063d10ecc](https://github.com/miyatsu/zephyr/commit/0712a88104c670b6147f44a95b0a690063d10ecc "nrf_polyu")*.

The files in **$(ZEPHYR_BASE)/boards/arm/nrf_polyu** are all copied from **$(ZEPHYR_BASE)/boards/arm/nrf_pca10028**

## Bluetooth sample test on BOARD nrf_polyu ##
We test some of the project in **$(ZEPHYR_BASE)/samples/bluetooth**. Some of them are running just fine and its broadcast information can be recived by cell phones. Here are some test result.

| Sample name    |  Result |
| -------------- |:-------:|
| beacon         | Success |
| eddystone      | Success |
| peripheral_dis | Success |
| peripheral     | Failed  |
| peripheral_csc | Failed  |
| peripheral_hr  | Failed  |
| peripheral_hids| Failed  |

By compare those project config, we find that a project once have **CONFIG_BLUETOOTH_LOG** config enable will not gonna runs normaly. The reason that cause this bug is still unknown by now.

For connectivty test purpose, we create a new project in **$(ZEPHYR_BASE)/samples/bluetooth/peripheral_uart**. The commit log can be find at *[https://github.com/miyatsu/zephyr/commit/0781d4cb2d4ce7c91bbc5ea0e2012624c1dcc642](https://github.com/miyatsu/zephyr/commit/0781d4cb2d4ce7c91bbc5ea0e2012624c1dcc642 "peripheral_uart")*.
***