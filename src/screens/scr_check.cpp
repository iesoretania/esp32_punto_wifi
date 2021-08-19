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

#include "scr_check.h"

static lv_obj_t *scr_check;      // Pantalla de comprobación
static lv_obj_t *lbl_estado_check;
static lv_obj_t *lbl_icon_check;

void create_scr_check() {
    // CREAR PANTALLA DE COMPROBACIÓN
    scr_check = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scr_check, LV_COLOR_MAKE(255, 255, 255), LV_PART_MAIN);

    // Icono del estado de comprobación
    lbl_icon_check = lv_label_create(scr_check);
    lv_obj_set_style_text_font(lbl_icon_check, &symbols, LV_PART_MAIN);
    set_icon_text(lbl_icon_check, "\uF252", LV_PALETTE_BLUE, 0, 0);

    // Etiqueta de estado de comprobación
    lbl_estado_check = lv_label_create(scr_check);
    lv_obj_set_style_text_font(lbl_estado_check, &mulish_32, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_estado_check, lv_palette_main(LV_PALETTE_BLUE_GREY), LV_PART_MAIN);
    lv_obj_set_style_text_align(lbl_estado_check, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_label_set_long_mode(lbl_estado_check, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(lbl_estado_check, LV_PCT(90)); // SCREEN_WIDTH * 9 / 10
    set_estado_check("");
}

void set_estado_check_format(const char *string, int p) {
    lv_label_set_text_fmt(lbl_estado_check, string, p);
    lv_obj_align_to(lbl_estado_check, lbl_icon_check, LV_ALIGN_OUT_BOTTOM_MID, 0, 25);
}

void set_estado_check(const char *string) {
    lv_label_set_text(lbl_estado_check, string);
    lv_obj_align_to(lbl_estado_check, lbl_icon_check, LV_ALIGN_OUT_BOTTOM_MID, 0, 25);
}

void set_icon_text_check(const char *text, lv_palette_t color, int bottom, int offset) {
    set_icon_text(lbl_icon_check, text, color, bottom, offset);
}

void load_scr_check() {
    lv_scr_load(scr_check);
}
