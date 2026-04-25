#pragma once
#include <stdint.h>
static inline void ssd1306_init(void) {}
static inline void ssd1306_write(const uint8_t *buf, uint32_t len) {}
static inline void ssd1306_scroll(int cols) {}
static inline void ssd1306_display_toggle(void) {}
