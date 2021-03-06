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

#ifndef ESP32_PUNTO_WIFI_SCR_SHARED_H
#define ESP32_PUNTO_WIFI_SCR_SHARED_H

#include <Arduino.h>
#include <lvgl.h>

void ta_config_click_event_cb(lv_event_t *e);
void ta_keypad_click_event_cb(lv_event_t *e);
void ta_kb_form_event_cb(lv_event_t *e);
lv_obj_t *create_config_button(lv_obj_t *parent);
void set_icon_text(lv_obj_t *label, const char *text, lv_palette_t color, int bottom, int offset);

void create_keyboard(lv_obj_t *parent);
void hide_keyboard();
void show_keyboard();

#endif //ESP32_PUNTO_WIFI_SCR_SHARED_H
