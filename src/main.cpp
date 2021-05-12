#include <Arduino.h>

#include <lvgl.h>
#include <SPI.h>
#include <TFT_eSPI.h>

#define SCREEN_WIDTH 480
#define SCREEN_HEIGHT 320

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
static lv_obj_t *scrSplash;     // Pantalla de inicio (splash screen)
static lv_obj_t *scrMain;       // Pantalla principal de lectura de código
static lv_obj_t *scrConfig;     // Pantalla de configuración

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

    // CREAR PANTALLA DE ARRANQUE (splash screen)
    scrSplash = lv_obj_create(nullptr, nullptr);
    static lv_style_t styleFondoBlanco;
    lv_obj_set_style_bg_color(scrSplash, 0, LV_STATE_DEFAULT, LV_COLOR_MAKE(255, 255, 255));

    LV_IMG_DECLARE(logo_ced);
    lv_obj_t * img = lv_img_create(scrSplash, nullptr);
    lv_img_set_src(img, &logo_ced);
    lv_obj_align(img, nullptr, LV_ALIGN_CENTER, 0, 0);

    lv_scr_load(scrSplash);
}

void loop() {
    lv_task_handler();
    delay(5);
}