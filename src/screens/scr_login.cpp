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
#include <lvgl.h>
#include "scr_shared.h"
#include "scr_login.h"
#include "scr_splash.h"
#include "seneca.h"

static lv_obj_t *scr_login;      // Pantalla de entrada de credenciales
static lv_obj_t *pnl_login;
static lv_obj_t *txt_usuario_login;
static lv_obj_t *txt_password_login;
static lv_obj_t *btn_login;
static lv_obj_t *lbl_error_login;

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

static void ta_login_submit_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        String datos;
        String usuario = lv_textarea_get_text(txt_usuario_login);
        String password = lv_textarea_get_text(txt_password_login);

        seneca_login(usuario, password);
        set_estado_splash("Activando usuario en Séneca");
    }
}

void create_scr_login() {
    // CREAR PANTALLA DE ENTRADA DE CREDENCIALES
    scr_login = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scr_login, LV_COLOR_MAKE(255, 255, 255), LV_PART_MAIN);

    // Crear teclado
    create_keyboard(scr_login);
    hide_keyboard();

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

    lv_obj_clear_flag(pnl_login, LV_OBJ_FLAG_SCROLLABLE);
    create_config_button(scr_login);
}

void set_error_login_text(const char *string) {
    lv_label_set_text(lbl_error_login, string);
}

void load_scr_login() {
    lv_scr_load(scr_login);
}
