#include <Arduino.h>

#include <punto_control.h>

#include <lvgl.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <lwip/apps/sntp.h>
#include <HTTPClient.h>
#include <MFRC522.h>
#include <TinyXML.h>

#include <ArduinoNvs.h>

#define SCREEN_WIDTH 480
#define SCREEN_HEIGHT 320

const char* ssid = "\x41\x6E\x64\x61\x72\x65\x64";
const char* password =  "6b629f4c299371737494c61b5a101693a2d4e9e1f3e1320f3ebf9ae379cecf32";

TFT_eSPI tft = TFT_eSPI();
MFRC522 mfrc522(RFID_SDA_PIN, RFID_RST_PIN);

ArduinoNvs nvs;

TinyXML xml;
uint8_t xmlBuffer[2048];

HTTPClient client;

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

// Pantallas
static lv_obj_t *scr_splash;     // Pantalla de inicio (splash screen)
static lv_obj_t *lbl_ip_splash;
static lv_obj_t *lbl_estado_splash;

static lv_obj_t *scr_main;       // Pantalla principal de lectura de código
static lv_obj_t *lbl_hora_main;
static lv_obj_t *lbl_fecha_main;
static lv_obj_t *lbl_estado_main;
static lv_obj_t *lbl_icon_main;

static lv_obj_t *scr_check;      // Pantalla de comprobación
static lv_obj_t *lbl_estado_check;
static lv_obj_t *lbl_icon_check;

static lv_obj_t *scr_config;     // Pantalla de configuración

void create_scr_splash();

void create_scr_main();

void create_scr_check();

void initialize_gui();

void task_main(lv_timer_t *);

void initialize_flash();

void set_icon_text(lv_obj_t *pObj, const char *string, lv_color_palette_t param, int bottom);

void set_estado_splash_format(const char *string, const char *p) {
    lv_label_set_text_fmt(lbl_estado_splash, string, p);
    lv_obj_align(lbl_estado_splash, nullptr, LV_ALIGN_IN_TOP_MID, 0, 15);
}

void set_estado_splash(const char *string) {
    lv_label_set_text(lbl_estado_splash, string);
    lv_obj_align(lbl_estado_splash, nullptr, LV_ALIGN_IN_TOP_MID, 0, 15);
}

void set_ip_splash_format(const char *string, const char *p) {
    lv_label_set_text_fmt(lbl_ip_splash, string, p);
    lv_obj_align(lbl_ip_splash, nullptr, LV_ALIGN_IN_BOTTOM_MID, 0, -15);
}

void set_ip_splash(const char *string) {
    lv_label_set_text(lbl_ip_splash, string);
    lv_obj_align(lbl_ip_splash, nullptr, LV_ALIGN_IN_BOTTOM_MID, 0, -15);
}

void set_hora_main_format(const char *string, const char *p) {
    lv_label_set_text_fmt(lbl_hora_main, string, p);
    lv_obj_align(lbl_hora_main, nullptr, LV_ALIGN_IN_TOP_MID, 0, 15);
}

void set_hora_main(const char *string) {
    lv_label_set_text(lbl_hora_main, string);
    lv_obj_align(lbl_hora_main, nullptr, LV_ALIGN_IN_TOP_MID, 0, 15);
}

void set_fecha_main_format(const char *string, const char *p) {
    lv_label_set_text_fmt(lbl_fecha_main, string, p);
    lv_obj_align(lbl_fecha_main, lbl_hora_main, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);
}

void set_fecha_main(const char *string) {
    lv_label_set_text(lbl_fecha_main, string);
    lv_obj_align(lbl_fecha_main, lbl_hora_main, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);
}

void set_estado_main_format(const char *string, const char *p) {
    lv_label_set_text_fmt(lbl_estado_main, string, p);
    lv_obj_align(lbl_estado_main, lbl_fecha_main, LV_ALIGN_OUT_BOTTOM_MID, 0, 20);
}

void set_estado_main(const char *string) {
    lv_label_set_text(lbl_estado_main, string);
    lv_obj_align(lbl_estado_main, lbl_fecha_main, LV_ALIGN_OUT_BOTTOM_MID, 0, 20);
}

void set_estado_check_format(const char *string, const char *p) {
    lv_label_set_text_fmt(lbl_estado_check, string, p);
    lv_obj_align(lbl_estado_check, lbl_icon_check, LV_ALIGN_OUT_BOTTOM_MID, 0, 25);
}

void set_estado_check(const char *string) {
    lv_label_set_text(lbl_estado_check, string);
    lv_obj_align(lbl_estado_check, lbl_icon_check, LV_ALIGN_OUT_BOTTOM_MID, 0, 25);
}

void task_wifi_connection(lv_timer_t *timer) {
    static enum {
        CONNECTING, NTP_START, NTP_WAIT, CHECKING, CHECK_WAIT, CHECK_RETRY, WAITING, DONE
    } state = CONNECTING;
    static int cuenta = 0;
    static HTTPClient client;
    static int code;
    IPAddress ip;

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
            state = CHECK_WAIT;
            break;
        case CHECK_WAIT:
            client.begin(PUNTO_CONTROL_URL);
            client.setReuse(false);
            code = client.GET();
            if (code != 200) {
                set_estado_splash("Error de acceso. Reintentando");
                state = CHECK_RETRY;
            } else {
                set_estado_splash("Acceso a Séneca confirmado");
                state = WAITING;
            }
            client.end();
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

String response;

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

int send_seneca_request(String url, String body, String cookie) {
    int ok = 0;

    // preparar petición
    client.begin(url);

    // añadir cabeceras
    client.addHeader("Cookie", NVS.getString("cookie") + "; " + cookie);
    client.addHeader("Content-Type", "application/x-www-form-urlencoded");
    client.addHeader("Accept", "text/html");
    client.addHeader("Accept-Encoding", "identity");

    // hacer POST
    int result = client.POST(body);
    if (result == 200) {
        // petición exitosa, extraer mensaje con la respuesta
        int index, index2;
        response = client.getString();

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
                    set_icon_text(lbl_icon_check, "\uF2F6", LV_COLOR_PALETTE_GREEN, 0);
                } else if (response.startsWith("Salida")) {
                    set_icon_text(lbl_icon_check, "\uF2F5", LV_COLOR_PALETTE_RED, 0);
                } else {
                    set_icon_text(lbl_icon_check, "\uF071", LV_COLOR_PALETTE_ORANGE, 0);
                }
                set_estado_check(response.c_str());

                ok = 1; // éxito
            }
        } else {
            set_icon_text(lbl_icon_check, "\uF071", LV_COLOR_PALETTE_ORANGE, 0);
            set_estado_check("Se ha obtenido una respuesta desconocida desde Séneca.");
        }
    } else {
        set_icon_text(lbl_icon_check, "\uF071", LV_COLOR_PALETTE_ORANGE, 0);
        set_estado_check("Ha ocurrido un error en la comunicación con Séneca.");
    }

    client.end();

    return ok; // 0 = error
}

// Estado del parser XML
String modo, punto, xCentro, cookie;

int xmlStatus;
String xml_current_input;

void xml_cookie_callback(uint8_t statusflags, char *tagName, uint16_t tagNameLen, char *data, uint16_t dataLen) {
    // nos quedamos con las etiquetas <input>
    if ((statusflags & STATUS_START_TAG) && strstr(tagName, "input")) {
        xmlStatus = 1;
    }
    if ((statusflags & STATUS_END_TAG) && strstr(tagName, "input")) {
        xmlStatus = 0;
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
}

void process_token(String uid) {
    // obtener cookie de sesión
    const char *headerKeys[] = {"Set-Cookie"};
    size_t headerKeysSize = sizeof(headerKeys) / sizeof(char *);

    // obtener página web
    client.begin(PUNTO_CONTROL_URL);
    client.addHeader("Cookie", NVS.getString("cookie"));
    client.addHeader("Accept", "text/html");
    client.addHeader("Accept-Encoding", "identity");

    client.collectHeaders(headerKeys, headerKeysSize);

    int resultCode = client.GET();

    // si se ha recibido con éxito
    if (resultCode == 200) {
        // almacenar cookie de sesión
        cookie = client.header("Set-Cookie");
        // nos quedamos con la última cookie de la cabecera, que es la de JSESSION
        cookie = cookie.substring(0, cookie.indexOf(";"));
        response = client.getString();

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

        // finalizar petición HTTP
        client.end();

        // enviar token a Séneca
        String params =
                "_MODO_=" + modo + "&PUNTO=" + punto + "&X_CENTRO=" + xCentro + "&C_TIPACCCONTPRE=&DARK_MODE=N&TOKEN=" +
                uid;

        send_seneca_request(PUNTO_CONTROL_URL, params, cookie);
    } else {
        client.end();
    }
}

void task_main(lv_timer_t *timer) {
    const char *dia_semana[] = {"Domingo", "Lunes", "Martes", "Miércoles", "Jueves", "Viernes", "Sábado"};
    static enum {
        DONE, IDLE, CARD_PRESENT, CHECK_RESULT_WAIT, NO_NETWORK
    } state = IDLE;
    static int cuenta = 0;
    static HTTPClient client;
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

                        set_icon_text(lbl_icon_check, "\uF252", LV_COLOR_PALETTE_TEAL, 0);
                        set_estado_check("Llavero detectado. Comunicando con Séneca, espere por favor...");
                        lv_scr_load(scr_check);
                        state = CARD_PRESENT;
                        cuenta = 0;
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
            state = CHECK_RESULT_WAIT;
            break;
        case CHECK_RESULT_WAIT:
            cuenta++;
            if (cuenta > 40 || mfrc522.PICC_IsNewCardPresent()) {
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

    // Crear pantalla de arranque y cambiar a ella
    create_scr_splash();
    create_scr_main();
    create_scr_check();
    lv_scr_load(scr_splash);

    // Inicializar WiFi
    set_estado_splash_format("Conectando a %s...", ssid);
    WiFi.begin(ssid, password);

    lv_timer_create(task_wifi_connection, 100, nullptr);
}

void initialize_flash() {
    NVS.begin();

    if (NVS.getString("cookie") == nullptr) {
        //NVS.setString("cookie", "...");
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
}

void create_scr_splash() {
    // CREAR PANTALLA DE ARRANQUE (splash screen)
    scr_splash = lv_obj_create(nullptr, nullptr);
    lv_obj_set_style_bg_color(scr_splash, 0, LV_STATE_DEFAULT, LV_COLOR_MAKE(255, 255, 255));

    // Logo de la CED centrado
    LV_IMG_DECLARE(logo_ced);
    lv_obj_t *img = lv_img_create(scr_splash, nullptr);
    lv_img_set_src(img, &logo_ced);
    lv_obj_align(img, nullptr, LV_ALIGN_CENTER, 0, 0);

    // Spinner para indicar operación en progreso en la esquina inferior derecha
    lv_obj_t *spinner = lv_spinner_create(scr_splash, 1000, 45);
    lv_obj_set_size(spinner, 50, 50);
    lv_obj_align(spinner, nullptr, LV_ALIGN_IN_BOTTOM_RIGHT, -10, -10);

    // Etiqueta de estado actual centrada en la parte superior
    lbl_estado_splash = lv_label_create(scr_splash, nullptr);
    lv_obj_set_style_text_font(lbl_estado_splash, LV_PART_MAIN, LV_STATE_DEFAULT, &mulish_24);
    lv_obj_set_style_text_align(lbl_estado_splash, LV_PART_MAIN, LV_STATE_DEFAULT, LV_TEXT_ALIGN_CENTER);
    set_estado_splash("Inicializando...");

    // Etiqueta de dirección IP actual centrada en la parte inferior
    lbl_ip_splash = lv_label_create(scr_splash, nullptr);
    lv_obj_set_style_text_font(lbl_ip_splash, LV_PART_MAIN, LV_STATE_DEFAULT, &mulish_16);
    lv_obj_set_style_text_align(lbl_ip_splash, LV_PART_MAIN, LV_STATE_DEFAULT, LV_TEXT_ALIGN_CENTER);
    set_ip_splash("Esperando configuración");

    // Etiqueta de versión del software en la esquina inferior izquierda
    lv_obj_t *lbl_version = lv_label_create(scr_splash, nullptr);
    lv_obj_set_style_text_font(lbl_version, LV_PART_MAIN, LV_STATE_DEFAULT, &mulish_16);
    lv_obj_set_style_text_align(lbl_version, LV_PART_MAIN, LV_STATE_DEFAULT, LV_TEXT_ALIGN_LEFT);
    lv_label_set_text(lbl_version, PUNTO_CONTROL_VERSION);
    lv_obj_align(lbl_version, nullptr, LV_ALIGN_IN_BOTTOM_LEFT, 10, -10);
}

void create_scr_main() {
    // CREAR PANTALLA PRINCIPAL
    scr_main = lv_obj_create(nullptr, nullptr);
    lv_obj_set_style_bg_color(scr_main, 0, LV_STATE_DEFAULT, LV_COLOR_MAKE(255, 255, 255));

    // Etiqueta de hora y fecha actual centrada en la parte superior
    lbl_hora_main = lv_label_create(scr_main, nullptr);
    lv_obj_set_style_text_font(lbl_hora_main, LV_PART_MAIN, LV_STATE_DEFAULT, &mulish_64_numbers);
    lv_obj_set_style_text_align(lbl_hora_main, LV_PART_MAIN, LV_STATE_DEFAULT, LV_TEXT_ALIGN_CENTER);
    lbl_fecha_main = lv_label_create(scr_main, nullptr);
    lv_obj_set_style_text_font(lbl_fecha_main, LV_PART_MAIN, LV_STATE_DEFAULT, &mulish_24);
    lv_obj_set_style_text_color(lbl_fecha_main, LV_PART_MAIN, LV_STATE_DEFAULT,
                                lv_color_get_palette_main(LV_COLOR_PALETTE_GREY));
    lv_obj_set_style_text_align(lbl_fecha_main, LV_PART_MAIN, LV_STATE_DEFAULT, LV_TEXT_ALIGN_CENTER);
    set_hora_main("");
    set_fecha_main("");

    // Etiqueta de estado centrada en pantalla
    lbl_estado_main = lv_label_create(scr_main, nullptr);
    lv_obj_set_style_text_font(lbl_estado_main, LV_PART_MAIN, LV_STATE_DEFAULT, &mulish_32);
    lv_obj_set_style_text_color(lbl_estado_main, LV_PART_MAIN, LV_STATE_DEFAULT,
                                lv_color_get_palette_main(LV_COLOR_PALETTE_BLUE_GREY));
    lv_obj_set_style_text_align(lbl_estado_main, LV_PART_MAIN, LV_STATE_DEFAULT, LV_TEXT_ALIGN_CENTER);
    set_estado_main("");

    // Imagen de la flecha en la parte inferior
    lbl_icon_main = lv_label_create(scr_main, nullptr);
    lv_obj_set_style_text_font(lbl_icon_main, LV_PART_MAIN, LV_STATE_DEFAULT, &symbols);
    set_icon_text(lbl_icon_main, "\uF063", LV_COLOR_PALETTE_BLUE, 1);
}

void create_scr_check() {
    // CREAR PANTALLA DE COMPROBACIÓN
    scr_check = lv_obj_create(nullptr, nullptr);
    lv_obj_set_style_bg_color(scr_check, 0, LV_STATE_DEFAULT, LV_COLOR_MAKE(255, 255, 255));

    // Icono del estado de comprobación
    lbl_icon_check = lv_label_create(scr_check, nullptr);
    lv_obj_set_style_text_font(lbl_icon_check, LV_PART_MAIN, LV_STATE_DEFAULT, &symbols);
    set_icon_text(lbl_icon_check, "\uF252", LV_COLOR_PALETTE_BLUE, 0);

    // Etiqueta de estado de comprobación
    lbl_estado_check = lv_label_create(scr_check, nullptr);
    lv_obj_set_style_text_font(lbl_estado_check, LV_PART_MAIN, LV_STATE_DEFAULT, &mulish_32);
    lv_obj_set_style_text_color(lbl_estado_check, LV_PART_MAIN, LV_STATE_DEFAULT,
                                lv_color_get_palette_main(LV_COLOR_PALETTE_BLUE_GREY));
    lv_obj_set_style_text_align(lbl_estado_check, LV_PART_MAIN, LV_STATE_DEFAULT, LV_TEXT_ALIGN_CENTER);
    lv_label_set_long_mode(lbl_estado_check, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(lbl_estado_check, SCREEN_WIDTH * 9 / 10);
    set_estado_check("");
}

void set_icon_text(lv_obj_t *label, const char *text, lv_color_palette_t color, int bottom) {
    lv_obj_set_style_text_color(label, LV_PART_MAIN, LV_STATE_DEFAULT, lv_color_get_palette_main(color));
    lv_label_set_text(label, text);
    lv_obj_set_style_text_align(label, LV_PART_MAIN, LV_STATE_DEFAULT, LV_TEXT_ALIGN_CENTER);
    lv_obj_align(label, nullptr, bottom ? LV_ALIGN_IN_BOTTOM_MID : LV_ALIGN_IN_TOP_MID, 0, bottom ? -15 : 15);
}

void loop() {
    lv_task_handler();
    delay(5);
}