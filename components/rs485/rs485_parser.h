#ifndef RS485_PARSER_H
#define RS485_PARSER_H

#include <stdint.h>

#define MIN_PACKET_SIZE 21
#define PACKET_START_BYTE_1 0xAA
#define PACKET_START_BYTE_2 0x55
#define PACKET_END_BYTE_1 0x0D
#define PACKET_END_BYTE_2 0x0D

void rs485_parser_process_byte(uint8_t byte);
void parse_rs485_packet(int uart_num, uint8_t *packet, int len);

#endif // RS485_PARSER_H