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
    int landscape_row = 31 - x;
    int page = landscape_row / 8;
    int bit = landscape_row % 8;
    if (on)
        fb[page * FB_WIDTH + y] |= (1 << bit);
    else
        fb[page * FB_WIDTH + y] &= ~(1 << bit);
}

void display_rect(int x, int y, int w, int h, int on) {
    for (int i = 0; i < w; i++)
        for (int j = 0; j < h; j++)
            display_pixel(x + i, y + j, on);
}

void display_char(int x, int y, char c, int scale) {
    const uint8_t *g = glyph(c);
    for (int col = 0; col < 5; col++) {
        for (int row = 0; row < 8; row++) {
            int on = (g[col] >> row) & 1;
            for (int sx = 0; sx < scale; sx++)
                for (int sy = 0; sy < scale; sy++)
                    display_pixel(x + col * scale + sx, y + row * scale + sy, on);
        }
    }
}

void display_scroll(int cols) {
    ssd1306_scroll(cols);
}

void display_toggle(void) {
    ssd1306_display_toggle();
}

void display_string(int x, int row, const char *s, int scale) {
    int y = row * 9;           // convert row → portrait y pixel
    int char_h = 8 * scale + 1;
    int char_w = 5 * scale + 1;
    while (*s) {
        display_char(x, y, *s++, scale);
        x += char_w;                        // advance across the screen
        if (x + char_w > FB_PAGES * 8) {   // no room for next char → next line
            x = 0;
            y += char_h;
            if (y + char_h > FB_WIDTH)      // off the bottom
                break;
        }
    }
}
