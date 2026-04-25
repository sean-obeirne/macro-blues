#include <stdint.h>
#include <string.h>
#include "ssd1306.h"
#include "i2c.h"
#include "display.h"
#include "glyphs.h"

#define FB_WIDTH 128
#define FB_PAGES 4
uint8_t fb[FB_PAGES * FB_WIDTH];

void display_init(void) {
    i2c_init();
    ssd1306_init();
    display_clear();
    display_flush();
}

void display_clear(void) {
    for (int i = 0; i < FB_PAGES * FB_WIDTH; i++)
        fb[i] = 0x00;
}

void display_flush(void) {
    ssd1306_write(fb, sizeof(fb));
}

void display_pixel(int x, int y, int on) {
    int page = y / 8;
    int bit = y % 8;
    if (on)
        fb[page * FB_WIDTH + x] |= (1 << bit);
    else
        fb[page * FB_WIDTH + x] &= ~(1 << bit);
}

void display_char(int x, int y, char c) {
    const uint8_t *g = glyph(c);
    int page = y / 8;
    for (int col = 0; col < 5; col++)
        fb[page * FB_WIDTH + x + col] = g[col];
    // col 5 = spacing gap, leave zero
}

void display_scroll(int cols) {
    ssd1306_scroll(cols);
}

void display_toggle(void) {
    ssd1306_display_toggle();
}

void display_string(int x, int y, const char *s) {
    while (*s) {
        display_char(x, y, *s++);
        x += 6; // 5 px char + 1 px gap
        if (x + 5 >= FB_WIDTH) { // wrap to next line if no room for next char
            x = 0;
            y += 8;
            if (y >= FB_PAGES * 8) // no more room on display
                break;
        }
    }
}
