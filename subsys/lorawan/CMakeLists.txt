zephyr_link_interface(LoRaWAN)

zephyr_library()

zephyr_library_sources_ifdef(CONFIG_SX1272 radio/sx1272.c)
zephyr_library_sources_ifdef(CONFIG_SX1276 radio/sx1276.c)

zephyr_library_sources_ifdef(CONFIG_LORAWAN_CLASS_A class/class_a.c)
zephyr_library_sources_ifdef(CONFIG_LORAWAN_CLASS_B class/class_b.c)
zephyr_library_sources_ifdef(CONFIG_LORAWAN_CLASS_C class/class_c.c)

zephyr_library_link_libraries(LoRaWAN)
