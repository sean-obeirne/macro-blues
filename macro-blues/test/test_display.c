/* Host-side display test. Stubs out hardware, renders framebuffer to stdout.
 * Build: make test
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* ── Framebuffer lives in display.c ── */
#define FB_WIDTH  128
#define FB_PAGES  4
extern uint8_t fb[];

/* ── Include the real display + glyph implementation ── */
#include "display.h"
#include "glyphs.h"

/* ── Dump framebuffer as ASCII art ── */
void fb_print(void) {
    printf("+");
    for (int x = 0; x < FB_WIDTH; x++) printf("-");
    printf("+\n");

    for (int page = 0; page < FB_PAGES; page++) {
        for (int bit = 0; bit < 8; bit++) {
            printf("|");
            for (int x = 0; x < FB_WIDTH; x++) {
                uint8_t byte = fb[page * FB_WIDTH + x];
                printf("%c", (byte >> bit) & 1 ? '#' : ' ');
            }
            printf("|\n");
        }
    }

    printf("+");
    for (int x = 0; x < FB_WIDTH; x++) printf("-");
    printf("+\n");
}

int main(void) {
    display_init();
    display_clear();

    display_string(0, 0, "Hello, world!");
    display_string(0, 8, "ABCDEFGHIJKLMNOPQRSTUVWXYZ");

    display_flush();
    fb_print();
    return 0;
}
