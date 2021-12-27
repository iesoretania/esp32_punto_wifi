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

#include <Arduino.h>
#include <ArduinoNvs.h>
#include <punto_control.h>
#include "flash.h"

void initialize_flash() {
    bool erase_flash = false;

    NVS.begin();

    // Botón Boot: si está pulsado durante el inicio, reinicializar flash
    pinMode(0, INPUT);

    if (digitalRead(0) != HIGH) {
        // esperar un segundo, si sigue pulsado el botón "BOOT", borrar flash
        delay(1000);
        erase_flash = digitalRead(0) != HIGH;
    }
    if (NVS.getInt("PUNTO_CONTROL") != 3 || erase_flash) {
        // borrando flash
        NVS.eraseAll(false);
        NVS.setInt("PUNTO_CONTROL", 3);
        NVS.setString("CPP-authToken", "");
        NVS.setString("CPP-puntToken", "");
        // configuración por defecto de la WiFi
        NVS.setString("net.wifi_ssid", PUNTO_CONTROL_SSID_PREDETERMINADO);
        NVS.setString("net.wifi_psk", PUNTO_CONTROL_PSK_PREDETERMINADA);
        NVS.commit();
    }
}

String flash_get_string(const String key) {
    return NVS.getString(key);
}

uint64_t flash_get_int(const String key) {
    return NVS.getInt(key);
}

bool flash_get_blob(const String key, uint8_t *data, size_t length) {
    return NVS.getBlob(key, data, length);
}

bool flash_set_string(const String key, String data) {
    return NVS.setString(key, data);
}

bool flash_set_int(const String key, uint64_t data) {
    return NVS.setInt(key, data);
}

bool flash_set_blob(const String key, uint8_t *data, size_t length) {
    return NVS.setBlob(key, data, length);
}

bool flash_erase(String key) {
    return NVS.erase(key);
}

bool flash_commit() {
    return NVS.commit();
}
