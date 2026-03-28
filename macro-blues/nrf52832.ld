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

    /* Initialized globals (e.g. `int x = 42;`).  The values are stored
       in flash (AT> FLASH) but live in RAM at run-time (> RAM).  The
       startup code copies them from _data_flash to _data_start.._data_end. */
    _data_flash = LOADADDR(.data);
    .data : {
        _data_start = .;
        *(.data*)
        _data_end = .;
    } > RAM AT> FLASH

    /* Zero-initialized globals. NOLOAD means the linker doesn't try to
       store zeros in flash — RAM just gets zeroed at startup.
       _bss_start / _bss_end let the startup code find and zero this region. */
    .bss (NOLOAD) : {
        _bss_start = .;
        *(.bss*)
        *(COMMON)
        _bss_end = .;
    } > RAM
}

