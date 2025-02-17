#ifndef RS485_H
#define RS485_H

#include "driver/uart.h"
#include "esp_err.h"

#define RS485_UART_PORT UART_NUM_1
#define RS485_TX_PIN 10
#define RS485_RX_PIN 9
#define RS485_BAUD_RATE 9600
#define RS485_DATA_BITS UART_DATA_8_BITS
#define RS485_STOP_BITS UART_STOP_BITS_1
#define RS485_PARITY UART_PARITY_DISABLE
#define RS485_BUF_SIZE 256
#define RS485_SEND_TIMEOUT 100

esp_err_t rs485_init(void);
esp_err_t rs485_send(const uint8_t *data, size_t len);
esp_err_t rs485_receive(uint8_t *data, size_t max_len, size_t *received_len);

#endif // RS485_H