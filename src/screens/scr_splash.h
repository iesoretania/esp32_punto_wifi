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

#ifndef ESP32_PUNTO_WIFI_SCR_SPLASH_H
#define ESP32_PUNTO_WIFI_SCR_SPLASH_H

#include "scr_shared.h"

void create_scr_splash();
void load_scr_splash();
int is_loaded_scr_splash();
void clean_scr_splash();

void set_estado_splash_format(const char *string, const char *p);
void set_estado_splash(const char *string);
void set_ip_splash_format(const char *string, const char *p);
void set_ip_splash(const char *string);

void hide_splash_spinner();
void show_splash_spinner();
void hide_splash_version();
void show_splash_version();
void hide_splash_config();
void show_splash_config();

#endif //ESP32_PUNTO_WIFI_SCR_SPLASH_H
