#ifndef GPIO_H
#define GPIO_H

#include <stdint.h>

// Base address for GPIO
#define GPIO_BASE_ADDRESS 0x50000000

// Register definitions
#define GPIO_OUT	(*(volatile uint32_t *) (GPIO_BASE_ADDRESS + 0x504))
#define GPIO_OUTSET	(*(volatile uint32_t *) (GPIO_BASE_ADDRESS + 0x508))
#define GPIO_OUTCLR	(*(volatile uint32_t *) (GPIO_BASE_ADDRESS + 0x50C))
#define GPIO_IN		(*(volatile uint32_t *) (GPIO_BASE_ADDRESS + 0x510))
#define GPIO_DIRSET	(*(volatile uint32_t *) (GPIO_BASE_ADDRESS + 0x518))
#define GPIO_DIRCLR	(*(volatile uint32_t *) (GPIO_BASE_ADDRESS + 0x51C))

// Pin config register, then pin definition offsets
#define GPIO_PIN_CNF(n) (*(volatile uint32_t *) ((uintptr_t)GPIO_BASE_ADDRESS + 0x700 + (n * 4)))
#define DIR_OFFSET 0
#define INPUT_OFFSET 1
#define PULL_OFFSET 2
#define DRIVE_OFFSET 8
#define SENSE_OFFSET 16

// Useful macros for GPIO configuration
#define GPIO_DIR_OUTPUT 1
#define GPIO_DIR_INPUT 0

#endif

