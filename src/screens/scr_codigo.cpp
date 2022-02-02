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
#include "scr_codigo.h"
#include "notify.h"
#include "rfid.h"

static lv_obj_t *scr_codigo;     // Pantalla de introducción de código manual
static lv_obj_t *lbl_estado_codigo;
static lv_obj_t *txt_codigo;

String read_code;
int keypad_requested = 0;
int keypad_done = 0;

String get_read_code() {
    return read_code;
}

void reset_read_code() {
    read_code = "";
}

void set_read_code(String s) {
    read_code = s;
}

void btn_numpad_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *obj = lv_event_get_target(e);
    lv_obj_t *txt = (lv_obj_t *) (lv_event_get_user_data(e));

    if (code == LV_EVENT_DRAW_PART_BEGIN) {
        lv_obj_draw_part_dsc_t *dsc = (lv_obj_draw_part_dsc_t *) lv_event_get_param(e);

        if (dsc->id == 10) {
            if (lv_btnmatrix_get_selected_btn(obj) == dsc->id) {
                dsc->rect_dsc->bg_color = lv_palette_darken(LV_PALETTE_RED, 3);
            } else {
                dsc->rect_dsc->bg_color = lv_palette_main(LV_PALETTE_RED);
            }
            dsc->label_dsc->color = lv_color_white();
        }
        if (dsc->id == 13) {
            if (lv_btnmatrix_get_selected_btn(obj) == dsc->id) {
                dsc->rect_dsc->bg_color = lv_palette_darken(LV_PALETTE_GREEN, 3);
            } else {
                dsc->rect_dsc->bg_color = lv_palette_main(LV_PALETTE_GREEN);
            }
            dsc->label_dsc->color = lv_color_white();
        }
    }
    if (code == LV_EVENT_VALUE_CHANGED) {
        uint32_t id = lv_btnmatrix_get_selected_btn(obj);
        const char *btn = lv_btnmatrix_get_btn_text(obj, id);
        switch (id) {
            case 10:
                read_code = "";
                keypad_done = 1;
                notify_button_press();
                break;
            case 11:
                lv_textarea_set_password_mode(txt, !lv_textarea_get_password_mode(txt));
                lv_textarea_cursor_left(txt);
                lv_textarea_cursor_right(txt);
                notify_button_press();
                break;
            case 12:
                lv_textarea_del_char(txt);
                notify_button_press();
                break;
            case 13:
                read_code = lv_textarea_get_text(txt);
                keypad_done = 1;
                notify_button_press();
                break;
            default:
                lv_textarea_add_char(txt, btn[0]);
                notify_key_press();
        }
    }
}

void create_scr_codigo() {
    keypad_requested = 0;
    scr_codigo = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scr_codigo, LV_COLOR_MAKE(255, 255, 255), LV_PART_MAIN);

    static const char *botones[] = {"1", "2", "3", "4", "5", "\n",
                                    "6", "7", "8", "9", "0", "\n",
                                    LV_SYMBOL_CLOSE, LV_SYMBOL_EYE_CLOSE, LV_SYMBOL_BACKSPACE, "Enviar", ""};

    txt_codigo = lv_textarea_create(scr_codigo);

    // Etiqueta de información
    lbl_estado_codigo = lv_label_create(scr_codigo);
    lv_obj_set_style_text_font(lbl_estado_codigo, &mulish_32, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_estado_codigo, lv_palette_main(LV_PALETTE_BLUE_GREY), LV_PART_MAIN);
    lv_obj_set_style_text_align(lbl_estado_codigo, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_label_set_long_mode(lbl_estado_codigo, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(lbl_estado_codigo, lv_pct(100));
    set_estado_codigo("");

    // Campo de código
    lv_obj_set_style_text_font(txt_codigo, &mulish_64_numbers, LV_PART_MAIN);
    lv_obj_set_style_text_color(txt_codigo, lv_palette_main(LV_PALETTE_INDIGO), LV_PART_MAIN);
    lv_textarea_set_align(txt_codigo, LV_TEXT_ALIGN_CENTER);
    lv_textarea_set_one_line(txt_codigo, true);
    lv_textarea_set_password_mode(txt_codigo, true);
    lv_obj_set_style_border_width(txt_codigo, 0, LV_PART_MAIN);
    lv_obj_set_width(txt_codigo, lv_pct(100));
    lv_obj_add_state(txt_codigo, LV_STATE_FOCUSED);

    // Teclado numérico
    static lv_style_t style_buttons;
    lv_style_init(&style_buttons);
    lv_style_set_text_font(&style_buttons, &mulish_24);

    lv_obj_t *btn_matrix_numeros = lv_btnmatrix_create(scr_codigo);
    lv_btnmatrix_set_map(btn_matrix_numeros, botones);
    lv_btnmatrix_set_btn_width(btn_matrix_numeros, 13, 2);
    lv_btnmatrix_set_btn_ctrl(btn_matrix_numeros, 11, LV_BTNMATRIX_CTRL_CHECKABLE);
    lv_obj_add_style(btn_matrix_numeros, &style_buttons, LV_BTNMATRIX_DRAW_PART_BTN);
    lv_obj_align(btn_matrix_numeros, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_event_cb(btn_matrix_numeros, btn_numpad_event_cb, LV_EVENT_ALL, txt_codigo);
    lv_obj_set_width(btn_matrix_numeros, lv_pct(100));
    lv_obj_set_height(btn_matrix_numeros, LV_VER_RES / 2 + 25);
    lv_obj_align(btn_matrix_numeros, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_clear_flag(btn_matrix_numeros, LV_OBJ_FLAG_CLICK_FOCUSABLE);

    lv_obj_align_to(txt_codigo, btn_matrix_numeros, LV_ALIGN_OUT_TOP_MID, 0, 0);
}

void config_lock_request() {
    read_code = "";
    keypad_request("Introduzca código de administración");
}

void keypad_request(const char *mensaje) {
    keypad_requested = 0;
    keypad_done = 0;
    read_code = "";
    rfid_clear();
    set_estado_codigo(mensaje);
    lv_textarea_set_text(txt_codigo, "");
    lv_scr_load(scr_codigo);
}

void set_estado_codigo(const char *string) {
    lv_label_set_text(lbl_estado_codigo, string);
    lv_obj_align(lbl_estado_codigo, LV_ALIGN_TOP_MID, 0, 25);
}

void set_codigo_text(const char *txt) {
    lv_textarea_set_text(txt_codigo, txt);
}

const char *get_codigo_text() {
    return lv_textarea_get_text(txt_codigo);
}
