#include <Arduino.h>

#include <punto_control.h>

#include <lvgl.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include <lwip/apps/sntp.h>
#include <HTTPClient.h>

#define SCREEN_WIDTH 480
#define SCREEN_HEIGHT 320

const char* ssid = "\x41\x6E\x64\x61\x72\x65\x64";
const char* password =  "6b629f4c299371737494c61b5a101693a2d4e9e1f3e1320f3ebf9ae379cecf32";

TFT_eSPI tft = TFT_eSPI();

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
static lv_obj_t * scr_config;     // Pantalla de configuración

void create_scr_splash();

void initialize_gui();

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

void task_wifi_connection(lv_timer_t * timer) {
    static enum { CONNECTING, NTP, CHECKING, CHECK_RETRY, WAITING, DONE } state = CONNECTING;
    static int cuenta = 0;
    static HTTPClient client;
    int code;

    switch (state) {
        case DONE:
            lv_timer_del(timer);
            return;
        case CONNECTING:
            if (WiFi.status() == WL_CONNECTED) {
                state = NTP;
                set_ip_splash_format("IP: %s", WiFi.localIP().toString().c_str());
                set_estado_splash("Ajustando hora...");
                setenv("TZ", PUNTO_CONTROL_HUSO_HORARIO, 1);
                tzset();
                sntp_setoperatingmode(SNTP_OPMODE_POLL);
                sntp_setservername(0, "pool.ntp.org");
                sntp_init();
            } else {
                cuenta++;
            }
            break;
        case NTP:
            if (sntp_get_sync_status() == SNTP_SYNC_STATUS_COMPLETED) {
                state = DONE;
            }
            state = CHECKING;
            cuenta = 0;
            break;
        case CHECKING:
            set_estado_splash("Comprobando acceso a Séneca...");
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
            if (cuenta == 20) {
                state = CHECKING;
            }
            break;
        case WAITING:
            cuenta++;
            if (cuenta == 20) {
                state = DONE;
            }
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

    // Inicializar GUI
    initialize_gui();

    // Crear pantalla de arranque y cambiar a ella
    create_scr_splash();
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
    lv_obj_set_style_text_font(lbl_estado_splash, LV_PART_MAIN, LV_STATE_DEFAULT, &lv_font_montserrat_24);
    lv_obj_set_style_text_align(lbl_estado_splash, LV_PART_MAIN, LV_STATE_DEFAULT, LV_TEXT_ALIGN_CENTER);
    set_estado_splash("Inicializando...");

    // Etiqueta de dirección IP actual centrada en la parte inferior
    lbl_ip_splash = lv_label_create(scr_splash, nullptr);
    lv_obj_set_style_text_font(lbl_ip_splash, LV_PART_MAIN, LV_STATE_DEFAULT, &lv_font_montserrat_18);
    lv_obj_set_style_text_align(lbl_ip_splash, LV_PART_MAIN, LV_STATE_DEFAULT, LV_TEXT_ALIGN_CENTER);
    set_ip_splash("Esperando configuración");

    // Etiqueta de versión del software en la esquina inferior izquierda
    lv_obj_t * lbl_version = lv_label_create(scr_splash, nullptr);
    lv_obj_set_style_text_font(lbl_version, LV_PART_MAIN, LV_STATE_DEFAULT, &lv_font_montserrat_14);
    lv_obj_set_style_text_align(lbl_version, LV_PART_MAIN, LV_STATE_DEFAULT, LV_TEXT_ALIGN_LEFT);
    lv_label_set_text(lbl_version, PUNTO_CONTROL_VERSION);
    lv_obj_align(lbl_version, nullptr, LV_ALIGN_IN_BOTTOM_LEFT, 10, -10);
}

void loop() {
    lv_task_handler();
    delay(5);
}