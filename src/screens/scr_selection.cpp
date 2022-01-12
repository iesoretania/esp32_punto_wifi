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
#include "seneca.h"
#include "scr_selection.h"
#include "scr_splash.h"

static lv_obj_t *scr_selection;  // Pantalla de selección de punto de control
static lv_obj_t *pnl_selection;
static lv_obj_t *lst_selection;

static void ta_select_form_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        // los datos de usuario contienen el código del punto de control
        seneca_set_punto((char *) lv_event_get_user_data(e));

        set_estado_splash("Activando punto de acceso en Séneca");
        load_scr_splash();
    }
    if (code == LV_EVENT_DELETE) {
        lv_mem_free(lv_event_get_user_data(e));
    }
}

void selection_add_punto(const char *codigo_punto, const char *nombre_punto) {
    lv_obj_t *btn = lv_list_add_btn(lst_selection, LV_SYMBOL_WIFI, nombre_punto);
    char *item = (char *) lv_mem_alloc(strlen(codigo_punto) + 1);
    strcpy(item, codigo_punto);
    lv_obj_add_event_cb(btn, ta_select_form_event_cb, LV_EVENT_CLICKED, item);
    lv_obj_add_event_cb(btn, ta_select_form_event_cb, LV_EVENT_DELETE, item);
}

void create_scr_selection() {
    // CREAR PANTALLA DE SELECCIÓN DE PUNTO DE CONTROL
    scr_selection = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scr_selection, LV_COLOR_MAKE(255, 255, 255), LV_PART_MAIN);

    // Crear panel del formulario
    pnl_selection = lv_obj_create(scr_selection);
    lv_obj_set_height(pnl_selection, LV_VER_RES);
    lv_obj_set_width(pnl_selection, lv_pct(100));

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

void load_scr_selection() {
    Serial.println("1");
    lv_scr_load(scr_selection);
    Serial.println("2");
}