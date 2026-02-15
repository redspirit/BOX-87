#include "vga.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include <cstring>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace VGA {
    static esp_lcd_panel_handle_t panel_handle = NULL;
    static uint8_t *fb[2] = {NULL, NULL};
    
    // Индекс текущего буфера для рисования (Back Buffer)
    static int back_fb_idx = 0;

    // Флаг готовности (VSync)
    static volatile bool ready_to_draw = false;

    // === CALLBACK ДЛЯ VSYNC ===
    // Вызывается из прерывания, когда монитор закончил кадр
    static bool on_vsync_event(esp_lcd_panel_handle_t panel, const esp_lcd_rgb_panel_event_data_t *edata, void *user_ctx) {
        ready_to_draw = true;
        return false;
    }

    void init() {
        // Конфигурация таймингов (Ваши идеальные настройки для 24MHz)
        esp_lcd_rgb_panel_config_t panel_config = {
            .clk_src = LCD_CLK_SRC_DEFAULT,
            .timings = {
                .pclk_hz = 24000000,        // 24 MHz ровно
                .h_res = VGA_WIDTH,
                .v_res = VGA_HEIGHT,
                .hsync_pulse_width = 64,
                .hsync_back_porch = 40,
                .hsync_front_porch = 16,
                .vsync_pulse_width = 2,
                .vsync_back_porch = 33,
                .vsync_front_porch = 10,
                .flags = {
                    .hsync_idle_low = 1,
                    .vsync_idle_low = 1,
                },
            },
            .data_width = 8,
            .bits_per_pixel = 0,
            .num_fbs = 2, // Включаем ДВОЙНОЙ БУФЕР
            .bounce_buffer_size_px = 0,
            .sram_trans_align = 8,
            .psram_trans_align = 64,
            .hsync_gpio_num = VGA_PIN_HSYNC,
            .vsync_gpio_num = VGA_PIN_VSYNC,
            .de_gpio_num = VGA_PIN_DE,
            .pclk_gpio_num = VGA_PIN_PCLK,
            .disp_gpio_num = -1,
            .data_gpio_nums = {
                VGA_PIN_DATA_0, VGA_PIN_DATA_1, VGA_PIN_DATA_2, 
                VGA_PIN_DATA_3, VGA_PIN_DATA_4, VGA_PIN_DATA_5, 
                VGA_PIN_DATA_6, VGA_PIN_DATA_7,
            },
            .flags = {
                .fb_in_psram = 1, // Обязательно в PSRAM
            },
        };

        // Инициализация драйвера
        ESP_ERROR_CHECK(esp_lcd_new_rgb_panel(&panel_config, &panel_handle));
        
        // Регистрация VSync callback
        esp_lcd_rgb_panel_event_callbacks_t cbs = {
            .on_vsync = on_vsync_event,
        };
        ESP_ERROR_CHECK(esp_lcd_rgb_panel_register_event_callbacks(panel_handle, &cbs, NULL));

        // Сброс и запуск
        ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
        ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));

        // Получение указателей на оба буфера
        void *fb0 = NULL;
        void *fb1 = NULL;
        ESP_ERROR_CHECK(esp_lcd_rgb_panel_get_frame_buffer(panel_handle, 2, &fb0, &fb1));
        
        fb[0] = (uint8_t*)fb0;
        fb[1] = (uint8_t*)fb1;
        
        // Очистка обоих буферов при старте
        memset(fb[0], 0, VGA_WIDTH * VGA_HEIGHT);
        memset(fb[1], 0, VGA_WIDTH * VGA_HEIGHT);
    }

    void clear(uint8_t color) {
        memset(fb[back_fb_idx], color, VGA_WIDTH * VGA_HEIGHT);
    }
    
    void clearAll(uint8_t color) {
        for (int i = 0; i < 2; i++) {
            if (fb[i] != NULL) {
                memset(fb[i], color, VGA_WIDTH * VGA_HEIGHT);
            }
        }
    }

    void dot(int x, int y, uint8_t color) {
        if (x < 0 || x >= VGA_WIDTH || y < 0 || y >= VGA_HEIGHT) return;
        fb[back_fb_idx][y * VGA_WIDTH + x] = color;
    }

    void fillRect(int x, int y, int w, int h, uint8_t color) {
        // Отсечение (Clipping)
        if (x >= VGA_WIDTH || y >= VGA_HEIGHT) return;
        if (x + w > VGA_WIDTH) w = VGA_WIDTH - x;
        if (y + h > VGA_HEIGHT) h = VGA_HEIGHT - y;
        if (x < 0) { w += x; x = 0; }
        if (y < 0) { h += y; y = 0; }
        if (w <= 0 || h <= 0) return;

        uint8_t *buffer = fb[back_fb_idx];
        for (int row = 0; row < h; row++) {
            memset(&buffer[(y + row) * VGA_WIDTH + x], color, w);
        }
    }

    void show() {
        while (!ready_to_draw) {
            // пока так, но если что можно использовать бинарный семафор 
            vTaskDelay(1);
        }
        ready_to_draw = false; 

        esp_lcd_panel_draw_bitmap(panel_handle, 0, 0, VGA_WIDTH, VGA_HEIGHT, fb[back_fb_idx]);

        // СМЕНА БУФЕРОВ (SWAP)
        back_fb_idx = (back_fb_idx == 0) ? 1 : 0;
    }

    uint8_t* getBuffer() {
        return fb[back_fb_idx];
    }

}