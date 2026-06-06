#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/i2s_types.h"
#include "driver/spi_master.h"
#include "esp_err.h"

typedef enum {
    VOICE_BOARD_BACKLIGHT_PWM,
    VOICE_BOARD_BACKLIGHT_BOARD,
} voice_board_backlight_mode_t;

typedef enum {
    VOICE_BOARD_AUDIO_ES8311,
    VOICE_BOARD_AUDIO_PDM,
} voice_board_audio_kind_t;

typedef struct {
    spi_host_device_t spi_host;
    uint16_t h_res;
    uint16_t v_res;
    uint16_t x_gap;
    uint16_t y_gap;
    gpio_num_t mosi_gpio;
    gpio_num_t sclk_gpio;
    gpio_num_t dc_gpio;
    gpio_num_t cs_gpio;
    gpio_num_t reset_gpio;
    gpio_num_t backlight_gpio;
    voice_board_backlight_mode_t backlight_mode;
    bool invert_color;
    bool mirror_x;
    bool mirror_y;
} voice_board_lcd_config_t;

typedef struct {
    voice_board_audio_kind_t kind;
    i2s_port_t i2s_port;
    gpio_num_t mclk_gpio;
    gpio_num_t bclk_gpio;
    gpio_num_t ws_gpio;
    gpio_num_t dout_gpio;
    gpio_num_t din_gpio;
    gpio_num_t pdm_clk_gpio;
    gpio_num_t pdm_din_gpio;
} voice_board_audio_config_t;

esp_err_t voice_board_init(void);
const char *voice_board_hardware_id(void);
const char *voice_board_display_name(void);
i2c_master_bus_handle_t voice_board_i2c_bus(void);
const voice_board_lcd_config_t *voice_board_lcd_config(void);
const voice_board_audio_config_t *voice_board_audio_config(void);
gpio_num_t voice_board_primary_button_gpio(void);
gpio_num_t voice_board_secondary_button_gpio(void);
gpio_num_t voice_board_power_irq_gpio(void);
gpio_num_t voice_board_deep_sleep_wake_gpio(void);
bool voice_board_buttons_use_internal_pullups(void);
bool voice_board_primary_button_pressed(void);
bool voice_board_secondary_button_pressed(void);
esp_err_t voice_board_set_lcd_brightness(uint8_t brightness);
esp_err_t voice_board_battery_voltage_mv(int *voltage_mv);
esp_err_t voice_board_vbus_voltage_mv(int *voltage_mv);
esp_err_t voice_board_battery_level(int *level_percent);
esp_err_t voice_board_battery_charging(bool *charging);
esp_err_t voice_board_usb_powered(bool *usb_powered);
esp_err_t voice_board_clear_power_irqs(uint8_t *sys_status);
void voice_board_prepare_deep_sleep(void);
