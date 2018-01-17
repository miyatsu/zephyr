#!/bin/bash

mcuboot_path="../../../mcuboot";
firmware_path="build/stm32f4_explo/zephyr";
firmware_version=1

output_file_name="${firmware_path}/vrbox_v${firmware_version}_debug.bin";

./${mcuboot_path}/scripts/imgtool.py sign \
	--key ${mcuboot_path}/root-ec-p256.pem \
	--header-size 0x200 \
	--align 8 \
	--version ${firmware_version} \
	--included-header \
	${firmware_path}/zephyr.bin ${firmware_path}/vrbox_v0.0.1_debug.bin

echo "Write signed firmware at: ${firmware_path}/vrbox_v0.0.1_debug.bin"

