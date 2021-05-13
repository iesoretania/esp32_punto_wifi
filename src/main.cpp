#include <Arduino.h>

#include <punto_control.h>

#include <lvgl.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <lwip/apps/sntp.h>
#include <HTTPClient.h>
#include <MFRC522.h>

#define SCREEN_WIDTH 480
#define SCREEN_HEIGHT 320

const char* ssid = "\x41\x6E\x64\x61\x72\x65\x64";
const char* password =  "6b629f4c299371737494c61b5a101693a2d4e9e1f3e1320f3ebf9ae379cecf32";

TFT_eSPI tft = TFT_eSPI();
MFRC522 mfrc522(RFID_SDA_PIN, RFID_RST_PIN);

/* Flushing en el display */
void espi_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors(&color_p->full, w * h, false);    // importante que swap sea true en este display
    tft.endWrite();

    lv_disp_flush_ready(disp);
}

// Pantallas
static lv_obj_t * scr_splash;     // Pantalla de inicio (splash screen)
static lv_obj_t * lbl_ip_splash;
static lv_obj_t * lbl_estado_splash;

static lv_obj_t * scr_main;       // Pantalla principal de lectura de código
static lv_obj_t * lbl_hora_main;
static lv_obj_t * lbl_fecha_main;
static lv_obj_t * lbl_estado_main;

static lv_obj_t * scr_config;     // Pantalla de configuración

void create_scr_splash();

void create_scr_main();

void initialize_gui();

void task_main(lv_timer_t *);

void set_estado_splash_format(const char * string, const char * p) {
    lv_label_set_text_fmt(lbl_estado_splash, string, p);
    lv_obj_align(lbl_estado_splash, nullptr, LV_ALIGN_IN_TOP_MID, 0, 15);
}

void set_estado_splash(const char * string) {
    lv_label_set_text(lbl_estado_splash, string);
    lv_obj_align(lbl_estado_splash, nullptr, LV_ALIGN_IN_TOP_MID, 0, 15);
}

void set_ip_splash_format(const char * string, const char * p) {
    lv_label_set_text_fmt(lbl_ip_splash, string, p);
    lv_obj_align(lbl_ip_splash, nullptr, LV_ALIGN_IN_BOTTOM_MID, 0, -15);
}

void set_ip_splash(const char * string) {
    lv_label_set_text(lbl_ip_splash, string);
    lv_obj_align(lbl_ip_splash, nullptr, LV_ALIGN_IN_BOTTOM_MID, 0, -15);
}

void set_hora_main_format(const char * string, const char * p) {
    lv_label_set_text_fmt(lbl_hora_main, string, p);
    lv_obj_align(lbl_hora_main, nullptr, LV_ALIGN_IN_TOP_MID, 0, 15);
}

void set_hora_main(const char * string) {
    lv_label_set_text(lbl_hora_main, string);
    lv_obj_align(lbl_hora_main, nullptr, LV_ALIGN_IN_TOP_MID, 0, 15);
}

void set_fecha_main_format(const char * string, const char * p) {
    lv_label_set_text_fmt(lbl_fecha_main, string, p);
    lv_obj_align(lbl_fecha_main, lbl_hora_main, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);
}

void set_fecha_main(const char * string) {
    lv_label_set_text(lbl_fecha_main, string);
    lv_obj_align(lbl_fecha_main, lbl_hora_main, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);
}

void set_estado_main_format(const char * string, const char * p) {
    lv_label_set_text_fmt(lbl_estado_main, string, p);
    lv_obj_align(lbl_estado_main, lbl_fecha_main, LV_ALIGN_OUT_BOTTOM_MID, 0, 20);
}

void set_estado_main(const char * string) {
    lv_label_set_text(lbl_estado_main, string);
    lv_obj_align(lbl_estado_main, lbl_fecha_main, LV_ALIGN_OUT_BOTTOM_MID, 0, 20);
}

void task_wifi_connection(lv_timer_t * timer) {
    static enum { CONNECTING, NTP_START, NTP_WAIT, CHECKING, CHECK_WAIT, CHECK_RETRY, WAITING, DONE } state = CONNECTING;
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

void task_main(lv_timer_t * timer) {
    const char *dia_semana[] = { "Domingo", "Lunes", "Martes", "Miércoles", "Jueves", "Viernes", "Sábado" };
    static enum { DONE, IDLE, CARD_PRESENT, NO_NETWORK } state = IDLE;
    static int cuenta = 0;
    static HTTPClient client;

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
                    state = CARD_PRESENT;
                    break;
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
                        set_estado_main("CODIGO QR AQUI");
                }
            }
            break;
        case CARD_PRESENT:
            if (mfrc522.PICC_ReadCardSerial()) {
                uint32_t uid = (uint32_t) (
                        mfrc522.uid.uidByte[0] +
                        (mfrc522.uid.uidByte[1] << 8) +
                        (mfrc522.uid.uidByte[2] << 16) +
                        (mfrc522.uid.uidByte[3] << 24));

                String uidS = (String) uid;

                while (uidS.length() < 10) {
                    uidS = "0" + uidS;
                }
            }
            mfrc522.PICC_HaltA();
            state = IDLE;
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

    // Inicializar GUI
    initialize_gui();

    // Crear pantalla de arranque y cambiar a ella
    create_scr_splash();
    create_scr_main();
    lv_scr_load(scr_splash);

    // Inicializar WiFi
    set_estado_splash_format("Conectando a %s...", ssid);
    WiFi.begin(ssid, password);

    lv_timer_create(task_wifi_connection, 100, nullptr);
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
    lv_obj_t * img = lv_img_create(scr_splash, nullptr);
    lv_img_set_src(img, &logo_ced);
    lv_obj_align(img, nullptr, LV_ALIGN_CENTER, 0, 0);

    // Spinner para indicar operación en progreso en la esquina inferior derecha
    lv_obj_t * spinner = lv_spinner_create(scr_splash, 1000, 45);
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
    lv_obj_t * lbl_version = lv_label_create(scr_splash, nullptr);
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
    lv_obj_set_style_text_color(lbl_fecha_main, LV_PART_MAIN, LV_STATE_DEFAULT, lv_color_get_palette_main(LV_COLOR_PALETTE_GREY));
    lv_obj_set_style_text_align(lbl_fecha_main, LV_PART_MAIN, LV_STATE_DEFAULT, LV_TEXT_ALIGN_CENTER);
    set_hora_main("");
    set_fecha_main("");

    // Etiqueta de estado centrada en pantalla
    lbl_estado_main = lv_label_create(scr_main, nullptr);
    lv_obj_set_style_text_font(lbl_estado_main, LV_PART_MAIN, LV_STATE_DEFAULT, &mulish_32);
    lv_obj_set_style_text_color(lbl_estado_main, LV_PART_MAIN, LV_STATE_DEFAULT, lv_color_get_palette_main(LV_COLOR_PALETTE_BLUE_GREY));
    lv_obj_set_style_text_align(lbl_estado_main, LV_PART_MAIN, LV_STATE_DEFAULT, LV_TEXT_ALIGN_CENTER);
    set_estado_main("");

    // Imagen del llavero centrado
    LV_IMG_DECLARE(llavero);
    lv_obj_t * img = lv_img_create(scr_main, nullptr);
    lv_img_set_src(img, &llavero);
    lv_obj_align(img, nullptr, LV_ALIGN_IN_BOTTOM_MID, 0, -15);
}

void loop() {
    lv_task_handler();
    delay(5);
}