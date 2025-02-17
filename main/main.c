#include <stdio.h>
#include "rs485.h"
#include "rs485_parser.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define RS485_RECEIVE_TASK_STACK_SIZE 4096
#define RS485_RECEIVE_BUFFER_SIZE 256

static void rs485_receive_task(void *arg)
{
    uint8_t rx_byte;
    size_t received_len = 0;

    while (1)
    {
        esp_err_t ret = rs485_receive(&rx_byte, 1, &received_len);
        if (ret == ESP_OK && received_len == 1)
        {
            rs485_parser_process_byte(rx_byte);
        }
        else if (ret != ESP_OK && ret != ESP_ERR_TIMEOUT)
        {
            printf("RS485 Receive Error: %d\n", ret);
        }

        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void app_main(void)
{
    if (rs485_init() != ESP_OK)
    {
        printf("RS485 Initialization failed!\n");
        return;
    }

    xTaskCreate(rs485_receive_task, "RS485_Receive_Task", RS485_RECEIVE_TASK_STACK_SIZE, NULL, 10, NULL);
}