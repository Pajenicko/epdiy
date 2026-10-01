// The board's I2C bus on the ESP-IDF 5 master driver (driver/i2c_master.h),
// in place of the legacy driver/i2c.h the rest of epdiy was written for.
//
// Local change for zivyobraz on Arduino 3: Arduino's Wire library runs on the
// new driver, and ESP-IDF 5 refuses to start with both drivers linked. The
// bus is shared: if someone (Wire) already created one on the port, epdiy
// adds its devices to that; otherwise it creates the bus and, on deinit,
// deletes it again.
#pragma once

#include <esp_err.h>
#include <stddef.h>
#include <stdint.h>

esp_err_t epd_i2c_init(int port, int sda, int scl, uint32_t hz);
void epd_i2c_deinit(void);
esp_err_t epd_i2c_write(uint8_t addr, const uint8_t* data, size_t len);
esp_err_t epd_i2c_write_read(uint8_t addr, const uint8_t* out, size_t out_len, uint8_t* in, size_t in_len);
