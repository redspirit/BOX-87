#include "VGA.h"
#include <esp_rom_gpio.h>
#include <esp_rom_sys.h>
#include <hal/gpio_hal.h>
#include <driver/periph_ctrl.h>
#include <driver/gpio.h>
#include <soc/lcd_cam_struct.h>
#include <esp_private/gdma.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"

#define HAL_FORCE_MODIFY_U32_REG_FIELD(base_reg, reg_field, field_val) { \
    uint32_t temp_val = base_reg.val; \
    typeof(base_reg) temp_reg; \
    temp_reg.val = temp_val; \
    temp_reg.reg_field = (field_val); \
    (base_reg).val = temp_reg.val; \
}

namespace VGA {
    // Внутренние переменные (замена приватным полям класса)
    namespace {
        Mode _mode;
        int _bufferCount = 2;
        int _bits;
        PinConfig _pins;
        int _backBuffer = 0;
        DMAVideoBuffer* _dmaBuffer = nullptr;
        bool _usePsram = true;
        int _dmaChannel = 0;
        SemaphoreHandle_t _vgaMutex = nullptr;
    }

    // Внутренняя утилита
    void attachPinToSignal(int pin, int signal) {
        esp_rom_gpio_connect_out_signal(pin, signal, false, false);
        gpio_hal_iomux_func_sel(GPIO_PIN_MUX_REG[pin], PIN_FUNC_GPIO);
        gpio_set_drive_capability((gpio_num_t)pin, (gpio_drive_cap_t)3);
    }

    bool init(const PinConfig cfgPins, const Mode mode, int bits) {
        _pins = cfgPins;
        _mode = mode;
        _bits = bits;
        _backBuffer = 0;

        if (_vgaMutex == nullptr) _vgaMutex = xSemaphoreCreateMutex();

        _dmaBuffer = new DMAVideoBuffer(_mode.vRes, _mode.hRes * (_bits / 8), _mode.vClones, true, _usePsram, _bufferCount);
        if (!_dmaBuffer->isValid()) {
            delete _dmaBuffer;
            return false;
        }

        periph_module_enable(PERIPH_LCD_CAM_MODULE);
        periph_module_reset(PERIPH_LCD_CAM_MODULE);
        LCD_CAM.lcd_user.lcd_reset = 1;
        esp_rom_delay_us(100);

        int N = round(240000000.0 / (double)_mode.frequency);
        if (N < 2) N = 2;

        LCD_CAM.lcd_clock.clk_en = 1;
        LCD_CAM.lcd_clock.lcd_clk_sel = 2; // PLL240M
        LCD_CAM.lcd_clock.lcd_clkm_div_a = 0;
        LCD_CAM.lcd_clock.lcd_clkm_div_b = 0;
        LCD_CAM.lcd_clock.lcd_clkm_div_num = N;
        LCD_CAM.lcd_clock.lcd_ck_out_edge = 0;
        LCD_CAM.lcd_clock.lcd_ck_idle_edge = 0;
        LCD_CAM.lcd_clock.lcd_clk_equ_sysclk = 1;

        LCD_CAM.lcd_ctrl.lcd_rgb_mode_en = 1;
        LCD_CAM.lcd_user.lcd_2byte_en = (_bits == 8) ? 0 : 1;
        LCD_CAM.lcd_user.lcd_dout = 1;
        LCD_CAM.lcd_user.lcd_always_out_en = 1;
        LCD_CAM.lcd_ctrl2.lcd_hsync_idle_pol = _mode.hPol ^ 1;
        LCD_CAM.lcd_ctrl2.lcd_vsync_idle_pol = _mode.vPol ^ 1;
        LCD_CAM.lcd_ctrl2.lcd_de_idle_pol = 1;

        LCD_CAM.lcd_misc.lcd_bk_en = 1;
        LCD_CAM.lcd_ctrl2.lcd_hsync_width = _mode.hSync - 1;
        LCD_CAM.lcd_ctrl.lcd_hb_front = _mode.blankHorizontal() - 1;
        LCD_CAM.lcd_ctrl1.lcd_ha_width = _mode.hRes - 1;
        LCD_CAM.lcd_ctrl1.lcd_ht_width = _mode.totalHorizontal();
        LCD_CAM.lcd_ctrl2.lcd_vsync_width = _mode.vSync - 1;

        HAL_FORCE_MODIFY_U32_REG_FIELD(LCD_CAM.lcd_ctrl1, lcd_vb_front, _mode.vSync + _mode.vBack - 1);
        LCD_CAM.lcd_ctrl.lcd_va_height = _mode.vRes * _mode.vClones - 1;
        LCD_CAM.lcd_ctrl.lcd_vt_height = _mode.totalVertical() - 1;

        LCD_CAM.lcd_ctrl2.lcd_hs_blank_en = 1;
        HAL_FORCE_MODIFY_U32_REG_FIELD(LCD_CAM.lcd_ctrl2, lcd_hsync_position, 0);
        LCD_CAM.lcd_misc.lcd_next_frame_en = 1;

        // Конфигурация пинов
        if (_bits == 8) {
            int p[] = {
				_pins.r[0], _pins.r[1], _pins.r[2], 
				_pins.g[0], _pins.g[1], _pins.g[2], 
				_pins.b[1], _pins.b[2]
			};
            for (int i = 0; i < 8; i++) 
				if (p[i] >= 0) 
					attachPinToSignal(p[i], LCD_DATA_OUT0_IDX + i);
        } else {
            int p[] = {
				_pins.r[0], _pins.r[1], _pins.r[2], 
				_pins.g[0], _pins.g[1], _pins.g[2], 
				_pins.b[0], _pins.b[1], _pins.b[2]
			};
            for (int i = 0; i < 9; i++) 
				if (p[i] >= 0) 
					attachPinToSignal(p[i], LCD_DATA_OUT0_IDX + i);
        }

        attachPinToSignal(_pins.hSync, LCD_H_SYNC_IDX);
        attachPinToSignal(_pins.vSync, LCD_V_SYNC_IDX);

        gdma_channel_alloc_config_t dma_chan_config = { .direction = GDMA_CHANNEL_DIRECTION_TX };
        gdma_channel_handle_t dmaCh;
        gdma_new_channel(&dma_chan_config, &dmaCh);
        _dmaChannel = (int)dmaCh;
        gdma_connect(dmaCh, GDMA_MAKE_TRIGGER(GDMA_TRIG_PERIPH_LCD, 0));
        gdma_transfer_ability_t ability = { .sram_trans_align = 4, .psram_trans_align = 64 };
        gdma_set_transfer_ability(dmaCh, &ability);

        return true;
    }

    bool start() {
        gdma_reset((gdma_channel_handle_t)_dmaChannel);
        esp_rom_delay_us(1);
        LCD_CAM.lcd_user.lcd_start = 0;
        LCD_CAM.lcd_user.lcd_update = 1;
        esp_rom_delay_us(1);
        LCD_CAM.lcd_misc.lcd_afifo_reset = 1;
        LCD_CAM.lcd_user.lcd_update = 1;
        gdma_start((gdma_channel_handle_t)_dmaChannel, (intptr_t)_dmaBuffer->getDescriptor());
        esp_rom_delay_us(1);
        LCD_CAM.lcd_user.lcd_update = 1;
        LCD_CAM.lcd_user.lcd_start = 1;
        return true;
    }

    bool show() {
        _dmaBuffer->flush(_backBuffer);
        if (_bufferCount <= 1) return true;
        _dmaBuffer->attachBuffer(_backBuffer);
        _backBuffer = (_backBuffer + 1) % _bufferCount;
        return true;
    }

	void stop() {
        if (_vgaMutex) xSemaphoreTake(_vgaMutex, portMAX_DELAY);
        
        // 1. Останавливаем LCD_CAM
        LCD_CAM.lcd_user.lcd_start = 0;
        LCD_CAM.lcd_user.lcd_update = 1;

        // 2. Останавливаем и сбрасываем GDMA
        if (_dmaChannel) {
            gdma_stop((gdma_channel_handle_t)_dmaChannel);
            gdma_reset((gdma_channel_handle_t)_dmaChannel);
        }

        // 3. Удаляем старый буфер, чтобы освободить память (особенно PSRAM)
        if (_dmaBuffer) {
            delete _dmaBuffer;
            _dmaBuffer = nullptr;
        }

        if (_vgaMutex) xSemaphoreGive(_vgaMutex);
    }	

	bool changeMode(const Mode newMode, int newBits) {	
        // Останавливаем текущий вывод
        stop();
        
        // Инициализируем заново с новыми параметрами
        if (init(_pins, newMode, newBits)) {
            return start();
        }
        return false;
    }	

    int width() { return _mode.hRes; }
    int height() { return _mode.vRes; }

    void dot8(int x, int y, int rgb) {
        if (x >= _mode.hRes || y >= _mode.vRes || x < 0 || y < 0) return;
        _dmaBuffer->getLineAddr8(y, _backBuffer)[x] = rgb;
    }

    void clear(uint8_t color) {
        _dmaBuffer->clearFast(color, _backBuffer);
    }

	void clearAll(uint8_t color) {
        if (!_dmaBuffer) return;

        // Блокируем, чтобы во время очистки не произошло переключение show()
        lock(); 
        for (int i = 0; i < _bufferCount; i++) {
            _dmaBuffer->clearFast(color, i);
        }
        unlock();
    }	

    void fillRect8(int x, int y, int w, int h, int rgb) {
        int x2 = std::min(x + w, (int)_mode.hRes);
        int y2 = std::min(y + h, (int)_mode.vRes);
        x = std::max(x, 0);
        y = std::max(y, 0);

        for (int yy = y; yy < y2; yy++) {
            uint8_t* line = _dmaBuffer->getLineAddr8(yy, _backBuffer);
            for (int xx = x; xx < x2; xx++) line[xx] = rgb;
        }
    }

    void lock() { if (_vgaMutex) xSemaphoreTake(_vgaMutex, portMAX_DELAY); }
    void unlock() { if (_vgaMutex) xSemaphoreGive(_vgaMutex); }

    uint8_t* getLinePtr8(int y) { return _dmaBuffer->getLineAddr8(y, _backBuffer); }
}