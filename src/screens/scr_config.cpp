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
#include <WiFi.h>
#include <lvgl.h>
#include <punto_control.h>
#include "flash.h"
#include "display.h"
#include "scr_config.h"
#include "scr_firmware.h"
#include "scr_shared.h"
#include "notify.h"

static lv_obj_t *scr_config;     // Pantalla de configuración
static lv_obj_t *txt_ssid_config;
static lv_obj_t *txt_psk_config;
static lv_obj_t *sld_brillo_config;
static lv_obj_t *sw_forzar_activacion_config;
static lv_obj_t *btn_firmware_config;
static lv_obj_t *lbl_firmware_config;
static lv_obj_t *lbl_read_code;

int configuring = 0;

static void btn_config_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *obj = lv_event_get_target(e);

    if (code == LV_EVENT_DRAW_PART_BEGIN) {
        lv_obj_draw_part_dsc_t *dsc = (lv_obj_draw_part_dsc_t *) lv_event_get_param(e);
        if (dsc->id == 1) {
            dsc->rect_dsc->radius = 0;
            if (lv_btnmatrix_get_selected_btn(obj) == dsc->id) {
                dsc->rect_dsc->bg_color = lv_palette_lighten(LV_PALETTE_RED, 4);
            } else {
                dsc->rect_dsc->bg_color = lv_palette_lighten(LV_PALETTE_RED, 2);
            }
        }
        if (dsc->id == 0) {
            dsc->rect_dsc->radius = 0;
            if (lv_btnmatrix_get_selected_btn(obj) == dsc->id) {
                dsc->rect_dsc->bg_color = lv_palette_lighten(LV_PALETTE_GREEN, 4);
            } else {
                dsc->rect_dsc->bg_color = lv_palette_lighten(LV_PALETTE_GREEN, 2);
            }
        }
    }
    if (code == LV_EVENT_VALUE_CHANGED) {
        uint32_t id = lv_btnmatrix_get_selected_btn(obj);
        if (id == 0) {
            int reboot = 0;
            const char *str_ssid = lv_textarea_get_text(txt_ssid_config);
            const char *str_psk = lv_textarea_get_text(txt_psk_config);
            if (!flash_get_string("net.wifi_ssid").equals(str_ssid) ||
            !flash_get_string("net.wifi_psk").equals(str_psk)
            ) {
                flash_set_string("net.wifi_ssid", lv_textarea_get_text(txt_ssid_config));
                flash_set_string("net.wifi_psk", lv_textarea_get_text(txt_psk_config));
                reboot = 1;
            }
            flash_set_int("scr.brightness", lv_slider_get_value(sld_brillo_config));
            flash_set_int("sec.force_code", lv_obj_has_state(sw_forzar_activacion_config, LV_STATE_CHECKED));
            flash_commit();
            if (reboot) {
                esp_restart();
            }
        } else {
            // deshacer cambios
            display_set_brightness(flash_get_int("scr.brightness"));
        }
        notify_button_press();
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

static void btn_reset_wifi_psk_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        lv_textarea_set_text(txt_psk_config, "");
    }
}

static void btn_recalibrate_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        flash_erase("CalData");
        flash_commit();
        esp_restart();
    }
}

static void sld_brightness_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t *slider = lv_event_get_target(e);
        display_set_brightness(lv_slider_get_value(slider));
    }
}

static void btn_punto_control_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        flash_erase("CPP-puntToken");
        flash_commit();
        esp_restart();
    }
}

static void btn_cambio_codigo_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        flash_erase("sec.code");
        flash_commit();
        esp_restart();
    }
}

static void btn_logout_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        flash_erase("CPP-authToken");
        flash_erase("CPP-puntToken");
        flash_commit();
        esp_restart();
    }
}

static void scroll_begin_event_cb(lv_event_t *e) {
    if (lv_event_get_code(e) == LV_EVENT_SCROLL_BEGIN) {
        lv_anim_t *a = (lv_anim_t *) lv_event_get_param(e);
        if (a) a->time = 0;
    }
}

static void btn_firmware_config_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        lv_label_set_text(lbl_firmware_config, "Comprobando actualizaciones...");
        lv_obj_add_state(btn_firmware_config, LV_STATE_DISABLED);
        lv_task_handler();
        update_scr_firmware();
        lv_task_handler();
        load_scr_firmware();
        lv_label_set_text(lbl_firmware_config, "Buscar y actualizar firmware");
        lv_obj_clear_state(btn_firmware_config, LV_STATE_DISABLED);
    }
}

void create_scr_config() {
    static const char *botones[] = {"" LV_SYMBOL_OK, "" LV_SYMBOL_CLOSE, nullptr};
    // CREAR PANTALLA DE CONFIGURACIÓN
    scr_config = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scr_config, LV_COLOR_MAKE(255, 255, 255), LV_PART_MAIN);

    // Ocultar teclado
    hide_keyboard();

    static lv_style_t style_text_muted;
    static lv_style_t style_title;

    lv_style_init(&style_title);
    lv_style_set_text_font(&style_title, &mulish_24);

    lv_style_init(&style_text_muted);
    lv_style_set_text_opa(&style_text_muted, LV_OPA_50);

    // Crear pestañas
    lv_obj_t *tabview_config;
    tabview_config = lv_tabview_create(scr_config, LV_DIR_TOP, 50);
    lv_obj_add_event_cb(lv_tabview_get_content(tabview_config), scroll_begin_event_cb, LV_EVENT_SCROLL_BEGIN, nullptr);

    lv_obj_t *tab_btns = lv_tabview_get_tab_btns(tabview_config);
    lv_obj_set_style_pad_right(tab_btns, LV_HOR_RES * 2 / 10 + 3, 0);

    lv_obj_set_height(tabview_config, LV_VER_RES);
    lv_obj_set_style_anim_time(tabview_config, 0, LV_PART_MAIN);
    lv_obj_t *tab_red_config = lv_tabview_add_tab(tabview_config, "Red");
    lv_obj_t *tab_pantalla_config = lv_tabview_add_tab(tabview_config, "Pantalla");
    lv_obj_t *tab_seguridad_config = lv_tabview_add_tab(tabview_config, "Acceso");
    lv_obj_t *tab_firmware_config = lv_tabview_add_tab(tabview_config, "Firmware");

    static lv_style_t style_bg;
    lv_style_init(&style_bg);
    lv_style_set_pad_all(&style_bg, 0);
    lv_style_set_pad_gap(&style_bg, 0);
    lv_style_set_clip_corner(&style_bg, true);
    lv_style_set_border_width(&style_bg, 0);
    lv_style_set_bg_color(&style_bg, lv_color_white());

    // Crear botón de aceptar y cancelar
    static lv_style_t style_buttons;

    lv_style_init(&style_buttons);
    lv_style_set_text_font(&style_buttons, &mulish_24);
    lv_style_set_text_color(&style_buttons, lv_color_white());
    lv_obj_t *btn_matrix_config = lv_btnmatrix_create(scr_config);
    lv_btnmatrix_set_map(btn_matrix_config, botones);
    lv_obj_set_width(btn_matrix_config, LV_HOR_RES * 2 / 10);
    lv_obj_set_height(btn_matrix_config, 50);
    lv_obj_align(btn_matrix_config, LV_ALIGN_TOP_RIGHT, 0, 0);
    lv_obj_add_style(btn_matrix_config, &style_bg, LV_PART_MAIN);
    lv_obj_add_style(btn_matrix_config, &style_buttons, LV_BTNMATRIX_DRAW_PART_BTN);
    lv_obj_add_event_cb(btn_matrix_config, btn_config_event_cb, LV_EVENT_ALL, nullptr);

    // PANEL DE CONFIGURACIÓN DE RED
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

    // Botón restaurar configuración de red
    lv_obj_t *btn_wifi_reset_config = lv_btn_create(tab_red_config);
    lv_obj_set_height(btn_wifi_reset_config, LV_SIZE_CONTENT);
    lv_obj_add_event_cb(btn_wifi_reset_config, btn_reset_wifi_config_event_cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *lbl_reset_wifi = lv_label_create(btn_wifi_reset_config);
    lv_label_set_text(lbl_reset_wifi, PUNTO_CONTROL_SSID_PREDETERMINADO);
    lv_obj_align(lbl_reset_wifi, LV_ALIGN_TOP_MID, 0, 0);

    // Botón borrar contraseña WiFi
    lv_obj_t *btn_wifi_psk_config = lv_btn_create(tab_red_config);
    lv_obj_set_height(btn_wifi_psk_config, LV_SIZE_CONTENT);
    lv_obj_add_event_cb(btn_wifi_psk_config, btn_reset_wifi_psk_event_cb, LV_EVENT_CLICKED, nullptr);

    lv_obj_t *lbl_reset_psk = lv_label_create(btn_wifi_psk_config);
    lv_label_set_text(lbl_reset_psk, "Vaciar contraseña");
    lv_obj_align(lbl_reset_psk, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t *lbl_mac_wifi = lv_label_create(tab_red_config);
    String mac = "Dirección MAC: " + WiFi.macAddress();
    lv_label_set_text(lbl_mac_wifi, mac.c_str());
    lv_obj_set_style_text_align(lbl_mac_wifi, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_add_style(lbl_mac_wifi, &style_text_muted, LV_PART_MAIN);

    // Colocar elementos en una rejilla (4 filas, 2 columnas)
    static lv_coord_t grid_col_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(3), LV_GRID_TEMPLATE_LAST};
    static lv_coord_t grid_red_row_dsc[] = {
            LV_GRID_CONTENT,  /* Título */
            LV_GRID_CONTENT,  /* SSID */
            LV_GRID_CONTENT,  /* PSK */
            LV_GRID_CONTENT,  /* Restaurar, botón */
            LV_GRID_CONTENT,  /* Info MAC */
            LV_GRID_TEMPLATE_LAST
    };

    lv_obj_set_grid_dsc_array(tab_red_config, grid_col_dsc, grid_red_row_dsc);

    lv_obj_set_grid_cell(lbl_red_config, LV_GRID_ALIGN_START, 0, 2, LV_GRID_ALIGN_CENTER, 0, 1);
    lv_obj_set_grid_cell(lbl_ssid_config, LV_GRID_ALIGN_END, 0, 1, LV_GRID_ALIGN_CENTER, 1, 1);
    lv_obj_set_grid_cell(txt_ssid_config, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_START, 1, 1);
    lv_obj_set_grid_cell(lbl_psk_config, LV_GRID_ALIGN_END, 0, 1, LV_GRID_ALIGN_CENTER, 2, 1);
    lv_obj_set_grid_cell(txt_psk_config, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_START, 2, 1);
    lv_obj_set_grid_cell(btn_wifi_reset_config, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_CENTER, 3, 1);
    lv_obj_set_grid_cell(btn_wifi_psk_config, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_CENTER, 3, 1);
    lv_obj_set_grid_cell(lbl_mac_wifi, LV_GRID_ALIGN_STRETCH, 0, 2, LV_GRID_ALIGN_CENTER, 4, 1);

    // PANEL DE CONFIGURACIÓN DE PANTALLA
    lv_obj_set_style_pad_left(tab_pantalla_config, LV_HOR_RES * 8 / 100, 0);
    lv_obj_set_style_pad_right(tab_pantalla_config, LV_HOR_RES * 8 / 100, 0);

    lv_obj_t *lbl_pantalla_config = lv_label_create(tab_pantalla_config);
    lv_label_set_text(lbl_pantalla_config, "Configuración de la pantalla");
    lv_obj_add_style(lbl_pantalla_config, &style_title, LV_PART_MAIN);

    // Brillo
    lv_obj_t *lbl_brillo_config = lv_label_create(tab_pantalla_config);
    lv_label_set_text(lbl_brillo_config, "Brillo");
    lv_obj_add_style(lbl_brillo_config, &style_text_muted, LV_PART_MAIN);

    sld_brillo_config = lv_slider_create(tab_pantalla_config);
    lv_slider_set_range(sld_brillo_config, 32, 255);
    lv_slider_set_value(sld_brillo_config, 255, LV_ANIM_OFF);
    lv_obj_add_event_cb(sld_brillo_config, sld_brightness_event_cb, LV_EVENT_VALUE_CHANGED, nullptr);

    // Botón para volver a calibrar
    lv_obj_t *btn_calibrar_config = lv_btn_create(tab_pantalla_config);
    lv_obj_set_height(btn_calibrar_config, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(btn_calibrar_config, lv_palette_main(LV_PALETTE_RED), LV_PART_MAIN);
    lv_obj_add_event_cb(btn_calibrar_config, btn_recalibrate_event_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *lbl_calibrar_config = lv_label_create(btn_calibrar_config);
    lv_label_set_text(lbl_calibrar_config, "Recalibrar panel táctil");
    lv_obj_align(lbl_calibrar_config, LV_ALIGN_TOP_MID, 0, 0);

    // Colocar elementos en una rejilla (5 filas, 2 columnas)
    static lv_coord_t grid_pantalla_row_dsc[] = {
            LV_GRID_CONTENT,  /* Título */
            5,                /* Separador */
            LV_GRID_CONTENT,  /* Barra de brillo */
            5,                /* Separador */
            LV_GRID_CONTENT,  /* Recalibrar, botón */
            LV_GRID_TEMPLATE_LAST
    };

    lv_obj_set_grid_dsc_array(tab_pantalla_config, grid_col_dsc, grid_pantalla_row_dsc);

    lv_obj_set_grid_cell(lbl_pantalla_config, LV_GRID_ALIGN_START, 0, 2, LV_GRID_ALIGN_CENTER, 0, 1);
    lv_obj_set_grid_cell(lbl_brillo_config, LV_GRID_ALIGN_END, 0, 1, LV_GRID_ALIGN_CENTER, 2, 1);
    lv_obj_set_grid_cell(sld_brillo_config, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_START, 2, 1);
    lv_obj_set_grid_cell(btn_calibrar_config, LV_GRID_ALIGN_STRETCH, 0, 2, LV_GRID_ALIGN_CENTER, 4, 1);

    // PANEL DE CONFIGURACIÓN DE SEGURIDAD
    lv_obj_set_style_pad_left(tab_seguridad_config, LV_HOR_RES * 8 / 100, 0);
    lv_obj_set_style_pad_right(tab_seguridad_config, LV_HOR_RES * 8 / 100, 0);

    lv_obj_t *lbl_seguridad_config = lv_label_create(tab_seguridad_config);
    lv_label_set_text(lbl_seguridad_config, "Parámetros de acceso");
    lv_obj_add_style(lbl_seguridad_config, &style_title, LV_PART_MAIN);

    // Botón para cambiar código de administración
    lv_obj_t *btn_cambio_codigo_config = lv_btn_create(tab_seguridad_config);
    lv_obj_set_height(btn_cambio_codigo_config, LV_SIZE_CONTENT);
    lv_obj_add_event_cb(btn_cambio_codigo_config, btn_cambio_codigo_event_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *lbl_cambio_codigo_config = lv_label_create(btn_cambio_codigo_config);
    lv_label_set_text(lbl_cambio_codigo_config, "Cambiar código de administración");
    lv_obj_align(lbl_cambio_codigo_config, LV_ALIGN_TOP_MID, 0, 0);

    // Botón para cambiar de punto de acceso
    lv_obj_t *btn_punto_acceso_config = lv_btn_create(tab_seguridad_config);
    lv_obj_set_height(btn_punto_acceso_config, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(btn_punto_acceso_config, lv_palette_main(LV_PALETTE_ORANGE), LV_PART_MAIN);
    lv_obj_add_event_cb(btn_punto_acceso_config, btn_punto_control_event_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *lbl_punto_acceso_config = lv_label_create(btn_punto_acceso_config);
    lv_label_set_text(lbl_punto_acceso_config, "Cambiar punto de acceso activo");
    lv_obj_align(lbl_punto_acceso_config, LV_ALIGN_TOP_MID, 0, 0);

    // Botón para cerrar sesión
    lv_obj_t *btn_logout_config = lv_btn_create(tab_seguridad_config);
    lv_obj_set_height(btn_logout_config, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(btn_logout_config, lv_palette_main(LV_PALETTE_RED), LV_PART_MAIN);
    lv_obj_add_event_cb(btn_logout_config, btn_logout_event_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *lbl_logout_config = lv_label_create(btn_logout_config);
    lv_label_set_text(lbl_logout_config, "Cerrar sesión");
    lv_obj_align(lbl_logout_config, LV_ALIGN_TOP_MID, 0, 0);

    // Switch de forzar activación
    sw_forzar_activacion_config = lv_switch_create(tab_seguridad_config);
    lv_obj_t *lbl_forzar_activacion_config = lv_label_create(tab_seguridad_config);
    lv_label_set_text(lbl_forzar_activacion_config, "Activar sólo con codigo");

    lbl_read_code = lv_label_create(tab_seguridad_config);
    lv_label_set_text(lbl_read_code, "Acerque llavero para leer");
    lv_obj_set_style_text_align(lbl_read_code, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_add_style(lbl_read_code, &style_text_muted, LV_PART_MAIN);

    // Colocar elementos en una rejilla (6 filas, 2 columnas)
    static lv_coord_t grid_seguridad_row_dsc[] = {
            LV_GRID_CONTENT,  /* Título */
            LV_GRID_CONTENT,  /* Cambiar código de administración, botón */
            LV_GRID_CONTENT,  /* Cambiar punto de acceso, botón */
            LV_GRID_CONTENT,  /* Cerrar sesión, botón */
            LV_GRID_CONTENT,  /* Forzar activación */
            LV_GRID_CONTENT,  /* Info código leido */
            LV_GRID_TEMPLATE_LAST
    };

    lv_obj_set_grid_dsc_array(tab_seguridad_config, grid_col_dsc, grid_seguridad_row_dsc);

    lv_obj_set_grid_cell(lbl_seguridad_config, LV_GRID_ALIGN_START, 0, 2, LV_GRID_ALIGN_CENTER, 0, 1);
    lv_obj_set_grid_cell(btn_cambio_codigo_config, LV_GRID_ALIGN_STRETCH, 0, 2, LV_GRID_ALIGN_CENTER, 1, 1);
    lv_obj_set_grid_cell(btn_punto_acceso_config, LV_GRID_ALIGN_STRETCH, 0, 2, LV_GRID_ALIGN_CENTER, 2, 1);
    lv_obj_set_grid_cell(btn_logout_config, LV_GRID_ALIGN_STRETCH, 0, 2, LV_GRID_ALIGN_CENTER, 3, 1);
    lv_obj_set_grid_cell(sw_forzar_activacion_config, LV_GRID_ALIGN_END, 0, 1, LV_GRID_ALIGN_CENTER, 4, 1);
    lv_obj_set_grid_cell(lbl_forzar_activacion_config, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_CENTER, 4, 1);
    lv_obj_set_grid_cell(lbl_read_code, LV_GRID_ALIGN_STRETCH, 0, 2, LV_GRID_ALIGN_CENTER, 5, 1);

    // PANEL DE CONFIGURACIÓN DE FIRMWARE
    lv_obj_set_style_pad_left(tab_firmware_config, LV_HOR_RES * 8 / 100, 0);
    lv_obj_set_style_pad_right(tab_firmware_config, LV_HOR_RES * 8 / 100, 0);

    lv_obj_t *lbl_firmware_title_config = lv_label_create(tab_firmware_config);
    lv_label_set_text(lbl_firmware_title_config, "Configuración del firmware");
    lv_obj_add_style(lbl_firmware_title_config, &style_title, LV_PART_MAIN);

    // Versión actual
    lv_obj_t *lbl_current_caption_config = lv_label_create(tab_firmware_config);
    lv_label_set_text(lbl_current_caption_config, "Actual:");
    lv_obj_t *lbl_current_version_config = lv_label_create(tab_firmware_config);
    lv_label_set_text(lbl_current_version_config, PUNTO_CONTROL_VERSION);

    // Botón para actualizar el firmware
    btn_firmware_config = lv_btn_create(tab_firmware_config);
    lv_obj_set_height(btn_firmware_config, LV_SIZE_CONTENT);
    lv_obj_add_event_cb(btn_firmware_config, btn_firmware_config_event_cb, LV_EVENT_CLICKED, nullptr);
    lbl_firmware_config = lv_label_create(btn_firmware_config);
    lv_label_set_text(lbl_firmware_config, "Buscar y actualizar firmware");
    lv_obj_align(lbl_firmware_config, LV_ALIGN_TOP_MID, 0, 0);

    // Colocar elementos en una rejilla (5 filas, 2 columnas)
    static lv_coord_t grid_firmware_row_dsc[] = {
            LV_GRID_CONTENT,  /* Título */
            LV_GRID_CONTENT,  /* Versión actual del firmware */
            LV_GRID_CONTENT,  /* Actualizar firmware, botón */
            LV_GRID_TEMPLATE_LAST
    };

    lv_obj_set_grid_dsc_array(tab_firmware_config, grid_col_dsc, grid_firmware_row_dsc);

    lv_obj_set_grid_cell(lbl_firmware_title_config, LV_GRID_ALIGN_START, 0, 2, LV_GRID_ALIGN_CENTER, 0, 1);
    lv_obj_set_grid_cell(lbl_current_caption_config, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_CENTER, 1, 1);
    lv_obj_set_grid_cell(lbl_current_version_config, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_CENTER, 1, 1);
    lv_obj_set_grid_cell(btn_firmware_config, LV_GRID_ALIGN_STRETCH, 0, 2, LV_GRID_ALIGN_CENTER, 2, 1);
}


void update_scr_config() {
    lv_textarea_set_text(txt_ssid_config, flash_get_string("net.wifi_ssid").c_str());
    lv_textarea_set_text(txt_psk_config, flash_get_string("net.wifi_psk").c_str());

    // Brillo de la pantalla
    uint64_t brillo = flash_get_int("scr.brightness");
    if (brillo == 0) brillo = PUNTO_CONTROL_BRILLO_PREDETERMINADO;
    lv_slider_set_value(sld_brillo_config, brillo, LV_ANIM_OFF);

    // Forzar activación con código
    uint64_t force = flash_get_int("sec.force_code");
    if (force) {
        lv_obj_add_state(sw_forzar_activacion_config, LV_STATE_CHECKED);
    } else {
        lv_obj_clear_state(sw_forzar_activacion_config, LV_STATE_CHECKED);
    }

    // Comenzar en la primera página
    lv_obj_scroll_to_view_recursive(txt_ssid_config, LV_ANIM_OFF);
}

void load_scr_config() {
    lv_scr_load(scr_config);
}

void set_config_read_code(String code) {
    lv_textarea_set_text(lbl_read_code, String("Leído: " + code).c_str());
}
