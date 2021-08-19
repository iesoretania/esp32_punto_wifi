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

#ifndef ESP32_PUNTO_WIFI_SCR_MAIN_H
#define ESP32_PUNTO_WIFI_SCR_MAIN_H

void create_scr_main();
void load_scr_main();
int is_loaded_scr_main();

void set_nombre_main(const char *string);
void set_hora_main(const char *string);
void set_fecha_main(const char *string);
void set_estado_main(const char *string);
void set_icon_text_main(const char *text, lv_palette_t color, int bottom, int offset);
void hide_main_icon();
void show_main_icon();
void hide_main_estado();
void show_main_estado();
void hide_main_qr();
void show_main_qr();
void update_main_qrcode(const char *code, int length);

#endif //ESP32_PUNTO_WIFI_SCR_MAIN_H
