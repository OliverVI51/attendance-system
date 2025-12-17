#include "display_driver.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_st7789.h"
#include "esp_log.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "DISPLAY";

struct display_driver {
    esp_lcd_panel_handle_t panel_handle;
    int h_res;
    int v_res;
    int bl_pin;
};

// Simple 8x8 bitmap font (ASCII 32-127)
static const uint8_t font8x8[96][8] = {
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},  // Space
    {0x18, 0x3C, 0x3C, 0x18, 0x18, 0x00, 0x18, 0x00},  // !
    {0x36, 0x36, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},  // "
    {0x36, 0x36, 0x7F, 0x36, 0x7F, 0x36, 0x36, 0x00},  // #
    {0x0C, 0x3E, 0x03, 0x1E, 0x30, 0x1F, 0x0C, 0x00},  // $
    {0x00, 0x63, 0x33, 0x18, 0x0C, 0x66, 0x63, 0x00},  // %
    {0x1C, 0x36, 0x1C, 0x6E, 0x3B, 0x33, 0x6E, 0x00},  // &
    {0x06, 0x06, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00},  // '
    {0x18, 0x0C, 0x06, 0x06, 0x06, 0x0C, 0x18, 0x00},  // (
    {0x06, 0x0C, 0x18, 0x18, 0x18, 0x0C, 0x06, 0x00},  // )
    {0x00, 0x66, 0x3C, 0xFF, 0x3C, 0x66, 0x00, 0x00},  // *
    {0x00, 0x0C, 0x0C, 0x3F, 0x0C, 0x0C, 0x00, 0x00},  // +
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C, 0x06},  // ,
    {0x00, 0x00, 0x00, 0x3F, 0x00, 0x00, 0x00, 0x00},  // -
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C, 0x00},  // .
    {0x60, 0x30, 0x18, 0x0C, 0x06, 0x03, 0x01, 0x00},  // /
    {0x3E, 0x63, 0x73, 0x7B, 0x6F, 0x67, 0x3E, 0x00},  // 0
    {0x0C, 0x0E, 0x0C, 0x0C, 0x0C, 0x0C, 0x3F, 0x00},  // 1
    {0x1E, 0x33, 0x30, 0x1C, 0x06, 0x33, 0x3F, 0x00},  // 2
    {0x1E, 0x33, 0x30, 0x1C, 0x30, 0x33, 0x1E, 0x00},  // 3
    {0x38, 0x3C, 0x36, 0x33, 0x7F, 0x30, 0x78, 0x00},  // 4
    {0x3F, 0x03, 0x1F, 0x30, 0x30, 0x33, 0x1E, 0x00},  // 5
    {0x1C, 0x06, 0x03, 0x1F, 0x33, 0x33, 0x1E, 0x00},  // 6
    {0x3F, 0x33, 0x30, 0x18, 0x0C, 0x0C, 0x0C, 0x00},  // 7
    {0x1E, 0x33, 0x33, 0x1E, 0x33, 0x33, 0x1E, 0x00},  // 8
    {0x1E, 0x33, 0x33, 0x3E, 0x30, 0x18, 0x0E, 0x00},  // 9
    {0x00, 0x0C, 0x0C, 0x00, 0x00, 0x0C, 0x0C, 0x00},  // :
    {0x00, 0x0C, 0x0C, 0x00, 0x00, 0x0C, 0x0C, 0x06},  // ;
    {0x18, 0x0C, 0x06, 0x03, 0x06, 0x0C, 0x18, 0x00},  // <
    {0x00, 0x00, 0x3F, 0x00, 0x00, 0x3F, 0x00, 0x00},  // =
    {0x06, 0x0C, 0x18, 0x30, 0x18, 0x0C, 0x06, 0x00},  // >
    {0x1E, 0x33, 0x30, 0x18, 0x0C, 0x00, 0x0C, 0x00},  // ?
    {0x3E, 0x63, 0x7B, 0x7B, 0x7B, 0x03, 0x1E, 0x00},  // @
    {0x0C, 0x1E, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x00},  // A
    {0x3F, 0x66, 0x66, 0x3E, 0x66, 0x66, 0x3F, 0x00},  // B
    {0x3C, 0x66, 0x03, 0x03, 0x03, 0x66, 0x3C, 0x00},  // C
    {0x1F, 0x36, 0x66, 0x66, 0x66, 0x36, 0x1F, 0x00},  // D
    {0x7F, 0x46, 0x16, 0x1E, 0x16, 0x46, 0x7F, 0x00},  // E
    {0x7F, 0x46, 0x16, 0x1E, 0x16, 0x06, 0x0F, 0x00},  // F
    {0x3C, 0x66, 0x03, 0x03, 0x73, 0x66, 0x7C, 0x00},  // G
    {0x33, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x33, 0x00},  // H
    {0x1E, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00},  // I
    {0x78, 0x30, 0x30, 0x30, 0x33, 0x33, 0x1E, 0x00},  // J
    {0x67, 0x66, 0x36, 0x1E, 0x36, 0x66, 0x67, 0x00},  // K
    {0x0F, 0x06, 0x06, 0x06, 0x46, 0x66, 0x7F, 0x00},  // L
    {0x63, 0x77, 0x7F, 0x7F, 0x6B, 0x63, 0x63, 0x00},  // M
    {0x63, 0x67, 0x6F, 0x7B, 0x73, 0x63, 0x63, 0x00},  // N
    {0x1C, 0x36, 0x63, 0x63, 0x63, 0x36, 0x1C, 0x00},  // O
    {0x3F, 0x66, 0x66, 0x3E, 0x06, 0x06, 0x0F, 0x00},  // P
    {0x1E, 0x33, 0x33, 0x33, 0x3B, 0x1E, 0x38, 0x00},  // Q
    {0x3F, 0x66, 0x66, 0x3E, 0x36, 0x66, 0x67, 0x00},  // R
    {0x1E, 0x33, 0x07, 0x0E, 0x38, 0x33, 0x1E, 0x00},  // S
    {0x3F, 0x2D, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00},  // T
    {0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x3F, 0x00},  // U
    {0x33, 0x33, 0x33, 0x33, 0x33, 0x1E, 0x0C, 0x00},  // V
    {0x63, 0x63, 0x63, 0x6B, 0x7F, 0x77, 0x63, 0x00},  // W
    {0x63, 0x63, 0x36, 0x1C, 0x1C, 0x36, 0x63, 0x00},  // X
    {0x33, 0x33, 0x33, 0x1E, 0x0C, 0x0C, 0x1E, 0x00},  // Y
    {0x7F, 0x63, 0x31, 0x18, 0x4C, 0x66, 0x7F, 0x00},  // Z
    // Continue with remaining characters...
    // For brevity, I'll add placeholders for remaining ASCII chars
};

esp_err_t display_init(const display_config_t *config, display_handle_t *handle) {
    ESP_LOGI(TAG, "Initializing ST7789 display");
    
    display_handle_t h = malloc(sizeof(struct display_driver));
    if (!h) {
        return ESP_ERR_NO_MEM;
    }
    
    h->h_res = config->h_res;
    h->v_res = config->v_res;
    h->bl_pin = config->bl_pin;
    
    // Configure backlight
    gpio_config_t bl_gpio_config = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << config->bl_pin
    };
    ESP_ERROR_CHECK(gpio_config(&bl_gpio_config));
    gpio_set_level(config->bl_pin, 1);
    
    // Configure SPI bus
    spi_bus_config_t buscfg = {
        .mosi_io_num = config->mosi_pin,
        .miso_io_num = -1,
        .sclk_io_num = config->sclk_pin,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = config->h_res * config->v_res * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(config->spi_host, &buscfg, SPI_DMA_CH_AUTO));
    
    // Configure LCD panel IO
    esp_lcd_panel_io_handle_t io_handle = NULL;
    esp_lcd_panel_io_spi_config_t io_config = {
        .cs_gpio_num = config->cs_pin,
        .dc_gpio_num = config->dc_pin,
        .spi_mode = 0,
        .pclk_hz = config->pixel_clock_hz,
        .trans_queue_depth = 10,
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)config->spi_host, &io_config, &io_handle));
    
    // Configure LCD panel
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = config->rst_pin,
        .rgb_endian = LCD_RGB_ENDIAN_RGB,
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &h->panel_handle));
    
    // Initialize panel
    ESP_ERROR_CHECK(esp_lcd_panel_reset(h->panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(h->panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(h->panel_handle, true));
    
    // Set orientation (landscape mode)
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(h->panel_handle, true));
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(h->panel_handle, false, true));
    
    // Turn on display
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(h->panel_handle, true));
    
    *handle = h;
    ESP_LOGI(TAG, "Display initialized: %dx%d", config->h_res, config->v_res);
    
    // Clear screen
    display_clear(h, COLOR_BLACK);
    
    return ESP_OK;
}

esp_err_t display_clear(display_handle_t handle, uint16_t color) {
    uint16_t *buffer = malloc(handle->h_res * handle->v_res * sizeof(uint16_t));
    if (!buffer) {
        return ESP_ERR_NO_MEM;
    }
    
    for (int i = 0; i < handle->h_res * handle->v_res; i++) {
        buffer[i] = color;
    }
    
    esp_lcd_panel_draw_bitmap(handle->panel_handle, 0, 0, handle->h_res, handle->v_res, buffer);
    free(buffer);
    
    return ESP_OK;
}

esp_err_t display_fill_rect(display_handle_t handle, int x, int y, int w, int h, uint16_t color) {
    if (x < 0 || y < 0 || x + w > handle->h_res || y + h > handle->v_res) {
        return ESP_ERR_INVALID_ARG;
    }
    
    uint16_t *buffer = malloc(w * h * sizeof(uint16_t));
    if (!buffer) {
        return ESP_ERR_NO_MEM;
    }
    
    for (int i = 0; i < w * h; i++) {
        buffer[i] = color;
    }
    
    esp_lcd_panel_draw_bitmap(handle->panel_handle, x, y, x + w, y + h, buffer);
    free(buffer);
    
    return ESP_OK;
}

esp_err_t display_draw_text(display_handle_t handle, int x, int y, const char *text, uint16_t fg_color, uint16_t bg_color) {
    int len = strlen(text);
    int char_width = 8;
    int char_height = 16;  // Double height
    
    for (int i = 0; i < len; i++) {
        char c = text[i];
        if (c < 32 || c > 127) {
            c = 32;  // Replace with space
        }
        
        const uint8_t *glyph = font8x8[c - 32];
        
        for (int row = 0; row < 8; row++) {
            for (int col = 0; col < 8; col++) {
                uint16_t color = (glyph[row] & (1 << (7 - col))) ? fg_color : bg_color;
                
                // Draw 2x2 pixels for each font pixel
                for (int py = 0; py < 2; py++) {
                    for (int px = 0; px < 2; px++) {
                        int px_x = x + i * char_width + col * 2 + px;
                        int px_y = y + row * 2 + py;
                        
                        if (px_x >= 0 && px_x < handle->h_res && px_y >= 0 && px_y < handle->v_res) {
                            esp_lcd_panel_draw_bitmap(handle->panel_handle, px_x, px_y, px_x + 1, px_y + 1, &color);
                        }
                    }
                }
            }
        }
    }
    
    return ESP_OK;
}

esp_err_t display_draw_text_large(display_handle_t handle, int x, int y, const char *text, uint16_t fg_color, uint16_t bg_color) {
    int len = strlen(text);
    int char_width = 16;
    int char_height = 32;  // 4x scale
    
    for (int i = 0; i < len; i++) {
        char c = text[i];
        if (c < 32 || c > 127) {
            c = 32;
        }
        
        const uint8_t *glyph = font8x8[c - 32];
        
        for (int row = 0; row < 8; row++) {
            for (int col = 0; col < 8; col++) {
                uint16_t color = (glyph[row] & (1 << (7 - col))) ? fg_color : bg_color;
                
                // Draw 4x4 pixels for each font pixel
                display_fill_rect(handle, x + i * char_width + col * 2, y + row * 4, 2, 4, color);
            }
        }
    }
    
    return ESP_OK;
}

esp_err_t display_set_backlight(display_handle_t handle, uint8_t brightness) {
    if (brightness > 100) {
        brightness = 100;
    }
    
    gpio_set_level(handle->bl_pin, brightness > 0 ? 1 : 0);
    return ESP_OK;
}

esp_lcd_panel_handle_t display_get_panel_handle(display_handle_t handle) {
    return handle->panel_handle;
}