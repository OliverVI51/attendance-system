#ifndef DISPLAY_DRIVER_H
#define DISPLAY_DRIVER_H

#include "esp_err.h"
#include "esp_lcd_panel_ops.h"
#include <stdint.h>

// RGB565 Color Definitions
#define COLOR_BLACK       0x0000
#define COLOR_WHITE       0xFFFF
#define COLOR_RED         0xF800
#define COLOR_GREEN       0x07E0
#define COLOR_BLUE        0x001F
#define COLOR_YELLOW      0xFFE0
#define COLOR_CYAN        0x07FF
#define COLOR_MAGENTA     0xF81F
#define COLOR_GRAY        0x8410
#define COLOR_DARKGRAY    0x4208
#define COLOR_ORANGE      0xFC00

// Display Configuration
typedef struct {
    int mosi_pin;
    int sclk_pin;
    int cs_pin;
    int dc_pin;
    int rst_pin;
    int bl_pin;
    int spi_host;
    int h_res;
    int v_res;
    int pixel_clock_hz;
} display_config_t;

// Display Handle
typedef struct display_driver* display_handle_t;

/**
 * @brief Initialize display
 */
esp_err_t display_init(const display_config_t *config, display_handle_t *handle);

/**
 * @brief Clear screen with color
 */
esp_err_t display_clear(display_handle_t handle, uint16_t color);

/**
 * @brief Draw filled rectangle
 */
esp_err_t display_fill_rect(display_handle_t handle, int x, int y, int w, int h, uint16_t color);

/**
 * @brief Draw text (simple 8x16 font)
 */
esp_err_t display_draw_text(display_handle_t handle, int x, int y, const char *text, uint16_t fg_color, uint16_t bg_color);

/**
 * @brief Draw large text (16x32 font)
 */
esp_err_t display_draw_text_large(display_handle_t handle, int x, int y, const char *text, uint16_t fg_color, uint16_t bg_color);

/**
 * @brief Set backlight brightness (0-100)
 */
esp_err_t display_set_backlight(display_handle_t handle, uint8_t brightness);

/**
 * @brief Get display handle for direct panel operations
 */
esp_lcd_panel_handle_t display_get_panel_handle(display_handle_t handle);

#endif // DISPLAY_DRIVER_H