cmake_minimum_required(VERSION 3.16)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(ws2812_smoke_test)

idf_build_get_property(python PYTHON)

set(MERGED_BIN ${CMAKE_BINARY_DIR}/${CMAKE_PROJECT_NAME}-merged.bin)

add_custom_target(merged_bin
	COMMAND ${python} -m esptool
		--chip esp32s3
		merge_bin
		--output ${MERGED_BIN}
		--flash_mode ${CONFIG_ESPTOOLPY_FLASHMODE}
		--flash_size ${CONFIG_ESPTOOLPY_FLASHSIZE}
		--flash_freq ${CONFIG_ESPTOOLPY_FLASHFREQ}
		0x0 ${CMAKE_BINARY_DIR}/bootloader/bootloader.bin
		0x8000 ${CMAKE_BINARY_DIR}/partition_table/partition-table.bin
		0x10000 ${CMAKE_BINARY_DIR}/${CMAKE_PROJECT_NAME}.bin
	DEPENDS app bootloader partition_table
	COMMENT "Generating merged binary: ${CMAKE_PROJECT_NAME}-merged.bin"
	VERBATIM
)