
//AUTORES: BSSC - LSSV
//PROPOSITO: Lectura sensores para su procesamiento y envio hacia el maestro, así como a la plataforma
//ThingSpeak
//UNIVERSIDAD DE SAN BUENAVENTURA CALI
//SISTEMA ODIE, ODIE SENSE


//libresrias 
#include <WiFi.h>
#include <esp_now.h>
#include "DHT.h"
#include <ThingSpeak.h>
//inicializacion sensores
#define DHTPIN 4
#define DHTTYPE DHT11
#define PIRPIN 15
#define TDSPIN 32
#define SOIL_PIN 33
DHT dht(DHTPIN, DHTTYPE);
//inicializacion leds
#define LEDROJO 22
#define LEDVERDE 23

// ---------------- MAC DEL MAESTRO Y ACTION ----------------
uint8_t MACORE[] = { 0xCC, 0xDB, 0xA7, 0x3D, 0xF6, 0xD4 };
uint8_t MACACTION[] = { 0x3C, 0x8A, 0x1F, 0xA8, 0xC0, 0x20 };

uint8_t msg;
// ---------------- Estructura del mensaje ----------------
typedef struct struct_message {
  float temperature;
  float humidity;
  int pirState;
  float tdsValue;
  float soilMoisture;
} struct_message;

struct_message sensorData;

// ---------------- Variables ----------------
bool DHT22_FLAG = false;
bool TDS = false;
bool PIR = false;
bool MSV2 = false;
bool EN = false;
bool FUNC_AUTO_PIR = false;
bool FUNC_AUTO_DHT = false;
bool FUNC_IoT = false;

// ---------------- Funciones lecturas ----------------
void FUNC_DHT22();
void FUNC_TDS();
void FUNC_PIR();
void FUNC_MSV2();
void conectarAction();
void desconectarAction();

//------------------------ Coneccion plataforma IoT ----------------------

// Configuración WiFi
const char* ssid = "XXXX";
const char* password = "XXXX";

// Configuración de ThingSpeak
unsigned long canalID = 3117122;
const char* tsApiKey = "GQSPKSDUTM5WDJN6";
WiFiClient client;


void OnDataSent(const esp_now_send_info_t *info, esp_now_send_status_t status) {}

void setup() {
  ThingSpeak.begin(client);
  pinMode(LEDROJO, OUTPUT);
  pinMode(LEDVERDE, OUTPUT);
  pinMode(PIRPIN, INPUT);
  dht.begin();

  // WiFi + ESP-NOW
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    return;
  }

  esp_now_register_send_cb(OnDataSent);

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, MACORE, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  esp_now_add_peer(&peerInfo);

  // Registrar peer ACTION
  memcpy(peerInfo.peer_addr, MACACTION, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  if (esp_now_add_peer(&peerInfo) != ESP_OK) return;

  // Estado inicial de los LEDs
  digitalWrite(LEDROJO, HIGH);
  digitalWrite(LEDVERDE, LOW);

  // Callback de recepción
  esp_now_register_recv_cb([](const esp_now_recv_info_t *info, const uint8_t *data, int len) {
    if (len == 1) {
      switch (data[0]) {
        case 1:  // ENCENDER
          EN = true;
          msg = 13; // mensaje de conexión
          esp_now_send(MACORE, &msg, sizeof(msg));
          digitalWrite(LEDROJO, LOW);
          digitalWrite(LEDVERDE, HIGH);
          break;

        case 2:  // DHT22
          DHT22_FLAG = !DHT22_FLAG;
          if (!DHT22_FLAG) desconectarAction();
          break;

        case 3:  // TDS
          TDS = !TDS;
          break;

        case 4:  // PIR
          PIR = !PIR;
          if (!PIR) desconectarAction();
          break;

        case 5:  // MSV2
          MSV2 = !MSV2;
          break;

        case 6:  // FUNC_AUTO_PIR
          FUNC_AUTO_PIR = !FUNC_AUTO_PIR;
          if (FUNC_AUTO_PIR) conectarAction();
          FUNC_AUTO_DHT = !FUNC_AUTO_DHT;
          if (FUNC_AUTO_DHT) conectarAction();
          else desconectarAction();
          break;

          case 7:  // FUNC_AUTO_PIR
          FUNC_IoT = !FUNC_IoT;
          static bool wifiConnected = false;
          if (!wifiConnected) {
            WiFi.begin(ssid, password);
            wifiConnected = true;
          }

          break;

        case 8:  // RESET
          EN = false;
          DHT22_FLAG = false;
          TDS = false;
          PIR = false;
          MSV2 = false;
          FUNC_AUTO_PIR = false;
          FUNC_AUTO_DHT = false;
          FUNC_IoT = false;
          desconectarAction();
          digitalWrite(LEDROJO, HIGH);
          digitalWrite(LEDVERDE, LOW);
          break;
      }
    }
  });
}

void loop() {
  if (EN) {
    // Resetear datos antes de leer sensores
    sensorData.temperature = 0;
    sensorData.humidity = 0;
    sensorData.pirState = 0;
    sensorData.tdsValue = 0;
    sensorData.soilMoisture = 0;

    // Leer TODOS los sensores activos
    if (DHT22_FLAG) FUNC_DHT22();
    if (TDS) FUNC_TDS();
    if (PIR) FUNC_PIR();
    if (MSV2) FUNC_MSV2();

    esp_now_send(MACORE, (uint8_t *)&sensorData, sizeof(sensorData));

    if(FUNC_IoT){
      enviarDatosThingSpeak(sensorData.temperature, sensorData.humidity, sensorData.tdsValue, sensorData.pirState, sensorData.soilMoisture);
    }
    delay(5000);
  }
}

// ---------------- FUNCIONES ----------------

void conectarAction() {
  uint8_t msg = 1;
  esp_now_send(MACACTION, &msg, sizeof(msg));
}

void desconectarAction() {
  uint8_t msg = 4;
  esp_now_send(MACACTION, &msg, sizeof(msg));
}

void FUNC_DHT22() {
  sensorData.temperature = dht.readTemperature();
  sensorData.humidity = dht.readHumidity();

  // Control automático de temperatura que evita envíos repetidos
  if (FUNC_AUTO_DHT) {
    // 'rele2Estado' si el rele2 ya fue activado por alta temperatura
    static bool rele2Estado = false;

    if (sensorData.temperature >= 28.0 && !rele2Estado) {
      uint8_t msg = 3;  // Activar relé 2
      esp_now_send(MACACTION, &msg, sizeof(msg));
      rele2Estado = true; // Marcar como activado
    }
    else if (sensorData.temperature < 27.0 && rele2Estado) {
      uint8_t msg = 3;  // Desactivar relé 2 (toggle)
      esp_now_send(MACACTION, &msg, sizeof(msg));
      rele2Estado = false; // Marcar como desactivado
    }
  }
}

void FUNC_TDS() {
  int tdsRaw = analogRead(TDSPIN);
  sensorData.tdsValue = (tdsRaw / 4095.0) * 1000.0;
}

void FUNC_PIR() {
  sensorData.pirState = digitalRead(PIRPIN);

  //  Control automático PIR que evita envíos repetidos
  if (FUNC_AUTO_PIR) {
    // 'rele1Estado'  si el rele1 ya fue activado por movimiento
    static bool rele1Estado = false;

    if (sensorData.pirState == HIGH && !rele1Estado) {
      uint8_t msg = 2;  // Activar relé 1
      esp_now_send(MACACTION, &msg, sizeof(msg));
      rele1Estado = true; // Marcar como activado
    }
    else if (sensorData.pirState == LOW && rele1Estado) {
      
      uint8_t msg = 2;  // Desactivar relé 1 (toggle)
      esp_now_send(MACACTION, &msg, sizeof(msg));
      rele1Estado = false; // Marcar como desactivado
    }
  }
}

void FUNC_MSV2() {
  int soilRaw = analogRead(SOIL_PIN);
  sensorData.soilMoisture = (soilRaw / 4095.0) * 100.0;
}

void enviarDatosThingSpeak(float temperatura, float humedad, float tds, int pir, float hums) {
    ThingSpeak.setField(1, temperatura);
    ThingSpeak.setField(2, humedad);
    ThingSpeak.setField(3, tds);
    ThingSpeak.setField(4, pir);
    ThingSpeak.setField(5, hums);
    int estado = ThingSpeak.writeFields(canalID, tsApiKey);
    if (estado == 200) {
        Serial.println("Datos enviados a ThingSpeak correctamente.");
    } else {
        Serial.print("Error enviando datos. Código: ");
        Serial.println(estado);
    }
}
