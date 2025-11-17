//AUTORES: LSSV-BSSC
//PROPOSITO:modulo automatizacion desde el maestro
//UNVIERSIDAD DE SAN BUENAVENTURA CALI
//SISTEMA ODIE, ODIE ACTION 


#include <esp_now.h>
#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>


#define ANCHO_PANTALLA 128
#define ALTO_PANTALLA 64

Adafruit_SH1106G display(ANCHO_PANTALLA, ALTO_PANTALLA, &Wire, -1);

// MAC del esclavo
uint8_t MACACTION[] = {0x3C, 0x8A, 0x1F, 0xA8, 0xC0, 0x20};

#define LED_ROJO 18   // Rojo en D18
#define LED_VERDE 19  // Verde en D19
#define BUTTON_PIN1 15   // Botón 1
#define BUTTON_PIN2 4    // Botón 2
#define BUTTON_PIN3 5    // Botón 3

uint8_t msg;
bool conectado = false;
bool mostrarConexion = false;
unsigned long tiempoConexion = 0;  // guarda el momento en que llegó la conexión

// Variables de estado
bool rele1Activo = false;
bool rele2Activo = false;

unsigned long lastAttempt = 0;
const unsigned long intervalo = 2000; // reintento cada 2 segundos

esp_now_peer_info_t peerInfo;

void onDataRecv(const esp_now_recv_info *info, const uint8_t *data, int len) {
  if (len == 1 && data[0] == 2) {   // Respuesta del esclavo
    digitalWrite(LED_ROJO, LOW);    // Apagar rojo
    digitalWrite(LED_VERDE, HIGH);  // Encender verde
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(SH110X_WHITE);
    display.setCursor(0, 0);
    display.println("Conexion exitosa");
    display.display();

    tiempoConexion = millis();   // Guardamos el momento
    mostrarConexion = true; 
    conectado=true;
    
  }
}

void setup() {
  Serial.begin(115200);

  pinMode(LED_ROJO, OUTPUT);
  pinMode(LED_VERDE, OUTPUT);
  pinMode(BUTTON_PIN1, INPUT_PULLUP);
  pinMode(BUTTON_PIN2, INPUT_PULLUP);
  pinMode(BUTTON_PIN3, INPUT_PULLUP);

  // 🔴 Estado inicial: Rojo encendido, Verde apagado
  digitalWrite(LED_ROJO, HIGH);
  digitalWrite(LED_VERDE, LOW);

  WiFi.mode(WIFI_STA);

  if (esp_now_init() != ESP_OK) {
    return;
  }

  memcpy(peerInfo.peer_addr, MACACTION, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    return;
  }

  // Inicializar pantalla SH1106
  if (!display.begin(0x3C, true)) {   // 0x3C dirección típica, prueba 0x3D si no
    for (;;);
  }
  
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0, 0);
  display.println("Conectando con ODE");
  display.display();

  esp_now_register_recv_cb(onDataRecv);
}

void loop() {
  if (mostrarConexion) {
    if (millis() - tiempoConexion >= 3000) {  
      mostrarConexion = false;  // Pasaron 3s → salir del mensaje de conexión
    } else {
      // Mientras está activo el mensaje de conexión, no hace nada más
      return;
    }
  }
  if (!conectado && millis() - lastAttempt > intervalo) {
    msg = 1; // mensaje de conexión
    esp_now_send(MACACTION, &msg, sizeof(msg));
    lastAttempt = millis();
  }

  if (conectado) {
    // Actualizar pantalla
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SH110X_WHITE);
    display.setCursor(0, 0);
    display.println(rele1Activo ? "RELE 1 ACTIVO" : "RELE 1 INACTIVO");
    display.setCursor(0, 16);
    display.println(rele2Activo ? "RELE 2 ACTIVO" : "RELE 2 INACTIVO");
    display.display();
  
    // Botón 1
    if (digitalRead(BUTTON_PIN1) == LOW) {
      uint8_t msg = 2;  // Mensaje para RELE1
      esp_now_send(MACACTION, &msg, sizeof(msg));
      delay(200);
      rele1Activo = !rele1Activo;
    } 

    // Botón 2
    if (digitalRead(BUTTON_PIN2) == LOW) {
      uint8_t msg = 3;  // Mensaje para RELE2
      esp_now_send(MACACTION, &msg, sizeof(msg));
      delay(200);
      rele2Activo = !rele2Activo;
    }
  }
}
