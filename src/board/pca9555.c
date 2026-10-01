

#include "pca9555.h"
#include "epd_i2c.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_err.h>
#include <esp_log.h>
#include <stdint.h>

#define REG_INPUT_PORT0 0
#define REG_INPUT_PORT1 1

#define REG_OUTPUT_PORT0 2
#define REG_OUTPUT_PORT1 3

#define REG_INVERT_PORT0 4
#define REG_INVERT_PORT1 5

#define REG_CONFIG_PORT0 6
#define REG_CONFIG_PORT1 7

// Transfers go through epd_i2c (the ESP-IDF 5 master driver on a shared bus);
// the port argument is kept for the API and ignored.
static esp_err_t i2c_master_read_slave(i2c_port_t i2c_num, uint8_t* data_rd, size_t size, int reg) {
    if (size == 0) {
        return ESP_OK;
    }
    uint8_t r = (uint8_t)reg;
    return epd_i2c_write_read(EPDIY_PCA9555_ADDR, &r, 1, data_rd, size);
}

static esp_err_t i2c_master_write_slave(
    i2c_port_t i2c_num, uint8_t ctrl, uint8_t* data_wr, size_t size
) {
    uint8_t buf[8];
    if (size + 1 > sizeof(buf)) {
        return ESP_ERR_INVALID_SIZE;
    }
    buf[0] = ctrl;
    for (size_t i = 0; i < size; i++) buf[i + 1] = data_wr[i];
    return epd_i2c_write(EPDIY_PCA9555_ADDR, buf, size + 1);
}

static esp_err_t pca9555_write_single(i2c_port_t port, int reg, uint8_t value) {
    uint8_t w_data[1] = { value };
    return i2c_master_write_slave(port, reg, w_data, sizeof(w_data));
}

esp_err_t pca9555_set_config(i2c_port_t port, uint8_t config_value, int high_port) {
    return pca9555_write_single(port, REG_CONFIG_PORT0 + high_port, config_value);
}

esp_err_t pca9555_set_inversion(i2c_port_t port, uint8_t config_value, int high_port) {
    return pca9555_write_single(port, REG_INVERT_PORT0 + high_port, config_value);
}

esp_err_t pca9555_set_value(i2c_port_t port, uint8_t config_value, int high_port) {
    return pca9555_write_single(port, REG_OUTPUT_PORT0 + high_port, config_value);
}

uint8_t pca9555_read_input(i2c_port_t i2c_port, int high_port) {
    esp_err_t err;
    uint8_t r_data[1];

    err = i2c_master_read_slave(i2c_port, r_data, 1, REG_INPUT_PORT0 + high_port);
    if (err != ESP_OK) {
        ESP_LOGE("PCA9555", "%s failed", __func__);
        return 0;
    }

    return r_data[0];
}
