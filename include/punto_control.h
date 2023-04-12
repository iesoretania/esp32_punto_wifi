#ifndef ESP32_PUNTO_WIFI_PUNTO_CONTROL_H
#define ESP32_PUNTO_WIFI_PUNTO_CONTROL_H

#define PUNTO_CONTROL_VERSION "v3.1.19"
#define PUNTO_CONTROL_HUSO_HORARIO "CET-1CEST-2,M3.5.0/02:00:00,M10.5.0/03:00:00"
#define PUNTO_CONTROL_URL "https://seneca.juntadeandalucia.es/seneca/controldepresencia/ControlPresencia.jsp"
#define PUNTO_CONTROL_LOGIN_URL "https://seneca.juntadeandalucia.es/seneca/controldepresencia/ComprobarUsuarioExt.jsp"
#define PUNTO_CONTROL_NTP_SERVER "europe.pool.ntp.org"
#define PUNTO_CONTROL_RELEASE_CHECK_URL "https://api.github.com/repos/iesoretania/esp32_punto_wifi/releases/latest"

#define PUNTO_CONTROL_SSID_PREDETERMINADO "\x41\x6E\x64\x61\x72\x65\x64_IoT"
#define PUNTO_CONTROL_PSK_PREDETERMINADA ""

#define PUNTO_CONTROL_BRILLO_PREDETERMINADO 192

#endif //ESP32_PUNTO_WIFI_PUNTO_CONTROL_H
