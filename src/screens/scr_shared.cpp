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

#include "scr_shared.h"
#include "scr_codigo.h"
#include "notify.h"

static lv_obj_t *kb;             // Teclado en pantalla

void ta_config_click_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        configuring = 1;
        notify_button_press();
    }
}

void ta_keypad_click_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        keypad_requested = 1;
        notify_button_press();
    }
}

void ta_kb_form_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *ta = lv_event_get_target(e);
    lv_obj_t *obj = (lv_obj_t *) lv_event_get_user_data(e);

    if (code == LV_EVENT_FOCUSED) {
        if (lv_indev_get_type(lv_indev_get_act()) != LV_INDEV_TYPE_KEYPAD) {
            lv_obj_set_parent(kb, lv_obj_get_screen(ta));
            lv_keyboard_set_textarea(kb, ta);
            lv_obj_set_style_max_height(kb, LV_VER_RES * 2 / 3, 0);
            lv_obj_set_height(obj, LV_VER_RES - lv_obj_get_height(kb));
            lv_obj_clear_flag(kb, LV_OBJ_FLAG_HIDDEN);
            lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
            lv_obj_update_layout(obj);
            lv_obj_scroll_to_view_recursive(ta, LV_ANIM_OFF);
        }
    } else if (code == LV_EVENT_DEFOCUSED) {
        lv_keyboard_set_textarea(kb, nullptr);
        lv_obj_set_height(obj, LV_VER_RES);
        lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
    } else if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
        lv_obj_set_height(obj, LV_VER_RES);
        lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_state(e->target, LV_STATE_FOCUSED);
        lv_indev_reset(nullptr, e->target);
    } else if (code == LV_EVENT_VALUE_CHANGED) {
        notify_key_press();
    }
}

lv_obj_t *create_config_button(lv_obj_t *parent) {
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_t *lbl = lv_label_create(btn);

    lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl, &mulish_32, LV_PART_MAIN);
    lv_label_set_text(lbl, LV_SYMBOL_SETTINGS);
    lv_obj_set_style_text_color(lbl, lv_palette_main(LV_PALETTE_INDIGO), LV_PART_MAIN);
    lv_obj_align_to(btn, parent, LV_ALIGN_BOTTOM_LEFT, 10, -10);
    lv_obj_add_event_cb(btn, ta_config_click_event_cb, LV_EVENT_CLICKED, parent);

    return btn;
}

void set_icon_text(lv_obj_t *label, const char *text, lv_palette_t color, int bottom, int offset) {
    lv_obj_set_style_text_color(label, lv_palette_main(color), LV_PART_MAIN);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(label, bottom ? LV_ALIGN_BOTTOM_MID : LV_ALIGN_TOP_MID, offset, bottom ? -15 : 15);
}

void create_keyboard(lv_obj_t *parent) {
    kb = lv_keyboard_create(parent);
}

void hide_keyboard() {
    lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
}

void show_keyboard() {
    lv_obj_clear_flag(kb, LV_OBJ_FLAG_HIDDEN);
}