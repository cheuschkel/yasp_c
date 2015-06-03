/*  Code for processing command messages
*/
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>

#include "yasp.h"
#include "serial_HAL.h"
#include "crc32/crc32.h"

static command_callback command_callbacks[REGISTRY_LENGTH];
static uint16_t commands[REGISTRY_LENGTH];
static uint8_t registered_commands = 0;

void register_yasp_command(command_callback callback, uint16_t command) {
    commands[registered_commands] = command;
    command_callbacks[registered_commands] = callback;
    registered_commands += 1;
}

#define SYNCH_BYTE              0x05
#define NUM_SYNCH_BYTES         1
#define MINIMUM_PACKET_SIZE     9
#define CRC32_SIZE              4
#define COMMAND_SIZE            2
#define COMMAND_LENGTH_SIZE     2

/* Packet Structure */
#define SYNCH_POS               0
#define START_CRC32_POS         1
#define LENGTH_POS              1  
#define COMMAND_POS             3
#define PAYLOAD_POS             5

/* Return Codes */
#define RET_CMD_INCOMPLETE      -1
#define RET_CMD_NOSYNCH         -2
#define RET_CMD_CORRUPT         -3
#define RET_CMD_NOT_REGISTERED  -4

int16_t process_yasp(uint8_t *buffer, uint16_t length)
{
    uint16_t cmd_len;
    uint16_t pkt_len;
    uint16_t i;
    uint32_t crc_32_calc = 0;
    uint32_t crc_32_received = 0;
    
    if (length < MINIMUM_PACKET_SIZE) 
    {
        return RET_CMD_INCOMPLETE;
    }
    
    if (buffer[SYNCH_POS] != SYNCH_BYTE)
    {
        return RET_CMD_NOSYNCH;
    }
    
    cmd_len = (((uint16_t) buffer[LENGTH_POS]) << 8) + ((uint16_t)buffer[LENGTH_POS+1]);
    pkt_len = cmd_len + CRC32_SIZE;

    /* Synch bytes not included in command length */
    if (pkt_len > (length - START_CRC32_POS)) 
    {
        return RET_CMD_INCOMPLETE;
    }
   
    crc_32_calc = crc32(crc_32_calc, &buffer[START_CRC32_POS], cmd_len );
    crc_32_received = crc32_deserialize(&buffer[START_CRC32_POS + cmd_len]);

#if defined DEBUG_YASP
    printf("\nYASP RX message: \t\t");
    for(i= 0; i < length; i++)
    {
        printf("%02x ", buffer[i]);
    }
    printf("\r\n");

    printf(" Calculated: %08lx, Received: %08lx, cmd_len: %d", 
            crc_32_calc, crc_32_received, cmd_len );
#endif 

    if (crc_32_calc != crc_32_received)
    {
        return RET_CMD_CORRUPT;
    }
   
    /* Check if command has been registered; return error otherwise. */
    for (i = 0; i < registered_commands; i++)
    {
        uint16_t cmd =  ((uint16_t)buffer[COMMAND_POS] << 8) + 
                        buffer[COMMAND_POS + 1]; 

        if (commands[i] == cmd) 
        {
            command_callbacks[i](&buffer[PAYLOAD_POS], cmd_len - CRC32_SIZE);
            break;
        }
    }
    
    if (i == registered_commands) 
    {
        return RET_CMD_NOT_REGISTERED;
    }
    
    return (NUM_SYNCH_BYTES + cmd_len + CRC32_SIZE);
}

void send_yasp_command(uint16_t cmd, uint8_t * payload, uint16_t length, uint8_t ack)
{
    uint8_t out_buffer[NUM_SYNCH_BYTES + COMMAND_LENGTH_SIZE + COMMAND_SIZE];
    uint8_t crc_buffer[CRC32_SIZE];
    uint32_t crc_32 = 0;
    uint16_t command_length = COMMAND_SIZE + COMMAND_LENGTH_SIZE + length; 
    out_buffer[SYNCH_POS] = SYNCH_BYTE;
    out_buffer[LENGTH_POS] = (uint8_t)(command_length >> 8);
    out_buffer[LENGTH_POS+1] = (uint8_t)(command_length);
    /* Add leading ack bit if necessary */
    cmd = ack ? (0x80 | cmd) : cmd;
    out_buffer[COMMAND_POS] = (uint8_t)(cmd >> 8);
    out_buffer[COMMAND_POS+1] = (uint8_t)(cmd);
    serial_tx(out_buffer, sizeof(out_buffer));
    crc_32 = crc32(crc_32, &out_buffer[LENGTH_POS], 4);
    if (length > 0)
    {
        crc_32 = crc32(crc_32, payload, length);
        serial_tx(payload, length);
    }
    crc32_serialize(crc_32, crc_buffer);
    serial_tx(crc_buffer, CRC32_SIZE); 

#if defined DEBUG_YASP
    int i = 0;
    printf("\nYASP TX message: \t\t");
    for(i= 0; i < sizeof(out_buffer); i++)
    {
        printf("%02x ", out_buffer[i]);
    }
    for(i= 0; i < length; i++)
    {
        printf("%02x ", payload[i]);
    }
    for(i= 0; i < sizeof(crc_buffer); i++)
    {
        printf("%02x ", crc_buffer[i]);
    }
    printf("\r\n");
#endif
}

uint8_t ack_buffer[16];

uint16_t yasp_rx(uint8_t *cmd_buffer, uint16_t buffer_len)
{
    int16_t return_code = 0;
    uint16_t handled = 0;
    
    while(1)
    {
        return_code = process_yasp(cmd_buffer+handled, buffer_len-handled);
        if (return_code > 0)
        {
            handled += return_code;
            if (handled < buffer_len)
            {
                continue;
            }
        }
        else if (return_code == RET_CMD_NOSYNCH)
        {
            sprintf((char *)ack_buffer, "Resynch!");
            send_yasp_command(0xFF, ack_buffer, strlen((const char *)ack_buffer)+1, true);
            while ((cmd_buffer[handled] != SYNCH_BYTE) && (handled < buffer_len))
            {
                handled++;
            }
        }
        else if (return_code == RET_CMD_CORRUPT)
        {
            sprintf((char *)ack_buffer, "Corrupt!");
            send_yasp_command(0xFF, ack_buffer, strlen((const char *)ack_buffer)+1, true);
            handled = buffer_len;
        }
        else if (return_code == RET_CMD_NOT_REGISTERED)
        {
            sprintf((char *)ack_buffer, "NotRegistered!");
            send_yasp_command(0xFF, ack_buffer, strlen((const char *)ack_buffer)+1, true);
            handled = buffer_len;
        }
        else if (return_code == RET_CMD_INCOMPLETE)
        {
            // Do nothing
        }
        break;
    }
    return handled;
}

void yasp_info(uint8_t * payload, uint16_t length)
{
    uint8_t info[3];
    (void) payload;
    (void) length;
    info[0] = REGISTRY_LENGTH;
    info[1] = (get_serial_buffer_size() & 0xFF00) >> 8;
    info[2] = get_serial_buffer_size() & 0xFF;
    send_yasp_command(CMD_YASP_INFO, info, sizeof(info), true);
}

void yasp_init()
{
    registered_commands = 0;
    register_yasp_command(yasp_info, CMD_YASP_INFO);
    setSerialRxHandler(yasp_rx);
}
