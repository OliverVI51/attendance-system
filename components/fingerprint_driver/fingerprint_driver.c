#include "fingerprint_driver.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "FP_DRIVER";

// Internal driver structure
struct fingerprint_driver {
    int uart_num;
    int tx_pin;
    int rx_pin;
    int baud_rate;
    uint32_t address;
};

// Allocate memory for the handle
static struct fingerprint_driver g_fp_dev;

// --- Helper Functions ---

static void send_packet(int uart_num, uint8_t pid, uint8_t cmd, uint8_t *data, uint16_t data_len) {
    // Packet Structure: Header(2) + Addr(4) + PID(1) + Len(2) + Cmd(1) + Data(N) + Checksum(2)
    uint16_t packet_len = 1 + data_len + 2; // Cmd + Data + Checksum
    
    uint16_t checksum = 0x01 + (uint8_t)(packet_len >> 8) + (uint8_t)(packet_len & 0xFF) + cmd;
    for (int i = 0; i < data_len; i++) {
        checksum += data[i];
    }
    
    // Header & Address
    const uint8_t header[] = {0xEF, 0x01, 0xFF, 0xFF, 0xFF, 0xFF};
    uart_write_bytes(uart_num, (const char*)header, 6);
    
    // PID & Length
    uint8_t pid_len[] = {pid, (uint8_t)(packet_len >> 8), (uint8_t)(packet_len & 0xFF)};
    uart_write_bytes(uart_num, (const char*)pid_len, 3);
    
    // Command
    uart_write_bytes(uart_num, (const char*)&cmd, 1);
    
    // Data
    if (data_len > 0) {
        uart_write_bytes(uart_num, (const char*)data, data_len);
    }
    
    // Checksum
    uint8_t sum[] = {(uint8_t)(checksum >> 8), (uint8_t)(checksum & 0xFF)};
    uart_write_bytes(uart_num, (const char*)sum, 2);
}

static esp_err_t read_ack(int uart_num, uint8_t *confirm_code) {
    uint8_t buf[20];
    // Min Ack Packet is 12 bytes
    int len = uart_read_bytes(uart_num, buf, 12, pdMS_TO_TICKS(1000));
    
    if (len >= 9 && buf[0] == 0xEF && buf[1] == 0x01) {
        // Confirmation code is usually at offset 9 (Header 2 + Addr 4 + PID 1 + Len 2)
        *confirm_code = buf[9];
        return ESP_OK;
    }
    return ESP_ERR_TIMEOUT;
}

// --- API Implementation ---

esp_err_t fingerprint_init(const fingerprint_config_t *config, fingerprint_handle_t *handle) {
    if (config == NULL || handle == NULL) return ESP_ERR_INVALID_ARG;

    uart_config_t uart_cfg = {
        .baud_rate = config->baud_rate,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    ESP_ERROR_CHECK(uart_driver_install(config->uart_num, 1024, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(config->uart_num, &uart_cfg));
    ESP_ERROR_CHECK(uart_set_pin(config->uart_num, config->tx_pin, config->rx_pin, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    g_fp_dev.uart_num = config->uart_num;
    g_fp_dev.tx_pin = config->tx_pin;
    g_fp_dev.rx_pin = config->rx_pin;
    g_fp_dev.baud_rate = config->baud_rate;
    g_fp_dev.address = config->address;

    *handle = &g_fp_dev;
    return ESP_OK;
}

esp_err_t fingerprint_get_image(fingerprint_handle_t handle) {
    struct fingerprint_driver *dev = (struct fingerprint_driver *)handle;
    uart_flush_input(dev->uart_num);
    send_packet(dev->uart_num, 0x01, FP_CMD_GETIMAGE, NULL, 0);
    
    uint8_t code;
    if (read_ack(dev->uart_num, &code) == ESP_OK && code == FP_OK) {
        return ESP_OK;
    }
    return ESP_FAIL;
}

esp_err_t fingerprint_image_to_tz(fingerprint_handle_t handle, uint8_t buffer_id) {
    struct fingerprint_driver *dev = (struct fingerprint_driver *)handle;
    uart_flush_input(dev->uart_num);
    send_packet(dev->uart_num, 0x01, FP_CMD_IMAGE2TZ, &buffer_id, 1);
    
    uint8_t code;
    if (read_ack(dev->uart_num, &code) == ESP_OK && code == FP_OK) {
        return ESP_OK;
    }
    return ESP_FAIL;
}

esp_err_t fingerprint_search(fingerprint_handle_t handle, uint16_t *fingerprint_id, uint16_t *score) {
    struct fingerprint_driver *dev = (struct fingerprint_driver *)handle;
    uart_flush_input(dev->uart_num);
    
    // BufferID (1), StartPage (2), PageNum (2)
    uint8_t data[] = {0x01, 0x00, 0x00, 0x00, 0xC8}; // Search 0 to 200
    send_packet(dev->uart_num, 0x01, FP_CMD_SEARCH, data, 5);
    
    uint8_t buf[16];
    int len = uart_read_bytes(dev->uart_num, buf, 16, pdMS_TO_TICKS(1000));
    
    if (len >= 12 && buf[9] == FP_OK) {
        *fingerprint_id = (buf[10] << 8) | buf[11];
        *score = (buf[12] << 8) | buf[13];
        return ESP_OK;
    }
    return ESP_FAIL;
}

esp_err_t fingerprint_create_model(fingerprint_handle_t handle) {
    struct fingerprint_driver *dev = (struct fingerprint_driver *)handle;
    uart_flush_input(dev->uart_num);
    send_packet(dev->uart_num, 0x01, FP_CMD_REGMODEL, NULL, 0);
    
    uint8_t code;
    if (read_ack(dev->uart_num, &code) == ESP_OK && code == FP_OK) {
        return ESP_OK;
    }
    return ESP_FAIL;
}

esp_err_t fingerprint_store_model(fingerprint_handle_t handle, uint16_t location) {
    struct fingerprint_driver *dev = (struct fingerprint_driver *)handle;
    uart_flush_input(dev->uart_num);
    
    // BufferID (1), PageID (2)
    uint8_t data[] = {0x01, (uint8_t)(location >> 8), (uint8_t)(location & 0xFF)};
    send_packet(dev->uart_num, 0x01, FP_CMD_STORE, data, 3);
    
    uint8_t code;
    if (read_ack(dev->uart_num, &code) == ESP_OK && code == FP_OK) {
        return ESP_OK;
    }
    return ESP_FAIL;
}

esp_err_t fingerprint_delete_model(fingerprint_handle_t handle, uint16_t location) {
    struct fingerprint_driver *dev = (struct fingerprint_driver *)handle;
    uart_flush_input(dev->uart_num);
    
    // PageID (2), N (2)
    uint8_t data[] = {
        (uint8_t)(location >> 8), 
        (uint8_t)(location & 0xFF),
        0x00, 0x01 // Delete 1
    };
    send_packet(dev->uart_num, 0x01, FP_CMD_DELETE, data, 4);
    
    uint8_t code;
    if (read_ack(dev->uart_num, &code) == ESP_OK && code == FP_OK) {
        return ESP_OK;
    }
    return ESP_FAIL;
}

esp_err_t fingerprint_empty_database(fingerprint_handle_t handle) {
    struct fingerprint_driver *dev = (struct fingerprint_driver *)handle;
    uart_flush_input(dev->uart_num);
    send_packet(dev->uart_num, 0x01, FP_CMD_EMPTY, NULL, 0);
    
    uint8_t code;
    if (read_ack(dev->uart_num, &code) == ESP_OK && code == FP_OK) {
        return ESP_OK;
    }
    return ESP_FAIL;
}

esp_err_t fingerprint_get_template_count(fingerprint_handle_t handle, uint16_t *count) {
    struct fingerprint_driver *dev = (struct fingerprint_driver *)handle;
    uart_flush_input(dev->uart_num);
    send_packet(dev->uart_num, 0x01, FP_CMD_TEMPLATECOUNT, NULL, 0);
    
    uint8_t buf[14];
    int len = uart_read_bytes(dev->uart_num, buf, 14, pdMS_TO_TICKS(1000));
    
    if (len >= 12 && buf[9] == FP_OK) {
        *count = (buf[10] << 8) | buf[11];
        return ESP_OK;
    }
    return ESP_FAIL;
}

esp_err_t fingerprint_self_test(fingerprint_handle_t handle) {
    struct fingerprint_driver *dev = (struct fingerprint_driver *)handle;
    uart_flush_input(dev->uart_num);
    
    // Read Sys Param command (0x0F)
    send_packet(dev->uart_num, 0x01, FP_CMD_READSYSPARAM, NULL, 0);
    
    uint8_t code;
    if (read_ack(dev->uart_num, &code) == ESP_OK && code == FP_OK) {
        return ESP_OK;
    }
    return ESP_FAIL;
}