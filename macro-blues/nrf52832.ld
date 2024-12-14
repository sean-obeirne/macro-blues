MEMORY {
    FLASH (rx) : ORIGIN = 0x00028000, LENGTH = 48K
    RAM (rwx) : ORIGIN = 0x20000000, LENGTH = 64K
}

SECTIONS {
    .text : { *(.text*) } > FLASH
}

