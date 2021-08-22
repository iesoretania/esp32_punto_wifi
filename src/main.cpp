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

#include <punto_control.h>

#include <lvgl.h>
#include <SPI.h>
#include <WiFi.h>
#include <lwip/apps/sntp.h>

#include "flash.h"
#include "seneca.h"
#include "notify.h"
#include "rfid.h"
#include "display.h"
#include "screens/scr_shared.h"
#include "screens/scr_splash.h"
#include "screens/scr_main.h"
#include "screens/scr_check.h"
#include "screens/scr_codigo.h"
#include "screens/scr_login.h"
#include "screens/scr_selection.h"
#include "screens/scr_config.h"

void initialize_gui();

void task_main(lv_timer_t *);

time_t actualiza_hora();

void config_request();

void config_lock_request();

void task_wifi_connection(lv_timer_t *timer) {
    static enum {
        INIT, CONNECTING, NTP_START, NTP_WAIT, CHECKING, CHECK_WAIT, CHECK_RETRY, LOGIN_SCREEN, WAITING,
        CONFIG_SCREEN_LOCK, CONFIG_SCREEN, CONFIG_CODE_1, CONFIG_CODE_2, CODE_WAIT, DONE
    } state = INIT;

    static int cuenta = 0;
    static int code;
    IPAddress ip;

    static String empty_body = "";

    static String old_code = "";

    String uidS;

    switch (state) {
        case DONE:
            lv_timer_del(timer);
            return;
        case INIT:
            if (flash_get_string("sec.code").isEmpty()) {
                state = CONFIG_CODE_1;
                keypad_request("Creación del código de administración");
                break;
            } else {
                if (flash_get_int("sec.force_code")) {
                    state = CODE_WAIT;
                    keypad_request("Active el punto con el código de administración");
                    break;
                }
                state = CONNECTING;
                load_scr_splash();
            }
        case CONNECTING:
            show_splash_spinner();
            if (configuring) {
                config_request();
                state = CONFIG_SCREEN;
                break;
            }
            if (WiFi.status() == WL_CONNECTED) {
                state = NTP_START;
                set_ip_splash_format("IP: %s", WiFi.localIP().toString().c_str());
                set_estado_splash("Comprobando conectividad Andared...");
                code = WiFi.hostByName("c0", ip);
            } else {
                cuenta++;
                // si a los 10 segundos no nos hemos conectado, cambiar el número de versión por el icono de config.
                if (cuenta % 100 == 0) {
                    hide_splash_version();
                    show_splash_config();
                }
            }
            break;
        case NTP_START:
            hide_splash_config();
            set_estado_splash("Ajustando hora...");
            setenv("TZ", PUNTO_CONTROL_HUSO_HORARIO, 1);
            tzset();
            sntp_setoperatingmode(SNTP_OPMODE_POLL);
            if (code != 1) {
                sntp_setservername(0, (char *) PUNTO_CONTROL_NTP_SERVER);
            } else {
                sntp_setservername(0, (char *) "c0");
            }
            sntp_init();
            state = NTP_WAIT;
            break;
        case NTP_WAIT:
            if (sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) {
                state = CHECKING;
                cuenta = 0;
            }
            break;
        case CHECKING:
            set_estado_splash("Comprobando acceso a Séneca...");
            seneca_prepare_request();

            if (!seneca_get_punto().isEmpty()) {
                seneca_process_token("0");
            } else {
                send_seneca_request_data(empty_body);
            }

            state = CHECK_WAIT;
            break;
        case CHECK_WAIT:
            if (seneca_get_request_status() == HTTP_ONGOING) {
                break;
            }
            if (seneca_get_request_status() != HTTP_DONE || seneca_get_http_status_code() != 200) {
                set_estado_splash("Error de acceso. Reintentando");
                state = CHECK_RETRY;
            } else {
                set_estado_splash("Acceso a Séneca confirmado");
                process_seneca_response();
                if (seneca_get_tipo_acceso().isEmpty()) {
                    if (!seneca_get_punto().isEmpty()) {
                        state = WAITING;
                        hide_splash_spinner();
                    } else {
                        state = LOGIN_SCREEN;
                        load_scr_selection();
                    }
                } else {
                    state = LOGIN_SCREEN;
                    seneca_clear_request_status();
                    load_scr_login();
                }
            }
            cuenta = 0;
            break;
        case CHECK_RETRY:
            cuenta++;
            if (cuenta == 10) {
                state = CHECKING;
            }
            break;
        case LOGIN_SCREEN:
            if (is_loaded_scr_splash()) {
                cuenta = 0;
                state = CHECKING;
            }
            if (seneca_get_request_status() == HTTP_DONE) {
                // procesar respuesta JSON del login
                String res = seneca_process_json_response();

                if (res.length() > 0) {
                    set_error_login_text(res.c_str());
                } else {
                    load_scr_splash();
                    cuenta = 0;
                    state = CHECK_WAIT;
                }
            }
            break;
        case WAITING:
            cuenta++;
            if (cuenta == 20) {
                notify_ready();

                state = DONE;
                load_scr_main();
                clean_scr_splash();
                lv_timer_create(task_main, 100, nullptr);
            }
            break;
        case CONFIG_SCREEN_LOCK:
            if (keypad_done == 1) {
                if (read_code.isEmpty()) {
                    keypad_done = 0;
                    configuring = 0;
                    state = CONNECTING;
                    load_scr_splash();
                    break;
                }
                if (read_code.equals(flash_get_string("sec.code"))) {
                    config_request();
                    state = CONFIG_SCREEN;
                    break;
                }
                read_code = "";
                set_estado_codigo("Código incorrecto");
                keypad_done = 0;

                set_codigo_text("");
            }
            break;
        case CONFIG_SCREEN:
            if (!configuring) {
                load_scr_splash();
                state = CONNECTING;
            }
            break;
        case CONFIG_CODE_1:
            // ver si hay algún llavero
            uidS = read_id();
            if (keypad_done || !uidS.isEmpty()) {
                keypad_done = 0;
                // si se ha leído un llavero, simular la introducción del código
                if (!uidS.isEmpty()) {
                    read_code = uidS;
                }
                if (!read_code.isEmpty()) {
                    old_code = read_code;
                    keypad_request("Confirme código");
                    state = CONFIG_CODE_2;
                }
            }
            break;
        case CONFIG_CODE_2:
            // ver si hay algún llavero
            uidS = read_id();
            if (keypad_done || !uidS.isEmpty()) {
                keypad_done = 0;
                // si se ha leído un llavero, simular la introducción del código
                if (!uidS.isEmpty()) {
                    read_code = uidS;
                }
                if (!read_code.isEmpty()) {
                    if (old_code.equals(read_code)) {
                        flash_set_string("sec.code", read_code);
                        flash_commit();
                        load_scr_splash();
                        state = CONNECTING;
                        read_code = "";
                    } else {
                        keypad_request("Los códigos no coinciden. Vuelva a escribir el código");
                        state = CONFIG_CODE_1;
                    }
                }
            }
            break;
        case CODE_WAIT:
            // ver si hay algún llavero
            uidS = read_id();
            if (keypad_done || !uidS.isEmpty()) {
                keypad_done = 0;
                // si se ha leído un llavero, simular la introducción del código
                if (!uidS.isEmpty()) {
                    read_code = uidS;
                }
                if (read_code.equals(flash_get_string("sec.code"))) {
                    state = CONNECTING;
                    read_code = "";
                    load_scr_splash();
                    break;
                }
                keypad_request("Código incorrecto");
                keypad_done = 0;
                set_codigo_text("");
            }
            break;
    }
}

void task_main(lv_timer_t *timer) {
    static enum {
        DONE, IDLE, CARD_PRESENT, CHECK_ONGOING, CHECK_RESULT_WAIT, CONFIG_SCREEN_LOCK, CONFIG_SCREEN, NO_NETWORK
    } state = IDLE;

    static int cuenta = 0;
    static String last_pin = "";
    static String pin = "";
    static String uidS;
    static uint32_t otp;
    static time_t last_now = 0;

    switch (state) {
        case DONE:
            lv_timer_del(timer);
            return;
        case IDLE:
            cuenta++;
            if (configuring) {
                config_lock_request();
                state = CONFIG_SCREEN_LOCK;
                cuenta = 0;
                break;
            }
            if (is_loaded_scr_main()) {
                pin = get_codigo_text();
                if (!pin.equals(last_pin)) {
                    last_pin = pin;
                    cuenta = 0;
                }
                // si a los 8 segundos no ha habido ningún cambio, cerrar pantalla de introducción de PIN
                if (cuenta > 80) load_scr_main();
            }
            if (keypad_requested) {
                keypad_request("Introduzca PIN");
                cuenta = 0;
                last_pin = "";
            }
            if (WiFi.status() != WL_CONNECTED) {
                set_icon_text_main("\uF252", LV_PALETTE_RED, 1, 0);
                show_main_estado();
                show_main_icon();
                hide_main_qr();
                state = NO_NETWORK;
                load_scr_main();
            } else {
                int llavero;
                if (!read_code.isEmpty()) {
                    uidS = read_code;
                    read_code = "";
                    llavero = 0;
                } else {
                    uidS = read_id();
                    llavero = 1;
                }

                if (!uidS.isEmpty()) {
                    set_icon_text_check("\uF252", LV_PALETTE_TEAL, 0, 0);
                    if (llavero) {
                        set_estado_check("Llavero detectado. Comunicando con Séneca, espere por favor...");
                    } else {
                        set_estado_check("Enviando PIN a Séneca, espere por favor...");
                    }
                    load_scr_check();
                    state = CARD_PRESENT;
                    cuenta = 0;
                    break;
                } else {
                    if (keypad_done) {
                        load_scr_main();
                        keypad_done = 0;
                    }
                }

                time_t now = actualiza_hora();

                if (last_now == 0 || now / 30 != last_now) {
                    last_now = now / 30;
                    uidS = seneca_get_qrcode_string(now);
                    update_main_qrcode(uidS.c_str(), uidS.length());
                }
                switch ((cuenta / 10) % 10) {
                    case 0:
                        show_main_estado();
                        show_main_icon();
                        hide_main_qr();
                        set_estado_main("Acerque llavero...");
                        break;
                    case 5:
                        show_main_estado();
                        show_main_icon();
                        hide_main_qr();
                        set_estado_main("Pulse pantalla para PIN");
                        break;
                    default:
                        hide_main_estado();
                        hide_main_icon();
                        show_main_qr();
                }
            }
            break;
        case CARD_PRESENT:
            seneca_process_token(uidS);
            cuenta = 0;
            state = CHECK_ONGOING;
            break;
        case CHECK_ONGOING:
            if (seneca_get_request_status() == HTTP_ONGOING) {
                break;
            }
            if (seneca_get_request_status() == HTTP_DONE && seneca_get_http_status_code() == 200) {
                process_seneca_response();
                parse_seneca_response();
                cuenta = 0;
                state = CHECK_RESULT_WAIT;
            } else {
                if (cuenta == 150) {
                    set_icon_text_check("\uF071", LV_PALETTE_RED, 0, 0);
                    set_estado_check("Desisto. No se ha podido notificar a Séneca. Fiche de nuevo.");
                    cuenta = 0;
                    state = CHECK_RESULT_WAIT;
                    break;
                }
                if (cuenta % 10 == 0) {
                    set_estado_check_format("Reintentando envío, espere por favor... Intento %d", cuenta / 10 + 1);
                    reset_seneca();
                    seneca_process_token(uidS);
                }
                cuenta++;
            }
            break;
        case CHECK_RESULT_WAIT:
            cuenta++;
            if (cuenta > 40 || (cuenta > 20 && rfid_new_card_detected())) {
                state = IDLE;
                load_scr_main();
            }
            break;
        case NO_NETWORK:
            cuenta++;
            actualiza_hora();
            set_estado_main("NO HAY RED, ESPERE...");
            if (WiFi.status() == WL_CONNECTED) {
                set_icon_text_main("\uF063", LV_PALETTE_BLUE, 1, 0);
                state = IDLE;
            } else {
                if (cuenta % 20 == 0) {
                    WiFi.reconnect();
                }
            }
            break;
        case CONFIG_SCREEN_LOCK:
            cuenta++;
            pin = get_codigo_text();
            if (!pin.equals(last_pin)) {
                last_pin = pin;
                cuenta = 0;
            }
            // si a los 8 segundos no ha habido ningún cambio, cerrar pantalla de introducción de código
            if (cuenta > 80) keypad_done = 1;

            uidS = read_id();
            if (keypad_done == 1 || !uidS.isEmpty()) {
                if (!uidS.isEmpty()) {
                    read_code = uidS;
                }
                if (read_code.isEmpty()) {
                    keypad_done = 0;
                    configuring = 0;
                    state = IDLE;
                    load_scr_main();
                    break;
                }
                if (read_code.equals(flash_get_string("sec.code"))) {
                    config_request();
                    state = CONFIG_SCREEN;
                    break;
                }
                keypad_request("Código incorrecto");
                keypad_done = 0;
                set_codigo_text("");
            }
            break;
        case CONFIG_SCREEN:
            if (configuring == 0) {
                state = IDLE;
                load_scr_main();
            }
    }
}

void config_request() {
    read_code = "";
    update_scr_config();
    load_scr_config();
}


time_t actualiza_hora() {
    const char *dia_semana[] = {"Domingo", "Lunes", "Martes", "Miércoles", "Jueves", "Viernes", "Sábado"};

    time_t now;
    char strftime_buf[64];
    char fecha_final[64];
    struct tm timeinfo{};

    time(&now);

    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%T", &timeinfo);
    set_hora_main(strftime_buf);

    strftime(strftime_buf, sizeof(strftime_buf), "%d/%m/%G", &timeinfo);
    strcpy(fecha_final, dia_semana[timeinfo.tm_wday]);
    strcat(fecha_final, ", ");
    strcat(fecha_final, strftime_buf);
    set_fecha_main(fecha_final);

    return now;
}

void setup() {
    // INICIALIZAR SUBSISTEMAS

    // Inicializar puerto serie (para mensajes de depuración)
    Serial.begin(115200);

    // Activar iluminación del display al 100%
    pinMode(BACKLIGHT_PIN, OUTPUT);
    ledcAttachPin(BACKLIGHT_PIN, 2);
    ledcSetup(2, 5000, 8);

    // Inicializar flash
    initialize_flash();

    // Inicializar bus SPI y el TFT conectado a través de él
    Serial.println("Inicializando SPI");
    SPI.begin();

    // Inicializar lector
    Serial.println("Inicializando RFID");
    initialize_rfid();

    // Inicializar GUI
    Serial.println("Inicializando GUI");
    initialize_gui();

    // Inicializar subsistema HTTP
    Serial.println("Inicializando HTTP");
    initialize_seneca();

    notify_start();

    // Crear pantallas y cambiar a la pantalla de arranque
    keypad_requested = 0;

    Serial.println("Creando pantalla splash");
    create_scr_splash();
    Serial.println("Creando pantalla main");
    create_scr_main();
    Serial.println("Creando pantalla check");
    create_scr_check();
    Serial.println("Creando pantalla login");
    create_scr_login();
    Serial.println("Creando pantalla selection");
    create_scr_selection();
    Serial.println("Creando pantalla config");
    create_scr_config();
    Serial.println("Creando pantalla codigo");
    create_scr_codigo();

    // Inicializar WiFi
    Serial.println("Inicializando Wi-Fi");
    set_estado_splash_format("Conectando a %s...", flash_get_string("net.wifi_ssid").c_str());
    WiFi.begin(flash_get_string("net.wifi_ssid").c_str(), flash_get_string("net.wifi_psk").c_str());

    // Comenzar la conexión a la red
    Serial.println("Creando tarea Wi-Fi");
    lv_timer_create(task_wifi_connection, 100, nullptr);
}


void loop() {
    lv_task_handler();
    delay(5);
}
