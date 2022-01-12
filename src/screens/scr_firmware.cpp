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
#include <esp_http_client.h>
#include <esp_https_ota.h>
#include <esp_ota_ops.h>
#include <esp_task_wdt.h>
#include "scr_shared.h"
#include "scr_firmware.h"
#include "scr_config.h"
#include "scr_splash.h"
#include "flash.h"
#include "punto_control.h"

typedef enum {
    FW_HTTP_IDLE, FW_HTTP_ERROR, FW_HTTP_ONGOING, FW_HTTP_DONE
} FirmwareHttpRequestStatus;

const char *CERT_PEM = R"EOF(
-----BEGIN CERTIFICATE-----
MIIDxTCCAq2gAwIBAgIQAqxcJmoLQJuPC3nyrkYldzANBgkqhkiG9w0BAQUFADBs
MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3
d3cuZGlnaWNlcnQuY29tMSswKQYDVQQDEyJEaWdpQ2VydCBIaWdoIEFzc3VyYW5j
ZSBFViBSb290IENBMB4XDTA2MTExMDAwMDAwMFoXDTMxMTExMDAwMDAwMFowbDEL
MAkGA1UEBhMCVVMxFTATBgNVBAoTDERpZ2lDZXJ0IEluYzEZMBcGA1UECxMQd3d3
LmRpZ2ljZXJ0LmNvbTErMCkGA1UEAxMiRGlnaUNlcnQgSGlnaCBBc3N1cmFuY2Ug
RVYgUm9vdCBDQTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBAMbM5XPm
+9S75S0tMqbf5YE/yc0lSbZxKsPVlDRnogocsF9ppkCxxLeyj9CYpKlBWTrT3JTW
PNt0OKRKzE0lgvdKpVMSOO7zSW1xkX5jtqumX8OkhPhPYlG++MXs2ziS4wblCJEM
xChBVfvLWokVfnHoNb9Ncgk9vjo4UFt3MRuNs8ckRZqnrG0AFFoEt7oT61EKmEFB
Ik5lYYeBQVCmeVyJ3hlKV9Uu5l0cUyx+mM0aBhakaHPQNAQTXKFx01p8VdteZOE3
hzBWBOURtCmAEvF5OYiiAhF8J2a3iLd48soKqDirCmTCv2ZdlYTBoSUeh10aUAsg
EsxBu24LUTi4S8sCAwEAAaNjMGEwDgYDVR0PAQH/BAQDAgGGMA8GA1UdEwEB/wQF
MAMBAf8wHQYDVR0OBBYEFLE+w2kD+L9HAdSYJhoIAu9jZCvDMB8GA1UdIwQYMBaA
FLE+w2kD+L9HAdSYJhoIAu9jZCvDMA0GCSqGSIb3DQEBBQUAA4IBAQAcGgaX3Nec
nzyIZgYIVyHbIUf4KmeqvxgydkAQV8GK83rZEWWONfqe/EW1ntlMMUu4kehDLI6z
eM7b41N5cdblIZQB2lWHmiRk9opmzN6cN82oNLFpmyPInngiK3BD41VHMWEZ71jF
hS9OMPagMRYjyOfiZRYzy78aG6A9+MpeizGLYAiJLQwGXFK3xPkKmNEVX58Svnw2
Yzi9RKR/5CYrCsSXaQ3pjOLAEFe4yHYSkVXySGnYvCoCWw9E1CAx2/S6cCZdkGCe
vEsXCS+0yx5DaMkHJ8HSXPfqIbloEpw8nL+e/IBcm2PN7EeqJSdnoDfzAIJ9VNep
+OkuE6N36B9K
-----END CERTIFICATE-----
-----BEGIN CERTIFICATE-----
MIIEsTCCA5mgAwIBAgIQBOHnpNxc8vNtwCtCuF0VnzANBgkqhkiG9w0BAQsFADBs
MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3
d3cuZGlnaWNlcnQuY29tMSswKQYDVQQDEyJEaWdpQ2VydCBIaWdoIEFzc3VyYW5j
ZSBFViBSb290IENBMB4XDTEzMTAyMjEyMDAwMFoXDTI4MTAyMjEyMDAwMFowcDEL
MAkGA1UEBhMCVVMxFTATBgNVBAoTDERpZ2lDZXJ0IEluYzEZMBcGA1UECxMQd3d3
LmRpZ2ljZXJ0LmNvbTEvMC0GA1UEAxMmRGlnaUNlcnQgU0hBMiBIaWdoIEFzc3Vy
YW5jZSBTZXJ2ZXIgQ0EwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQC2
4C/CJAbIbQRf1+8KZAayfSImZRauQkCbztyfn3YHPsMwVYcZuU+UDlqUH1VWtMIC
Kq/QmO4LQNfE0DtyyBSe75CxEamu0si4QzrZCwvV1ZX1QK/IHe1NnF9Xt4ZQaJn1
itrSxwUfqJfJ3KSxgoQtxq2lnMcZgqaFD15EWCo3j/018QsIJzJa9buLnqS9UdAn
4t07QjOjBSjEuyjMmqwrIw14xnvmXnG3Sj4I+4G3FhahnSMSTeXXkgisdaScus0X
sh5ENWV/UyU50RwKmmMbGZJ0aAo3wsJSSMs5WqK24V3B3aAguCGikyZvFEohQcft
bZvySC/zA/WiaJJTL17jAgMBAAGjggFJMIIBRTASBgNVHRMBAf8ECDAGAQH/AgEA
MA4GA1UdDwEB/wQEAwIBhjAdBgNVHSUEFjAUBggrBgEFBQcDAQYIKwYBBQUHAwIw
NAYIKwYBBQUHAQEEKDAmMCQGCCsGAQUFBzABhhhodHRwOi8vb2NzcC5kaWdpY2Vy
dC5jb20wSwYDVR0fBEQwQjBAoD6gPIY6aHR0cDovL2NybDQuZGlnaWNlcnQuY29t
L0RpZ2lDZXJ0SGlnaEFzc3VyYW5jZUVWUm9vdENBLmNybDA9BgNVHSAENjA0MDIG
BFUdIAAwKjAoBggrBgEFBQcCARYcaHR0cHM6Ly93d3cuZGlnaWNlcnQuY29tL0NQ
UzAdBgNVHQ4EFgQUUWj/kK8CB3U8zNllZGKiErhZcjswHwYDVR0jBBgwFoAUsT7D
aQP4v0cB1JgmGggC72NkK8MwDQYJKoZIhvcNAQELBQADggEBABiKlYkD5m3fXPwd
aOpKj4PWUS+Na0QWnqxj9dJubISZi6qBcYRb7TROsLd5kinMLYBq8I4g4Xmk/gNH
E+r1hspZcX30BJZr01lYPf7TMSVcGDiEo+afgv2MW5gxTs14nhr9hctJqvIni5ly
/D6q1UEL2tU2ob8cbkdJf17ZSHwD2f2LSaCYJkJA69aSEaRkCldUxPUd1gJea6zu
xICaEnL6VpPX/78whQYwvwt/Tv9XBZ0k7YXDK/umdaisLRbvfXknsuvCnQsH6qqF
0wGjIChBWUMo0oHjqvbsezt3tkBigAVBRQHvFwY+3sAzm2fTYS5yh+Rp/BIAV0Ae
cPUeybQ=
-----END CERTIFICATE-----
)EOF";

static lv_obj_t *scr_firmware;     // Pantalla de configuración
static lv_obj_t *txt_url_firmware;
static lv_obj_t *sw_enforce_check;
static lv_obj_t *lbl_latest_version_firmware;
static lv_obj_t *btn_update_firmware;

esp_err_t fw_http_event_handle(esp_http_client_event_t *evt);
esp_http_client_config_t fw_http_config;
esp_http_client_handle_t fw_http_client = nullptr;
String fw_response;
String firmware_url;
uint32_t firmware_status = 0;

FirmwareHttpRequestStatus firmware_http_request_status;

char update_url[1000];

void do_firmware_upgrade(const char *url) {
    static int status = 0;
    static int cuenta = 0;
    static esp_http_client_config_t config;
    static esp_https_ota_config_t ota_config;
    static esp_https_ota_handle_t https_ota_handle = nullptr;

    esp_err_t err;

    if (status == 0) {
        strncpy(update_url, url, 1000);
        config.url = update_url;
        config.cert_pem = CERT_PEM;
        config.buffer_size = 4096;

        ota_config.http_config = &config;

        esp_task_wdt_init(20,false);
        esp_err_t ret = esp_https_ota_begin(&ota_config, &https_ota_handle);

        if (ret != ESP_OK) {
            set_estado_splash("Error al iniciar actualización OTA");
            status = -1;
            return;
        }
        set_estado_splash("Comenzando descarga...");

        /*esp_app_desc_t app_desc;
        err = esp_https_ota_get_img_desc(https_ota_handle, &app_desc);
        if (err != ESP_OK) {
            set_estado_splash_format("Error al obtener imagen: 0x%s", String(err, 16).c_str());
            status = -1;
            return;
        }*/

        cuenta = 0;
        status = 1;
        return;
    }

    if (status == 1) {
        err = esp_https_ota_perform(https_ota_handle);
        if (err != ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
            if (err == ESP_OK) {
                status = 2;
                cuenta = 0;
                set_estado_splash("Descarga terminada");
                return;
            } else {
                set_estado_splash("Ocurrió un error de descarga");
                status = -1;
                return;
            }
        } else {
            if (cuenta % 10 == 0) {
                set_estado_splash_format("Leídos %s bytes",
                                         String(esp_https_ota_get_image_len_read(https_ota_handle)).c_str());
            }
            cuenta++;
        }
    }

    if (status == 2) {
        esp_err_t ota_finish_err = esp_https_ota_finish(https_ota_handle);

        if (esp_https_ota_is_complete_data_received(https_ota_handle)) {
            if (ota_finish_err == ESP_OK) {
                set_estado_splash("Firmware actualizado, reinicio inminente");
                cuenta = 0;
                status = 4;
                return;
            } else {
                if (ota_finish_err == ESP_ERR_OTA_VALIDATE_FAILED) {
                    set_estado_splash("Fallo de validación del firmware");
                } else {
                    set_estado_splash("Error desconocido al actualizar");
                }
            }
            status = 3;
            return;
        } else {
            set_estado_splash("¡No se ha leído el firmware completo!");
            status = 3;
            return;
        }
    }

    if (status == 4) {
        cuenta++;
        if (cuenta > 40) esp_restart();
        set_estado_splash_format("Firmware actualizado, reinicio en %s s.", String(4 - cuenta / 10).c_str());
    }
}

static void btn_update_firmware_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        flash_set_string("firmware_url", lv_textarea_get_text(txt_url_firmware));
        flash_commit();
        esp_restart();
    }
}

static void btn_cancel_firmware_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        load_scr_config();
    }
}

static void ta_url_form_event(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);

    if (code == LV_EVENT_VALUE_CHANGED) {
        const char *url = lv_textarea_get_text(txt_url_firmware);
        if (strlen(url) >= 12 && (strncmp(url, "https://", 8) || strncmp(url, "http://", 7))) {
            lv_obj_clear_state(btn_update_firmware, LV_STATE_DISABLED);
        } else {
            lv_obj_add_state(btn_update_firmware, LV_STATE_DISABLED);
        }
    }
}

void create_scr_firmware() {
    // CREAR PANTALLA DE ACTUALIZACIÓN DE FIRMWARE
    scr_firmware = lv_obj_create(nullptr);
    lv_obj_set_style_bg_color(scr_firmware, LV_COLOR_MAKE(255, 255, 255), LV_PART_MAIN);

    // Ocultar teclado
    create_keyboard(scr_firmware);
    hide_keyboard();

    static lv_style_t style_text_muted;
    static lv_style_t style_title;

    lv_obj_t *pnl_firmware = lv_obj_create(scr_firmware);
    lv_obj_set_height(pnl_firmware, LV_VER_RES);
    lv_obj_set_width(pnl_firmware, lv_pct(90));

    lv_style_init(&style_title);
    lv_style_set_text_font(&style_title, &mulish_24);

    lv_style_init(&style_text_muted);
    lv_style_set_text_opa(&style_text_muted, LV_OPA_50);

    // Título
    lv_obj_t *lbl_title_firmware = lv_label_create(pnl_firmware);
    lv_label_set_text(lbl_title_firmware, "Actualización de firmware");
    lv_obj_add_style(lbl_title_firmware, &style_title, LV_PART_MAIN);

    // Versión actual
    lv_obj_t *lbl_current_caption_firmware = lv_label_create(pnl_firmware);
    lv_label_set_text(lbl_current_caption_firmware, "Actual:");
    lv_obj_t *lbl_current_version_firmware = lv_label_create(pnl_firmware);
    lv_label_set_text(lbl_current_version_firmware, PUNTO_CONTROL_VERSION);

    // Última versión
    lv_obj_t *lbl_latest_caption_firmware = lv_label_create(pnl_firmware);
    lv_label_set_text(lbl_latest_caption_firmware, "Última:");
    lbl_latest_version_firmware = lv_label_create(pnl_firmware);
    lv_label_set_text(lbl_latest_version_firmware, "Pendiente");

    // URL
    txt_url_firmware = lv_textarea_create(pnl_firmware);
    lv_textarea_set_one_line(txt_url_firmware, true);
    lv_textarea_set_placeholder_text(txt_url_firmware, "https://");
    lv_obj_add_event_cb(txt_url_firmware, ta_kb_form_event_cb, LV_EVENT_ALL, pnl_firmware);
    lv_obj_add_event_cb(txt_url_firmware, ta_url_form_event, LV_EVENT_VALUE_CHANGED, nullptr);

    sw_enforce_check = lv_switch_create(pnl_firmware);
    lv_obj_t *lbl_enforce_check_firmware = lv_label_create(pnl_firmware);
    lv_label_set_text(lbl_enforce_check_firmware, "Verificar el servidor");
    lv_obj_add_state(sw_enforce_check, LV_STATE_CHECKED);
    lv_obj_add_state(sw_enforce_check, LV_STATE_DISABLED);

    // Botón para actualizar el firmware
    btn_update_firmware = lv_btn_create(pnl_firmware);
    lv_obj_set_height(btn_update_firmware, LV_SIZE_CONTENT);
    lv_obj_add_event_cb(btn_update_firmware, btn_update_firmware_event_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *lbl_update_firmware = lv_label_create(btn_update_firmware);
    lv_label_set_text(lbl_update_firmware, "Actualizar firmware");
    lv_obj_align(lbl_update_firmware, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_add_state(btn_update_firmware, LV_STATE_DISABLED);

    // Botón para cancelar la actualización
    lv_obj_t *btn_cancel_firmware = lv_btn_create(pnl_firmware);
    lv_obj_set_height(btn_cancel_firmware, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(btn_cancel_firmware, lv_palette_main(LV_PALETTE_RED), LV_PART_MAIN);
    lv_obj_add_event_cb(btn_cancel_firmware, btn_cancel_firmware_event_cb, LV_EVENT_CLICKED, nullptr);
    lv_obj_t *lbl_cancel_firmware = lv_label_create(btn_cancel_firmware);
    lv_label_set_text(lbl_cancel_firmware, "Cancelar actualización");
    lv_obj_align(lbl_cancel_firmware, LV_ALIGN_TOP_MID, 0, 0);

    // Colocar elementos en una rejilla (6 filas, 2 columnas)
    static lv_coord_t grid_firmware_col_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(3), LV_GRID_TEMPLATE_LAST};
    static lv_coord_t grid_firmware_row_dsc[] = {
            LV_GRID_CONTENT,  /* Título */
            LV_GRID_CONTENT,  /* Versión actual */
            LV_GRID_CONTENT,  /* Versión nueva */
            LV_GRID_CONTENT,  /* URL botón */
            LV_GRID_CONTENT,  /* Forzar activación */
            LV_GRID_CONTENT,  /* Actualizar firmware, botón */
            LV_GRID_CONTENT,  /* Cancelar, botón */
            LV_GRID_TEMPLATE_LAST
    };

    lv_obj_set_grid_dsc_array(pnl_firmware, grid_firmware_col_dsc, grid_firmware_row_dsc);

    lv_obj_set_grid_cell(lbl_title_firmware, LV_GRID_ALIGN_START, 0, 2, LV_GRID_ALIGN_CENTER, 0, 1);
    lv_obj_set_grid_cell(lbl_current_caption_firmware, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_CENTER, 1, 1);
    lv_obj_set_grid_cell(lbl_current_version_firmware, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_CENTER, 1, 1);
    lv_obj_set_grid_cell(lbl_latest_caption_firmware, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_CENTER, 2, 1);
    lv_obj_set_grid_cell(lbl_latest_version_firmware, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_CENTER, 2, 1);
    lv_obj_set_grid_cell(txt_url_firmware, LV_GRID_ALIGN_STRETCH, 0, 2, LV_GRID_ALIGN_CENTER, 3, 1);
    lv_obj_set_grid_cell(sw_enforce_check, LV_GRID_ALIGN_END, 0, 1, LV_GRID_ALIGN_CENTER, 4, 1);
    lv_obj_set_grid_cell(lbl_enforce_check_firmware, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_CENTER, 4, 1);
    lv_obj_set_grid_cell(btn_update_firmware, LV_GRID_ALIGN_STRETCH, 0, 2, LV_GRID_ALIGN_CENTER, 5, 1);
    lv_obj_set_grid_cell(btn_cancel_firmware, LV_GRID_ALIGN_STRETCH, 0, 2, LV_GRID_ALIGN_CENTER, 6, 1);

    // Quitar borde al panel y pegarlo a la parte superior de la pantalla
    lv_obj_set_style_border_width(pnl_firmware, 0, LV_PART_MAIN);
    lv_obj_align(pnl_firmware, LV_ALIGN_TOP_MID, 0, 0);
}

/**
 * Gestiona la petición HTTP a GitHub
 */
esp_err_t fw_http_event_handle(esp_http_client_event_t *evt) {
    String cookie_value, cookie_name, tmp;
    int pos, pos2;

    switch (evt->event_id) {
        case HTTP_EVENT_ERROR:
            firmware_http_request_status = FW_HTTP_ERROR;
            break;
        case HTTP_EVENT_ON_DATA:
            // concatenamos el bloque de datos recibidos a lo que ya teníamos
            fw_response += String((char *) evt->data).substring(0, evt->data_len);
            break;
        case HTTP_EVENT_ON_FINISH:
            // acabó la petición, almacenar el código de respuesta e indicar que estamos listos
            pos = fw_response.indexOf("\"browser_download_url\":\"https://");
            if (pos >= 0) {
                pos2 = fw_response.indexOf('"', pos + 24);
                if (pos2 >= 0) {
                    firmware_url = fw_response.substring(pos + 24, pos2);
                    lv_textarea_set_text(txt_url_firmware, firmware_url.c_str());
                }
            }
            pos = fw_response.indexOf("\"name\":\"");
            if (pos >= 0) {
                pos2 = fw_response.indexOf('"', pos + 8);
                if (pos2 >= 0) {
                    lv_label_set_text(lbl_latest_version_firmware, fw_response.substring(pos + 8, pos2).c_str());
                }
            }
            firmware_http_request_status = FW_HTTP_DONE;
            fw_response = "";
            firmware_status = 1;
            break;
        default:
            // uy, algo no esperado: error
            firmware_http_request_status = FW_HTTP_ERROR;
            break;
    }
    return ESP_OK;
}

void update_scr_firmware() {
    firmware_status = 0;

    if (fw_http_client == nullptr) {
        fw_http_config.event_handler = fw_http_event_handle;
        fw_http_config.url = PUNTO_CONTROL_RELEASE_CHECK_URL;
        fw_http_client = esp_http_client_init(&fw_http_config);
    }
    fw_response = "";
    firmware_http_request_status = FW_HTTP_ONGOING;

    if (esp_http_client_perform(fw_http_client) != ESP_OK) {
        firmware_status = -1;
    }
}

void load_scr_firmware() {
    lv_scr_load(scr_firmware);
}