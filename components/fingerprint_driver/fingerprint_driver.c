#include "fingerprint_driver.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char *TAG = "FP_DRIVER";

#define FP_STARTCODE 0xEF01
#define FP_DEFAULT_TIMEOUT_MS 1000
#define FP_RX_BUF_SIZE 256
#define FP_TX_BUF_SIZE 256

struct fingerprint_driver {
    int uart_num;
    uint32_t address;
};

// Calculate checksum
static uint16_t calculate_checksum(const uint8_t *data, size_t len) {
    uint16_t sum = 0;
    for (size_t i = 0; i < len; i++) {
        sum += data[i];
    }
    return sum;
}

// Send packet to sensor
static esp_err_t send_packet(fingerprint_handle_t handle, uint8_t type, const uint8_t *data, uint16_t len) {
    uint8_t packet[FP_TX_BUF_SIZE];
    uint16_t idx = 0;

    // Start code
    packet[idx++] = (FP_STARTCODE >> 8) & 0xFF;
    packet[idx++] = FP_STARTCODE & 0xFF;

    // Address
    packet[idx++] = (handle->address >> 24) & 0xFF;
    packet[idx++] = (handle->address >> 16) & 0xFF;
    packet[idx++] = (handle->address >> 8) & 0xFF;
    packet[idx++] = handle->address & 0xFF;

    // Package identifier
    packet[idx++] = type;

    // Package length (data + checksum)
    uint16_t pkg_len = len + 2;
    packet[idx++] = (pkg_len >> 8) & 0xFF;
    packet[idx++] = pkg_len & 0xFF;

    // Data
    if (data && len > 0) {
        memcpy(&packet[idx], data, len);
        idx += len;
    }

    // Checksum
    uint16_t checksum = calculate_checksum(&packet[6], len + 3);
    packet[idx++] = (checksum >> 8) & 0xFF;
    packet[idx++] = checksum & 0xFF;

    int written = uart_write_bytes(handle->uart_num, packet, idx);
    return (written == idx) ? ESP_OK : ESP_FAIL;
}

// Receive packet from sensor
static esp_err_t receive_packet(fingerprint_handle_t handle, uint8_t *type, uint8_t *data, uint16_t *len) {
    uint8_t buf[FP_RX_BUF_SIZE];
    int received = uart_read_bytes(handle->uart_num, buf, FP_RX_BUF_SIZE, pdMS_TO_TICKS(FP_DEFAULT_TIMEOUT_MS));

    if (received < 9) {
        return ESP_ERR_TIMEOUT;
    }

    // Verify start code
    uint16_t start = (buf[0] << 8) | buf[1];
    if (start != FP_STARTCODE) {
        return ESP_ERR_INVALID_RESPONSE;
    }

    // Extract type
    *type = buf[6];

    // Extract length
    uint16_t pkg_len = (buf[7] << 8) | buf[8];
    uint16_t data_len = pkg_len - 2;

    if (data_len > 0 && data) {
        memcpy(data, &buf[9], data_len);
    }
    if (len) {
        *len = data_len;
    }

    return ESP_OK;
}

// Send command and get response
static esp_err_t send_command(fingerprint_handle_t handle, uint8_t cmd, const uint8_t *params, uint16_t param_len, uint8_t *response, uint16_t *resp_len) {
    uint8_t cmd_data[FP_TX_BUF_SIZE];
    cmd_data[0] = cmd;

    if (params && param_len > 0) {
        memcpy(&cmd_data[1], params, param_len);
    }

    esp_err_t ret = send_packet(handle, 0x01, cmd_data, param_len + 1);
    if (ret != ESP_OK) {
        return ret;
    }

    uint8_t type;
    return receive_packet(handle, &type, response, resp_len);
}

esp_err_t fingerprint_init(const fingerprint_config_t *config, fingerprint_handle_t *handle) {
    ESP_LOGI(TAG, "Initializing fingerprint sensor");

    fingerprint_handle_t h = malloc(sizeof(struct fingerprint_driver));
    if (!h) {
        return ESP_ERR_NO_MEM;
    }

    h->uart_num = config->uart_num;
    h->address = config->address;

    uart_config_t uart_config = {
        .baud_rate = config->baud_rate,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK(uart_driver_install(config->uart_num, FP_RX_BUF_SIZE * 2, FP_TX_BUF_SIZE * 2, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(config->uart_num, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(config->uart_num, config->tx_pin, config->rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    *handle = h;
    ESP_LOGI(TAG, "Fingerprint sensor initialized");
    return ESP_OK;
}

esp_err_t fingerprint_get_image(fingerprint_handle_t handle) {
    uint8_t response[32];
    uint16_t len;

    esp_err_t ret = send_command(handle, FP_CMD_GETIMAGE, NULL, 0, response, &len);
    if (ret != ESP_OK) {
        return ret;
    }

    if (response[0] == FP_OK) {
        return ESP_OK;
    } else if (response[0] == FP_NO_FINGER) {
        return ESP_ERR_NOT_FOUND;
    }

    return ESP_FAIL;
}

esp_err_t fingerprint_image_to_tz(fingerprint_handle_t handle, uint8_t buffer_id) {
    uint8_t params[1] = {buffer_id};
    uint8_t response[32];
    uint16_t len;

    esp_err_t ret = send_command(handle, FP_CMD_IMAGE2TZ, params, 1, response, &len);
    if (ret != ESP_OK) {
        return ret;
    }

    return (response[0] == FP_OK) ? ESP_OK : ESP_FAIL;
}

esp_err_t fingerprint_search(fingerprint_handle_t handle, uint16_t *fingerprint_id, uint16_t *score) {
    uint8_t params[5] = {0x01, 0x00, 0x00, 0x00, 0x14};  // Buffer 1, start 0, count 20
    uint8_t response[32];
    uint16_t len;

    esp_err_t ret = send_command(handle, FP_CMD_SEARCH, params, 5, response, &len);
    if (ret != ESP_OK) {
        return ret;
    }

    if (response[0] == FP_OK) {
        *fingerprint_id = (response[1] << 8) | response[2];
        *score = (response[3] << 8) | response[4];
        return ESP_OK;
    } else if (response[0] == FP_NOTFOUND) {
        return ESP_ERR_NOT_FOUND;
    }

    return ESP_FAIL;
}

esp_err_t fingerprint_create_model(fingerprint_handle_t handle) {
    uint8_t response[32];
    uint16_t len;

    esp_err_t ret = send_command(handle, FP_CMD_REGMODEL, NULL, 0, response, &len);
    if (ret != ESP_OK) {
        return ret;
    }

    return (response[0] == FP_OK) ? ESP_OK : ESP_FAIL;
}

esp_err_t fingerprint_store_model(fingerprint_handle_t handle, uint16_t location) {
    uint8_t params[3] = {0x01, (location >> 8) & 0xFF, location & 0xFF};
    uint8_t response[32];
    uint16_t len;

    esp_err_t ret = send_command(handle, FP_CMD_STORE, params, 3, response, &len);
    if (ret != ESP_OK) {
        return ret;
    }

    return (response[0] == FP_OK) ? ESP_OK : ESP_FAIL;
}

esp_err_t fingerprint_get_template_count(fingerprint_handle_t handle, uint16_t *count) {
    uint8_t response[32];
    uint16_t len;

    esp_err_t ret = send_command(handle, FP_CMD_TEMPLATECOUNT, NULL, 0, response, &len);
    if (ret != ESP_OK) {
        return ret;
    }

    if (response[0] == FP_OK) {
        *count = (response[1] << 8) | response[2];
        return ESP_OK;
    }

    return ESP_FAIL;
}

esp_err_t fingerprint_delete_model(fingerprint_handle_t handle, uint16_t location) {
    uint8_t params[4] = {(location >> 8) & 0xFF, location & 0xFF, 0x00, 0x01};
    uint8_t response[32];
    uint16_t len;

    esp_err_t ret = send_command(handle, FP_CMD_DELETE, params, 4, response, &len);
    if (ret != ESP_OK) {
        return ret;
    }

    return (response[0] == FP_OK) ? ESP_OK : ESP_FAIL;
}

esp_err_t fingerprint_empty_database(fingerprint_handle_t handle) {
    uint8_t response[32];
    uint16_t len;

    esp_err_t ret = send_command(handle, FP_CMD_EMPTY, NULL, 0, response, &len);
    if (ret != ESP_OK) {
        return ret;
    }

    return (response[0] == FP_OK) ? ESP_OK : ESP_FAIL;
}