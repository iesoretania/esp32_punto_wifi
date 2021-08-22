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

#ifndef ESP32_PUNTO_WIFI_FLASH_H
#define ESP32_PUNTO_WIFI_FLASH_H

#include <Arduino.h>

void initialize_flash();

String flash_get_string(String key);
uint64_t flash_get_int(String key);
bool flash_get_blob(String key, uint8_t *data, size_t length);
bool flash_set_string(String key, String data);
bool flash_set_int(String key, uint64_t data);
bool flash_set_blob(String key, uint8_t *data, size_t length);
bool flash_erase(String key);
bool flash_commit();

#endif //ESP32_PUNTO_WIFI_FLASH_H
