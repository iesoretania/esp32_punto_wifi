#ifndef ESP32_PUNTO_WIFI_PUNTO_CONTROL_H
#define ESP32_PUNTO_WIFI_PUNTO_CONTROL_H

#define PUNTO_CONTROL_VERSION "v3.0.0"
#define PUNTO_CONTROL_HUSO_HORARIO "CET-1CEST-2,M3.5.0/02:00:00,M10.5.0/03:00:00"
#define PUNTO_CONTROL_URL "https://seneca.juntadeandalucia.es/seneca/controldepresencia/ControlPresencia.jsp"
#define PUNTO_CONTROL_LOGIN_URL "https://seneca.juntadeandalucia.es/seneca/controldepresencia/ComprobarUsuarioExt.jsp"
#define PUNTO_CONTROL_NTP_SERVER "europe.pool.ntp.org"
#define PUNTO_CONTROL_RELEASE_CHECK_URL "https://api.github.com/repos/iesoretania/esp32_punto_wifi/releases/latest"

#define PUNTO_CONTROL_SSID_PREDETERMINADO "\x41\x6E\x64\x61\x72\x65\x64"
#define PUNTO_CONTROL_PSK_PREDETERMINADA "\x36\x62\x36\x32\x39\x66\x34\x63\x32\x39\x39\x33\x37\x31\x37\x33\x37\x34\x39\x34\x63\x36\x31\x62\x35\x61\x31\x30\x31\x36\x39\x33\x61\x32\x64\x34\x65\x39\x65\x31\x66\x33\x65\x31\x33\x32\x30\x66\x33\x65\x62\x66\x39\x61\x65\x33\x37\x39\x63\x65\x63\x66\x33\x32"

#define PUNTO_CONTROL_BRILLO_PREDETERMINADO 192

#endif //ESP32_PUNTO_WIFI_PUNTO_CONTROL_H
