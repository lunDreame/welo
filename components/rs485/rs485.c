#include "rs485.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "RS485";
static SemaphoreHandle_t tx_mutex = NULL;

esp_err_t rs485_init(void)
{
    uart_config_t uart_config = {
        .baud_rate = RS485_BAUD_RATE,
        .data_bits = RS485_DATA_BITS,
        .parity = RS485_PARITY,
        .stop_bits = RS485_STOP_BITS,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };

    esp_err_t ret = uart_driver_install(RS485_UART_PORT, RS485_BUF_SIZE, RS485_BUF_SIZE, 10, NULL, 0);
    if (ret != ESP_OK)
        return ret;

    ret = uart_param_config(RS485_UART_PORT, &uart_config);
    if (ret != ESP_OK)
        return ret;

    ret = uart_set_pin(RS485_UART_PORT, RS485_TX_PIN, RS485_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (ret != ESP_OK)
        return ret;

    tx_mutex = xSemaphoreCreateMutex();
    return (tx_mutex == NULL) ? ESP_ERR_NO_MEM : ESP_OK;
}

esp_err_t rs485_send(const uint8_t *data, size_t len)
{
    if (!data || len == 0)
        return ESP_ERR_INVALID_ARG;

    if (xSemaphoreTake(tx_mutex, pdMS_TO_TICKS(RS485_SEND_TIMEOUT)) == pdFALSE)
        return ESP_ERR_TIMEOUT;

    esp_err_t ret = uart_write_bytes(RS485_UART_PORT, (const char *)data, len);
    uart_wait_tx_done(RS485_UART_PORT, pdMS_TO_TICKS(RS485_SEND_TIMEOUT));

    xSemaphoreGive(tx_mutex);
    return ret;
}

esp_err_t rs485_receive(uint8_t *data, size_t max_len, size_t *received_len)
{
    if (!data || !received_len)
        return ESP_ERR_INVALID_ARG;

    int len = uart_read_bytes(RS485_UART_PORT, data, max_len, pdMS_TO_TICKS(100));

    if (len < 0)
    {
        ESP_LOGE(TAG, "RS485 Receive Error: %d", len);
        return ESP_FAIL;
    }

    *received_len = len;
    return ESP_OK;
}