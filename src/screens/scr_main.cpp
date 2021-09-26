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

#include <lv_qrcode.h>
#include "scr_main.h"
#include "scr_shared.h"

static lv_obj_t *scr_main;       // Pantalla principal de lectura de código
static lv_obj_t *lbl_nombre_main;
static lv_obj_t *lbl_hora_main;
static lv_obj_t *lbl_fecha_main;
static lv_obj_t *lbl_estado_main;
static lv_obj_t *lbl_icon_main;
static lv_obj_t *qr_main;

void create_scr_main() {
    // CREAR PANTALLA PRINCIPAL

    Serial.println("\t* Creando scr_main");
    scr_main = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scr_main, LV_COLOR_MAKE(255, 255, 255), LV_PART_MAIN);

    // Etiqueta con el nombre del punto de acceso en la parte superior
    Serial.println("\t* Creando lbl_nombre_main");
    lbl_nombre_main = lv_label_create(scr_main);
    lv_label_set_text(lbl_nombre_main, "Punto de control WiFi");
    lv_obj_set_style_text_font(lbl_nombre_main, &mulish_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_nombre_main, lv_palette_main(LV_PALETTE_DEEP_ORANGE), LV_PART_MAIN);
    lv_obj_set_style_text_align(lbl_nombre_main, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_label_set_long_mode(lbl_nombre_main, LV_LABEL_LONG_SCROLL_CIRCULAR);

    // Etiqueta de hora y fecha actual centrada justo debajo
    Serial.println("\t* Creando lbl_hora_main");
    lbl_hora_main = lv_label_create(scr_main);
    lv_obj_set_style_text_font(lbl_hora_main, &mulish_64_numbers, LV_PART_MAIN);
    lv_obj_set_style_text_align(lbl_hora_main, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lbl_fecha_main = lv_label_create(scr_main);
    lv_obj_set_style_text_font(lbl_fecha_main, &mulish_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_fecha_main, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
    lv_obj_set_style_text_align(lbl_fecha_main, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    set_hora_main("");
    set_fecha_main("");

    // Etiqueta de estado centrada en pantalla
    Serial.println("\t* Creando lbl_estado_main");
    lbl_estado_main = lv_label_create(scr_main);
    lv_obj_set_style_text_font(lbl_estado_main, &mulish_32, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_estado_main, lv_palette_main(LV_PALETTE_BLUE_GREY), LV_PART_MAIN);
    lv_obj_set_style_text_align(lbl_estado_main, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    set_estado_main("");

    // Imagen de la flecha en la parte inferior
    Serial.println("\t* Creando lbl_icon_main");
    lbl_icon_main = lv_label_create(scr_main);
    lv_obj_set_style_text_font(lbl_icon_main, &symbols, LV_PART_MAIN);
    set_icon_text(lbl_icon_main, "\uF063", LV_PALETTE_BLUE, 1, 0);
    lv_obj_add_flag(lbl_icon_main, LV_OBJ_FLAG_HIDDEN);

    // Icono de configuración
    Serial.println("\t* Creando btn_config_main");
    create_config_button(scr_main);

    // Icono de teclado
    Serial.println("\t* Creando btn_keypad_main");
    lv_obj_t *btn_keypad_main = lv_btn_create(scr_main);
    lv_obj_t *lbl = lv_label_create(btn_keypad_main);
    lv_obj_set_style_bg_opa(btn_keypad_main, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl, &mulish_32, LV_PART_MAIN);
    lv_label_set_text(lbl, LV_SYMBOL_KEYBOARD);
    lv_obj_set_style_text_color(lbl, lv_palette_main(LV_PALETTE_INDIGO), LV_PART_MAIN);
    lv_obj_align(btn_keypad_main, LV_ALIGN_BOTTOM_RIGHT, -10, -10);
    lv_obj_add_event_cb(btn_keypad_main, ta_keypad_click_event_cb, LV_EVENT_CLICKED, scr_main);
    lv_obj_add_event_cb(scr_main, ta_keypad_click_event_cb, LV_EVENT_CLICKED, scr_main);

    // Código QR
    Serial.println("\t* Creando qr_main");
    qr_main = lv_qrcode_create(scr_main, 170, lv_color_hex3(0x0), lv_color_hex3(0xfff));
    lv_qrcode_update(qr_main, "", 0);
    lv_obj_align(qr_main, LV_ALIGN_BOTTOM_MID, 0, -15);
    lv_obj_add_flag(qr_main, LV_OBJ_FLAG_HIDDEN);
}

void set_nombre_main(const char *string) {
    lv_label_set_text(lbl_nombre_main, string);
    lv_label_set_long_mode(lbl_nombre_main, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(lbl_nombre_main, LV_PCT(100)); // SCREEN_WIDTH
    lv_obj_set_style_text_align(lbl_nombre_main, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(lbl_nombre_main, LV_ALIGN_TOP_MID, 0, 15);
}

void set_hora_main(const char *string) {
    lv_label_set_text(lbl_hora_main, string);
    lv_obj_align_to(lbl_hora_main, lbl_nombre_main, LV_ALIGN_OUT_BOTTOM_MID, 0, 15);
}

void set_fecha_main(const char *string) {
    lv_label_set_text(lbl_fecha_main, string);
    lv_obj_align_to(lbl_fecha_main, lbl_hora_main, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);
}

void set_estado_main(const char *string) {
    lv_label_set_text(lbl_estado_main, string);
    lv_obj_align_to(lbl_estado_main, lbl_fecha_main, LV_ALIGN_OUT_BOTTOM_MID, 0, 20);
}

void set_icon_text_main(const char *text, lv_palette_t color, int bottom, int offset) {
    set_icon_text(lbl_icon_main, text, color, bottom, offset);
}

void hide_main_icon() {
    lv_obj_add_flag(lbl_icon_main, LV_OBJ_FLAG_HIDDEN);
}

void show_main_icon() {
    lv_obj_clear_flag(lbl_icon_main, LV_OBJ_FLAG_HIDDEN);
}

void hide_main_estado() {
    lv_obj_add_flag(lbl_estado_main, LV_OBJ_FLAG_HIDDEN);
}

void show_main_estado() {
    lv_obj_clear_flag(lbl_estado_main, LV_OBJ_FLAG_HIDDEN);
}

void hide_main_qr() {
    lv_obj_add_flag(qr_main, LV_OBJ_FLAG_HIDDEN);
}

void show_main_qr() {
    lv_obj_clear_flag(qr_main, LV_OBJ_FLAG_HIDDEN);
}

void update_main_qrcode(const char *code, int length) {
    lv_qrcode_update(qr_main, code, length);
}

void load_scr_main() {
    lv_scr_load(scr_main);
}

int is_loaded_scr_main() {
    return lv_scr_act() == scr_main;
}
