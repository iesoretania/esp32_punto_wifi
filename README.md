Punto de control de presencia Wi-Fi
===================================
Este repositorio contiene el firmware para el punto de control de presencia Wi-Fi que 
se comunica con la aplicación Séneca de la Junta de Andalucía.

El código de la aplicación está publicado bajo licencia GPL 3.0.

¿Qué es?
--------
El punto de control de presencia Wi-Fi es un dispositivo que permite interactuar con el
servicio de control de presencia de Séneca, de la Consejería de Educación y Deporte 
de la Junta de Andalucía.

Gracias a él, no será necesario tener un ordenador o una tablet de forma exclusiva para 
esta función. El dispositivo solamente necesitará una toma de corriente y cobertura Wi-Fi
a Internet (por ejemplo, cobertura de Andared), pudiéndose colocar en una pared o sobre 
cualquier superficie.

¿Quién lo ha diseñado?
----------------------
Profesorado del [I.E.S. Oretania de Linares (Jaén)](https://iesoretania.es).
El centro ha participado en el programa piloto de control de presencia de la Consejería y 
pone a disposición de la comunidad educativa el desarrollo completo, tanto el software como
el hardware.

¿Qué se necesita?
-----------------
La aplicación está pensada para ser cargada en un ESP32 de Expressif. El diseño original
está basado en un [ESP32 DevKit-C](https://www.espressif.com/en/products/devkits/esp32-devkitc/overview)
que puede encontrarse en muchos sitios a precios asequibles.

El ESP32 es sólo una pieza de todo el sistema: estará conectado a una pantalla táctil, a un lector de tarjetas RFID y a un zumbador.

1. La pantalla táctil mostrará información relevante al usuario, tal como la hora o el código
   QR para fichar, permitirá la configuración del sistema e incluso la introducción un código PIN.
   * `3.5" 480*320 SPI Serial TFT LCD Module Display Screen with Touch Panel. Driver ILI9488`

2. El lector RFID permitirá usar llaveros o tarjetas. El diseño actual soporta tanto la tecnología
   de 13.56 MHz como de 125 kHz, dependiendo del tipo de lector instalado. No se soportan ambos
   lectores simultáneamente.
   * `RC522 Antenna RFID SPI IC Wireless Module` (13.56 MHz)
   * `125Khz RFID Reader Module RDM6300 UART Output Access Control System` (125 kHz)

4. El zumbador, o buzzer, proporcionará una respuesta acústica a cada acción de fichaje.
   * `3.3V Passive Buzzer`
   
5. Un LED multicolor, mostrará distintos tonos en función del estado o el resultado del envío del código
   * `WS2812 RGB LED Breakout module` (1 único LED, recomendado) o cualquier otro compatible hasta 8 LEDs.

6. Todo lo anterior estará interconectado con una placa de circuito impreso o PCB. Más abajo se enlazará
a una propuesta de diseño lista para descargar.

Por último, el sistema necesita una caja. Se propone un diseño personalizable en formato SCAD
que puede imprimirse con una impresora 3D.

El coste de todos los materiales en un conjunto es extremadamente bajo.

¿Cómo se conecta con la aplicación Séneca?
------------------------------------------
Actualmente, el funcionamiento simula el acceso a la página web de control de presencia
de Séneca como si se hiciera desde un navegador web. Esto quiere decir que podría dejar
de funcionar si el Equipo de Desarrollo de Séneca realiza algún cambio de relevancia a
la estructura interna de dicha página.

En principio parece ser que tendremos una API específica para acceder a esta funcionalidad.
Con ella, todos los "hacks" que hemos implementado podrán ser eliminados del código
otorgando más fiabilidad y soporte a medio plazo.

Actualización del firmware
--------------------------
El firmware puede buscar y descargar la última versión disponible usando la conexión WiFi
de forma autónoma (_OTA, Over-The-Air_) a partir de la versión 3.0.0.

La opción para hacerlo está en la pestaña "Firmware" del menú de configuración.

Cómo cargar el firmware inicial en una placa ESP32-DevKitC
----------------------------------------------------------

**Importante: Si usas el lector de 125 kHz (RDM6300), desconéctalo de la placa antes de programar, pues
si está conectado se impedirá la programación del firmware. También puede dar
problemas la lectura o los cambios de la configuración en la flash. Hay
que escribir el comando `espefuse.py set_flash_voltage 3.3V` 
y confirmar los cambios para evitar estos problemas y poder dejar el lector
conectado de forma permanente.**

### Obligatorio salvo en Linux: Instalación de drivers USB

Independientemente del método a utilizar, para programar el dispositivo es necesario que los controladores
USB estén correctamente instalados en el sistema operativo.

Descárgalos e instálalos desde [este enlace](https://www.silabs.com/developers/usb-to-uart-bridge-vcp-drivers).

Puedes probar estos enlaces directos, aunque pueden dejar de funcionar en cualquier momento (si fuera el caso,
usa el enlace anterior):
- Windows: [CP210x Universal Windows Driver v10.1.10](https://www.silabs.com/documents/public/software/CP210x_Universal_Windows_Driver.zip)
- macOS: [CP210x VCP Mac OSX Driver v6.0.1](https://www.silabs.com/documents/public/software/Mac_OSX_VCP_Driver.zip)
- Linux: Los kernel 3.x y superiores deberían detectar el controlador automáticamente

### MÉTODO A. Sencillo, bajo Windows ejecutando _ExpressIF Flash Download Tools_
*ATENCIÓN: Guía en progreso, próximamente se incluirán capturas de pantalla*

Esta opción no necesita instalar nada en el PC, simplemente bajar un software programador y 4 ficheros que contienen
el firmware del ESP32.

#### 1. Ficheros .bin
En la sección [Releases](https://github.com/iesoretania/esp32_punto_wifi/releases) de este repositorio
podrás descargar dos de los ficheros que usaremos: `firmware.bin` y `partitions.bin`

También son necesarios los siguientes dos ficheros (haz clic en los nombres para ir a la página de descarga
y selecciona `Download`):
- [`boot_app0.bin`](https://github.com/espressif/arduino-esp32/blob/master/tools/partitions/boot_app0.bin)
- [`bootloader_dio_40m.bin`](https://github.com/espressif/arduino-esp32/blob/master/tools/sdk/esp32/bin/bootloader_dio_40m.bin)

#### 2. Software de programación

Descarga el programa [Flash Download Tools](https://www.espressif.com/en/support/download/other-tools) desde el enlace.

#### 3. Instalación del firmware
- Conecta el puerto MicroUSB del ESP32-DevKitC con un cable USB al PC. Deberá encenderse una luz roja en la placa
- Abre el programa `flash_download_tool.exe`
- Selecciona el botón `ESP32 DownloadTool` y asegúrate de que la pestaña `SPIDownload` está seleccionada
- Con los botones de puntos suspensivos ("`...`") selecciona cada uno de los cuatro ficheros .bin descargados antes.
El orden es indistinto
- Es importante que junto a cada fichero escribas estas direcciones correctamente detrás de la arroba (@):

  | Fichero                  | Posición    |
  |--------------------------|-------------|
  | `bootloader_dio_40m.bin` | @`0x1000`   |
  | `partitions.bin`         | @`0x8000`   |
  | `boot_app0.bin`          | @`0xe000`   |
  | `firmware.bin`           | @`0x10000`  |

- No cambies nada de las otras opciones y pulsa el botón "START"
  * Si no te detecta el dispositivo, prueba a dejar pulsado el botón `BOOT` que hay en la placa del ESP32 DevKit-C
  hasta que comience la transferencia
- Cuando aparezca el mensaje "FINISH 完成" puedes cerrar el programa y desconectar el dispositivo

### MÉTODO B: Avanzado. Bajo Windows, Linux o macOS usando _PlatformIO_
- Instala [PlatformIO Core (CLI)](https://docs.platformio.org/en/latest/core/installation.html#system-requirements)
- Clona este repositorio
- Conecta el conector MicroUSB del ESP32-DevKitC al PC mediante un cable USB
- Entra en el directorio del repositorio y ejecuta `pio run -t upload`
  * Si no detecta el comando `pio`, cierra la terminal y vuelve a abrirla para actualizar el PATH
- Espera a que termine la compilación y comenzará la subida (upload)
  * Si no te detecta el dispositivo (sale la secuencia `...---...` hasta fallar), prueba a dejar pulsado
  el botón `BOOT` que hay en la placa ESP32 DevKit-C hasta que comience la subida (un porcentaje que se incrementa)

Otros recursos
--------------------
Este repositorio sólo contiene el código fuente del firmware. Puedes descargar otros
ficheros del proyecto desde [este enlace](https://drive.google.com/drive/folders/19vDfP-dDWeiWx2o5_p8hLuGxcC6yYe33?usp=sharing).
