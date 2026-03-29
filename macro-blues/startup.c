/*
 * startup.c — vector table and reset handler
 *
 * When the chip powers on (or resets), the Cortex-M4 hardware does two things:
 *   1. Loads the stack pointer from address 0x26000 (first word in flash)
 *   2. Jumps to the reset handler at address 0x26004 (second word in flash)
 *
 * That's it — no bootloader magic, no OS. The hardware reads two addresses
 * and starts running. Everything after that is our code.
 *
 * The vector table is just an array of function pointers. We place it in a
 * section called ".isr_vector" which the linker script maps to the start
 * of flash. The rest of the entries (NMI, HardFault, etc.) are exception
 * handlers — if we don't need them yet, they point to Default_Handler
 * which just spins forever (a safe place to land during debugging).
 */

#include <stdint.h>

/* _stack_top is defined in the linker script (top of RAM).
   We declare it as extern so we can take its address. */
extern uint32_t _stack_top;

/* Forward declarations */
extern int main(void);
void Reset_Handler(void);
void Default_Handler(void);

/* "weak" means: use this unless someone defines a real one.
   "alias" means: point to Default_Handler for now.
   So if you later write a real HardFault_Handler function,
   the linker will automatically use yours instead. */
void NMI_Handler(void) __attribute__((weak, alias("Default_Handler")));
void HardFault_Handler(void) __attribute__((weak, alias("Default_Handler")));
void SVC_Handler(void) __attribute__((weak, alias("Default_Handler")));
void PendSV_Handler(void) __attribute__((weak, alias("Default_Handler")));
void SysTick_Handler(void) __attribute__((weak, alias("Default_Handler")));
void GPIOTE_IRQHandler(void) __attribute__((weak, alias("Default_Handler")));

/*
 * The actual vector table.
 *
 * __attribute__((section(".isr_vector"))) — puts this array in the
 *   ".isr_vector" section, which the linker script places at 0x26000.
 *
 * __attribute__((used)) — tells the compiler "don't optimize this away"
 *   even though no C code references it directly (the hardware does).
 *
 * Entry 0:  Initial stack pointer (not a function — we cast the address)
 * Entry 1:  Reset handler — where execution starts
 * Entry 2:  NMI
 * Entry 3:  HardFault
 * Entries 4-10: Reserved (zeros)
 * Entry 11: SVCall (used by SoftDevice later)
 * Entries 12-13: Reserved
 * Entry 14: PendSV
 * Entry 15: SysTick
 *
 * We stop at 16 entries. Peripheral interrupts (entry 16+) default to
 * Default_Handler, with specific overrides for GPIOTE etc.
 */
__attribute__((section(".isr_vector"), used)) void (*const vectors[64])(void) = {
    (void (*)(void))(&_stack_top), /* 0:  Initial stack pointer */
    Reset_Handler,                 /* 1:  Reset — entry point */
    NMI_Handler,                   /* 2:  Non-maskable interrupt */
    HardFault_Handler,             /* 3:  Hard fault */
    0, 0, 0, 0, 0, 0, 0,           /* 4-10: Reserved */
    SVC_Handler,                   /* 11: Supervisor call */
    0, 0,                          /* 12-13: Reserved */
    PendSV_Handler,                /* 14: Pendable service request */
    SysTick_Handler,               /* 15: System tick timer */
    [16 ... 63] = Default_Handler, /* All peripheral IRQs → Default_Handler */
    [22] = GPIOTE_IRQHandler,      /* IRQ 6:  GPIOTE (overrides Default) */
};

/*
 * Reset_Handler — first C code that runs.
 */
void Reset_Handler(void)
{
    /*
     * NOTE: We do NOT set VTOR here.  It stays at 0x00000000 (reset default),
     * pointing to the MBR's vector table.  The MBR forwards SVC calls to the
     * SoftDevice and other interrupts to our app's table at 0x26000.
     * Setting VTOR = 0x26000 would break all SoftDevice SVC calls.
     */

    /* 1. Enable FPU. We compile with -mfloat-abi=hard, meaning the compiler
     * might emit FPU instructions at any time. If the FPU is not enabled
     * in the CPACR (Coprocessor Access Control Register), we will HardFault.
     * SCB->CPACR is at 0xE000ED88. Bits 20-23 must be 0xF to enable CP10/CP11. */
    *((volatile uint32_t *)0xE000ED88) |= (0xF << 20);
    // Execute a sync barrier to ensure FPU is enabled before proceeding
    __asm volatile("dsb \n isb \n");

    /* 2. Clear pending FPU interrupts. Adafruit's bootloader often leaves
     * FPU interrupts pending, which causes immediate Lockup if not cleared.
     * IRQ 39 (FPU) is NVIC->ICPR[1] bit 7 (0xE000E284). */
    *((volatile uint32_t *)0xE000E284) = (1 << 7);

    /* 3. Copy .data section from flash (LMA) into RAM (VMA).
     *    Initialized globals like `int x = 42;` are stored in flash by the
     *    linker, but the C code expects them in RAM. We copy them here. */
    extern uint32_t _data_flash, _data_start, _data_end;
    uint32_t *src = &_data_flash;
    uint32_t *dst = &_data_start;
    while (dst < &_data_end)
        *dst++ = *src++;

    /* 4. Zero the .bss section.
     *    Uninitialized globals must start at 0 per the C standard. */
    extern uint32_t _bss_start, _bss_end;
    dst = &_bss_start;
    while (dst < &_bss_end)
        *dst++ = 0;

    main();
    while (1)
        ; /* If main() ever returns, don't let the CPU run wild */
}

/*
 * Default_Handler — landing pad for any unhandled exceptions.
 * Spins forever. When debugging with a J-Link you'd break here and
 * check which exception fired. For now it's just a safety net.
 */
void Default_Handler(void)
{
    while (1)
        ;
}
