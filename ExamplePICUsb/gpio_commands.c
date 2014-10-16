#include <stdint.h>
#include <p18f4550.h>

#include "../yasp.h"
#include "cmd_def.h"

#define PORT_POS        0

#define OPERATION_POS   1
    #define BITWISE_OR      0
    #define BITWISE_AND     1
    #define SET_PORT        2

#define MASK_POS        2

void gpio_inout(uint8_t * payload, uint16_t length)
{
    volatile unsigned char * target_port;
    switch (payload[PORT_POS])
    {
        case 'A':
            target_port = &TRISA;
            break;
        case 'B':
            target_port = &TRISB;
            break;
        case 'C':
            target_port = &TRISC;
            break;
        case 'D':
            target_port = &TRISD;
            break;
        default:
            return;
    }
    switch (payload[OPERATION_POS])
    {
        case BITWISE_OR:
            (*target_port) |= payload[MASK_POS];
            break;
        case BITWISE_AND:
            (*target_port) &= payload[MASK_POS];
            break;
        case SET_PORT:
            (*target_port) = payload[MASK_POS];
            break;
        default:
            return;
    }
    send_yasp_ack(CMD_GPIO_INOUT, 0, 0);
}

void gpio_hilo(uint8_t * payload, uint16_t length)
{
    volatile unsigned char * target_port;
    uint8_t yasp_ack[2];
    yasp_ack[0] = payload[PORT_POS];
    switch (payload[PORT_POS])
    {
        case 'A':
            yasp_ack[1] = PORTA;
            target_port = &LATA;
            break;
        case 'B':
            yasp_ack[1] = PORTB;
            target_port = &LATB;
            break;
        case 'C':
            yasp_ack[1] = PORTC;
            target_port = &LATC;
            break;
        case 'D':
            yasp_ack[1] = PORTD;
            target_port = &LATD;
            break;
        default:
            return;
    }
    switch (payload[OPERATION_POS])
    {
        case BITWISE_OR:
            (*target_port) |= payload[MASK_POS];
            break;
        case BITWISE_AND:
            (*target_port) &= payload[MASK_POS];
            break;
        case SET_PORT:
            (*target_port) = payload[MASK_POS];
            break;
        default:
            break;
    }
    send_yasp_ack(CMD_GPIO_HILO, yasp_ack, 2);
}

void init_gpio_commands()
{
    TRISA = 0xFF;
    TRISB = 0xFF;
    TRISC = 0xFF;
    TRISD = 0xFF;
    register_yasp_command((command_callback)gpio_inout, CMD_GPIO_INOUT);
    register_yasp_command((command_callback)gpio_hilo, CMD_GPIO_HILO);
}
