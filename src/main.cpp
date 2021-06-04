#include <Arduino.h>

#include <punto_control.h>

#include <lvgl.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <lwip/apps/sntp.h>
#include <MFRC522.h>
#include <TinyXML.h>
#include <ArduinoNvs.h>
#include <mbedtls/bignum.h>
#include <esp_http_client.h>

#define SCREEN_WIDTH 480
#define SCREEN_HEIGHT 320


TFT_eSPI tft = TFT_eSPI();
MFRC522 mfrc522(RFID_SDA_PIN, RFID_RST_PIN);

ArduinoNvs nvs;

TinyXML xml;
uint8_t xmlBuffer[2048];

// Gestión de peticiones HTTP
int send_seneca_request_data(String &body);

esp_err_t http_event_handle(esp_http_client_event_t *evt);

esp_err_t err;
esp_http_client_config_t http_config;

esp_http_client_handle_t http_client;
String cookieCPP_authToken;
String cookieCPP_puntToken;
String cookieJSESSIONID;
String cookieSenecaP;

typedef enum { HTTP_IDLE, HTTP_ERROR, HTTP_ONGOING, HTTP_DONE } HttpRequestStatus;

HttpRequestStatus http_request_status;
int http_status_code;
String response;

esp_err_t http_event_handle(esp_http_client_event_t *evt) {
    String cookie_value, cookie_name, tmp;
    int index;

    switch (evt->event_id) {
        case HTTP_EVENT_ERROR:
            http_request_status = HTTP_ERROR;
            break;
        case HTTP_EVENT_ON_HEADER:
            if (strcmp(evt->header_key, "Set-Cookie") == 0) {
                cookie_name = evt->header_value;
                index = cookie_name.indexOf('=');
                if (index > -1) {
                    cookie_name = cookie_name.substring(0, index);
                    cookie_value = evt->header_value;
                    index = cookie_value.indexOf(';');
                    if (index > -1) {
                        cookie_value = cookie_value.substring(0, index);
                    }
                    if (cookie_name.equals("JSESSIONID")) {
                        cookieJSESSIONID = cookie_value;
                        break;
                    }
                    if (cookie_name.equals("SenecaP")) {
                        cookieSenecaP = cookie_value;
                        break;
                    }
                    if (cookie_name.equals("CPP-authToken")) {
                        cookieCPP_authToken = cookie_value;
                        tmp = NVS.getString("CPP-authToken");
                        if (tmp.length() == 0) {    // actualizar en flash solamente si no existe
                            NVS.setString("CPP-authToken", cookieCPP_authToken);
                        }
                        break;
                    }
                    if (cookie_name.equals("CPP-puntToken")) {
                        cookieCPP_puntToken = cookie_value;
                        tmp = NVS.getString("CPP-puntToken");
                        if (tmp.length() == 0 || !tmp.equals(cookieCPP_puntToken)) {
                            NVS.setString("CPP-puntToken", cookieCPP_puntToken);
                        }
                        break;
                    }
                }
            }
            break;
        case HTTP_EVENT_ON_DATA:
            response += String((char *) evt->data).substring(0, evt->data_len);
            break;
        case HTTP_EVENT_ON_FINISH:
            http_request_status = HTTP_DONE;
            http_status_code = esp_http_client_get_status_code(evt->client);
            break;
        default:
            http_request_status = HTTP_ERROR;
            break;
    }
    return ESP_OK;
}

/* Flushing en el display */
void espi_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors(&color_p->full, w * h, false);    // importante que swap sea true en este display
    tft.endWrite();

    lv_disp_flush_ready(disp);
}

/* Lectura de la pantalla táctil */
void espi_touch_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
    uint16_t touchX, touchY;

    data->continue_reading = false;
    bool touched = tft.getTouch(&touchX, &touchY, 600);

    if (!touched) {
        data->state = LV_INDEV_STATE_REL;
        return;
    } else {
        data->state = LV_INDEV_STATE_PR;
    }

    /* Establecer coordenadas */
    data->point.x = touchX;
    data->point.y = touchY;
}

// Pantallas
static lv_obj_t *scr_splash;     // Pantalla de inicio (splash screen)
static lv_obj_t *spinner_splash;
static lv_obj_t *lbl_ip_splash;
static lv_obj_t *lbl_estado_splash;
static lv_obj_t *lbl_version_splash;
static lv_obj_t *btn_config_splash;

static lv_obj_t *scr_main;       // Pantalla principal de lectura de código
static lv_obj_t *lbl_nombre_main;
static lv_obj_t *lbl_hora_main;
static lv_obj_t *lbl_fecha_main;
static lv_obj_t *lbl_estado_main;
static lv_obj_t *lbl_icon_main;
static lv_obj_t *btn_config_main;

static lv_obj_t *scr_check;      // Pantalla de comprobación
static lv_obj_t *lbl_estado_check;
static lv_obj_t *lbl_icon_check;

static lv_obj_t *scr_login;      // Pantalla de entrada de credenciales
static lv_obj_t *pnl_login;
static lv_obj_t *txt_usuario_login;
static lv_obj_t *txt_password_login;
static lv_obj_t *btn_login;
static lv_obj_t *lbl_error_login;

static lv_obj_t *scr_selection;  // Pantalla de selección de punto de control
static lv_obj_t *pnl_selection;
static lv_obj_t *lst_selection;

static lv_obj_t *scr_calibracion;// Pantalla de calibración de panel táctil

static lv_obj_t *scr_config;     // Pantalla de configuración
static lv_obj_t *txt_ssid_config;
static lv_obj_t *txt_psk_config;

static lv_obj_t *scr_codigo;     // Pantalla de introducción de código manual
static lv_obj_t *lbl_estado_codigo;
static lv_obj_t *txt_codigo;

static lv_obj_t *kb;             // Teclado en pantalla

static lv_obj_t *last_scr;

static int configuring;

static int keypad_requested;
static int keypad_done;

static String read_code;

void create_scr_splash();

void create_scr_main();

void create_scr_check();

void create_scr_login();

void create_scr_config();

void create_scr_selection();

void create_scr_calibracion();

void create_scr_codigo();

void update_scr_config();

void initialize_gui();

void initialize_flash();

void initialize_http_client();

void task_main(lv_timer_t *);

static void ta_select_form_event_cb(lv_event_t *e);
static void ta_config_click_event_cb(lv_event_t *e);

String read_id();

void actualiza_hora();

void config_request(lv_obj_t *scr);

void keypad_request(const char *mensaje);

void set_estado_splash_format(const char *string, const char *p) {
    lv_label_set_text_fmt(lbl_estado_splash, string, p);
    lv_obj_align(lbl_estado_splash, LV_ALIGN_TOP_MID, 0, 15);
}

void set_estado_splash(const char *string) {
    lv_label_set_text(lbl_estado_splash, string);
    lv_obj_align(lbl_estado_splash, LV_ALIGN_TOP_MID, 0, 15);
}

void set_ip_splash_format(const char *string, const char *p) {
    lv_label_set_text_fmt(lbl_ip_splash, string, p);
    lv_obj_align(lbl_ip_splash, LV_ALIGN_BOTTOM_MID, 0, -15);
}

void set_ip_splash(const char *string) {
    lv_label_set_text(lbl_ip_splash, string);
    lv_obj_align(lbl_ip_splash, LV_ALIGN_BOTTOM_MID, 0, -15);
}

void set_nombre_main(const char *string) {
    lv_label_set_text(lbl_nombre_main, string);
    lv_label_set_long_mode(lbl_nombre_main, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(lbl_nombre_main, SCREEN_WIDTH);
    lv_obj_set_style_text_align(lbl_nombre_main, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(lbl_nombre_main, LV_ALIGN_TOP_MID, 0, 15);
}

void set_hora_main(const char *string) {
    lv_label_set_text(lbl_hora_main, string);
    lv_obj_align_to(lbl_hora_main, lbl_nombre_main, LV_ALIGN_OUT_BOTTOM_MID, 0, 15);
}

void set_fecha_main(const char *string) {
    lv_label_set_text(lbl_fecha_main, string);
    lv_obj_align_to(lbl_fecha_main, lbl_hora_main, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);
}

void set_estado_main(const char *string) {
    lv_label_set_text(lbl_estado_main, string);
    lv_obj_align_to(lbl_estado_main, lbl_fecha_main, LV_ALIGN_OUT_BOTTOM_MID, 0, 20);
}

void set_estado_check(const char *string) {
    lv_label_set_text(lbl_estado_check, string);
    lv_obj_align_to(lbl_estado_check, lbl_icon_check, LV_ALIGN_OUT_BOTTOM_MID, 0, 25);
}

void set_estado_codigo(const char *string) {
    lv_label_set_text(lbl_estado_codigo, string);
    lv_obj_align(lbl_estado_codigo, LV_ALIGN_TOP_MID, 0, 25);
}

void set_icon_text(lv_obj_t *label, const char *text, lv_palette_t color, int bottom) {
    lv_obj_set_style_text_color(label, lv_palette_main(color), LV_PART_MAIN);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(label, bottom ? LV_ALIGN_BOTTOM_MID : LV_ALIGN_TOP_MID, 0, bottom ? -15 : 15);
}

void set_error_login_text(const char *string) {
    lv_label_set_text(lbl_error_login, string);
}

lv_obj_t *create_config_button(lv_obj_t *parent) {
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_t *lbl = lv_label_create(btn);

    lv_obj_set_style_bg_opa(btn, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl, &mulish_32, LV_PART_MAIN);
    lv_label_set_text(lbl, LV_SYMBOL_SETTINGS);
    lv_obj_set_style_text_color(lbl, lv_palette_main(LV_PALETTE_INDIGO), LV_PART_MAIN);
    lv_obj_align_to(btn, parent, LV_ALIGN_BOTTOM_LEFT, 10, -10);
    lv_obj_add_event_cb(btn, ta_config_click_event_cb, LV_EVENT_CLICKED, parent);

    return btn;
}

String iso_8859_1_to_utf8(String &str) {
    String strOut;
    for (int i = 0; i < str.length(); i++) {
        uint8_t ch = str.charAt(i);
        if (ch < 0x80) {
            strOut.concat((char) ch);
        } else {
            strOut.concat((char) (0xc0 | ch >> 6));
            strOut.concat((char) (0x80 | (ch & 0x3f)));
        }
    }
    return strOut;
}

int send_seneca_data(String &body) {
    // preparar cookies de petición
    String http_cookie = "";
    int primero = 1;

    if (!cookieCPP_authToken.isEmpty()) {
        http_cookie = cookieCPP_authToken;
        primero = 0;
    }
    if (!cookieCPP_puntToken.isEmpty()) {
        if (!primero) {
            http_cookie += "; ";
        }
        primero = 0;
        http_cookie +=  cookieCPP_puntToken;
    }
    if (!cookieSenecaP.isEmpty()) {
        if (!primero) {
            http_cookie += "; ";
        }
        primero = 0;
        http_cookie += cookieSenecaP;
    }
    if (!cookieJSESSIONID.isEmpty()) {
        if (!primero) {
            http_cookie += "; ";
        }
        http_cookie += cookieJSESSIONID;
    }

    //Serial.println("Cookies enviadas: " + http_cookie);

    // añadir cabeceras
    esp_http_client_set_header(http_client, "Cookie", http_cookie.c_str());

    if (body.length() > 0) {
        esp_http_client_set_header(http_client, "Content-Type", "application/x-www-form-urlencoded");
        esp_http_client_set_method(http_client, HTTP_METHOD_POST);
        esp_http_client_set_post_field(http_client, body.c_str(), (int) body.length());
    } else {
        esp_http_client_set_method(http_client, HTTP_METHOD_GET);
    }

    esp_http_client_set_header(http_client, "Accept", "text/html");
    esp_http_client_set_header(http_client, "Accept-Encoding", "identity");

    http_status_code = 0;
    http_request_status = HTTP_ONGOING;

    // hacer la petición
    return esp_http_client_perform(http_client);
}

int send_seneca_login_data(String &body) {
    esp_http_client_set_url(http_client, PUNTO_CONTROL_LOGIN_URL);

    return send_seneca_data(body);
}

int send_seneca_request_data(String &body) {
    esp_http_client_set_url(http_client, PUNTO_CONTROL_URL);

    return send_seneca_data(body);
}

int parse_seneca_response() {
    int ok = 0;

    if (http_status_code == 200) {
        // petición exitosa, extraer mensaje con la respuesta
        int index, index2;

        // la respuesta está dentro de una etiqueta <strong>, no me lo cambiéis :)
        index = response.indexOf("<strong>");
        if (index > 0) {
            index2 = response.indexOf("</strong>");
            if (index2 > index) {
                response = response.substring(index + 8, index2);
                while (response.indexOf("<") >= 0) {
                    auto startpos = response.indexOf("<");
                    auto endpos = response.indexOf(">") + 1;

                    if (endpos > startpos) {
                        response = response.substring(0, startpos) + response.substring(endpos);
                    }
                }

                // convertir respuesta a UTF-8
                response = iso_8859_1_to_utf8(response);

                // mostrar respuesta en pantalla
                if (response.startsWith("Entrada")) {
                    set_icon_text(lbl_icon_check, "\uF2F6", LV_PALETTE_GREEN, 0);
                } else if (response.startsWith("Salida")) {
                    set_icon_text(lbl_icon_check, "\uF2F5", LV_PALETTE_RED, 0);
                } else {
                    set_icon_text(lbl_icon_check, "\uF071", LV_PALETTE_ORANGE, 0);
                }
                set_estado_check(response.c_str());

                ok = 1; // éxito
            }
        } else {
            set_icon_text(lbl_icon_check, "\uF071", LV_PALETTE_ORANGE, 0);
            set_estado_check("Se ha obtenido una respuesta desconocida desde Séneca.");
        }
    } else {
        set_icon_text(lbl_icon_check, "\uF071", LV_PALETTE_ORANGE, 0);
        set_estado_check("Ha ocurrido un error en la comunicación con Séneca.");
    }

    return ok; // 0 = error
}

// Estado del parser XML
String modo, punto, xCentro, nombre_punto, tipo_acceso, nuevo_punto, usuario;

int xmlStatus;
String xml_current_input;

void xml_html_callback(uint8_t statusflags, char *tagName, uint16_t tagNameLen, char *data, uint16_t dataLen) {
    // nos quedamos con las etiquetas <h1> y <a>
    if ((statusflags & STATUS_END_TAG) && strstr(tagName, "h1")) {
        xmlStatus = 0;
    }
    if ((statusflags & STATUS_START_TAG) && strstr(tagName, "h1")) {
        xmlStatus = 2;
    }
    if ((statusflags & STATUS_START_TAG) && strlen(tagName) > 2 && tagName[tagNameLen - 1] =='a' && tagName[tagNameLen - 2]  == '/') {
        xmlStatus = 3;
    }
    if ((statusflags & STATUS_END_TAG) && strlen(tagName) > 2 && tagName[tagNameLen - 1] =='a' && tagName[tagNameLen - 2]  == '/') {
        xmlStatus = 0;
    }

    // buscar el atributo "name" y "value", que están en input
    if ((statusflags & STATUS_ATTR_TEXT) && !strcasecmp(tagName, "name")) {
        xml_current_input = data;
    }

    // campos que nos interesan: _MODO_, PUNTO y X_CENTRO
    if ((statusflags & STATUS_ATTR_TEXT) && !strcasecmp(tagName, "value")) {
        if (xml_current_input.equalsIgnoreCase("_MODO_")) {
            modo = data;
        } else if (xml_current_input.equalsIgnoreCase("PUNTO")) {
            punto = data;
        } else if (xml_current_input.equalsIgnoreCase("X_CENTRO")) {
            xCentro = data;
        }
    }

    // si estamos dentro de una etiqueta <h1>, obtener nombre del punto de control
    if (xmlStatus == 2 && statusflags & STATUS_TAG_TEXT) {
        nombre_punto = data;
    }

    // si estamos dentro de una etiqueta <a>, comprobar si es una lista de puntos de control
    if (xmlStatus == 3 && statusflags & STATUS_ATTR_TEXT && !strcasecmp(tagName, "href") && strstr(data, "activarPuntoAcceso")) {
        nuevo_punto = data;
        nuevo_punto = nuevo_punto.substring(31, nuevo_punto.indexOf(')') - 1);
    }

    if (xmlStatus == 3 && statusflags & STATUS_TAG_TEXT && !nuevo_punto.isEmpty()) {
        lv_obj_t *btn = lv_list_add_btn(lst_selection, LV_SYMBOL_WIFI, data);
        char *item = (char *) lv_mem_alloc(nuevo_punto.length() + 1);
        strcpy(item, nuevo_punto.c_str());
        lv_obj_add_event_cb(btn, ta_select_form_event_cb, LV_EVENT_CLICKED, item);
        lv_obj_add_event_cb(btn, ta_select_form_event_cb, LV_EVENT_DELETE, item);
        nuevo_punto = "";
        xmlStatus = 0;
    }

    // encontrar una clase de tipo "login" activa el tipo de acceso de la pantalla de entrada
    if ((statusflags & STATUS_ATTR_TEXT) && !strcasecmp(tagName, "class") && !strcasecmp(data, "login")) {
        tipo_acceso = "login";
    }
}

void process_token(String uid) {
    // enviar token a Séneca
    String params =
            "_MODO_=" + modo + "&PUNTO=" + punto + "&X_CENTRO=" + xCentro + "&C_TIPACCCONTPRE=&DARK_MODE=N&TOKEN=" +
            uid;
    response = "";
    send_seneca_request_data(params);
}

void process_seneca_response() {
    modo = ""; punto = ""; xCentro = ""; nombre_punto = ""; tipo_acceso = "";

    // análisis XML del HTML devuelto para extraer los valores de los campos ocultos
    xml.init(xmlBuffer, 2048, xml_html_callback);

    // inicialmente no hay etiqueta abierta, procesar carácter a carácter
    xmlStatus = 0;

    char *cadena = const_cast<char *>(response.c_str());
    while (*cadena != 0) {
        xml.processChar(*cadena);
        cadena++;
    }

    // reiniciar parser XML
    xmlStatus = 0;
    xml.reset();

    if (!nombre_punto.isEmpty()) {
        set_nombre_main(nombre_punto.c_str());
    } else {
        set_nombre_main("Punto de acceso WiFi");
    }
}

void task_wifi_connection(lv_timer_t *timer) {
    static enum {
        CONNECTING, NTP_START, NTP_WAIT, CHECKING, CHECK_WAIT, CHECK_RETRY, LOGIN_SCREEN, WAITING, CONFIG_SCREEN, DONE
    } state = CONNECTING;

    static int cuenta = 0;
    static int code;
    IPAddress ip;

    String body = "";

    switch (state) {
        case DONE:
            lv_timer_del(timer);
            return;
        case CONNECTING:
            lv_obj_clear_flag(spinner_splash, LV_OBJ_FLAG_HIDDEN);
            if (configuring) {
                config_request(scr_splash);
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
                    lv_obj_add_flag(lbl_version_splash, LV_OBJ_FLAG_HIDDEN);
                    lv_obj_clear_flag(btn_config_splash, LV_OBJ_FLAG_HIDDEN);
                }
            }
            break;
        case NTP_START:
            lv_obj_add_flag(btn_config_splash, LV_OBJ_FLAG_HIDDEN);
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
            response = "";
            http_request_status = HTTP_ONGOING;
            http_status_code = 0;

            if (!punto.isEmpty()) {
                process_token("0");
            } else {
                send_seneca_request_data(body);
            }

            state = CHECK_WAIT;
            break;
        case CHECK_WAIT:
            if (http_request_status == HTTP_ONGOING) {
                break;
            }
            if (http_request_status != HTTP_DONE || http_status_code != 200) {
                set_estado_splash("Error de acceso. Reintentando");
                state = CHECK_RETRY;
            } else {
                set_estado_splash("Acceso a Séneca confirmado");
                process_seneca_response();
                if (tipo_acceso.isEmpty()) {
                    if (!punto.isEmpty()) {
                        state = WAITING;
                        lv_obj_add_flag(spinner_splash, LV_OBJ_FLAG_HIDDEN);
                    } else {
                        state = LOGIN_SCREEN;
                        lv_scr_load(scr_selection);
                    }
                } else {
                    state = LOGIN_SCREEN;
                    http_request_status = HTTP_IDLE;
                    lv_scr_load(scr_login);
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
            cuenta++;
            if (lv_scr_act() == scr_splash) {
                cuenta = 0;
                state = CHECKING;
            }
            if (http_request_status == HTTP_DONE) {
                // procesar respuesta JSON del login
                if (response.indexOf("<correcto>SI") > -1) {
                    // es correcta, volver a cargar página principal del punto de acceso con nueva cookie
                    String param = "_MODO_=" + modo + "&USUARIO=" + usuario + "&CLAVE_P=";
                    response = "";
                    send_seneca_request_data(param);
                    lv_scr_load(scr_splash);
                    cuenta = 0;
                    state = CHECK_WAIT;
                } else {
                    // error: mostrar mensaje en formulario
                    String error = response.substring(response.indexOf("<mensaje>") + 9,
                                                      response.indexOf("</mensaje>"));
                    set_error_login_text(error.c_str());
                    http_request_status = HTTP_IDLE;
                }
            }
            break;
        case WAITING:
            cuenta++;
            if (cuenta == 20) {
                state = DONE;
                lv_scr_load(scr_main);
                lv_obj_clean(scr_splash);
                lv_timer_create(task_main, 100, nullptr);
            }
            break;
        case CONFIG_SCREEN:
            if (!configuring) {
                lv_scr_load(scr_splash);
                state = CONNECTING;
            }
            break;
    }
}

void task_main(lv_timer_t *timer) {
    static enum {
        DONE, IDLE, CARD_PRESENT, CHECK_ONGOING, CHECK_RESULT_WAIT, CONFIG_SCREEN, NO_NETWORK
    } state = IDLE;

    static int cuenta = 0;
    static String uidS;

    switch (state) {
        case DONE:
            lv_timer_del(timer);
            return;

        case IDLE:
            cuenta++;
            if (configuring) {
                config_request(scr_main);
                state = CONFIG_SCREEN;
                break;
            }
            if (keypad_requested) {
                keypad_request("Introduzca PIN");
            }
            if (WiFi.status() != WL_CONNECTED) {
                set_icon_text(lbl_icon_main, "\uF252", LV_PALETTE_RED, 1);
                state = NO_NETWORK;
                lv_scr_load(scr_main);
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
                    set_icon_text(lbl_icon_check, "\uF252", LV_PALETTE_TEAL, 0);
                    if (llavero) {
                        set_estado_check("Llavero detectado. Comunicando con Séneca, espere por favor...");
                    } else {
                        set_estado_check("Enviando PIN a Séneca, espere por favor...");
                    }
                    lv_scr_load(scr_check);
                    state = CARD_PRESENT;
                    cuenta = 0;
                    break;
                } else {
                    if (keypad_done) {
                        lv_scr_load(scr_main);
                        keypad_done = 0;
                    }
                }

                actualiza_hora();

                switch ((cuenta / 40) % 3) {
                    case 0:
                        set_estado_main("Acerque llavero...");
                        break;
                    case 1:
                        set_estado_main("Pulse pantalla para PIN");
                        break;
                    case 2:
                        set_estado_main("CÓDIGO QR IRÁ AQUÍ");
                }
            }
            break;
        case CARD_PRESENT:
            process_token(uidS);
            cuenta = 0;
            state = CHECK_ONGOING;
            break;
        case CHECK_ONGOING:
            if (http_request_status == HTTP_ERROR || (http_request_status == HTTP_DONE && http_status_code != 200)) {
                cuenta++;
                if (cuenta > 40) {
                    cuenta = 0;
                    set_estado_check("Reintentando envío, espere por favor...");
                    process_token(uidS);
                }
                break;
            }
            if (http_request_status == HTTP_ONGOING) {
                break;
            }
            process_seneca_response();
            parse_seneca_response();
            cuenta = 0;
            state = CHECK_RESULT_WAIT;
            break;
        case CHECK_RESULT_WAIT:
            cuenta++;
            if (cuenta > 40 || (cuenta > 20 && mfrc522.PICC_IsNewCardPresent())) {
                state = IDLE;
                lv_scr_load(scr_main);
            }
            break;
        case NO_NETWORK:
            cuenta++;
            actualiza_hora();
            set_estado_main("NO HAY RED, ESPERE...");
            if (WiFi.status() == WL_CONNECTED) {
                set_icon_text(lbl_icon_main, "\uF063", LV_PALETTE_BLUE, 1);
                state = IDLE;
            } else {
                if (cuenta % 20 == 0) {
                    WiFi.reconnect();
                }
            }
            break;
        case CONFIG_SCREEN:
            if (configuring == 0) {
                state = IDLE;
                lv_scr_load(scr_main);
            }
    }
}

String read_id() {
    String uidS;

    if (mfrc522.PICC_IsNewCardPresent()) {
        if (mfrc522.PICC_ReadCardSerial()) {
            uint32_t uid = (uint32_t) (
                    mfrc522.uid.uidByte[0] +
                    (mfrc522.uid.uidByte[1] << 8) +
                    (mfrc522.uid.uidByte[2] << 16) +
                    (mfrc522.uid.uidByte[3] << 24));

            uidS = (String) uid;

            while (uidS.length() < 10) {
                uidS = "0" + uidS;
            }
        }
        mfrc522.PICC_HaltA();
    }
    return uidS;
}

void config_request(lv_obj_t *scr) {
    update_scr_config();
    set_estado_codigo("Código de administrador");
    lv_textarea_set_text(txt_codigo, "");
    lv_scr_load(scr_config);
}

void keypad_request(const char *mensaje) {
    keypad_requested = 0;
    keypad_done = 0;
    set_estado_codigo(mensaje);
    lv_textarea_set_text(txt_codigo, "");
    lv_scr_load(scr_codigo);
}

void actualiza_hora() {
    const char *dia_semana[] = {"Domingo", "Lunes", "Martes", "Miércoles", "Jueves", "Viernes", "Sábado"};

    time_t now;
    char strftime_buf[64];
    char fecha_final[64];
    struct tm timeinfo;

    time(&now);

    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%T", &timeinfo);
    set_hora_main(strftime_buf);

    strftime(strftime_buf, sizeof(strftime_buf), "%d/%m/%G", &timeinfo);
    strcpy(fecha_final, dia_semana[timeinfo.tm_wday]);
    strcat(fecha_final, ", ");
    strcat(fecha_final, strftime_buf);
    set_fecha_main(fecha_final);
}

void setup() {
    // INICIALIZAR SUBSISTEMAS

    // Inicializar puerto serie (para mensajes de depuración)
    Serial.begin(115200);

    // Activar iluminación del display al 100%
    pinMode(BACKLIGHT_PIN, OUTPUT);
    ledcAttachPin(BACKLIGHT_PIN, 1);
    ledcSetup(1, 5000, 8);
    ledcWrite(1, 255);

    // Inicializar flash
    initialize_flash();

    // Inicializar bus SPI y el TFT conectado a través de él
    SPI.begin();
    tft.begin();            /* Inicializar TFT */
    tft.setRotation(3);     /* Orientación horizontal */

    // Inicializar lector
    mfrc522.PCD_Init();

    // Inicializar GUI
    initialize_gui();

    // Inicializar subsistema HTTP
    initialize_http_client();

    // Crear pantallas y cambiar a la pantalla de arranque
    keypad_requested = 0;
    create_scr_splash();
    create_scr_main();
    create_scr_check();
    create_scr_login();
    create_scr_selection();
    create_scr_config();
    create_scr_codigo();
    lv_scr_load(scr_splash);

    // Inicializar WiFi
    set_estado_splash_format("Conectando a %s...", NVS.getString("net.wifi_ssid").c_str());
    WiFi.begin(NVS.getString("net.wifi_ssid").c_str(), NVS.getString("net.wifi_psk").c_str());

    lv_timer_create(task_wifi_connection, 100, nullptr);
}

void initialize_http_client() {
    http_config.event_handler = http_event_handle;
    http_config.url = PUNTO_CONTROL_URL;

    http_client = esp_http_client_init(&http_config);

    http_request_status = HTTP_IDLE;
}

void initialize_flash() {
    NVS.begin();

    // Botón Boot: si está pulsado durante el inicio, reinicializar flash
    pinMode(0, INPUT);

    if (NVS.getInt("PUNTO_CONTROL") == 3 && digitalRead(0) == HIGH) {
        cookieCPP_authToken = NVS.getString("CPP-authToken");
        cookieCPP_puntToken = NVS.getString("CPP-puntToken");
    } else {
        Serial.println("Borrando flash...");
        NVS.eraseAll(true);
        NVS.setInt("PUNTO_CONTROL", 3);
        NVS.setString("CPP-authToken", "");
        NVS.setString("CPP-puntToken", "");
        // configuración por defecto de la WiFi
        NVS.setString("net.wifi_ssid", PUNTO_CONTROL_SSID_PREDETERMINADO);
        NVS.setString("net.wifi_psk", PUNTO_CONTROL_PSK_PREDETERMINADA);
    }
}

void initialize_gui() {
    lv_init();

    // Inicializar buffers del GUI
    static lv_disp_draw_buf_t disp_buf;
    static lv_color_t buf1[LV_HOR_RES_MAX * 20];
    static lv_color_t buf2[LV_HOR_RES_MAX * 20];

    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, LV_HOR_RES_MAX * 20);

    // Inicializar interfaz gráfica
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = SCREEN_WIDTH;
    disp_drv.ver_res = SCREEN_HEIGHT;
    disp_drv.antialiasing = 1;
    disp_drv.flush_cb = espi_disp_flush;
    disp_drv.draw_buf = &disp_buf;
    lv_disp_drv_register(&disp_drv);

    // Inicializar panel táctil
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = espi_touch_read;
    lv_indev_drv_register(&indev_drv);

    uint16_t calData[5];

    // Obtener datos de calibración del panel táctil
    if (!NVS.getBlob("CalData", (uint8_t *) calData, sizeof(calData))) {
        // Cargar pantalla de calibración
        create_scr_calibracion();
        lv_scr_load(scr_calibracion);
        lv_task_handler();

        tft.calibrateTouch(calData, TFT_RED, TFT_WHITE, 20);

        // Guardar datos de calibración
        NVS.setBlob("CalData", (uint8_t *) calData, sizeof(calData));
    }

    tft.setTouch(calData);
}

static void ta_kb_form_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *ta = lv_event_get_target(e);
    lv_obj_t *obj = (lv_obj_t *) lv_event_get_user_data(e);

    if (code == LV_EVENT_FOCUSED) {
        if (lv_indev_get_type(lv_indev_get_act()) != LV_INDEV_TYPE_KEYPAD) {
            lv_obj_set_parent(kb, lv_obj_get_screen(ta));
            lv_keyboard_set_textarea(kb, ta);
            lv_obj_set_style_max_height(kb, LV_VER_RES * 2 / 3, 0);
            lv_obj_set_height(obj, LV_VER_RES - lv_obj_get_height(kb));
            lv_obj_clear_flag(kb, LV_OBJ_FLAG_HIDDEN);
            lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
            lv_obj_update_layout(obj);
            lv_obj_scroll_to_view_recursive(ta, LV_ANIM_OFF);
        }
    } else if (code == LV_EVENT_DEFOCUSED) {
        lv_keyboard_set_textarea(kb, nullptr);
        lv_obj_set_height(obj, LV_VER_RES);
        lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
    } else if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
        lv_obj_set_height(obj, LV_VER_RES);
        lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_state(e->target, LV_STATE_FOCUSED);
        lv_indev_reset(nullptr, e->target);
    }
}

static void ta_login_form_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_VALUE_CHANGED) {
        if (strlen(lv_textarea_get_text(txt_usuario_login)) >= 8 && strlen(lv_textarea_get_text(txt_password_login))) {
            lv_obj_clear_state(btn_login, LV_STATE_DISABLED);
        } else {
            lv_obj_add_state(btn_login, LV_STATE_DISABLED);
        }
    }
}

static void ta_select_form_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        // los datos de usuario contienen el código del punto de control
        punto = (char *) lv_event_get_user_data(e);
        set_estado_splash("Activando punto de acceso en Séneca");
        lv_scr_load(scr_splash);
    }
    if (code == LV_EVENT_DELETE) {
        lv_mem_free(lv_event_get_user_data(e));
    }
}

static void ta_config_click_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        configuring = 1;
    }
}

static void ta_keypad_click_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        keypad_requested = 1;
    }
}

String rsa_cifrar(const char *cadena, const char *e, const char *n) {
    char buffer[100];
    size_t buffer_len;

    // resultado = m^e mod n
    mbedtls_mpi m_m, m_e, m_n, m_resultado;
    mbedtls_mpi_init(&m_m);
    mbedtls_mpi_init(&m_e);
    mbedtls_mpi_init(&m_n);
    mbedtls_mpi_init(&m_resultado);

    // convertir las cadenas en enteros grandes
    mbedtls_mpi_read_string(&m_m, 10, cadena);
    mbedtls_mpi_read_string(&m_e, 10, e);
    mbedtls_mpi_read_string(&m_n, 10, n);
    mbedtls_mpi_exp_mod(&m_resultado, &m_m, &m_e, &m_n, nullptr);

    // convertir el entero grande del resultado en cadena
    mbedtls_mpi_write_string(&m_resultado, 10, buffer, sizeof(buffer), &buffer_len);

    return String(buffer);
}

static void ta_login_submit_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        String datos;
        usuario = lv_textarea_get_text(txt_usuario_login);

        // La autenticación de Séneca convierte cada carácter del password a su código ASCII y se concatenan
        // El resultado es el número al que se aplica el cifrado RSA
        String clave_convertida = "";
        const char *ptr = lv_textarea_get_text(txt_password_login);

        while(*ptr) {
            clave_convertida += (int) (*ptr);
            ptr++;
        }

        // e y n están sacados del formulario
        String clave = rsa_cifrar(clave_convertida.c_str(), "3584956249", "356056806984207294102423357623243547284021");

        // generar parámetros del formulario de autenticación
        datos = "N_V_=NV_" + String(random(10000)) + "&rndval=" + random(100000000) + "&NAV_WEB_NOMBRE=Netscape&NAV_WEB_VERSION=5&RESOLUCION=1024&CLAVECIFRADA="
                + clave + "&USUARIO=" + usuario + "&C_INTERFAZ=SENECA";

        response = "";

        send_seneca_login_data(datos);
        set_estado_splash("Activando usuario en Séneca");
    }
}

static void btn_config_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *obj = lv_event_get_target(e);

    if (code == LV_EVENT_DRAW_PART_BEGIN) {
        lv_obj_draw_part_dsc_t *dsc = (lv_obj_draw_part_dsc_t *) lv_event_get_param(e);
        dsc->label_dsc->font = &mulish_24;

        if (dsc->id == 1) {
            if (lv_btnmatrix_get_selected_btn(obj) == dsc->id) {
                dsc->rect_dsc->bg_color = lv_palette_darken(LV_PALETTE_RED, 3);
            } else {
                dsc->rect_dsc->bg_color = lv_palette_main(LV_PALETTE_RED);
            }
            dsc->label_dsc->color = lv_color_white();
        }
        if (dsc->id == 0) {
            if (lv_btnmatrix_get_selected_btn(obj) == dsc->id) {
                dsc->rect_dsc->bg_color = lv_palette_darken(LV_PALETTE_GREEN, 3);
            } else {
                dsc->rect_dsc->bg_color = lv_palette_main(LV_PALETTE_GREEN);
            }
            dsc->label_dsc->color = lv_color_white();
        }
    }
    if (code == LV_EVENT_VALUE_CHANGED) {
        uint32_t id = lv_btnmatrix_get_selected_btn(obj);
        if (id == 0) {
            int reboot = 0;
            const char *str_ssid = lv_textarea_get_text(txt_ssid_config);
            const char *str_psk = lv_textarea_get_text(txt_psk_config);
            if (!NVS.getString("net.wifi_ssid").equals(str_ssid) ||
                !NVS.getString("net.wifi_psk").equals(str_psk)
            ) {
                NVS.setString("net.wifi_ssid", lv_textarea_get_text(txt_ssid_config));
                NVS.setString("net.wifi_psk", lv_textarea_get_text(txt_psk_config));
                reboot = 1;
            }
            NVS.commit();
            if (reboot) {
                esp_restart();
            }
        }
        configuring = 0;
    }
}

static void btn_reset_wifi_config_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        lv_textarea_set_text(txt_ssid_config, PUNTO_CONTROL_SSID_PREDETERMINADO);
        lv_textarea_set_text(txt_psk_config, PUNTO_CONTROL_PSK_PREDETERMINADA);
    }
}

static void btn_numpad_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *obj = lv_event_get_target(e);
    lv_obj_t *txt = (lv_obj_t *) (lv_event_get_user_data(e));

    if (code == LV_EVENT_DRAW_PART_BEGIN) {
        lv_obj_draw_part_dsc_t *dsc = (lv_obj_draw_part_dsc_t *) lv_event_get_param(e);
        dsc->label_dsc->font = &mulish_24;

        if (dsc->id == 10) {
            if (lv_btnmatrix_get_selected_btn(obj) == dsc->id) {
                dsc->rect_dsc->bg_color = lv_palette_darken(LV_PALETTE_RED, 3);
            } else {
                dsc->rect_dsc->bg_color = lv_palette_main(LV_PALETTE_RED);
            }
            dsc->label_dsc->color = lv_color_white();
        }
        if (dsc->id == 13) {
            if (lv_btnmatrix_get_selected_btn(obj) == dsc->id) {
                dsc->rect_dsc->bg_color = lv_palette_darken(LV_PALETTE_GREEN, 3);
            } else {
                dsc->rect_dsc->bg_color = lv_palette_main(LV_PALETTE_GREEN);
            }
            dsc->label_dsc->color = lv_color_white();
        }
    }
    if (code == LV_EVENT_VALUE_CHANGED) {
        uint32_t id = lv_btnmatrix_get_selected_btn(obj);
        const char *btn = lv_btnmatrix_get_btn_text(obj, id);
        switch (id) {
            case 10:
                read_code = "";
                keypad_done = 1;
                break;
            case 11:
                lv_textarea_set_password_mode(txt, !lv_textarea_get_password_mode(txt));
                lv_textarea_cursor_left(txt);
                lv_textarea_cursor_right(txt);
                break;
            case 12:
                lv_textarea_del_char(txt);
                break;
            case 13:
                read_code = lv_textarea_get_text(txt);
                keypad_done = 1;
                break;
            default:
                lv_textarea_add_char(txt, btn[0]);
        }
    }
}

void create_scr_calibracion() {
    scr_calibracion = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scr_calibracion, LV_COLOR_MAKE(255, 255, 255), LV_PART_MAIN);

    // Etiqueta con instrucciones de calibración
    lv_obj_t *lbl_calibracion = lv_label_create(scr_calibracion);
    lv_label_set_text(lbl_calibracion, "Presione las puntas de las flechas rojas con la máxima precisión posible");
    lv_obj_set_style_text_font(lbl_calibracion, &mulish_32, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_calibracion, lv_palette_main(LV_PALETTE_LIGHT_BLUE), LV_PART_MAIN);
    lv_obj_set_style_text_align(lbl_calibracion, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_label_set_long_mode(lbl_calibracion, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(lbl_calibracion, SCREEN_WIDTH * 9 / 10);
    lv_obj_align(lbl_calibracion, LV_ALIGN_CENTER, 0, 0);
}

void create_scr_splash() {
    // CREAR PANTALLA DE ARRANQUE (splash screen)
    scr_splash = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scr_splash, LV_COLOR_MAKE(255, 255, 255), LV_PART_MAIN);

    // Logo de la CED centrado
    LV_IMG_DECLARE(logo_ced);
    lv_obj_t *img = lv_img_create(scr_splash);
    lv_img_set_src(img, &logo_ced);
    lv_obj_align(img, LV_ALIGN_CENTER, 0, 0);

    // Spinner para indicar operación en progreso en la esquina inferior derecha
    spinner_splash = lv_spinner_create(scr_splash, 1000, 45);
    lv_obj_set_size(spinner_splash, 50, 50);
    lv_obj_align(spinner_splash, LV_ALIGN_BOTTOM_RIGHT, -10, -10);

    // Etiqueta de estado actual centrada en la parte superior
    lbl_estado_splash = lv_label_create(scr_splash);
    lv_obj_set_style_text_font(lbl_estado_splash, &mulish_24, LV_PART_MAIN);
    lv_obj_set_style_text_align(lbl_estado_splash, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    set_estado_splash("Inicializando...");

    // Etiqueta de dirección IP actual centrada en la parte inferior
    lbl_ip_splash = lv_label_create(scr_splash);
    lv_obj_set_style_text_font(lbl_ip_splash, &mulish_16, LV_PART_MAIN);
    lv_obj_set_style_text_align(lbl_ip_splash, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    set_ip_splash("Esperando configuración");

    // Etiqueta de versión del software en la esquina inferior izquierda
    lbl_version_splash = lv_label_create(scr_splash);
    lv_obj_set_style_text_font(lbl_version_splash, &mulish_16, LV_PART_MAIN);
    lv_obj_set_style_text_align(lbl_version_splash, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    lv_label_set_text(lbl_version_splash, PUNTO_CONTROL_VERSION);
    lv_obj_align(lbl_version_splash, LV_ALIGN_BOTTOM_LEFT, 10, -10);

    btn_config_splash = create_config_button(scr_splash);
    lv_obj_add_flag(btn_config_splash, LV_OBJ_FLAG_HIDDEN);
}

void create_scr_main() {
    // CREAR PANTALLA PRINCIPAL
    scr_main = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scr_main, LV_COLOR_MAKE(255, 255, 255), LV_PART_MAIN);

    // Etiqueta con el nombre del punto de acceso en la parte superior
    lbl_nombre_main = lv_label_create(scr_main);
    lv_label_set_text(lbl_nombre_main, "Punto de control WiFi");
    lv_obj_set_style_text_font(lbl_nombre_main, &mulish_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_nombre_main, lv_palette_main(LV_PALETTE_DEEP_ORANGE), LV_PART_MAIN);
    lv_obj_set_style_text_align(lbl_nombre_main, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_label_set_long_mode(lbl_nombre_main, LV_LABEL_LONG_SCROLL_CIRCULAR);

    // Etiqueta de hora y fecha actual centrada justo debajo
    lbl_hora_main = lv_label_create(scr_main);
    lv_obj_set_style_text_font(lbl_hora_main, &mulish_64_numbers, LV_PART_MAIN);
    lv_obj_set_style_text_align(lbl_hora_main, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lbl_fecha_main = lv_label_create(scr_main);
    lv_obj_set_style_text_font(lbl_fecha_main, &mulish_24, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_fecha_main, lv_palette_main(LV_PALETTE_GREY), LV_PART_MAIN);
    lv_obj_set_style_text_align(lbl_fecha_main, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    set_hora_main("");
    set_fecha_main("");

    // Etiqueta de estado centrada en pantalla
    lbl_estado_main = lv_label_create(scr_main);
    lv_obj_set_style_text_font(lbl_estado_main, &mulish_32, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_estado_main, lv_palette_main(LV_PALETTE_BLUE_GREY), LV_PART_MAIN);
    lv_obj_set_style_text_align(lbl_estado_main, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    set_estado_main("");

    // Imagen de la flecha en la parte inferior
    lbl_icon_main = lv_label_create(scr_main);
    lv_obj_set_style_text_font(lbl_icon_main, &symbols, LV_PART_MAIN);
    set_icon_text(lbl_icon_main, "\uF063", LV_PALETTE_BLUE, 1);

    // Icono de configuración
    btn_config_main = create_config_button(scr_main);

    // Icono de teclado
    lv_obj_t *btn_keypad_main = lv_btn_create(scr_main);
    lv_obj_t *lbl = lv_label_create(btn_keypad_main);
    lv_obj_set_style_bg_opa(btn_keypad_main, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_text_font(lbl, &mulish_32, LV_PART_MAIN);
    lv_label_set_text(lbl, LV_SYMBOL_KEYBOARD);
    lv_obj_set_style_text_color(lbl, lv_palette_main(LV_PALETTE_INDIGO), LV_PART_MAIN);
    lv_obj_align(btn_keypad_main, LV_ALIGN_BOTTOM_RIGHT, -10, -10);
    lv_obj_add_event_cb(btn_keypad_main, ta_keypad_click_event_cb, LV_EVENT_CLICKED, scr_main);
    lv_obj_add_event_cb(scr_main, ta_keypad_click_event_cb, LV_EVENT_CLICKED, scr_main);
}

void create_scr_check() {
    // CREAR PANTALLA DE COMPROBACIÓN
    scr_check = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scr_check, LV_COLOR_MAKE(255, 255, 255), LV_PART_MAIN);

    // Icono del estado de comprobación
    lbl_icon_check = lv_label_create(scr_check);
    lv_obj_set_style_text_font(lbl_icon_check, &symbols, LV_PART_MAIN);
    set_icon_text(lbl_icon_check, "\uF252", LV_PALETTE_BLUE, 0);

    // Etiqueta de estado de comprobación
    lbl_estado_check = lv_label_create(scr_check);
    lv_obj_set_style_text_font(lbl_estado_check, &mulish_32, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_estado_check, lv_palette_main(LV_PALETTE_BLUE_GREY), LV_PART_MAIN);
    lv_obj_set_style_text_align(lbl_estado_check, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_label_set_long_mode(lbl_estado_check, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(lbl_estado_check, SCREEN_WIDTH * 9 / 10);
    set_estado_check("");
}

void create_scr_login() {
    // CREAR PANTALLA DE ENTRADA DE CREDENCIALES
    scr_login = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scr_login, LV_COLOR_MAKE(255, 255, 255), LV_PART_MAIN);

    // Crear teclado
    kb = lv_keyboard_create(scr_login);
    lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);

    // Crear panel del formulario
    pnl_login = lv_obj_create(scr_login);
    lv_obj_set_height(pnl_login, LV_VER_RES);
    lv_obj_set_width(pnl_login, lv_pct(90));

    static lv_style_t style_text_muted;
    static lv_style_t style_title;

    lv_style_init(&style_title);
    lv_style_set_text_font(&style_title, &mulish_24);

    lv_style_init(&style_text_muted);
    lv_style_set_text_opa(&style_text_muted, LV_OPA_50);

    // Título del panel
    lv_obj_t *lbl_titulo_login = lv_label_create(pnl_login);
    lv_label_set_text(lbl_titulo_login, "Activar punto de control WiFi");
    lv_obj_add_style(lbl_titulo_login, &style_title, LV_PART_MAIN);

    // Campo de usuario IdEA
    lv_obj_t *lbl_usuario_login = lv_label_create(pnl_login);
    lv_label_set_text(lbl_usuario_login, "Usuario IdEA");
    lv_obj_add_style(lbl_usuario_login, &style_text_muted, LV_PART_MAIN);

    txt_usuario_login = lv_textarea_create(pnl_login);
    lv_textarea_set_one_line(txt_usuario_login, true);
    lv_textarea_set_placeholder_text(txt_usuario_login, "Personal directivo/administrativo");
    lv_obj_add_event_cb(txt_usuario_login, ta_kb_form_event_cb, LV_EVENT_ALL, pnl_login);
    lv_obj_add_event_cb(txt_usuario_login, ta_login_form_event_cb, LV_EVENT_VALUE_CHANGED, nullptr);

    // Campo de contraseña
    lv_obj_t *lbl_password_login = lv_label_create(pnl_login);
    lv_label_set_text(lbl_password_login, "Contraseña");
    lv_obj_add_style(lbl_password_login, &style_text_muted, LV_PART_MAIN);

    txt_password_login = lv_textarea_create(pnl_login);
    lv_textarea_set_one_line(txt_password_login, true);
    lv_textarea_set_password_mode(txt_password_login, true);
    lv_textarea_set_placeholder_text(txt_password_login, "No se almacenará");
    lv_obj_add_event_cb(txt_password_login, ta_kb_form_event_cb, LV_EVENT_ALL, pnl_login);
    lv_obj_add_event_cb(txt_password_login, ta_login_form_event_cb, LV_EVENT_VALUE_CHANGED, nullptr);

    // Botón de inicio de sesión
    btn_login = lv_btn_create(pnl_login);
    lv_obj_add_state(btn_login, LV_STATE_DISABLED);
    lv_obj_set_height(btn_login, LV_SIZE_CONTENT);
    lv_obj_add_event_cb(btn_login, ta_login_submit_event_cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *lbl_login = lv_label_create(btn_login);
    lv_label_set_text(lbl_login, "Iniciar sesión");
    lv_obj_align(lbl_login, LV_ALIGN_TOP_MID, 0, 0);

    lbl_error_login = lv_label_create(pnl_login);
    lv_label_set_text(lbl_error_login, "");
    lv_obj_set_style_text_font(lbl_error_login, &mulish_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_error_login, lv_palette_main(LV_PALETTE_RED), LV_PART_MAIN);
    lv_obj_set_style_text_align(lbl_error_login, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_label_set_long_mode(lbl_error_login, LV_LABEL_LONG_WRAP);

    // Colocar elementos en una rejilla (9 filas, 2 columnas)
    static lv_coord_t grid_login_col_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(3), LV_GRID_TEMPLATE_LAST};
    static lv_coord_t grid_login_row_dsc[] = {
            LV_GRID_CONTENT,  /* Título */
            5,                /* Separador */
            LV_GRID_CONTENT,  /* Usuario */
            5,                /* Separador */
            LV_GRID_CONTENT,  /* Contraseña */
            5,                /* Separador */
            LV_GRID_CONTENT,  /* Botón */
            5,                /* Separador */
            LV_GRID_CONTENT,  /* Etiqueta error */
            LV_GRID_TEMPLATE_LAST
    };

    lv_obj_set_grid_dsc_array(pnl_login, grid_login_col_dsc, grid_login_row_dsc);

    lv_obj_set_grid_cell(lbl_titulo_login, LV_GRID_ALIGN_START, 0, 2, LV_GRID_ALIGN_CENTER, 0, 1);
    lv_obj_set_grid_cell(lbl_usuario_login, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, 2, 1);
    lv_obj_set_grid_cell(txt_usuario_login, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_START, 2, 1);
    lv_obj_set_grid_cell(lbl_password_login, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, 4, 1);
    lv_obj_set_grid_cell(txt_password_login, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_START, 4, 1);
    lv_obj_set_grid_cell(btn_login, LV_GRID_ALIGN_STRETCH, 0, 2, LV_GRID_ALIGN_START, 6, 1);
    lv_obj_set_grid_cell(lbl_error_login, LV_GRID_ALIGN_STRETCH, 0, 2, LV_GRID_ALIGN_START, 8, 1);

    // Quitar borde al panel y pegarlo a la parte superior de la pantalla
    lv_obj_set_style_border_width(pnl_login, 0, LV_PART_MAIN);
    lv_obj_align(pnl_login, LV_ALIGN_TOP_MID, 0, 0);
}

void create_scr_config() {
    static const char *botones[] = { "" LV_SYMBOL_OK, "" LV_SYMBOL_CLOSE, nullptr };
    // CREAR PANTALLA DE CONFIGURACIÓN
    scr_config = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scr_config, LV_COLOR_MAKE(255, 255, 255), LV_PART_MAIN);

    // Ocultar teclado
    lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);

    static lv_style_t style_text_muted;
    static lv_style_t style_title;

    lv_style_init(&style_title);
    lv_style_set_text_font(&style_title, &mulish_24);

    lv_style_init(&style_text_muted);
    lv_style_set_text_opa(&style_text_muted, LV_OPA_50);

    // Crear pestañas
    lv_obj_t *tabview_config;
    tabview_config = lv_tabview_create(scr_config, LV_DIR_TOP, 50);
    lv_obj_t * tab_btns = lv_tabview_get_tab_btns(tabview_config);
    lv_obj_set_style_pad_right(tab_btns, LV_HOR_RES * 2 / 10 + 3, 0);

    lv_obj_set_height(tabview_config, LV_VER_RES);
    lv_obj_t *tab_red_config = lv_tabview_add_tab(tabview_config, "Red");
    lv_obj_t *tab2 = lv_tabview_add_tab(tabview_config, "Pantalla");
    lv_obj_t *tab3 = lv_tabview_add_tab(tabview_config, "Seguridad");

    // Crear botón de aceptar y cancelar
    lv_obj_t *btn_matrix_config = lv_btnmatrix_create(scr_config);
    lv_btnmatrix_set_map(btn_matrix_config, botones);
    lv_obj_set_width(btn_matrix_config, LV_HOR_RES * 2 / 10);
    lv_obj_set_height(btn_matrix_config, 50);
    lv_obj_set_style_pad_all(btn_matrix_config, 3, LV_PART_MAIN);
    lv_obj_align(btn_matrix_config, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_add_event_cb(btn_matrix_config, btn_config_event_cb, LV_EVENT_ALL, nullptr);

    // Panel de configuración de red
    lv_obj_set_style_pad_left(tab_red_config, LV_HOR_RES * 8 / 100, 0);
    lv_obj_set_style_pad_right(tab_red_config, LV_HOR_RES * 8 / 100, 0);

    lv_obj_t *lbl_red_config = lv_label_create(tab_red_config);
    lv_label_set_text(lbl_red_config, "Configuración de WiFi");
    lv_obj_add_style(lbl_red_config, &style_title, LV_PART_MAIN);

    // Campo SSID
    lv_obj_t *lbl_ssid_config = lv_label_create(tab_red_config);
    lv_label_set_text(lbl_ssid_config, "Nombre red");
    lv_obj_add_style(lbl_ssid_config, &style_text_muted, LV_PART_MAIN);

    txt_ssid_config = lv_textarea_create(tab_red_config);
    lv_textarea_set_one_line(txt_ssid_config, true);
    lv_textarea_set_placeholder_text(txt_ssid_config, "SSID de la red");
    lv_obj_add_event_cb(txt_ssid_config, ta_kb_form_event_cb, LV_EVENT_ALL, tabview_config);

    // Campo de PSK de la WiFi
    lv_obj_t *lbl_psk_config = lv_label_create(tab_red_config);
    lv_label_set_text(lbl_psk_config, "Contraseña");
    lv_obj_add_style(lbl_psk_config, &style_text_muted, LV_PART_MAIN);

    txt_psk_config = lv_textarea_create(tab_red_config);
    lv_textarea_set_one_line(txt_psk_config, true);
    lv_textarea_set_password_mode(txt_psk_config, true);
    lv_textarea_set_placeholder_text(txt_psk_config, "Mínimo 6 caracteres");
    lv_obj_add_event_cb(txt_psk_config, ta_kb_form_event_cb, LV_EVENT_ALL, tabview_config);

    // Botón restaurar configuración
    lv_obj_t *btn_wifi_reset_config = lv_btn_create(tab_red_config);
    lv_obj_set_height(btn_wifi_reset_config, LV_SIZE_CONTENT);
    lv_obj_add_event_cb(btn_wifi_reset_config, btn_reset_wifi_config_event_cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *lbl_reset_wifi = lv_label_create(btn_wifi_reset_config);
    lv_label_set_text(lbl_reset_wifi, "Restaurar conexión " PUNTO_CONTROL_SSID_PREDETERMINADO);
    lv_obj_align(lbl_reset_wifi, LV_ALIGN_TOP_MID, 0, 0);

    // Colocar elementos en una rejilla (7 filas, 2 columnas)
    static lv_coord_t grid_login_col_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(3), LV_GRID_TEMPLATE_LAST};
    static lv_coord_t grid_login_row_dsc[] = {
            LV_GRID_CONTENT,  /* Título */
            LV_GRID_CONTENT,  /* SSID */
            LV_GRID_CONTENT,  /* PSK */
            LV_GRID_CONTENT,  /* Restaurar, botón */
            LV_GRID_TEMPLATE_LAST
    };

    lv_obj_set_grid_dsc_array(tab_red_config, grid_login_col_dsc, grid_login_row_dsc);

    lv_obj_set_grid_cell(lbl_red_config, LV_GRID_ALIGN_START, 0, 2, LV_GRID_ALIGN_CENTER, 0, 1);
    lv_obj_set_grid_cell(lbl_ssid_config, LV_GRID_ALIGN_END, 0, 1, LV_GRID_ALIGN_CENTER, 1, 1);
    lv_obj_set_grid_cell(txt_ssid_config, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_START, 1, 1);
    lv_obj_set_grid_cell(lbl_psk_config, LV_GRID_ALIGN_END, 0, 1, LV_GRID_ALIGN_CENTER, 2, 1);
    lv_obj_set_grid_cell(txt_psk_config, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_START, 2, 1);
    lv_obj_set_grid_cell(btn_wifi_reset_config, LV_GRID_ALIGN_STRETCH, 0, 2, LV_GRID_ALIGN_CENTER, 3, 1);

    lv_obj_scroll_to_view_recursive(txt_ssid_config, LV_ANIM_ON);
}

void create_scr_selection() {
    // CREAR PANTALLA DE SELECCIÓN DE PUNTO DE CONTROL
    scr_selection = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scr_selection, LV_COLOR_MAKE(255, 255, 255), LV_PART_MAIN);

    // Crear panel del formulario
    pnl_selection = lv_obj_create(scr_selection);
    lv_obj_set_height(pnl_selection, LV_VER_RES);
    lv_obj_set_width(pnl_selection, lv_pct(90));

    static lv_style_t style_text_muted;
    static lv_style_t style_title;

    lv_style_init(&style_title);
    lv_style_set_text_font(&style_title, &mulish_24);

    lv_style_init(&style_text_muted);
    lv_style_set_text_opa(&style_text_muted, LV_OPA_50);

    // Título del panel
    lv_obj_t *lbl_titulo_selection = lv_label_create(pnl_selection);
    lv_label_set_text(lbl_titulo_selection, "Seleccionar punto de control");
    lv_obj_add_style(lbl_titulo_selection, &style_title, LV_PART_MAIN);

    // Lista de puntos de acceso
    lst_selection = lv_list_create(pnl_selection);
    lv_obj_set_width(lst_selection, lv_pct(100));
    lv_obj_set_height(lst_selection, LV_SIZE_CONTENT);
    lv_obj_align_to(lst_selection, lbl_titulo_selection, LV_ALIGN_TOP_MID, 0, 30);

    // Quitar borde al panel y pegarlo a la parte superior de la pantalla
    lv_obj_set_style_border_width(pnl_selection, 0, LV_PART_MAIN);
    lv_obj_align(pnl_selection, LV_ALIGN_TOP_MID, 0, 0);
}

void create_scr_codigo() {
    scr_codigo = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scr_codigo, LV_COLOR_MAKE(255, 255, 255), LV_PART_MAIN);

    static const char *botones[] = {"1", "2", "3", "4", "5", "\n",
                                    "6", "7", "8", "9", "0", "\n",
                                    LV_SYMBOL_CLOSE, LV_SYMBOL_EYE_CLOSE, LV_SYMBOL_BACKSPACE, "Enviar", ""};

    txt_codigo = lv_textarea_create(scr_codigo);

    // Etiqueta de información
    lbl_estado_codigo = lv_label_create(scr_codigo);
    lv_obj_set_style_text_font(lbl_estado_codigo, &mulish_32, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_estado_codigo, lv_palette_main(LV_PALETTE_BLUE_GREY), LV_PART_MAIN);
    lv_obj_set_style_text_align(lbl_estado_codigo, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_label_set_long_mode(lbl_estado_codigo, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(lbl_estado_codigo, lv_pct(100));
    set_estado_codigo("");

    // Campo de código
    lv_obj_set_style_text_font(txt_codigo, &mulish_64_numbers, LV_PART_MAIN);
    lv_obj_set_style_text_color(txt_codigo, lv_palette_main(LV_PALETTE_INDIGO), LV_PART_MAIN);
    lv_textarea_set_align(txt_codigo, LV_TEXT_ALIGN_CENTER);
    lv_textarea_set_one_line(txt_codigo, true);
    lv_textarea_set_password_mode(txt_codigo, true);
    lv_obj_set_style_border_width(txt_codigo, 0, LV_PART_MAIN);
    lv_obj_set_width(txt_codigo, lv_pct(100));
    lv_obj_add_state(txt_codigo, LV_STATE_FOCUSED);

    // Teclado numérico
    lv_obj_t *btn_matrix_numeros = lv_btnmatrix_create(scr_codigo);
    lv_btnmatrix_set_map(btn_matrix_numeros, botones);
    lv_btnmatrix_set_btn_width(btn_matrix_numeros, 13, 2);
    lv_btnmatrix_set_btn_ctrl(btn_matrix_numeros, 11, LV_BTNMATRIX_CTRL_CHECKABLE);
    lv_obj_align(btn_matrix_numeros, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_event_cb(btn_matrix_numeros, btn_numpad_event_cb, LV_EVENT_ALL, txt_codigo);
    lv_obj_set_width(btn_matrix_numeros, lv_pct(100));
    lv_obj_set_height(btn_matrix_numeros, LV_VER_RES / 2 + 25);
    lv_obj_align(btn_matrix_numeros, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_clear_flag(btn_matrix_numeros, LV_OBJ_FLAG_CLICK_FOCUSABLE);

    lv_obj_align_to(txt_codigo, btn_matrix_numeros, LV_ALIGN_OUT_TOP_MID, 0, 0);
}

void update_scr_config() {
    lv_textarea_set_text(txt_ssid_config, NVS.getString("net.wifi_ssid").c_str());
    lv_textarea_set_text(txt_psk_config, NVS.getString("net.wifi_psk").c_str());
}

void loop() {
    lv_task_handler();
    delay(5);
}
