#include "epd_i2c.h"

#include <driver/i2c_master.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static const char* TAG = "epdiy_i2c";

static i2c_master_bus_handle_t s_bus;
static bool s_bus_is_ours;
static uint32_t s_hz;

// TPS65185 and PCA9555: two devices, a small table is plenty.
static struct {
    uint8_t addr;
    i2c_master_dev_handle_t dev;
} s_devs[4];

esp_err_t epd_i2c_init(int port, int sda, int scl, uint32_t hz) {
    s_hz = hz;
    if (i2c_master_get_bus_handle(port, &s_bus) == ESP_OK) {
        s_bus_is_ours = false;
        ESP_LOGI(TAG, "sharing the I2C bus on port %d", port);
        return ESP_OK;
    }
    const i2c_master_bus_config_t cfg = {
        .i2c_port = port,
        .sda_io_num = sda,
        .scl_io_num = scl,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };
    esp_err_t err = i2c_new_master_bus(&cfg, &s_bus);
    s_bus_is_ours = err == ESP_OK;
    return err;
}

void epd_i2c_deinit(void) {
    for (size_t i = 0; i < sizeof(s_devs) / sizeof(s_devs[0]); i++) {
        if (s_devs[i].dev != NULL) {
            i2c_master_bus_rm_device(s_devs[i].dev);
            s_devs[i].dev = NULL;
        }
    }
    if (s_bus != NULL && s_bus_is_ours) {
        i2c_del_master_bus(s_bus);
    }
    s_bus = NULL;
}

static i2c_master_dev_handle_t device(uint8_t addr) {
    size_t free_slot = SIZE_MAX;
    for (size_t i = 0; i < sizeof(s_devs) / sizeof(s_devs[0]); i++) {
        if (s_devs[i].dev != NULL && s_devs[i].addr == addr) {
            return s_devs[i].dev;
        }
        if (s_devs[i].dev == NULL && free_slot == SIZE_MAX) {
            free_slot = i;
        }
    }
    if (s_bus == NULL || free_slot == SIZE_MAX) {
        return NULL;
    }
    const i2c_device_config_t cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = addr,
        .scl_speed_hz = s_hz,
    };
    if (i2c_master_bus_add_device(s_bus, &cfg, &s_devs[free_slot].dev) != ESP_OK) {
        s_devs[free_slot].dev = NULL;
        return NULL;
    }
    s_devs[free_slot].addr = addr;
    return s_devs[free_slot].dev;
}

// A few tries: the TPS65185 does not answer for a moment after WAKEUP, and
// the new driver reports that NACK as ESP_ERR_INVALID_STATE.
#define TRIES 5

esp_err_t epd_i2c_write(uint8_t addr, const uint8_t* data, size_t len) {
    i2c_master_dev_handle_t dev = device(addr);
    if (dev == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t err = ESP_FAIL;
    for (int i = 0; i < TRIES && err != ESP_OK; i++) {
        if (i > 0) vTaskDelay(pdMS_TO_TICKS(2));
        err = i2c_master_transmit(dev, data, len, 1000);
    }
    return err;
}

// Register write, STOP, then the read: the same two transactions the legacy
// code made. Not a repeated start, which these chips were never driven with.
esp_err_t epd_i2c_write_read(uint8_t addr, const uint8_t* out, size_t out_len, uint8_t* in, size_t in_len) {
    i2c_master_dev_handle_t dev = device(addr);
    if (dev == NULL) {
        return ESP_ERR_INVALID_STATE;
    }
    esp_err_t err = ESP_FAIL;
    for (int i = 0; i < TRIES && err != ESP_OK; i++) {
        if (i > 0) vTaskDelay(pdMS_TO_TICKS(2));
        err = i2c_master_transmit(dev, out, out_len, 1000);
        if (err == ESP_OK) {
            err = i2c_master_receive(dev, in, in_len, 1000);
        }
    }
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "0x%02x does not answer: %s", addr, esp_err_to_name(err));
    }
    return err;
}
