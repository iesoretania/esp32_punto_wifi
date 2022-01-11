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

#ifndef ESP32_PUNTO_WIFI_SENECA_H
#define ESP32_PUNTO_WIFI_SENECA_H

typedef enum {
    HTTP_IDLE, HTTP_ERROR, HTTP_ONGOING, HTTP_DONE
} HttpRequestStatus;

void initialize_seneca();
void reset_seneca();

String iso_8859_1_to_utf8(String &str);

void seneca_prepare_request();
void process_seneca_response();
int parse_seneca_response();
int send_seneca_request_data(String &body);
void seneca_process_token(String uid);
String seneca_process_json_response();
String seneca_get_qrcode_string(time_t now);
void seneca_login(String &usuario, String &password);

HttpRequestStatus seneca_get_request_status();
int seneca_get_http_status_code();
void seneca_clear_request_status();

String seneca_cifrar_rsa(const char *cadena, const char *e, const char *n);
uint32_t seneca_calcula_otp(time_t now);
void seneca_set_punto(char *nombre);
String seneca_get_punto();
String seneca_get_tipo_acceso();
void seneca_set_clave_punto(char *pos_inicio);
int is_seneca_qrcode_enabled();

#endif //ESP32_PUNTO_WIFI_SENECA_H
