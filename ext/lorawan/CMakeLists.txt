zephyr_interface_library_named(LoRaWAN)

# Add include directories for componets that link with LoRaWAN
# For example:	subsys/lorawan/
#				drivers/lorawan/radio
target_include_directories(LoRaWAN INTERFACE
	mac
	mac/region
	system
	system/crypto
	board
	radio
	radio/sx1272
	radio/sx1276
	)

zephyr_library()

# Add include directories for the ext/lorawan/ files
zephyr_library_include_directories(
	mac
	mac/region
	system
	system/crypto
	board
	radio
	radio/sx1272
	radio/sx1276
	)

zephyr_library_sources(
	mac/LoRaMac.c
	mac/LoRaMacCrypto.c
	mac/region/Region.c
	mac/region/RegionCommon.c
	)
zephyr_library_sources(
	system/utilities.c
	system/timer.c
	system/delay.c
	system/crypto/aes.c
	system/crypto/cmac.c
	)
zephyr_library_sources_ifdef(CONFIG_SX1272 radio/sx1272/sx1272.c)
zephyr_library_sources_ifdef(CONFIG_SX1276 radio/sx1276/sx1276.c)


if(CONFIG_REGION_AS923)
	zephyr_library_sources(mac/region/RegionAS923.c)
	add_definitions(-DREGION_AS923)
elseif(CONFIG_REGION_AS915)
	zephyr_library_sources(mac/region/RegionAS915.c)
	add_definitions(-DREGION_AS915)
elseif(CONFIG_REGION_CN470)
	zephyr_library_sources(mac/region/RegionCN470.c)
	add_definitions(-DREGION_CN470)
elseif(CONFIG_REGION_CN779)
	zephyr_library_sources(mac/region/RegionCN779.c)
	add_definitions(-DREGION_CN779)
elseif(CONFIG_REGION_EU433)
	zephyr_library_sources(mac/region/RegionEU433.c)
	add_definitions(-DREGION_EU433)
elseif(CONFIG_REGION_EU868)
	zephyr_library_sources(mac/region/RegionEU868,c)
	add_definitions(-DREGION_EU868)
elseif(CONFIG_REGION_IN865)
	zephyr_library_sources(mac/region/RegionIN865.c)
	add_definitions(-DREGION_IN865)
elseif(CONFIG_REGION_KR920)
	zephyr_library_sources(mac/region/RegionKR920.c)
	add_definitions(-DREGION_KR920)
elseif(CONFIG_REGION_US915)
	zephyr_library_sources(mac/region/RegionUS915.c)
	add_definitions(-DREGION_US915)
elseif(CONFIG_REGION_US915_HYBRID)
	zephyr_library_sources(mac/region/RegionUS915-Hybird.c)
	add_definitions(-DREGION_US915_HYBRID)
else()
	message("Warning: Must define a supported LoRaWAN radio frequency!!")
endif()

zephyr_library_link_libraries(LoRaWAN)
target_link_libraries(LoRaWAN INTERFACE zephyr_interface)
