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

#include <lvgl.h>
#include "punto_control.h"
#include "scr_splash.h"

static lv_obj_t *scr_splash;     // Pantalla de inicio (splash screen)
static lv_obj_t *spinner_splash;
static lv_obj_t *lbl_ip_splash;
static lv_obj_t *lbl_estado_splash;
static lv_obj_t *lbl_version_splash;
static lv_obj_t *btn_config_splash;

lv_obj_t *img;

void create_scr_splash() {
    // CREAR PANTALLA DE ARRANQUE (splash screen)
    scr_splash = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scr_splash, LV_COLOR_MAKE(255, 255, 255), LV_PART_MAIN);

    // Logo de la CED centrado
    LV_IMG_DECLARE(logo_ced)
    img = lv_img_create(scr_splash);
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
    lv_obj_set_style_text_color(lbl_version_splash, lv_palette_main(LV_PALETTE_BLUE_GREY), LV_PART_MAIN);
    lv_label_set_text(lbl_version_splash, PUNTO_CONTROL_VERSION);
    lv_obj_align(lbl_version_splash, LV_ALIGN_BOTTOM_LEFT, 10, -10);

    btn_config_splash = create_config_button(scr_splash);
    lv_obj_add_flag(btn_config_splash, LV_OBJ_FLAG_HIDDEN);
}

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

void hide_splash_spinner() {
    lv_obj_add_flag(spinner_splash, LV_OBJ_FLAG_HIDDEN);
}

void show_splash_spinner() {
    lv_obj_clear_flag(spinner_splash, LV_OBJ_FLAG_HIDDEN);
}

void hide_splash_version() {
    lv_obj_add_flag(lbl_version_splash, LV_OBJ_FLAG_HIDDEN);
}

void show_splash_version() {
    lv_obj_clear_flag(lbl_version_splash, LV_OBJ_FLAG_HIDDEN);
}

void hide_splash_config() {
    lv_obj_add_flag(btn_config_splash, LV_OBJ_FLAG_HIDDEN);
}

void show_splash_config() {
    lv_obj_clear_flag(btn_config_splash, LV_OBJ_FLAG_HIDDEN);
}

void load_scr_splash() {
    lv_scr_load(scr_splash);
}

int is_loaded_scr_splash() {
    return lv_scr_act() == scr_splash;
}

void clean_scr_splash() {
    lv_obj_clean(scr_splash);
}

void change_logo_splash() {
    LV_IMG_DECLARE(logo_seneca)
    lv_img_set_src(img, &logo_seneca);
    lv_obj_align(img, LV_ALIGN_CENTER, 0, 0);
}