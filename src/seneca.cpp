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
#include <mbedtls/bignum.h>
#include <mbedtls/md.h>
#include <esp_http_client.h>
#include <TinyXML.h>

#include <punto_control.h>
#include "seneca.h"
#include "flash.h"
#include "notify.h"
#include "screens/scr_main.h"
#include "screens/scr_check.h"
#include "screens/scr_selection.h"

// ATENCIÓN: El código es muy "chapucero" aunque funciona bien mientras el Equipo de Séneca no toque nada
// desde el lado servidor
//
// No se refactorizará porque en breve "una API is coming!"

byte punto_key[80];
int base32_key_length;

esp_err_t http_event_handle(esp_http_client_event_t *evt);

esp_err_t err;
esp_http_client_config_t http_config;

esp_http_client_handle_t http_client;
String cookieCPP_authToken;
String cookieCPP_puntToken;
String cookieJSESSIONID;
String cookieSenecaP;

// Estado del parser XML
String modo, punto, xCentro, nombre_punto, tipo_acceso, codigo_nuevo_punto, last_usuario;

int isClavePuntoSet = 0;

TinyXML xml;
uint8_t xmlBuffer[2048];

int xmlStatus;
String xml_current_input;

HttpRequestStatus http_request_status;
int http_status_code;
String response;
uint32_t punto_id = 0;

HttpRequestStatus seneca_get_request_status() {
    return http_request_status;
}

int seneca_get_http_status_code() {
    return http_status_code;
}

/***
 * Séneca trabaja con ISO-8859-1 mientras que lo demás espera UTF-8
 */
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

/**
 * Envía una petición a Séneca montando las cookies y las cabeceras necesarias
 * Si "body" está vacío, será una petición GET. Será POST si contiene algo
*/
int send_seneca_data(String &body) {
    // preparar cookies de petición
    String http_cookie = "";
    int primero = 1;

    cookieCPP_authToken = flash_get_string("CPP-authToken");
    if (!cookieCPP_authToken.isEmpty()) {
        http_cookie = cookieCPP_authToken;
        primero = 0;
    }
    cookieCPP_puntToken = flash_get_string("CPP-puntToken");
    if (!cookieCPP_puntToken.isEmpty()) {
        if (!primero) {
            http_cookie += "; ";
        }
        primero = 0;
        http_cookie += cookieCPP_puntToken;
    }
    if (!cookieSenecaP.isEmpty()) {
        if (!primero) {
            http_cookie += "; ";
        }
        primero = 0;
        http_cookie += cookieSenecaP;
    }
    if (!cookieJSESSIONID.isEmpty()) {
        if (!primero) {
            http_cookie += "; ";
        }
        http_cookie += cookieJSESSIONID;
    }

    // añadir cabeceras
    esp_http_client_set_header(http_client, "Cookie", http_cookie.c_str());

    if (body.length() > 0) {
        esp_http_client_set_header(http_client, "Content-Type", "application/x-www-form-urlencoded");
        esp_http_client_set_method(http_client, HTTP_METHOD_POST);
        esp_http_client_set_post_field(http_client, body.c_str(), (int) body.length());
    } else {
        esp_http_client_set_method(http_client, HTTP_METHOD_GET);
    }

    esp_http_client_set_header(http_client, "Accept", "text/html");
    esp_http_client_set_header(http_client, "Accept-Encoding", "identity");

    http_status_code = 0;
    http_request_status = HTTP_ONGOING;

    // hacer la petición
    int r = esp_http_client_perform(http_client);
    if (r) http_request_status = HTTP_ERROR;

    return r;
}

/**
 * Selecciona la URL de login y envía la información
 */
int send_seneca_login_data(String &body) {
    esp_http_client_set_url(http_client, PUNTO_CONTROL_LOGIN_URL);

    return send_seneca_data(body);
}

/**
 * Selecciona la URL del punto de control y envía la información
 */
int send_seneca_request_data(String &body) {
    esp_http_client_set_url(http_client, PUNTO_CONTROL_URL);

    return send_seneca_data(body);
}

/**
 * Procesar la respuesta del punto de control (sólo para esa URL, cuidado)
 */
int parse_seneca_response() {
    int ok = 0;

    if (http_status_code == 200) {
        // petición exitosa, extraer mensaje con la respuesta
        int index, index2;

        // la respuesta del picaje está dentro de una etiqueta <strong>, no me lo cambiéis :)
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
                    notify_check_success_in();
                    set_icon_text_check("\uF2F6", LV_PALETTE_GREEN, 0, 0);
                } else if (response.startsWith("Salida")) {
                    notify_check_success_out();
                    set_icon_text_check("\uF2F5", LV_PALETTE_RED, 0, 0);
                } else {
                    notify_check_error();
                    set_icon_text_check("\uF071", LV_PALETTE_ORANGE, 0, 0);
                }
                set_estado_check(response.c_str());

                ok = 1; // éxito
            }
        } else {
            // no hay etiqueta <strong>. Algo no hemos hecho bien
            set_icon_text_check("\uF071", LV_PALETTE_ORANGE, 0, 0);
            set_estado_check("Se ha obtenido una respuesta desconocida desde Séneca.");
        }
    } else {
        // no se ha devuelto un código 200
        set_icon_text_check("\uF071", LV_PALETTE_ORANGE, 0, 0);
        set_estado_check("Ha ocurrido un error en la comunicación con Séneca.");
    }

    return ok; // 0 = error
}

/**
 * Gestiona la petición HTTP en curso. Es necesario para interceptar las cookies que envía Séneca
 */
esp_err_t http_event_handle(esp_http_client_event_t *evt) {
    String cookie_value, cookie_name, tmp;
    int index;

    switch (evt->event_id) {
        case HTTP_EVENT_ERROR:
            http_request_status = HTTP_ERROR;
            break;
            case HTTP_EVENT_ON_HEADER:
                // vienen cookies
                if (strcmp(evt->header_key, "Set-Cookie") == 0) {
                    cookie_name = evt->header_value;
                    index = cookie_name.indexOf('=');
                    if (index > -1) {
                        cookie_name = cookie_name.substring(0, index);
                        cookie_value = evt->header_value;
                        index = cookie_value.indexOf(';');
                        if (index > -1) {
                            cookie_value = cookie_value.substring(0, index);
                        }
                        if (cookie_name.equals("JSESSIONID")) {
                            cookieJSESSIONID = cookie_value;
                            break;
                        }
                        if (cookie_name.equals("SenecaP")) {
                            cookieSenecaP = cookie_value;
                            break;
                        }
                        if (cookie_name.equals("CPP-authToken")) {
                            cookieCPP_authToken = cookie_value;
                            // si no existe en la flash, almacenar su valor en flash
                            // TODO: cambia continuamente, pero su validez es muy larga (hablamos de años)
                            //  aún así sería interesante actualizarla una vez al día o cada semana para no
                            //  gastar la flash en lugar de ignorar los cambios como hacemos ahora
                            tmp = flash_get_string("CPP-authToken");
                            if (tmp.length() == 0) {    // actualizar en flash solamente si no existe
                                flash_set_string("CPP-authToken", cookieCPP_authToken);
                            }
                            break;
                        }
                        if (cookie_name.equals("CPP-puntToken")) {
                            cookieCPP_puntToken = cookie_value;
                            // si no existe en la flash o ha cambiado, almacenar su valor en flash
                            tmp = flash_get_string("CPP-puntToken");
                            if (tmp.length() == 0 || !tmp.equals(cookieCPP_puntToken)) {
                                flash_set_string("CPP-puntToken", cookieCPP_puntToken);
                            }
                            break;
                        }
                    }
                }
                break;
            case HTTP_EVENT_ON_DATA:
                // concatenamos el bloque de datos recibidos a lo que ya teníamos
                response += String((char *) evt->data).substring(0, evt->data_len);
                break;
            case HTTP_EVENT_ON_FINISH:
                // acabó la petición, almacenar el código de respuesta e indicar que estamos listos
                http_request_status = HTTP_DONE;
                http_status_code = esp_http_client_get_status_code(evt->client);
                break;
            default:
                // uy, algo no esperado: error
                http_request_status = HTTP_ERROR;
                break;
    }
    return ESP_OK;
}

/**
 * Codificar usando RSA un número m dado (cadena ASCII con número en decimal) con los e y n dados
 * usando el coprocesador criptográfico
 *
 * r = m^e mod n
 */
String seneca_cifrar_rsa(const char *cadena, const char *e, const char *n) {
    char buffer[100];
    size_t buffer_len;

    // resultado = m^e mod n
    mbedtls_mpi m_m, m_e, m_n, m_resultado;
    mbedtls_mpi_init(&m_m);
    mbedtls_mpi_init(&m_e);
    mbedtls_mpi_init(&m_n);
    mbedtls_mpi_init(&m_resultado);

    // convertir las cadenas en enteros grandes
    mbedtls_mpi_read_string(&m_m, 10, cadena);
    mbedtls_mpi_read_string(&m_e, 10, e);
    mbedtls_mpi_read_string(&m_n, 10, n);
    mbedtls_mpi_exp_mod(&m_resultado, &m_m, &m_e, &m_n, nullptr);

    // convertir el entero grande del resultado en cadena
    mbedtls_mpi_write_string(&m_resultado, 10, buffer, sizeof(buffer), &buffer_len);

    return String(buffer);
}

/**
 * Calcula el OTP necesario para el código QR. Se deriva de una parte de un hash criptográfico SHA1
 * cuya clave es distinta para cada punto de control (almacenada en punto_key previamente) y que se
 * aplica a la hora actual en una granularidad de 30 segundos
 */
uint32_t seneca_calcula_otp(time_t now) {
    byte hmacResult[20];
    byte payload[8] = { 0 };

    // configurar contexto
    mbedtls_md_context_t ctx;
    mbedtls_md_type_t md_type = MBEDTLS_MD_SHA1;
    const size_t payload_length = 8;

    size_t key_length = base32_key_length * 5 / 8;

    // cambiar el timestamp a una granularidad de 30 segundos y almacenarlo en 4 bytes al final del payload
    now /= 30;
    payload[7] = now & 0xFF;
    payload[6] = (now >> 8) & 0xFF;
    payload[5] = (now >> 16) & 0xFF;
    payload[4] = (now >> 24) & 0xFF;

    // obtener el HMAC SHA1 del payload con la clave del punto de acceso previamente configurada
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 1);
    mbedtls_md_hmac_starts(&ctx, (const unsigned char *) punto_key, key_length);
    mbedtls_md_hmac_update(&ctx, (const unsigned char *) payload, payload_length);
    mbedtls_md_hmac_finish(&ctx, hmacResult);
    mbedtls_md_free(&ctx);

    // extraer los 4 bits menos significativos del hash. Eso nos dará el offset del hash a partir
    // del cual sacaremos los 4 bytes que necesitamos
    int offset = hmacResult[19] & 0xF;

    // obtener los 4 bytes desde el offset en un entero de 32 bits
    uint32_t otp = hmacResult[offset];
    otp = (otp << 8) + hmacResult[offset + 1];
    otp = (otp << 8) + hmacResult[offset + 2];
    otp = (otp << 8) + hmacResult[offset + 3];

    otp = otp & 0x7fffffff;

    // devolver los últimos 6 dígitos de su representación en decimal: es el parámetro que irá en el
    // código QR junto con el número de punto de control
    return otp % 1000000;
}

/**
 * Cumplimenta la clave del punto de acceso a partir de su representación en BASE32
 */
void seneca_set_clave_punto(char *pos_inicio) {
    char clave_punto[81] = { 0 };
    const char base32[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

    strncpy(clave_punto, pos_inicio, 80);
    clave_punto[80] = '\0';
    base32_key_length = strlen(clave_punto);
    long long acum = 0;
    int num_bits = 0;
    int pos_key = 0;
    char *pos = clave_punto;
    while (*pos != '\0') {
        char *v = strchr(base32, *pos);
        if (v == nullptr) {
            break;
        }
        acum = acum * 32 + (v - base32);
        num_bits += 5;
        if (num_bits >= 8) {
            punto_key[pos_key] = acum >> (num_bits - 8);
            acum -= punto_key[pos_key] << (num_bits - 8);
            num_bits -= 8;
            pos_key++;
        }
        pos++;
    }
}

void seneca_set_punto_id(int id) {
    punto_id = id;
}

void seneca_prepare_request() {
    response = "";
    http_request_status = HTTP_ONGOING;
    http_status_code = 0;
}

/**
 * Parser del HTML devuelto por Séneca. Tiene muchos hacks porque el código HTML de Séneca no es XML puro
 * (etiquetas sin cerrar o atributos que tienen el igual con whitespaces en medio)
 */
void xml_html_callback(uint8_t statusflags, char *tagName, uint16_t tagNameLen, char *data, uint16_t dataLen) {
    // nos quedamos con las etiquetas <h1> y <a>
    if ((statusflags & STATUS_END_TAG) && strstr(tagName, "h1")) {
        xmlStatus = 0;
    }
    if ((statusflags & STATUS_START_TAG) && strstr(tagName, "h1")) {
        xmlStatus = 2;
    }
    if ((statusflags & STATUS_START_TAG) && strlen(tagName) > 2 && tagName[tagNameLen - 1] == 'a' &&
    tagName[tagNameLen - 2] == '/') {
        xmlStatus = 3;
    }
    if ((statusflags & STATUS_END_TAG) && strlen(tagName) > 2 && tagName[tagNameLen - 1] == 'a' &&
    tagName[tagNameLen - 2] == '/') {
        xmlStatus = 0;
    }

    // buscar el atributo "name" y "value", que están en input
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

    // si estamos dentro de una etiqueta <h1>, obtener nombre del punto de control
    if (xmlStatus == 2 && statusflags & STATUS_TAG_TEXT) {
        String nombre = data;
        nombre_punto = iso_8859_1_to_utf8(nombre);
    }

    // si estamos dentro de una etiqueta <a>, comprobar si es una lista de puntos de control
    if (xmlStatus == 3 && statusflags & STATUS_ATTR_TEXT && !strcasecmp(tagName, "href") &&
    strstr(data, "activarPuntoAcceso")) {
        codigo_nuevo_punto = data;
        codigo_nuevo_punto = codigo_nuevo_punto.substring(31, codigo_nuevo_punto.indexOf(')') - 1);
    }

    if (xmlStatus == 3 && statusflags & STATUS_TAG_TEXT && !codigo_nuevo_punto.isEmpty()) {
        selection_add_punto(codigo_nuevo_punto.c_str(), data);
        codigo_nuevo_punto = "";
        xmlStatus = 0;
    }

    // encontrar una clase de tipo "login" activa el tipo de acceso de la pantalla de entrada
    if ((statusflags & STATUS_ATTR_TEXT) && !strcasecmp(tagName, "class") && !strcasecmp(data, "login")) {
        tipo_acceso = "login";
    }
}

/**
 * Inicializa el cliente asíncrono HTTP
 */
void initialize_seneca() {
    http_config.event_handler = http_event_handle;
    http_config.url = PUNTO_CONTROL_URL;

    http_client = esp_http_client_init(&http_config);

    http_request_status = HTTP_IDLE;
}

/**
 * Formulario POST del punto de control de presencia para enviar un código
 */
void seneca_process_token(String uid) {
    // enviar token a Séneca
    String params =
            "_MODO_=" + modo + "&PUNTO=" + punto + "&X_CENTRO=" + xCentro + "&C_TIPACCCONTPRE=&DARK_MODE=N&TOKEN=" +
            uid;
    response = "";
    send_seneca_request_data(params);
}

void reset_seneca() {
    esp_http_client_cleanup(http_client);
    initialize_seneca();
}

void seneca_set_punto(char *nombre) {
    punto = nombre;
}

String seneca_get_punto() {
    return punto;
}

/**
 *
 */
void process_seneca_response() {
    modo = "";
    punto = "";
    xCentro = "";
    nombre_punto = "";
    tipo_acceso = "";
    isClavePuntoSet = 0;

    // análisis XML del HTML devuelto para extraer los valores de los campos ocultos
    xml.init(xmlBuffer, 2048, xml_html_callback);

    // inicialmente no hay etiqueta abierta, procesar carácter a carácter
    xmlStatus = 0;

    // Id del punto de control
    char *cadena = (char *)(response.c_str());
    char *pos = strstr(cadena, "|X_PUNACCCONTPRE=");
    if (pos) {
        pos += 17;
        punto_id = 0;
        while (*pos >= '0' && *pos <= '9') {
            punto_id = punto_id * 10 + (*pos - '0');
            pos++;
        }

        // Clave del punto de control (para generar QR)
        pos = strstr(cadena, "base32ToHex('");
        if (pos) {
            seneca_set_clave_punto(pos + 13); // convertir lo que hay detrás de la comilla inicial
            isClavePuntoSet = 1;
        }
    }
    while (*cadena != 0) {
        xml.processChar(*cadena);
        cadena++;
    }

    // reiniciar parser XML
    xmlStatus = 0;
    xml.reset();

    if (!nombre_punto.isEmpty()) {
        set_nombre_main(nombre_punto.c_str());
    } else {
        set_nombre_main("Punto de acceso WiFi");
    }
}

void seneca_clear_request_status() {
    http_request_status = HTTP_IDLE;
}

String seneca_get_tipo_acceso() {
    return tipo_acceso;
}

/**
 * Procesar respuesta JSON del login
 */
String seneca_process_json_response() {
    if (response.indexOf("<correcto>SI") > -1) {
        // es correcta, volver a cargar página principal del punto de acceso con nueva cookie
        String param = "_MODO_=" + modo + "&USUARIO=" + last_usuario + "&CLAVE_P=";
        response = "";
        send_seneca_request_data(param);
        return "";
    } else {
        // error: mostrar mensaje en formulario
        String error = response.substring(response.indexOf("<mensaje>") + 9,
                                          response.indexOf("</mensaje>"));
        http_request_status = HTTP_IDLE;
        return error;
    }
}

String seneca_get_qrcode_string(time_t now) {
    uint32_t otp = seneca_calcula_otp(now);
    return String("AUTHKEY=" + String(otp) + "|X_PUNACCCONTPRE=" + punto_id);
}

/**
 * Proceso de "logueo" en Séneca para obtener las cookies de sesión
 */
void seneca_login(String &usuario, String &password) {
    // la autenticación de Séneca convierte cada carácter del password a su código ASCII y se concatenan
    // el resultado es el número al que se aplica el cifrado RSA con las claves fijas dadas
    String clave_convertida = "";
    const char *ptr = password.c_str();

    last_usuario = usuario;

    while (*ptr) {
        clave_convertida += (int) (*ptr);
        ptr++;
    }

    // e y n están sacados del formulario
    String clave = seneca_cifrar_rsa(clave_convertida.c_str(), "3584956249",
                                     "356056806984207294102423357623243547284021");

    // generar parámetros del formulario de autenticación
    String datos = "N_V_=NV_" + String(random(10000)) + "&rndval=" + random(100000000) +
            "&NAV_WEB_NOMBRE=Netscape&NAV_WEB_VERSION=5&RESOLUCION=1024&CLAVECIFRADA="
            + clave + "&USUARIO=" + usuario + "&C_INTERFAZ=SENECA";

    response = "";

    send_seneca_login_data(datos);
}

int is_seneca_qrcode_enabled() {
    return isClavePuntoSet;
}
