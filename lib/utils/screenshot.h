#pragma once

#include <stdint.h>

namespace Screenshot {
    /**
     * Сделать скриншот экрана и сохранить его на SD карту.
     * Файл сохраняется в корне SD карты с именем вида /screen_XXXXXXXX.bmp
     * где XXXXXXXX — количество секунд с момента старта.
     * 
     * @return true если скриншот успешно создан, false в случае ошибки
     */
    bool capture();
}
