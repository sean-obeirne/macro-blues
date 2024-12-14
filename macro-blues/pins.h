#ifndef PINS
#define PINS

// GPIO pins //
#define AIN0 2
#define AIN1 3
#define AIN2 4
#define AIN3 5
#define AIN4 28
#define AIN5 29
#define SPI_CLOCK 12 // sync data transfer
#define MOSI 13 // transmit data from master device to slave device
#define MISO 14 // transmit data from slave device to master device
#define UART_RECEIVE 8 // receive serial data from another device
#define UART_TRANSMIT 6 // transmit serial data to another device
#define DFU 20 // device firmware upgrade
#define NFC2 10
#define NFC1 9
#define BLUE_LED 19
#define RED_LED 17
#define UART_DATA_OUT_1 16 // transmit serial data, debugging/extra channels
#define UART_DATA_OUT_2 15 // transmit serial data, debugging/extra channels
#define GPIO1 7
#define GPIO2 11
#define AIN7_BAT 31 // DO NOT USE- used for battery voltage measurement
#define AIN6 30
#define GPIO3 27
#define I2C_CLOCK 26 // clock line for I2C communication
#define I2C 25 // data line for I2C, bidirectional


#endif
