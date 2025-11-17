# SISTEMA-ODIE-
ODIE (Open Digital Interactive Environment) es un ecosistema modular diseñado para enseñanza, experimentación y prototipado rápido en el campo de la electrónica y la automatización. El sistema está compuesto por cuatro módulos —ODIE CORE, ODIE SENSE, ODIE ACTION y ODIE SPARK— que utilizan ESP32, comunicación inalámbrica (ESP-NOW / WiFi), sensores, actuadores y una interfaz de usuario simple basada en botones, LEDs y pantallas OLED.

Su propósito es ofrecer una herramienta educativa práctica que permita a estudiantes y entusiastas aprender conceptos STEAM, lógica digital, comunicación inalámbrica y diseño electrónico mediante experiencias interactivas y proyectos reales.

Los diseños de hardware del sistema ODIE se encuentran disponibles en el repositorio institucional de la Universidad de San Buenaventura Cali, dentro del trabajo de grado titulado “SISTEMA MODULAR ODIE: Objeto Didáctico Electrónico Interconectado Multifunción para Fomentar el Interés en Vocaciones Tecnológicas”.


Especificaciones:

### ODIE CORE
Módulo principal encargado de la gestión del sistema. Permite la interacción con el usuario mediante OLED, botones y LEDs, y coordina la comunicación inalámbrica entre módulos.
-----------------------------------------------------------------------------
| Componente / Parámetro | Especificación  |     Unidad / Observación       |
|------------------------|---------------- |--------------------------------|
| Microcontrolador       | ESP32-WROOM-32  | Doble núcleo, WiFi + Bluetooth |
| Voltaje de operación   | 5 V             | —                              |
| Corriente máxima       | 214.38 mA       | Consumo total estimado         |
| Tipo de comunicación   | ESP-NOW         | Inalámbrica                    |
| Tipo de alimentación   | USB / Batería   | Cable USB-C                    |
| Tipo de montaje        | Tapa impresa 3D | Fijación con amarras plásticas |
-----------------------------------------------------------------------------

### ODIE SENSE
Unidad de adquisición de datos. Integra sensores digitales y analógicos para medir variables ambientales y enviar datos mediante ESP-NOW y WiFi.
-----------------------------------------------------------------------------
| Componente / Parámetro | Especificación  |     Unidad / Observación       |
|------------------------|---------------- |--------------------------------|
| Microcontrolador       | ESP32-WROOM-32  | Doble núcleo, WiFi + Bluetooth |
| Voltaje de operación   | 5 V             | —                              |
| Corriente máxima       | 185.49 mA       | Consumo total estimado         |
| Tipo de comunicación   | ESP-NOW + WiFi  | Inalámbrica                    |
| Tipo de alimentación   | USB / Batería   | Cable USB-C                    |
| Tipo de montaje        | Tapa impresa 3D | Fijación con amarras plásticas |
-----------------------------------------------------------------------------

### ODIE ACTION
Módulo orientado al control de actuadores. Utiliza relés para activar dispositivos externos y ejecutar acciones derivadas de la lógica del sistema.
-----------------------------------------------------------------------------
| Componente / Parámetro | Especificación   |     Unidad / Observación       |
|------------------------|----------------  |--------------------------------|
| Microcontrolador       | ESP32-WROOM-32   | Doble núcleo, WiFi + Bluetooth |
| Voltaje de operación   | 5 V              | —                              |
| Corriente máxima       | 296.35 mA        | Consumo total estimado         |
| Tipo de comunicación   | ESP-NOW          | Inalámbrica                    |
| Tipo de alimentación   | USB / Batería    | Cable USB-C                    | 
| Tipo de montaje        | Tapa impresa 3D  | Fijación con amarras plásticas |
-----------------------------------------------------------------------------

### ODIE SPARK
Módulo interactivo enfocado en actividades didácticas. Combina botones, LEDs y retroalimentación inmediata para ejercicios de lógica, juegos y prácticas introductorias.
-----------------------------------------------------------------------------
| Componente / Parámetro | Especificación  |      Unidad / Observación      |
|------------------------|---------------- |--------------------------------|
| Microcontrolador       | ESP32-WROOM-32  | Doble núcleo, WiFi + Bluetooth |
| Voltaje de operación   | 5 V             | —                              |
| Corriente máxima       | 170.43 mA       | Consumo total estimado         |
| Tipo de comunicación   | ESP-NOW         | Inalámbrica                    |
| Tipo de alimentación   | USB / Batería   | Cable USB-C                    |
| Tipo de montaje        | Tapa impresa 3D | Fijación con amarras plásticas |
-----------------------------------------------------------------------------
