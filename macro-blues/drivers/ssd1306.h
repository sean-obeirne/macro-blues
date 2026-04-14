#ifndef SSD1306_H
#define SSD1306_H

#include <stdint.h>

#define SSD1306_WIDTH 128
#define SSD1306_HEIGHT 32

void ssd1306_init(void);
void ssd1306_fill(uint8_t pattern);
void ssd1306_fill_all(uint8_t pattern);
void ssd1306_display_toggle(void);
void ssd1306_scroll(int rows);

#endif /* SSD1306_H */
