# RAOP Core Component for ESP-IDF

idf_component_register(
    SRCS
        "raop.c"
        "raop_sink.c"
        "rtp.c"
        "dmap_parser.c"
        "util.c"
        "audio_buffer.c"
    INCLUDE_DIRS
        "."
        "codecs/alac"
    REQUIRES
        esp_timer
        nvs_flash
        lwip
        mbedtls
    PRIV_REQUIRES
        driver
)

# Link the pre-compiled ALAC library
target_link_libraries(${COMPONENT_LIB} INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/codecs/alac/libalac.a"
)