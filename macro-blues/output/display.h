#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>

/*
 * display.h — high-level display API for the 128×32 OLED
 *
 * Sits on top of the ssd1306 driver.  Maintains a 512-byte framebuffer
 * in RAM; call display_flush() to push it to the panel over I2C.
 *
 * Coordinate system: (0,0) = top-left, x increases downward (0–31), y increases right (0–127).
 * Font: 5×7 pixels per character, 1-pixel gap between characters.
 *   → 21 chars per row, 4 rows at 8 px row-height on a 128×32 panel.
 */

extern uint8_t fb[];

void display_init(void);
void display_clear(void);
void display_flush(void);

void display_pixel(int x, int y, int on);

/* Draw a single ASCII character at pixel position (x, y). */
void display_char(int x, int y, char c);

/* Draw a null-terminated string starting at pixel position (x, y).
 * Wraps to the next 8-px-aligned row when the line is full. */
void display_string(int x, int y, const char *s);

/* Hardware scroll the panel content by cols columns. */
void display_scroll(int cols);

/* Toggle the display on/off without clearing the framebuffer. */
void display_toggle(void);

#endif /* DISPLAY_H */
