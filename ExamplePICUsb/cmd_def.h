#include <stdint.h>

#define CMD_I2C_WRITE   1
#define CMD_I2C_READ    2
void init_i2c_commands();

#define CMD_GPIO_INOUT  3
#define CMD_GPIO_HILO   4
void init_gpio_commands();

#define CMD_UART_CONFIG 5
#define CMD_UART_WRITE  6
