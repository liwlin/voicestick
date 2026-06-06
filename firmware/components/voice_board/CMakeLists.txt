set(VOICE_BOARD_SRCS "")

if(CONFIG_VOICESTICK_BOARD_M5STICKC_PLUS)
    list(APPEND VOICE_BOARD_SRCS "boards/m5stickc_plus/voice_board_m5stickc_plus.c")
else()
    list(APPEND VOICE_BOARD_SRCS "boards/stick_s3/voice_board_stick_s3.c")
endif()

idf_component_register(
    SRCS ${VOICE_BOARD_SRCS}
    INCLUDE_DIRS "include"
    REQUIRES driver esp_driver_i2c esp_driver_i2s
)
