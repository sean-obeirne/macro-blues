#include <string.h>
#include "ssd1306.h"
#include "i2c.h"

#define SSD1306_ADDR 0x3C

/* I2C control bytes (Co=0 in both cases → rest of packet is payload):
 *   0x00 : D/C#=0 → command byte stream follows
 *   0x40 : D/C#=1 → data byte stream follows */
#define CTRL_CMD  0x00
#define CTRL_DATA 0x40

/* RAM buffer for command writes (control byte + commands).
 * Must be in RAM — TWIM EasyDMA cannot read from flash. */
static uint8_t cmd_buf[28];

/* nRF52832 TWIM TXD.MAXCNT is 8 bits → 255 byte max per transfer.
 * Chunk buffer: 1 control byte + 254 data bytes = 255. */
#define CHUNK_DATA 254
static uint8_t chunk_buf[1 + CHUNK_DATA];

#define FB_SIZE (SSD1306_WIDTH * SSD1306_HEIGHT / 8)

static void ssd1306_cmd(const uint8_t *cmds, uint32_t len)
{
	cmd_buf[0] = CTRL_CMD;
	memcpy(&cmd_buf[1], cmds, len);
	i2c_write(SSD1306_ADDR, cmd_buf, len + 1);
}

void ssd1306_init(void)
{
	/* SSD1306 init sequence for 128×32
	 * Ref: SSD1306 datasheet §8, Adafruit SSD1306 library */
	static const uint8_t init_cmds[] = {
		0xAE,       /* Display OFF                                    */
		0xD5, 0x80, /* Set display clock: default oscillator, div 1   */
		0xA8, 0x1F, /* Set MUX ratio: 32 rows (0x1F = 31)            */
		0xD3, 0x00, /* Set display offset: none                       */
		0x40,       /* Set start line: 0                              */
		0x8D, 0x14, /* Charge pump: enable (0x14)                     */
		0x20, 0x00, /* Memory addressing mode: horizontal             */
		0xA1,       /* Segment re-map: col 127 → SEG0                 */
		0xC8,       /* COM scan direction: remapped (bottom-to-top)   */
		0xDA, 0x02, /* COM pins config: sequential, no remap (128×32) */
		0x81, 0x8F, /* Set contrast: 0x8F (mid-high)                  */
		0xD9, 0xF1, /* Pre-charge period: phase1=1, phase2=15         */
		0xDB, 0x40, /* VCOMH deselect level: ~0.77×Vcc                */
		0xA4,       /* Entire display ON: follow RAM content          */
		0xA6,       /* Normal display (not inverted)                  */
		0xAF,       /* Display ON                                     */
	};

	ssd1306_cmd(init_cmds, sizeof(init_cmds));
}

void ssd1306_fill(uint8_t pattern)
{
	/* Set column address range: 0–127 */
	static const uint8_t col_cmd[] = { 0x21, 0x00, 0x7F };
	ssd1306_cmd(col_cmd, sizeof(col_cmd));

	/* Set page address range: 0–3 (4 pages × 8 rows = 32 rows) */
	static const uint8_t page_cmd[] = { 0x22, 0x00, 0x03 };
	ssd1306_cmd(page_cmd, sizeof(page_cmd));

	/* Fill display RAM in 254-byte chunks (TWIM max is 255 including
	 * the control byte).  Horizontal addressing mode auto-advances
	 * the SSD1306 write pointer across transactions. */
	uint32_t remaining = FB_SIZE;
	chunk_buf[0] = CTRL_DATA;
	while (remaining > 0)
	{
		uint32_t n = remaining < CHUNK_DATA ? remaining : CHUNK_DATA;
		memset(&chunk_buf[1], pattern, n);
		i2c_write(SSD1306_ADDR, chunk_buf, n + 1);
		remaining -= n;
	}
}
