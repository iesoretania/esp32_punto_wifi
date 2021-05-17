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

const char* ssid = "\x41\x6E\x64\x61\x72\x65\x64";
const char* psk =  "6b629f4c299371737494c61b5a101693a2d4e9e1f3e1320f3ebf9ae379cecf32";

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

    return;
}

// Pantallas
static lv_obj_t *scr_splash;     // Pantalla de inicio (splash screen)
static lv_obj_t *lbl_ip_splash;
static lv_obj_t *lbl_estado_splash;

static lv_obj_t *scr_main;       // Pantalla principal de lectura de código
static lv_obj_t *lbl_nombre_main;
static lv_obj_t *lbl_hora_main;
static lv_obj_t *lbl_fecha_main;
static lv_obj_t *lbl_estado_main;
static lv_obj_t *lbl_icon_main;

static lv_obj_t *scr_check;      // Pantalla de comprobación
static lv_obj_t *lbl_estado_check;
static lv_obj_t *lbl_icon_check;

static lv_obj_t *scr_login;      // Pantalla de entrada de credenciales
static lv_obj_t *pnl_login;
static lv_obj_t *txt_usuario_login;
static lv_obj_t *txt_password_login;
static lv_obj_t *btn_login;

static lv_obj_t *scr_config;     // Pantalla de configuración

void create_scr_splash();

void create_scr_main();

void create_scr_check();

void create_scr_login();

void initialize_gui();

void task_main(lv_timer_t *);

void initialize_flash();

void initialize_http_client();

void set_icon_text(lv_obj_t *pObj, const char *string, lv_color_t param, int bottom);

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
    lv_obj_align(lbl_nombre_main, LV_ALIGN_TOP_MID, 0, 15);
}

void set_hora_main_format(const char *string, const char *p) {
    lv_label_set_text_fmt(lbl_hora_main, string, p);
    lv_obj_align_to(lbl_hora_main, lbl_nombre_main, LV_ALIGN_OUT_BOTTOM_MID, 0, 15);
}

void set_hora_main(const char *string) {
    lv_label_set_text(lbl_hora_main, string);
    lv_obj_align_to(lbl_hora_main, lbl_nombre_main, LV_ALIGN_OUT_BOTTOM_MID, 0, 15);
}

void set_fecha_main_format(const char *string, const char *p) {
    lv_label_set_text_fmt(lbl_fecha_main, string, p);
    lv_obj_align_to(lbl_fecha_main, lbl_hora_main, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);
}

void set_fecha_main(const char *string) {
    lv_label_set_text(lbl_fecha_main, string);
    lv_obj_align_to(lbl_fecha_main, lbl_hora_main, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);
}

void set_estado_main_format(const char *string, const char *p) {
    lv_label_set_text_fmt(lbl_estado_main, string, p);
    lv_obj_align_to(lbl_estado_main, lbl_fecha_main, LV_ALIGN_OUT_BOTTOM_MID, 0, 20);
}

void set_estado_main(const char *string) {
    lv_label_set_text(lbl_estado_main, string);
    lv_obj_align_to(lbl_estado_main, lbl_fecha_main, LV_ALIGN_OUT_BOTTOM_MID, 0, 20);
}

void set_estado_check_format(const char *string, const char *p) {
    lv_label_set_text_fmt(lbl_estado_check, string, p);
    lv_obj_align_to(lbl_estado_check, lbl_icon_check, LV_ALIGN_OUT_BOTTOM_MID, 0, 25);
}

void set_estado_check(const char *string) {
    lv_label_set_text(lbl_estado_check, string);
    lv_obj_align_to(lbl_estado_check, lbl_icon_check, LV_ALIGN_OUT_BOTTOM_MID, 0, 25);
}

void set_icon_text(lv_obj_t *label, const char *text, lv_palette_t color, int bottom) {
    lv_obj_set_style_text_color(label, lv_palette_main(color), LV_PART_MAIN);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(label, bottom ? LV_ALIGN_BOTTOM_MID : LV_ALIGN_TOP_MID, 0, bottom ? -15 : 15);
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

int send_seneca_request_data(String &body) {
    // preparar cookies de petición
    String http_cookie = "";

    if (cookieCPP_authToken && cookieCPP_authToken.length() > 0) {
        http_cookie = cookieCPP_authToken;

        if (cookieCPP_puntToken && cookieCPP_puntToken.length() > 0) {
            http_cookie += "; " + cookieCPP_puntToken;
        }
        if (cookieSenecaP && cookieSenecaP.length() > 0) {
            http_cookie += "; " + cookieSenecaP;
        }
        if (cookieJSESSIONID && cookieJSESSIONID.length() > 0) {
            http_cookie += "; " + cookieJSESSIONID;
        }
    }

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
String modo, punto, xCentro, nombre_punto;

int xmlStatus;
String xml_current_input;

void xml_cookie_callback(uint8_t statusflags, char *tagName, uint16_t tagNameLen, char *data, uint16_t dataLen) {
    // nos quedamos con las etiquetas <input>
    if ((statusflags & STATUS_START_TAG) && strstr(tagName, "input")) {
        xmlStatus = 1;
    }
    if ((statusflags & STATUS_END_TAG) && (strstr(tagName, "input") || strstr(tagName, "h1"))) {
        xmlStatus = 0;
    }
    if ((statusflags & STATUS_START_TAG) && strstr(tagName, "h1")) {
        xmlStatus = 2;
    }

    // si estamos dentro de una etiqueta input, buscar el atributo "name" y "value"
    if (xmlStatus == 1) {
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
    }

    // si estamos dentro de una etiqueta <h1>, obtener nombre del punto de control
    if (xmlStatus == 2 && statusflags & STATUS_TAG_TEXT) {
        nombre_punto = data;
    }
}

void process_token(String uid) {
    // enviar token a Séneca
    String params =
            "_MODO_=" + modo + "&PUNTO=" + punto + "&X_CENTRO=" + xCentro + "&C_TIPACCCONTPRE=&DARK_MODE=N&TOKEN=" +
            uid;
    send_seneca_request_data(params);
}

void process_seneca_response() {
    // análisis XML del HTML devuelto para extraer los valores de los campos ocultos
    xml.init(xmlBuffer, 2048, xml_cookie_callback);

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
    }
}

void task_wifi_connection(lv_timer_t *timer) {
    static enum {
        CONNECTING, NTP_START, NTP_WAIT, CHECKING, CHECK_WAIT, CHECK_RETRY, WAITING, DONE
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
            if (WiFi.status() == WL_CONNECTED) {
                state = NTP_START;
                set_ip_splash_format("IP: %s", WiFi.localIP().toString().c_str());
                set_estado_splash("Comprobando conectividad Andared...");
                code = WiFi.hostByName("c0", ip);
            } else {
                cuenta++;
            }
            break;
        case NTP_START:
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

            send_seneca_request_data(body);

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
                state = WAITING;
            }
            cuenta = 0;
            break;
        case CHECK_RETRY:
            cuenta++;
            if (cuenta == 10) {
                state = CHECKING;
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
    }
}

void task_main(lv_timer_t *timer) {
    const char *dia_semana[] = {"Domingo", "Lunes", "Martes", "Miércoles", "Jueves", "Viernes", "Sábado"};
    static enum {
        DONE, IDLE, CARD_PRESENT, CHECK_ONGOING, CHECK_RESULT_WAIT, NO_NETWORK
    } state = IDLE;
    static int cuenta = 0;
    static String uidS;

    time_t now;
    char strftime_buf[64];
    char fecha_final[64];
    struct tm timeinfo;

    switch (state) {
        case DONE:
            lv_timer_del(timer);
            return;
        case IDLE:
            cuenta++;

            if (WiFi.status() != WL_CONNECTED) {
                state = NO_NETWORK;
            } else {
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

                        set_icon_text(lbl_icon_check, "\uF252", LV_PALETTE_TEAL, 0);
                        set_estado_check("Llavero detectado. Comunicando con Séneca, espere por favor...");
                        lv_scr_load(scr_check);
                        state = CARD_PRESENT;
                        cuenta = 0;
                        mfrc522.PICC_HaltA();
                        break;
                    }
                    mfrc522.PICC_HaltA();
                }

                time(&now);

                localtime_r(&now, &timeinfo);
                strftime(strftime_buf, sizeof(strftime_buf), "%T", &timeinfo);
                set_hora_main(strftime_buf);

                strftime(strftime_buf, sizeof(strftime_buf), "%d/%m/%G", &timeinfo);
                strcpy(fecha_final, dia_semana[timeinfo.tm_wday]);
                strcat(fecha_final, ", ");
                strcat(fecha_final, strftime_buf);
                set_fecha_main(fecha_final);

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
                if (cuenta % 40 == 0) {
                    process_token(uidS);
                }
                break;
            }
            if (http_request_status == HTTP_ONGOING) {
                break;
            }
            process_seneca_response();
            parse_seneca_response();
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
            set_hora_main("NO HAY RED");
            if (WiFi.status() == WL_CONNECTED) {
                state = IDLE;
            }
            break;
    }
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

    // Inicializar bus SPI y el TFT conectado a través de él
    SPI.begin();
    tft.begin();            /* Inicializar TFT */
    tft.setRotation(3);     /* Orientación horizontal */

    // Inicializar lector
    mfrc522.PCD_Init();

    // Inicializar flash
    initialize_flash();

    // Inicializar GUI
    initialize_gui();

    // Inicializar subsistema HTTP
    initialize_http_client();

    // Crear pantallas y cambiar a la pantalla de arranque
    create_scr_splash();
    create_scr_main();
    create_scr_check();
    create_scr_login();
    lv_scr_load(scr_splash);

    // Inicializar WiFi
    set_estado_splash_format("Conectando a %s...", ssid);
    WiFi.begin(ssid, psk);

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

    if (NVS.getInt("PUNTO_CONTROL") == 2) {
        cookieCPP_authToken = NVS.getString("CPP-authToken");
        cookieCPP_puntToken = NVS.getString("CPP-puntToken");
    } else {
        NVS.eraseAll();
        NVS.setInt("PUNTO_CONTROL", 2);
        NVS.setString("CPP-authToken", "");
        NVS.setString("CPP-puntToken", "");
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

    // Inicializar pantalla táctil
    static lv_indev_drv_t indev_drv;
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = espi_touch_read;
    lv_indev_drv_register(&indev_drv);

    /*uint16_t calData[5];

    tft.calibrateTouch(calData, TFT_MAGENTA, TFT_BLACK, 15);

    tft.setTouch(calData);*/
}

static void ta_login_form_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *ta = lv_event_get_target(e);
    lv_obj_t *kb = (lv_obj_t *) lv_event_get_user_data(e);

    if (code == LV_EVENT_VALUE_CHANGED) {
        if (strlen(lv_textarea_get_text(txt_usuario_login)) >= 8 && strlen(lv_textarea_get_text(txt_password_login))) {
            lv_obj_clear_state(btn_login, LV_STATE_DISABLED);
        } else {
            lv_obj_add_state(btn_login, LV_STATE_DISABLED);
        }
    } else if (code == LV_EVENT_FOCUSED) {
        if (lv_indev_get_type(lv_indev_get_act()) != LV_INDEV_TYPE_KEYPAD) {
            lv_keyboard_set_textarea(kb, ta);
            lv_obj_set_style_max_height(kb, LV_VER_RES * 2 / 3, 0);
            lv_obj_update_layout(pnl_login);
            lv_obj_set_height(pnl_login, LV_VER_RES - lv_obj_get_height(kb));
            lv_obj_clear_flag(kb, LV_OBJ_FLAG_HIDDEN);
            lv_obj_scroll_to_view_recursive(ta, LV_ANIM_OFF);
        }
    } else if (code == LV_EVENT_DEFOCUSED) {
        lv_keyboard_set_textarea(kb, NULL);
        lv_obj_set_height(pnl_login, LV_VER_RES);
        lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
    } else if (code == LV_EVENT_READY || code == LV_EVENT_CANCEL) {
        lv_obj_set_height(pnl_login, LV_VER_RES);
        lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_state(e->target, LV_STATE_FOCUSED);
        lv_indev_reset(NULL, e->target);
    }
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
    lv_obj_t *spinner = lv_spinner_create(scr_splash, 1000, 45);
    lv_obj_set_size(spinner, 50, 50);
    lv_obj_align(spinner, LV_ALIGN_BOTTOM_RIGHT, -10, -10);

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
    lv_obj_t *lbl_version = lv_label_create(scr_splash);
    lv_obj_set_style_text_font(lbl_version, &mulish_16, LV_PART_MAIN);
    lv_obj_set_style_text_align(lbl_version, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    lv_label_set_text(lbl_version, PUNTO_CONTROL_VERSION);
    lv_obj_align(lbl_version, LV_ALIGN_BOTTOM_LEFT, 10, -10);
}

void create_scr_main() {
    // CREAR PANTALLA PRINCIPAL
    scr_main = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scr_main, LV_COLOR_MAKE(255, 255, 255), LV_PART_MAIN);

    // Etiqueta con el nombre del punto de acceso en la parte superior
    lbl_nombre_main = lv_label_create(scr_main);
    lv_obj_set_style_text_font(lbl_nombre_main, &mulish_16, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_nombre_main, lv_palette_main(LV_PALETTE_DEEP_ORANGE), LV_PART_MAIN);
    lv_obj_set_style_text_align(lbl_nombre_main, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

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
    lv_obj_t *kb = lv_keyboard_create(scr_login);
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
    lv_label_set_text(lbl_titulo_login, "Activar punto de acceso WiFi");
    lv_obj_add_style(lbl_titulo_login, &style_title, LV_PART_MAIN);

    // Campo de usuario IdEA
    lv_obj_t *lbl_usuario_login = lv_label_create(pnl_login);
    lv_label_set_text(lbl_usuario_login, "Usuario IdEA");
    lv_obj_add_style(lbl_usuario_login, &style_text_muted, LV_PART_MAIN);

    txt_usuario_login = lv_textarea_create(pnl_login);
    lv_textarea_set_one_line(txt_usuario_login, true);
    lv_textarea_set_placeholder_text(txt_usuario_login, "Personal directivo/administrativo");
    lv_obj_add_event_cb(txt_usuario_login, ta_login_form_event_cb, LV_EVENT_ALL, kb);

    // Campo de contraseña
    lv_obj_t *lbl_password_login = lv_label_create(pnl_login);
    lv_label_set_text(lbl_password_login, "Contraseña");
    lv_obj_add_style(lbl_password_login, &style_text_muted, LV_PART_MAIN);

    txt_password_login = lv_textarea_create(pnl_login);
    lv_textarea_set_one_line(txt_password_login, true);
    lv_textarea_set_password_mode(txt_password_login, true);
    lv_textarea_set_placeholder_text(txt_password_login, "No se almacenará");
    lv_obj_add_event_cb(txt_password_login, ta_login_form_event_cb, LV_EVENT_ALL, kb);

    // Botón de inicio de sesión
    btn_login = lv_btn_create(pnl_login);
    lv_obj_add_state(btn_login, LV_STATE_DISABLED);
    lv_obj_set_height(btn_login, LV_SIZE_CONTENT);
    //lv_obj_add_event_cb(btn_login, ta_login_submit_event_cb, LV_EVENT_CLICKED);


    lv_obj_t *lbl_login = lv_label_create(btn_login);
    lv_label_set_text(lbl_login, "Iniciar sesión");
    lv_obj_align(lbl_login, LV_ALIGN_TOP_MID, 0, 0);

    // Colocar elementos en una rejilla (7 filas, 2 columnas)
    static lv_coord_t grid_login_col_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(3), LV_GRID_TEMPLATE_LAST};
    static lv_coord_t grid_login_row_dsc[] = {
            LV_GRID_CONTENT,  /* Título */
            5,                /* Separador */
            LV_GRID_CONTENT,  /* Usuario */
            5,                /* Separador */
            LV_GRID_CONTENT,  /* Contraseña */
            5,                /* Separador */
            LV_GRID_CONTENT,  /* Botón */
            LV_GRID_TEMPLATE_LAST
    };

    lv_obj_set_grid_dsc_array(pnl_login, grid_login_col_dsc, grid_login_row_dsc);

    lv_obj_set_grid_cell(lbl_titulo_login, LV_GRID_ALIGN_START, 0, 2, LV_GRID_ALIGN_CENTER, 0, 1);
    lv_obj_set_grid_cell(lbl_usuario_login, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, 2, 1);
    lv_obj_set_grid_cell(txt_usuario_login, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_START, 2, 1);
    lv_obj_set_grid_cell(lbl_password_login, LV_GRID_ALIGN_START, 0, 1, LV_GRID_ALIGN_CENTER, 4, 1);
    lv_obj_set_grid_cell(txt_password_login, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_START, 4, 1);
    lv_obj_set_grid_cell(btn_login, LV_GRID_ALIGN_STRETCH, 0, 2, LV_GRID_ALIGN_START, 6, 1);

    // Quitar borde al panel y pegarlo a la parte superior de la pantalla
    lv_obj_set_style_border_width(pnl_login, 0, LV_PART_MAIN);
    lv_obj_align(pnl_login, LV_ALIGN_TOP_MID, 0, 0);
}

void loop() {
    lv_task_handler();
    delay(5);
}
