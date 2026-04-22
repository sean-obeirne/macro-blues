#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>

/*
 * display.h — high-level display API for the 128×32 OLED
 *
 * Sits on top of the ssd1306 driver.  Maintains a 512-byte framebuffer
 * in RAM; call display_flush() to push it to the panel over I2C.
 *
 * Coordinate system: (0,0) = top-left, x increases right, y increases down.
 * Font: 5×7 pixels per character, 1-pixel gap between characters.
 *   → 21 chars per line, 4 lines at 8 px line-height on a 128×32 panel.
 */

void display_init(void);
void display_clear(void);
void display_flush(void);

void display_pixel(int x, int y, int on);

/* Draw a single ASCII character at pixel position (x, y). */
void display_char(int x, int y, char c);

/* Draw a null-terminated string starting at pixel position (x, y).
 * Wraps to the next 8-px-aligned row when the line is full. */
void display_string(int x, int y, const char *s);

#endif /* DISPLAY_H */
