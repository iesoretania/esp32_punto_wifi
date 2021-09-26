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

#include <lvgl.h>
#include "scr_calibration.h"

static lv_obj_t *scr_calibracion;// Pantalla de calibración de panel táctil

lv_obj_t *lbl_calibracion;

void create_scr_calibration() {
    scr_calibracion = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scr_calibracion, LV_COLOR_MAKE(255, 255, 255), LV_PART_MAIN);

    // Etiqueta con instrucciones de calibración
    lbl_calibracion = lv_label_create(scr_calibracion);
    lv_label_set_text(lbl_calibracion, "Presione las puntas de las flechas rojas con la máxima precisión posible");
    lv_obj_set_style_text_font(lbl_calibracion, &mulish_32, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_calibracion, lv_palette_main(LV_PALETTE_LIGHT_BLUE), LV_PART_MAIN);
    lv_obj_set_style_text_align(lbl_calibracion, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_label_set_long_mode(lbl_calibracion, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(lbl_calibracion, LV_PCT(90));
    lv_obj_align(lbl_calibracion, LV_ALIGN_CENTER, 0, 0);
}

void load_scr_calibration() {
    lv_scr_load(scr_calibracion);
}

void set_calibration_main_text(const char *string) {
    lv_label_set_text(lbl_calibracion, string);
}
