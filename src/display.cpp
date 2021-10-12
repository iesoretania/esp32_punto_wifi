//
// Copyright (C) 2020-2021: Luis Ramón López López
//
// This file is part of "Punto de Control de Presencia Wi-Fi".
//
// "Punto de Control de Presencia Wi-Fi" is free software: you can redistribute
// it and/or modify it under the terms of the GNU General Public License as
// published by the Free Software Foundation, either version 3 of the License,
// or (at your option) any later version.
//
// "Punto de Control de Presencia Wi-Fi" is distributed in the hope that it will be
// useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with Foobar.  If not, see <https://www.gnu.org/licenses/>.
//

#include <Arduino.h>
#include <lvgl.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <punto_control.h>

#include "screens/scr_calibration.h"
#include "display.h"
#include "flash.h"

TFT_eSPI tft = TFT_eSPI();

/* Flushing en el display */
void espi_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    int32_t w = (area->x2 - area->x1 + 1);
    int32_t h = (area->y2 - area->y1 + 1);

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors(&color_p->full, w * h, false);    // importante que swap sea true en este display
    tft.endWrite();

    lv_disp_flush_ready(disp);
}

/* Lectura de la pantalla táctil */
void espi_touch_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
    uint16_t touchX, touchY;

    data->continue_reading = false;
    bool touched = tft.getTouch(&touchX, &touchY, 600);

    if (!touched) {
        data->state = LV_INDEV_STATE_REL;
        return;
    } else {
        data->state = LV_INDEV_STATE_PR;
    }

    /* Establecer coordenadas */
    data->point.x = touchX;
    data->point.y = touchY;
}

void initialize_gui() {
    // activar iluminación del display al 100%
    pinMode(BACKLIGHT_PIN, OUTPUT);
    ledcAttachPin(BACKLIGHT_PIN, 2);
    ledcSetup(2, 5000, 8);

    // recuperar brillo de pantalla
    display_set_brightness(flash_get_int("scr.brightness"));

    // inicializar LVGL
    lv_init();

    // Inicializar buffers del GUI
    static lv_disp_draw_buf_t disp_buf;
    static lv_color_t buf1[LV_HOR_RES_MAX * 10];

    lv_disp_draw_buf_init(&disp_buf, buf1, nullptr, LV_HOR_RES_MAX * 10);

    // Inicializar interfaz gráfica
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = SCREEN_WIDTH;
    disp_drv.ver_res = SCREEN_HEIGHT;
    disp_drv.antialiasing = 1;
    disp_drv.flush_cb = espi_disp_flush;
    disp_drv.draw_buf = &disp_buf;
    lv_disp_drv_register(&disp_drv);

    // Inicializar panel táctil
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = espi_touch_read;
    lv_indev_drv_register(&indev_drv);

    tft.begin();            /* Inicializar TFT */
    tft.setRotation(3);     /* Orientación horizontal */

    uint16_t calData[5];

    // Obtener datos de calibración del panel táctil
    if (!flash_get_blob("CalData", (uint8_t *) calData, sizeof(calData))) {
        // Cargar pantalla de calibración
        create_scr_calibration();
        load_scr_calibration();
        lv_task_handler();

        int startTime = millis();
        tft.calibrateTouch(calData, TFT_RED, TFT_WHITE, 20);
        if (millis() - startTime < 4200) {
            // Seguramente la calibración no se haya realizado con éxito si ha tardado menos de 4,2 segundos
            set_calibration_main_text("Ocurrió un error al calibrar el panel. Reiniciando en 10 segundos");
            lv_task_handler();
            // Esperar 10 segundos y reiniciar
            delay(10000);
            ESP.restart();
        } else {
            // Guardar datos de calibración y continuar
            flash_set_blob("CalData", (uint8_t *) calData, sizeof(calData));
            flash_commit();
        }
    }

    tft.setTouch(calData);
}

void display_set_brightness(uint64_t brillo) {
    if (brillo == 0) brillo = PUNTO_CONTROL_BRILLO_PREDETERMINADO; // brillo por defecto
    if (brillo < 32) brillo = 32;
    if (brillo > 255) brillo = 255;

    ledcWrite(2, brillo);
}