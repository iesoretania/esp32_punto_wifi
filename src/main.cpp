#include <Arduino.h>

#include <lvgl.h>
#include <SPI.h>
#include <TFT_eSPI.h>

#define screenWidth 480
#define screenHeight 320

TFT_eSPI tft = TFT_eSPI();
static lv_disp_draw_buf_t disp_buf;
static lv_color_t buf1[LV_HOR_RES_MAX * 20];
static lv_color_t buf2[LV_HOR_RES_MAX * 20];

/* Flushing en el display */
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
    uint32_t w = (area->x2 - area->x1 + 1);
    uint32_t h = (area->y2 - area->y1 + 1);

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);
    tft.pushColors(&color_p->full, w * h, true);    // importante que swap sea true en este display
    tft.endWrite();

    lv_disp_flush_ready(disp);
}

// Pantallas
lv_obj_t *scrSplash;     // Pantalla de inicio (splash screen)
lv_obj_t *scrMain;     // Pantalla principal de lectura de código
lv_obj_t *scrConfig;     // Pantalla de configuración

void setup() {

    // INICIALIZAR SUBSISTEMAS

    // Inicializar puerto serie (para mensajes de depuración)
    Serial.begin(115200);

    // Activar iluminación del display al 100%
    pinMode(BACKLIGHT_PIN, OUTPUT);
    ledcAttachPin(BACKLIGHT_PIN, 1);
    ledcWrite(1, 255);

    // Inicializar bus SPI y el TFT conectado a través de él
    SPI.begin();
    tft.begin();            /* Inicializar TFT */
    tft.setRotation(3);     /* Orientación horizontal */

    // Inicializar buffers del GUI
    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, LV_HOR_RES_MAX * 20);

    // Inicializar interfaz gráfica
    lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = 480;
    disp_drv.ver_res = 320;
    disp_drv.flush_cb = my_disp_flush;
    disp_drv.draw_buf = &disp_buf;
    lv_disp_drv_register(&disp_drv);

    lv_init();

    // CREAR PANTALLA DE ARRANQUE (splash screen)
    scrSplash = lv_obj_create(nullptr, nullptr);

    LV_IMG_DECLARE(logo_junta2);
    lv_obj_t * img = lv_img_create(scrSplash, nullptr);
    lv_img_set_src(img, &logo_junta2);
    lv_obj_align(img, nullptr, LV_ALIGN_CENTER, 0, 0);

    lv_scr_load(scrSplash);
}

void loop() {
    lv_task_handler();
    delay(5);
}