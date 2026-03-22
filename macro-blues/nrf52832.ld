MEMORY {
    FLASH (rx) : ORIGIN = 0x00026000, LENGTH = 0x5A000
    RAM (rwx)  : ORIGIN = 0x20002CE0, LENGTH = 0xD320
}

/* The stack grows downward from the top of RAM.
   This symbol gets used as entry 0 in the vector table. */
_stack_top = ORIGIN(RAM) + LENGTH(RAM);

SECTIONS {
    /* Vector table MUST be first in flash — the CPU looks here on boot.
       KEEP() prevents --gc-sections from discarding it, since nothing
       "references" the vector table in normal code flow. */
    .isr_vector : {
        KEEP(*(.isr_vector))
    } > FLASH

    /* All your actual code and read-only data (string literals etc) */
    .text : {
        *(.text*)
        *(.rodata*)
    } > FLASH

    /* Zero-initialized globals. NOLOAD means the linker doesn't try to
       store zeros in flash — RAM just gets zeroed at startup. */
    .bss (NOLOAD) : {
        *(.bss*)
        *(COMMON)
    } > RAM
}

