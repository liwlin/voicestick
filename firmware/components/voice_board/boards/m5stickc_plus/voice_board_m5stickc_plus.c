#include "voice_board.h"

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "esp_check.h"
#include "esp_log.h"

static const char *TAG = "voice_board_m5stickc_plus";
static i2c_master_bus_handle_t s_i2c_bus;
static i2c_master_dev_handle_t s_axp_dev;

#define M5STICKC_PLUS_PIN_BUTTON_A 37
#define M5STICKC_PLUS_PIN_BUTTON_B 39

#define M5STICKC_PLUS_PIN_I2C_SCL 22
#define M5STICKC_PLUS_PIN_I2C_SDA 21

#define M5STICKC_PLUS_PIN_LCD_MOSI 15
#define M5STICKC_PLUS_PIN_LCD_SCK  13
#define M5STICKC_PLUS_PIN_LCD_DC   23
#define M5STICKC_PLUS_PIN_LCD_CS   5
#define M5STICKC_PLUS_PIN_LCD_RST  18

#define M5STICKC_PLUS_PIN_MIC_CLK  0
#define M5STICKC_PLUS_PIN_MIC_DATA 34

#define M5STICKC_PLUS_I2C_FREQ_HZ 400000

#define AXP192_ADDR 0x34
#define AXP192_REG_POWER_STATUS 0x00
#define AXP192_REG_CHARGE_STATUS 0x01
#define AXP192_REG_OUTPUT_CTRL 0x12
#define AXP192_REG_VBUS_VOLTAGE 0x5a
#define AXP192_REG_BAT_POWER 0x70
#define AXP192_REG_BAT_VOLTAGE 0x78
#define AXP192_REG_CHARGE_CURRENT 0x7a
#define AXP192_REG_DISCHARGE_CURRENT 0x7c
#define AXP192_REG_LDO23_VOLTAGE 0x28

#define AXP192_OUTPUT_EXTEN  BIT(6)
#define AXP192_OUTPUT_LDO3   BIT(3)
#define AXP192_OUTPUT_LDO2   BIT(2)
#define AXP192_OUTPUT_DCDC1  BIT(0)

#define AXP192_LCD_OUTPUTS (AXP192_OUTPUT_EXTEN | AXP192_OUTPUT_LDO3 | \
                            AXP192_OUTPUT_LDO2 | AXP192_OUTPUT_DCDC1)
#define AXP192_LDO_CODE_MIN 7
#define AXP192_LDO_CODE_MAX 14

static const voice_board_lcd_config_t s_lcd_config = {
    .spi_host = SPI2_HOST,
    .h_res = 135,
    .v_res = 240,
    .x_gap = 52,
    .y_gap = 40,
    .mosi_gpio = M5STICKC_PLUS_PIN_LCD_MOSI,
    .sclk_gpio = M5STICKC_PLUS_PIN_LCD_SCK,
    .dc_gpio = M5STICKC_PLUS_PIN_LCD_DC,
    .cs_gpio = M5STICKC_PLUS_PIN_LCD_CS,
    .reset_gpio = M5STICKC_PLUS_PIN_LCD_RST,
    .backlight_gpio = GPIO_NUM_NC,
    .backlight_mode = VOICE_BOARD_BACKLIGHT_BOARD,
    .invert_color = true,
    .mirror_x = false,
    .mirror_y = false,
};

static const voice_board_audio_config_t s_audio_config = {
    .kind = VOICE_BOARD_AUDIO_PDM,
    .i2s_port = I2S_NUM_0,
    .mclk_gpio = GPIO_NUM_NC,
    .bclk_gpio = GPIO_NUM_NC,
    .ws_gpio = GPIO_NUM_NC,
    .dout_gpio = GPIO_NUM_NC,
    .din_gpio = GPIO_NUM_NC,
    .pdm_clk_gpio = M5STICKC_PLUS_PIN_MIC_CLK,
    .pdm_din_gpio = M5STICKC_PLUS_PIN_MIC_DATA,
};

static bool read_active_low_button(gpio_num_t pin)
{
    return gpio_get_level(pin) == 0;
}

static esp_err_t axp_write_reg(uint8_t reg, uint8_t value)
{
    const uint8_t data[] = {reg, value};
    return i2c_master_transmit(s_axp_dev, data, sizeof(data), 100);
}

static esp_err_t axp_read_reg(uint8_t reg, uint8_t *value)
{
    return i2c_master_transmit_receive(s_axp_dev, &reg, 1, value, 1, 100);
}

static esp_err_t axp_read_regs(uint8_t reg, uint8_t *data, size_t len)
{
    return i2c_master_transmit_receive(s_axp_dev, &reg, 1, data, len, 100);
}

static esp_err_t axp_update_reg(uint8_t reg, uint8_t clear_mask, uint8_t set_mask)
{
    uint8_t value = 0;
    esp_err_t err = axp_read_reg(reg, &value);
    if (err != ESP_OK) {
        return err;
    }

    value &= ~clear_mask;
    value |= set_mask;
    return axp_write_reg(reg, value);
}

static uint8_t lcd_brightness_to_ldo2_code(uint8_t brightness)
{
    if (brightness == 0) {
        return 0;
    }

    /*
     * M5StickC Plus controls the TFT backlight from AXP192 LDO2. Match the
     * official ScreenBreath() voltage band, but saturate at the UI's normal
     * active value so the default screen state is visibly on.
     */
    uint16_t scaled = brightness;
    if (scaled > 128) {
        scaled = 128;
    }
    return AXP192_LDO_CODE_MIN +
           (uint8_t)((scaled * (AXP192_LDO_CODE_MAX - AXP192_LDO_CODE_MIN)) / 128);
}

static esp_err_t axp_read_12bit(uint8_t reg, uint16_t *value)
{
    if (!value) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t data[2] = {0};
    esp_err_t err = axp_read_regs(reg, data, sizeof(data));
    if (err != ESP_OK) {
        return err;
    }

    *value = ((uint16_t)data[0] << 4) | (data[1] & 0x0f);
    return ESP_OK;
}

static esp_err_t axp_read_13bit(uint8_t reg, uint16_t *value)
{
    if (!value) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t data[2] = {0};
    esp_err_t err = axp_read_regs(reg, data, sizeof(data));
    if (err != ESP_OK) {
        return err;
    }

    *value = ((uint16_t)data[0] << 5) | (data[1] & 0x1f);
    return ESP_OK;
}

static esp_err_t init_i2c(void)
{
    const i2c_master_bus_config_t bus_config = {
        .i2c_port = I2C_NUM_0,
        .sda_io_num = M5STICKC_PLUS_PIN_I2C_SDA,
        .scl_io_num = M5STICKC_PLUS_PIN_I2C_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    esp_err_t err = i2c_new_master_bus(&bus_config, &s_i2c_bus);
    if (err != ESP_OK) {
        return err;
    }

    const i2c_device_config_t dev_config = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = AXP192_ADDR,
        .scl_speed_hz = M5STICKC_PLUS_I2C_FREQ_HZ,
    };
    return i2c_master_bus_add_device(s_i2c_bus, &dev_config, &s_axp_dev);
}

static void init_axp192(void)
{
    uint8_t power_status = 0;
    esp_err_t err = axp_read_reg(AXP192_REG_POWER_STATUS, &power_status);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "AXP192 not found at 0x%02x: %s", AXP192_ADDR, esp_err_to_name(err));
        return;
    }

    ESP_ERROR_CHECK_WITHOUT_ABORT(axp_write_reg(0x28, 0xcc));
    ESP_ERROR_CHECK_WITHOUT_ABORT(axp_write_reg(0x82, 0xff));
    ESP_ERROR_CHECK_WITHOUT_ABORT(axp_write_reg(0x33, 0xc0));
    ESP_ERROR_CHECK_WITHOUT_ABORT(axp_update_reg(0x12, 0x00, AXP192_LCD_OUTPUTS));
    ESP_ERROR_CHECK_WITHOUT_ABORT(axp_write_reg(0x36, 0x0c));
    ESP_ERROR_CHECK_WITHOUT_ABORT(axp_write_reg(0x91, 0xf0));
    ESP_ERROR_CHECK_WITHOUT_ABORT(axp_write_reg(0x90, 0x02));
    ESP_ERROR_CHECK_WITHOUT_ABORT(axp_write_reg(0x30, 0x80));
    ESP_ERROR_CHECK_WITHOUT_ABORT(axp_write_reg(0x39, 0xfc));
    ESP_ERROR_CHECK_WITHOUT_ABORT(axp_write_reg(0x35, 0xa2));
    ESP_ERROR_CHECK_WITHOUT_ABORT(axp_write_reg(0x32, 0x46));
    ESP_ERROR_CHECK_WITHOUT_ABORT(voice_board_set_lcd_brightness(128));

    uint8_t output_ctrl = 0;
    uint8_t ldo23 = 0;
    (void)axp_read_reg(AXP192_REG_OUTPUT_CTRL, &output_ctrl);
    (void)axp_read_reg(AXP192_REG_LDO23_VOLTAGE, &ldo23);
    ESP_LOGI(TAG, "AXP192 initialized status=0x%02x output=0x%02x ldo23=0x%02x",
             power_status, output_ctrl, ldo23);
}

esp_err_t voice_board_init(void)
{
    esp_err_t err = init_i2c();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "I2C init failed: %s", esp_err_to_name(err));
    } else {
        init_axp192();
    }

    const gpio_config_t button_config = {
        .pin_bit_mask = (1ULL << M5STICKC_PLUS_PIN_BUTTON_A) |
                        (1ULL << M5STICKC_PLUS_PIN_BUTTON_B),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    return gpio_config(&button_config);
}

const char *voice_board_hardware_id(void)
{
    return "m5stickc_plus";
}

const char *voice_board_display_name(void)
{
    return "M5StickC Plus";
}

i2c_master_bus_handle_t voice_board_i2c_bus(void)
{
    return s_i2c_bus;
}

const voice_board_lcd_config_t *voice_board_lcd_config(void)
{
    return &s_lcd_config;
}

const voice_board_audio_config_t *voice_board_audio_config(void)
{
    return &s_audio_config;
}

gpio_num_t voice_board_primary_button_gpio(void)
{
    return M5STICKC_PLUS_PIN_BUTTON_A;
}

gpio_num_t voice_board_secondary_button_gpio(void)
{
    return M5STICKC_PLUS_PIN_BUTTON_B;
}

gpio_num_t voice_board_power_irq_gpio(void)
{
    return GPIO_NUM_NC;
}

gpio_num_t voice_board_deep_sleep_wake_gpio(void)
{
    return M5STICKC_PLUS_PIN_BUTTON_A;
}

bool voice_board_buttons_use_internal_pullups(void)
{
    return false;
}

bool voice_board_primary_button_pressed(void)
{
    return read_active_low_button(M5STICKC_PLUS_PIN_BUTTON_A);
}

bool voice_board_secondary_button_pressed(void)
{
    return read_active_low_button(M5STICKC_PLUS_PIN_BUTTON_B);
}

esp_err_t voice_board_set_lcd_brightness(uint8_t brightness)
{
    if (!s_axp_dev) {
        return ESP_ERR_INVALID_STATE;
    }

    if (brightness == 0) {
        return axp_update_reg(AXP192_REG_OUTPUT_CTRL, AXP192_OUTPUT_LDO2, 0x00);
    }

    const uint8_t ldo2_code = lcd_brightness_to_ldo2_code(brightness);
    ESP_RETURN_ON_ERROR(axp_update_reg(AXP192_REG_OUTPUT_CTRL, 0x00, AXP192_LCD_OUTPUTS),
                        TAG, "enable lcd outputs");
    return axp_update_reg(AXP192_REG_LDO23_VOLTAGE, 0xf0, (uint8_t)(ldo2_code << 4));
}

esp_err_t voice_board_battery_voltage_mv(int *voltage_mv)
{
    if (!voltage_mv) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_axp_dev) {
        return ESP_ERR_INVALID_STATE;
    }

    uint16_t raw = 0;
    esp_err_t err = axp_read_12bit(AXP192_REG_BAT_VOLTAGE, &raw);
    if (err != ESP_OK) {
        return err;
    }

    *voltage_mv = (raw * 11) / 10;
    return *voltage_mv > 0 ? ESP_OK : ESP_FAIL;
}

esp_err_t voice_board_vbus_voltage_mv(int *voltage_mv)
{
    if (!voltage_mv) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_axp_dev) {
        return ESP_ERR_INVALID_STATE;
    }

    uint16_t raw = 0;
    esp_err_t err = axp_read_12bit(AXP192_REG_VBUS_VOLTAGE, &raw);
    if (err != ESP_OK) {
        return err;
    }

    *voltage_mv = (raw * 17) / 10;
    return ESP_OK;
}

esp_err_t voice_board_battery_level(int *level_percent)
{
    if (!level_percent) {
        return ESP_ERR_INVALID_ARG;
    }

    int voltage_mv = 0;
    esp_err_t err = voice_board_battery_voltage_mv(&voltage_mv);
    if (err != ESP_OK) {
        return err;
    }

    int level = (voltage_mv - 3300) * 100 / (4150 - 3350);
    if (level < 0) {
        level = 0;
    } else if (level > 100) {
        level = 100;
    }

    *level_percent = level;
    return ESP_OK;
}

esp_err_t voice_board_battery_charging(bool *charging)
{
    if (!charging) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!s_axp_dev) {
        return ESP_ERR_INVALID_STATE;
    }

    uint16_t raw_charge = 0;
    uint16_t raw_discharge = 0;
    esp_err_t err = axp_read_13bit(AXP192_REG_CHARGE_CURRENT, &raw_charge);
    if (err != ESP_OK) {
        return err;
    }
    err = axp_read_13bit(AXP192_REG_DISCHARGE_CURRENT, &raw_discharge);
    if (err != ESP_OK) {
        return err;
    }

    *charging = raw_charge > raw_discharge + 20;
    return ESP_OK;
}

esp_err_t voice_board_usb_powered(bool *usb_powered)
{
    if (!usb_powered) {
        return ESP_ERR_INVALID_ARG;
    }

    int voltage_mv = 0;
    esp_err_t err = voice_board_vbus_voltage_mv(&voltage_mv);
    if (err != ESP_OK) {
        return err;
    }

    *usb_powered = voltage_mv > 4500;
    return ESP_OK;
}

esp_err_t voice_board_clear_power_irqs(uint8_t *sys_status)
{
    if (sys_status) {
        *sys_status = 0;
    }
    return s_axp_dev ? ESP_OK : ESP_ERR_INVALID_STATE;
}

void voice_board_prepare_deep_sleep(void)
{
    if (!s_axp_dev) {
        return;
    }

    (void)voice_board_set_lcd_brightness(0);
}
