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
#include <WiFi.h>
#include <lwip/apps/sntp.h>

#include "flash.h"
#include "seneca.h"
#include "notify.h"
#include "rfid.h"
#include "display.h"
#include "screens/scr_splash.h"
#include "screens/scr_main.h"
#include "screens/scr_check.h"
#include "screens/scr_codigo.h"
#include "screens/scr_login.h"
#include "screens/scr_selection.h"
#include "screens/scr_config.h"
#include "screens/scr_firmware.h"

void task_main(lv_timer_t *);

time_t actualiza_hora();

void config_request();

TaskHandle_t notify_task = nullptr;

void task_wifi_connection(lv_timer_t *timer) {
    // Esta tarea es la primera en ejecutarse al arrancar el punto de acceso.
    // Se ejecuta en segundo plano cada 100 ms y es una máquina de estados:
    static enum {
        INIT, CONNECTING, NTP_START, NTP_WAIT, CHECKING, CHECK_WAIT, CHECK_RETRY, LOGIN_SCREEN, WAITING,
        CONFIG_SCREEN_LOCK, CONFIG_SCREEN, CONFIG_CODE_1, CONFIG_CODE_2, CODE_WAIT, FIRMWARE_FLASHING, DONE
    } state = INIT;

    static int cuenta = 0;
    static int code;
    IPAddress ip;

    static String empty_body = "";

    static String old_code = "";

    String uidS;

    static String url;

    switch (state) {
        case DONE:
            // fin de la tarea: eliminar temporizador
            lv_timer_del(timer);
            return;
        case INIT:
            // estado inicial:
            // lo primero es comprobar si hay código de administración. Si no existe, solicitamos su creación
            if (flash_get_string("sec.code").isEmpty()) {
                state = CONFIG_CODE_1;
                keypad_request("Creación del código de administración");
                break;
            } else {
                // si existe código, comprobamos si hay código de activación. En ese caso, esperamos a que se
                // introduzca correctamente antes de seguir
                if (flash_get_int("sec.force_code")) {
                    state = CODE_WAIT;
                    keypad_request("Active el punto con el código de administración");
                    break;
                }
                // si no hay código de activación, mostramos la pantalla de carga y pasamos al estado de conexión
                state = CONNECTING;
                notify_start();
                load_scr_splash();
            }
        case CONNECTING:
            // nos aseguramos que el indicador de carga está visible
            show_splash_spinner();

            // si se ha pulsado el botón de configuración, pasar a la pantalla correspondiente
            // NOTA: el botón aparece transcurridos 10 segundos sin haber completado la conexión a la red
            if (configuring) {
                config_lock_request();
                state = CONFIG_SCREEN_LOCK;
                break;
            }

            // comprobar estado de conexión a la Wi-Fi
            // si acabamos de conectarnos, comenzar puesta en hora
            if (WiFi.status() == WL_CONNECTED) {
                state = NTP_START;
                set_ip_splash_format("IP: %s", WiFi.localIP().toString().c_str());
                set_estado_splash("Comprobando conectividad Andared...");

                // intentamos resolver "c0" por DNS. Si tiene éxito, suponemos que estamos conectados
                // a la red corporativa desde un centro educativo y usamos como servidor NTP c0.
                // De otra forma, la salida a Internet del servicio será bloqueada por el cortafuegos
                code = WiFi.hostByName("c0", ip);
            } else {
                // aún no hay conexión. Llevar la cuenta de las décimas de segundo que estamos así
                cuenta++;
                // si a los 10 segundos no nos hemos conectado, cambiar el número de versión por el icono de config.
                if (cuenta == 100) {
                    hide_splash_version();
                    show_splash_config();
                }
            }
            break;
        case NTP_START:
            // ocultar icono de configuración por si estuviera visible
            hide_splash_config();
            set_estado_splash("Ajustando hora...");
            if (!sntp_enabled()) {
                // ajustamos la zona horaria según la cadena de configuración
                setenv("TZ", PUNTO_CONTROL_HUSO_HORARIO, 1);
                tzset();
                // modo de operación SNTP: poll unicast
                sntp_setoperatingmode(SNTP_OPMODE_POLL);
                // si ha fallado la resolución DNS antes, usar el servidor NTP del pool español
                // si no ha fallado, usar c0 como servidor NTP
                if (code != 1) {
                    sntp_setservername(0, (char *) PUNTO_CONTROL_NTP_SERVER);
                } else {
                    sntp_setservername(0, (char *) "c0");
                }
                // inicializar SNTP para poner el reloj interno en hora
                sntp_init();
                state = NTP_WAIT;
            } else {
                state = CHECKING;
            }
            break;
        case NTP_WAIT:
            // comprobar si ya estamos en hora. En ese caso, pasar al estado de comprobación de conectividad
            if (sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) {
                // el reloj ha sido sincronizado
                // cambiar el logo por el de Séneca y comprobar si estamos actualizando el firmware
                change_logo_splash();
                if (flash_get_string("firmware_url").length() > 0) {
                    state = FIRMWARE_FLASHING;
                    // no podemos dejar la tarea que ilumina los LEDs, así que dejamos un color fijo
                    // y la eliminamos. Interfiere e impide la descarga.
                    notify_firmware_start();
                    delay(60);
                    vTaskDelete(notify_task);
                    url = flash_get_string("firmware_url");
                    flash_erase("firmware_url");
                    set_estado_splash("Actualizando firmware...");
                } else {
                    // no actualizamos el firmware, pasamos a comprobar conectividad con Séneca
                    state = CHECKING;
                }
                cuenta = 0;
            }
            break;
        case FIRMWARE_FLASHING:
            // llamar a la rutina de actualización del firmware para que avance
            do_firmware_upgrade(url.c_str());
            break;
        case CHECKING:
            // estado de comprobación de conectividad
            set_estado_splash("Comprobando acceso a Séneca...");

            // mandaremos una petición "vacía" a la aplicación de control de presencia de Séneca
            // que servirá para comprobar si tenemos acceso y, de paso, obtener algunos parámetros
            // de configuración
            seneca_prepare_request();

            // si el punto de control está seleccionado, enviamos el código PIN 0
            // si no lo está, enviamos un formulario vacío
            if (!seneca_get_punto().isEmpty()) {
                seneca_process_token("0");
            } else {
                send_seneca_request_data(empty_body);
            }

            // pasamos al estado donde esperamos la respuesta
            state = CHECK_WAIT;
            break;
        case CHECK_WAIT:
            // esperar hasta obtener la respuesta
            if (seneca_get_request_status() == HTTP_ONGOING) {
                break;
            }
            // aquí estaremos cuando haya finalizado la petición a Séneca

            // si ha ocurrido algún error con la petición, pasar al estado de reintentar
            if (seneca_get_request_status() != HTTP_DONE || seneca_get_http_status_code() != 200) {
                set_estado_splash("Error de acceso. Reintentando");
                state = CHECK_RETRY;
            } else {
                // no ha habido error, procesar respuesta para obtener todos los datos posibles
                set_estado_splash("Acceso a Séneca confirmado");
                process_seneca_response();
                notify_stop();
                // si "tipo de acceso" contiene algo, es que con las cookies estamos "logueados"
                if (seneca_get_tipo_acceso().isEmpty()) {
                    if (!seneca_get_punto().isEmpty()) {
                        // si está configurado el punto de acceso, ya estamos listos: lo indicamos
                        // durante unos segundos y pasamos de pantalla
                        state = WAITING;
                        hide_splash_spinner();
                    } else {
                        // si no está configurado el punto de acceso, pasar el control a la pantalla de selección
                        state = LOGIN_SCREEN;
                        seneca_clear_request_status();
                        load_scr_selection();
                    }
                } else {
                    // es necesario hacer login de un directivo o administrativo para obtener la sesión
                    state = LOGIN_SCREEN;
                    seneca_clear_request_status();
                    load_scr_login();
                }
            }
            cuenta = 0;
            break;
        case CHECK_RETRY:
            // ha ocurrido un error al acceder a Séneca, esperamos un segundo y lo volvemos a intentar
            cuenta++;
            if (cuenta == 10) {
                state = CHECKING;
            }
            break;
        case LOGIN_SCREEN:
            // en el momento que estemos en la splash screen, es que ya estamos en condiciones de comprobar
            // la conectividad
            if (is_loaded_scr_splash()) {
                cuenta = 0;
                state = CHECKING;
            }
            // si se pulsa el botón de configurar
            if (configuring) {
                config_lock_request();
                state = CONFIG_SCREEN_LOCK;
                break;
            }
            // si hemos terminado la petición de login, procesarla para ver si ha tenido éxito
            if (seneca_get_request_status() == HTTP_DONE) {
                // procesar respuesta JSON del login:
                // devuelve una cadena vacía si está correcto, en caso contrario una cadena con el mensaje
                // de error devuelto
                String res = seneca_process_json_response();

                if (res.length() > 0) {
                    set_error_login_text(iso_8859_1_to_utf8(res).c_str());
                } else {
                    load_scr_splash();
                    cuenta = 0;
                    state = CHECK_WAIT;
                }
            }
            break;
        case WAITING:
            // estado final en el que esperamos dos segundos para mostrar el mensaje de que estamos conectados
            cuenta++;
            if (cuenta == 20) {
                // notificar acústicamente que estamos listos
                notify_ready();

                // acabamos nuestra función: cargar pantalla principal, liberar los recursos de la splash screen
                // y crear la tarea principal que lleva la lógica del fichaje
                state = DONE;
                load_scr_main();
                clean_scr_splash();
                lv_timer_create(task_main, 100, nullptr);
            }
            break;
        case CONFIG_SCREEN_LOCK:
            // cuando estamos en este estado, estamos esperando a que se introduzca el código
            // de administración para acceder a la pantalla de configuración
            notify_stop();
            // Comprobar si se ha cerrado la pantalla de introducción del código o se ha acercado un llavero
            uidS = rfid_read_id();

            // se ha cerrado el teclado o se ha leído un llavero: procesar
            if (keypad_done == 1 || !uidS.isEmpty()) {
                if (!uidS.isEmpty()) {
                    // hay un llavero, copiar su ID en el valor leído por teclado
                    set_read_code(uidS);
                }
                // si no se ha introducido el código, volver al estado de conexión a la red
                if (get_read_code().isEmpty()) {
                    keypad_done = 0;
                    configuring = 0;
                    state = CONNECTING;
                    notify_start();
                    load_scr_splash();
                    break;
                }
                // si se ha introducido un código y es el correcto, pasar a la pantalla de configuración
                if (get_read_code().equals(flash_get_string("sec.code"))) {
                    config_request();
                    state = CONFIG_SCREEN;
                    break;
                }
                // si el código es incorrecto, indicarlo y seguir como estábamos
                reset_read_code();
                set_estado_codigo("Código incorrecto");
                keypad_done = 0;

                set_codigo_text("");
            }
            break;
        case CONFIG_SCREEN:
            // permanecemos en este estado mientras está abierta la pantalla de configuración

            // si la hemos cerrado, volver a la pantalla y al estado de conexión a la red
            if (!configuring) {
                load_scr_splash();
                state = CONNECTING;
                notify_start();
            }
            break;
        case CONFIG_CODE_1:
            // estamos configurando el código de administrador
            // en este estado lo leeremos la primera vez

            // comprobar si hay algún llavero
            uidS = rfid_read_id();
            if (keypad_done || !uidS.isEmpty()) {
                keypad_done = 0;
                // si se ha leído un llavero, simular la introducción del código
                if (!uidS.isEmpty()) {
                    set_read_code(uidS);
                }
                if (!get_read_code().isEmpty()) {
                    // pasamos al estado de comprobación del segundo código
                    old_code = get_read_code();
                    keypad_request("Confirme código");
                    state = CONFIG_CODE_2;
                }
            }
            break;
        case CONFIG_CODE_2:
            // aquí se comprueba la introducción del código de administración por segunda vez
            // para ver si coincide con el primero

            // comprobar si hay algún llavero
            uidS = rfid_read_id();
            if (keypad_done || !uidS.isEmpty()) {
                keypad_done = 0;
                // si se ha leído un llavero, simular la introducción del código
                if (!uidS.isEmpty()) {
                    set_read_code(uidS);
                }
                if (!get_read_code().isEmpty()) {
                    // ¿coinciden?
                    if (old_code.equals(get_read_code())) {
                        // sí: almacenar en flash y volver al estado de conexión a la red
                        flash_set_string("sec.code", get_read_code());
                        flash_commit();
                        load_scr_splash();
                        state = CONNECTING;
                        notify_start();
                        reset_read_code();
                    } else {
                        // no: volver al estado de introducción del primer código
                        keypad_request("Los códigos no coinciden. Vuelva a escribir el código");
                        state = CONFIG_CODE_1;
                    }
                }
            }
            break;
        case CODE_WAIT:
            // en este estado estamos esperando la introducción del código de activación del lector

            // comprobar si hay algún llavero o se ha introducido un código por teclado
            uidS = rfid_read_id();
            if (keypad_done || !uidS.isEmpty()) {
                keypad_done = 0;
                // si se ha leído un llavero, simular la introducción del código
                if (!uidS.isEmpty()) {
                    set_read_code(uidS);
                }
                // ¿coincide con el código de activación grabado?
                if (get_read_code().equals(flash_get_string("sec.code"))) {
                    // sí: pasamos al estado de comprobación de red
                    state = CONNECTING;
                    notify_start();
                    reset_read_code();
                    load_scr_splash();
                    break;
                }
                // no: mostramos mensaje de error y seguimos esperando a que introduzca el correcto
                keypad_request("Código incorrecto");
                keypad_done = 0;
                set_codigo_text("");
            }
            break;
    }
}

void task_main(lv_timer_t *timer) {
    // Esta tarea es la que gestiona la lógica de la gestión de presencia.
    // Se ejecuta en segundo plano cada 100 ms y es una máquina de estados:
    static enum {
        DONE, IDLE, CARD_PRESENT, CHECK_ONGOING, CHECK_RESULT_WAIT, CONFIG_SCREEN_LOCK, CONFIG_SCREEN, NO_NETWORK
    } state = IDLE;

    static int cuenta = 0;
    static String last_pin = "";
    static String pin = "";
    static String uidS;
    static time_t last_now = 0;

    switch (state) {
        case DONE:
            // en este estado acabamos, aunque no debería ocurrir nunca porque ésta es la tarea final
            lv_timer_del(timer);
            return;
        case IDLE:
            // estado principal
            // indicar que ha transcurrido un tick (100 ms)
            cuenta++;

            // ¿se ha pedido ir a la pantalla de configuración?
            if (configuring) {
                // sí: pedir código de administrador
                config_lock_request();
                state = CONFIG_SCREEN_LOCK;
                cuenta = 0;
                break;
            }
            // si no estamos en la pantalla principal es que estamos en alguna pantalla de introducción
            // de código. Transcurridos 8 segundos sin cambios en el texto introducido, salimos de la pantalla
            if (!is_loaded_scr_main()) {
                pin = get_codigo_text();
                if (!pin.equals(last_pin)) {
                    last_pin = pin;
                    cuenta = 0;
                }
                // si a los 8 segundos no ha habido ningún cambio, cerrar pantalla de introducción de PIN
                if (cuenta > 80) load_scr_main();
            }
            // si han pulsado en el display, mostrar pantalla de introducción de PIN
            if (keypad_requested) {
                keypad_request("Introduzca PIN");
                cuenta = 0;
                last_pin = "";
            }
            // si ya no estamos conectados a la red Wi-Fi, mostrar un mensaje indicándolo y cambiar de estado
            if (WiFi.status() != WL_CONNECTED) {
                // mostramos un icono rojo indicando problemas y ocultamos el código QR
                // TODO: aunque no haya red el QR sigue siendo válido, deberíamos dejarlo visible y actualizado todo
                //  el rato aunque eso implique fusionar los estados para no duplicar código
                set_icon_text_main("\uF252", LV_PALETTE_RED, 1, 0);
                show_main_estado();
                show_main_icon();
                hide_main_qr();
                state = NO_NETWORK;
                // cambiar a la pantalla principal aunque estemos en
                load_scr_main();
            } else {
                // estamos en el caso normal
                int llavero; // 0 = PIN; 1 = código leído del llavero

                // ¿se ha introducido un código por teclado?
                if (!get_read_code().isEmpty()) {
                    // sí: leerlo
                    uidS = get_read_code();
                    reset_read_code();
                    llavero = 0;
                } else {
                    // no: leer el ID del llavero actual por RFID (si hay)
                    uidS = rfid_read_id();
                    llavero = 1;
                }

                // ¡hay un código para enviar!
                if (!uidS.isEmpty()) {
                    // indicar que estamos contactando con Séneca
                    notify_check_start();
                    set_icon_text_check("\uF252", LV_PALETTE_TEAL, 0, 0);
                    if (llavero) {
                        set_estado_check("Llavero detectado. Comunicando con Séneca, espere por favor...");
                    } else {
                        set_estado_check("Enviando PIN a Séneca, espere por favor...");
                    }
                    // cargar a la pantalla de comprobación y cambiar de estado
                    load_scr_check();
                    state = CARD_PRESENT;
                    cuenta = 0;
                    break;
                } else {
                    // si el teclado se ha cerrado, volver a la pantalla principal
                    if (keypad_done) {
                        load_scr_main();
                        keypad_done = 0;
                    }
                }

                hide_main_estado();

                // actualizar el texto de la hora que aparece en pantalla
                time_t now = actualiza_hora();

                // si es la primera vez que entramos o estamos en el segundo 0 o 30, actualizar código QR
                if (is_seneca_qrcode_enabled() && (last_now == 0 || now / 30 != last_now)) {
                    last_now = now / 30;
                    uidS = seneca_get_qrcode_string(now);
                    update_main_qrcode(uidS.c_str(), uidS.length());
                    hide_main_icon();
                    show_main_qr();
                }

                if (!is_seneca_qrcode_enabled()) {
                    show_main_icon();
                    hide_main_qr();
                }

                // dependiendo del tiempo transcurrido mostramos u ocultamos información
                // la secuencia se repite cada 10 segundos:
                //    Segundo 1 y 2: Se muestra "Acerque llavero"
                //    Segundo 6 y 7: Se muestra "Pulse pantalla para PIN"
                //            Resto: Se muestra la fecha actual
                switch ((cuenta / 10) % 10) {
                    case 0:
                    case 1:
                        set_fecha_main("Acerque llavero...");
                        break;
                    case 5:
                    case 6:
                        set_fecha_main("Pulse pantalla para PIN");
                        break;
                }
            }
            break;
        case CARD_PRESENT:
            // hay un código que procesar, enviar petición a Séneca
            seneca_process_token(uidS);
            cuenta = 0;
            state = CHECK_ONGOING;
            break;
        case CHECK_ONGOING:
            // estamos esperando respuesta de Séneca
            if (seneca_get_request_status() == HTTP_ONGOING) {
                // aún no hay nada, salir
                break;
            }
            // ya tenemos respuesta
            if (seneca_get_request_status() == HTTP_DONE && seneca_get_http_status_code() == 200) {
                // ¿ha tenido éxito? Si es así, procesar la respuesta y obtener toda la información que podamos
                process_seneca_response();
                // dependiendo del resultado, mostrar un mensaje u otro
                // TODO: es mejor que la función devuelva el resultado y hacer todos los cambios en las pantallas
                //  aquí en lugar de allí
                parse_seneca_response();
                cuenta = 0;
                state = CHECK_RESULT_WAIT;
            } else {
                // no ha tenido éxito, hacer otra petición transcurrido un segundo
                if (cuenta >= 150) {
                    // se han hecho 15 intentos, desistir de intentarlo
                    set_icon_text_check("\uF071", LV_PALETTE_RED, 0, 0);
                    set_estado_check("Desisto. No se ha podido notificar a Séneca. Fiche de nuevo.");
                    notify_check_error();
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
            // aquí se espera para mostrar el resultado del fichaje (bien sea correcto o un error)
            // pasados 4 segundos se cierra, o tras 2 si se acerca otro llavero
            cuenta++;
            if (cuenta > 40 || (cuenta > 20 && rfid_new_card_detected())) {
                state = IDLE;
                load_scr_main();
            }
            break;
        case NO_NETWORK:
            // no hay red, indicarlo
            cuenta++;
            actualiza_hora();
            set_estado_main("NO HAY RED, ESPERE...");
            if (WiFi.status() == WL_CONNECTED) {
                // ¡ha vuelto la red!
                set_icon_text_main("\uF063", LV_PALETTE_BLUE, 1, 0);
                state = IDLE;
            } else {
                // pasados 2 segundos, intentar forzar la reconexión (no se hace automáticamente)
                if (cuenta % 20 == 0) {
                    WiFi.reconnect();
                }
            }
            break;
        case CONFIG_SCREEN_LOCK:
            // en este estado estamos esperando que se introduzca el código de administrador que da acceso
            // a la configuración
            cuenta++;

            // se cierra automáticamente a los 8 segundos sin cambios
            pin = get_codigo_text();
            if (!pin.equals(last_pin)) {
                last_pin = pin;
                cuenta = 0;
            }
            // si a los 8 segundos no ha habido ningún cambio, cerrar pantalla de introducción de código
            if (cuenta > 80) keypad_done = 1;

            // comprobar si hay llavero. Su ID puede usarse de código de administración
            uidS = rfid_read_id();

            // se ha cerrado el teclado o se ha leído un llavero: procesar
            if (keypad_done == 1 || !uidS.isEmpty()) {
                if (!uidS.isEmpty()) {
                    // hay un llavero, copiar su ID en el valor leído por teclado
                    set_read_code(uidS);
                }
                // si no hay código es que se ha cerrado el teclado sin introducir ninguno, volver a la pantalla
                // principal
                if (get_read_code().isEmpty()) {
                    keypad_done = 0;
                    configuring = 0;
                    state = IDLE;
                    load_scr_main();
                    break;
                }
                // comprobar si el código introducido coincide con el configurado
                if (get_read_code().equals(flash_get_string("sec.code"))) {
                    // sí: cambiar a la pantalla de configuración
                    config_request();
                    state = CONFIG_SCREEN;
                    break;
                }
                // no: indicarlo y permitir volver a intentarlo
                keypad_request("Código incorrecto");
                keypad_done = 0;
                set_codigo_text("");
            }
            break;
        case CONFIG_SCREEN:
            // configurando. Esperar aquí hasta que se cierre la pantalla, entonces pasamos a la principal
            if (configuring == 0) {
                state = IDLE;
                load_scr_main();
            }
    }
}

[[noreturn]] void xTask_notify(void *pvParameter) {
    // Esta tarea actualiza el estado de los LEDs
    while (true) {
        delay(50);
        notify_tick();
    }
}

void config_request() {
    reset_read_code();
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

    // Inicializar flash
    initialize_flash();

    // Inicializar lector
    initialize_rfid();

    // Inicializar GUI
    initialize_gui();

    // Inicializar subsistema HTTP
    initialize_seneca();

    // Crear pantallas y cambiar a la pantalla de arranque
    create_scr_splash();
    create_scr_main();
    create_scr_check();
    create_scr_login();
    create_scr_selection();
    create_scr_config();
    create_scr_codigo();
    create_scr_firmware();

    // Inicializar WiFi
    set_estado_splash_format("Conectando a %s...", flash_get_string("net.wifi_ssid").c_str());
    WiFi.begin(flash_get_string("net.wifi_ssid").c_str(), flash_get_string("net.wifi_psk").c_str());

    // Inicializar luces de estado
    xTaskCreatePinnedToCore(&xTask_notify, "LED", 4096, nullptr, 10, &notify_task, 1);

    // Comenzar la conexión a la red
    lv_timer_create(task_wifi_connection, 100, nullptr);
}

void loop() {
    lv_task_handler();
    delay(5);
}
