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
Profesorado del I.E.S. Oretania de Linares (Jaén). El centro ha participado en el programa
piloto de control de presencia de la Consejería y pone a disposición de la comunidad
educativa el desarrollo completo, tanto el software como el hardware.

¿Qué se necesita?
-----------------
La aplicación está pensada para ser cargada en un ESP32 de Expressif. El diseño original
está basado en un [ESP32 DevKit-C](https://www.espressif.com/en/products/devkits/esp32-devkitc/overview)
que puede encontrarse en muchos sitios a precios asequibles.

El ESP32 es sólo una pieza de todo el sistema: estará conectado a una pantalla táctil, a un lector de tarjetas RFID y a un zumbador.

* La pantalla táctil mostrará información relevante al usuario, tal como la hora o el código
  QR para fichar, permitirá la configuración del sistema e incluso la introducción un código PIN.

* El lector RFID permitirá usar llaveros o tarjetas. El diseño actual sólo soporta la tecnología
  de 13.56 MHz, pero se trabajará en permitir etiquetas de 125 kHz con cambios menores en el
  hardware en un futuro próximo.

* El zumbador, o buzzer, proporcionará una respuesta acústica a cada acción de fichaje.

Todo estará interconectado con una placa de circuito impreso o PCB. Más abajo se enlazará
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

Descarga de recursos
--------------------
Este repositorio sólo contiene el código fuente del firmware. Puedes descargar la última
versión de los ficheros del proyecto desde [este enlace](https://drive.google.com/drive/folders/19vDfP-dDWeiWx2o5_p8hLuGxcC6yYe33?usp=sharing).