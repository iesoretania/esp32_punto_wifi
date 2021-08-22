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

#ifndef ESP32_PUNTO_WIFI_SCR_CODIGO_H
#define ESP32_PUNTO_WIFI_SCR_CODIGO_H

#include <Arduino.h>

extern int keypad_requested;
extern int configuring;
extern int keypad_done;

void create_scr_codigo();
void config_lock_request();
void keypad_request(const char *mensaje);
void set_estado_codigo(const char *string);

void set_codigo_text(const char *txt);
const char *get_codigo_text();

String get_read_code();
void reset_read_code();
void set_read_code(String s);

#endif //ESP32_PUNTO_WIFI_SCR_CODIGO_H
