
//AUTORES: Briguitt Selena Salinas Chavez - Laura Sofia Solarte
//PROPOSITO:Modulo maestro del sistema ODIE, coordinar la comunicacion de los modulos esclavos 
//UNIVERSIDAD DE SAN BUENAVENTURA CALI
//SISTEMA ODIE: ODIE CORE

#include <esp_now.h>
#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SH110X.h>

// Dimensiones OLED SH1106 1.3"
#define ANCHO_PANTALLA 128
#define ALTO_PANTALLA 64

Adafruit_SH1106G display(ANCHO_PANTALLA, ALTO_PANTALLA, &Wire, -1);

//Definicion codigo numerico para el envio 
const int B_ARRIBA = 15;
const int B_ABAJO = 4;
const int B_ENTER = 5;

// Menús ------------------------------------------------------------------
int menuInicial = 0;
int menuIndex = 0;
int menuMemoria = -1;
const char* menuPrincipal[] = { "ODE ACTION", "ODE SENSE", "ODE SPARK" };
const int CANT_OPCIONES_PRINCIPAL = sizeof(menuPrincipal) / sizeof(menuPrincipal[0]);

const char* subMenus[][5] = {
  { "Jugar de nuevo ", "Volver", "", "", "" },
  { "DHT22", "TDS", "PIR", "MSV2", "Volver" }
};

// Rebote botones ---------------------------------------------------------
unsigned long r_Arriba = 0;
unsigned long r_Abajo = 0;
unsigned long r_Enter = 0;
const unsigned long delayRebote = 200;
int lastEnterState = HIGH;

// ESP-NOW MAC ESCLAVOS ----------------------------------------------------------------
uint8_t MACACTION[] = { 0x3C, 0x8A, 0x1F, 0xA8, 0xC0, 0x20 };
uint8_t MACSPARK[] = { 0x80, 0xF3, 0xDA, 0x5D, 0xBB, 0x94 };
uint8_t MACSENSE[] = { 0x80, 0xF3, 0xDA, 0x64, 0xD9, 0xFC };

uint8_t msg;
esp_now_peer_info_t peerInfo;

// ---------- Estados ACTION ----------
bool conectado = false;
bool mostrarConexion = false;
bool ActivarFunc = false;
bool AvisoConexion = true;
bool enFuncionReles = false;
bool rele1Activo = false;
bool rele2Activo = false;

// ---------- Estados SPARK ----------
bool conectadoSpark = false;
bool AvisoConexionSpark = true;
bool enFuncionSpark = false;
bool juegoIniciado = false;
bool TerminoJuego = false;
bool Rejugar = false;

unsigned long tiempoConexion = 0;
unsigned long lastAttempt = 0;
const unsigned long intervalo = 2000;
int NivelJuego = 1;

// ---------- Estados Sense ------------
bool conectadoSense = false;
bool AvisoConexionSense = true;
bool enFuncionSense = false;
bool mostrarMenuSense = false;
bool enMediciones = false;

// ---------- Sistema de Desbloqueo ------------
int victoriasSpark = 0;
bool funcionIoTDesbloqueada = false;
bool funcionAutoDesbloqueada = false;
bool funcionIoTActiva = false;
bool funcionAutoActiva = false;
bool autoActivado = false;

// ---------- Estructura de datos SENSE ------------
typedef struct struct_message {
  float temperature;
  float humidity;
  int pirState;  
  float tdsValue;
  float soilMoisture;
} struct_message;

struct_message sensorData;

// ---------- Estados sensores ------------
bool sensorDHT22Activo = false;
bool sensorTSDActivo = false;
bool sensorPIRActivo = false;
bool sensorMSV2Activo = false;

unsigned long ultimaActualizacionMediciones = 0;
const unsigned long intervaloMediciones = 1000;

// LEDs -------------------------------------------------------------------
const int ledAzul = 23;
const int ledVerde = 18;
const int ledRojo = 19;

// ------------------------------------------------------------------------
void setup() {
  Wire.begin(21, 22);

  pinMode(B_ARRIBA, INPUT_PULLUP);
  pinMode(B_ABAJO, INPUT_PULLUP);
  pinMode(B_ENTER, INPUT_PULLUP);

  pinMode(ledAzul, OUTPUT);
  pinMode(ledVerde, OUTPUT);
  pinMode(ledRojo, OUTPUT);

  digitalWrite(ledRojo, HIGH);
  digitalWrite(ledVerde, LOW);
  digitalWrite(ledAzul, LOW);

  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) return;

  // Registrar peer ACTION
  memcpy(peerInfo.peer_addr, MACACTION, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  if (esp_now_add_peer(&peerInfo) != ESP_OK) return;

  // Registrar peer SPARK
  memcpy(peerInfo.peer_addr, MACSPARK, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  esp_now_add_peer(&peerInfo);

  // Registrar peer SENSE
  memcpy(peerInfo.peer_addr, MACSENSE, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  esp_now_add_peer(&peerInfo);

  // Registrar callback
  esp_now_register_recv_cb(onDataRecv);

  if (!display.begin(0x3C, true)) {
    for (;;)
      ;
  }

  lastEnterState = digitalRead(B_ENTER);

  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(10, 25);
  display.println("Bienvenido ODE");
  display.display();
  delay(2000);

  //Resetear ACTION
  uint8_t msgACTION = 4;
  esp_now_send(MACACTION, &msgACTION, sizeof(msg));
  //Resetear SENSE
  uint8_t msgSense = 8;
  esp_now_send(MACSENSE, &msgSense, sizeof(msg));
  //Resetear SPARK
  uint8_t msgSPARK = 3;
  esp_now_send(MACSPARK, &msgSPARK, sizeof(msg));
}

void loop() {
  if (enMediciones) {
    mostrarMediciones();
  } else if (ActivarFunc) {
    Funcion_Reles();
  } else if (enFuncionSpark) {
    if (Rejugar) {
      mostrarMenuRejugar();
      leerBotonesRejugar();
    } else {
      Funcion_Spark();
    }
  } else if (enFuncionSense) {
    Funcion_Sense();
  } else if (mostrarMenuSense) {
    mostrarMenuSenseFunc();
    leerBotonesSense();
  } else {
    actualizarPantalla();
    leerBotones();
  }
}

// SISTEMA DE DESBLOQUEO --------------------------------------------------
void actualizarDesbloqueos() {
  // 1 victoria -> Automatización
  if (victoriasSpark >= 1 && !funcionAutoDesbloqueada) {
    funcionAutoDesbloqueada = true;
    mostrarMensajeDesbloqueo("AUTO DESBLOQUEADO!");
  }
  
  // 2 victorias -> IoT
  if (victoriasSpark >= 2 && !funcionIoTDesbloqueada) {
    funcionIoTDesbloqueada = true;
    mostrarMensajeDesbloqueo("IoT DESBLOQUEADO!");
  }
}

void mostrarMensajeDesbloqueo(const char* mensaje) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("FELICIDADES!");
  display.setTextSize(2);
  display.setCursor(0, 20);
  display.println(mensaje);
  display.display();
  delay(3000);
}

// FUNCIÓN AUTOMATIZACIÓN -------------------------------------------------
void activarAutomatizacion() {
  if (funcionAutoDesbloqueada && !autoActivado) {
    uint8_t msg = 6;
    esp_now_send(MACSENSE, &msg, sizeof(msg));
    autoActivado = true;
    Serial.println("Automatización ACTIVADA en ODE SENSE");
    digitalWrite(ledAzul, HIGH);
  }
}

void desactivarAutomatizacion() {
  if (autoActivado) {
    uint8_t msg = 6;
    esp_now_send(MACSENSE, &msg, sizeof(msg));
    autoActivado = false;
    Serial.println("Automatización DESACTIVADA en ODE SENSE");
    digitalWrite(ledAzul, LOW);
  }
}

// FUNCIÓN IoT-------------------------------------------------
void activarIoT() {
  if (funcionIoTDesbloqueada && !funcionIoTActiva) {
    uint8_t msg = 7;
    esp_now_send(MACSENSE, &msg, sizeof(msg));
    digitalWrite(ledAzul, HIGH);
    funcionIoTActiva = true;
  }
}

void desactivarIoT() {
  if (funcionIoTActiva) {
    uint8_t msg = 7;
    esp_now_send(MACSENSE, &msg, sizeof(msg));
    digitalWrite(ledAzul, LOW);
    funcionIoTActiva = false;
  }
}

// BOTONES ----------------------------------------------------------------
void leerBotones() {
  unsigned long ahora = millis();

  if (digitalRead(B_ARRIBA) == LOW && (ahora - r_Arriba > delayRebote)) {
    r_Arriba = ahora;
    moverArriba();
  }

  if (digitalRead(B_ABAJO) == LOW && (ahora - r_Abajo > delayRebote)) {
    r_Abajo = ahora;
    moverAbajo();
  }

  int estadoEnter = digitalRead(B_ENTER);
  if (estadoEnter == LOW && lastEnterState == HIGH && (ahora - r_Enter > delayRebote)) {
    r_Enter = ahora;
    moverEnter();
  }
  lastEnterState = estadoEnter;
}

void moverArriba() {
  int maxOpciones;
  if (menuInicial == 0) {
    maxOpciones = CANT_OPCIONES_PRINCIPAL;
  } else {
    int idx = (menuMemoria >= 0) ? menuMemoria : 1;
    maxOpciones = getNumOpciones(idx);
    if (maxOpciones == 0) maxOpciones = 2;
  }
  menuIndex = (menuIndex > 0) ? menuIndex - 1 : maxOpciones - 1;
}

void moverAbajo() {
  int maxOpciones;
  if (menuInicial == 0) {
    maxOpciones = CANT_OPCIONES_PRINCIPAL;
  } else {
    int idx = (menuMemoria >= 0) ? menuMemoria : 1;
    maxOpciones = getNumOpciones(idx);
    if (maxOpciones == 0) maxOpciones = 2;
  }
  menuIndex = (menuIndex < maxOpciones - 1) ? menuIndex + 1 : 0;
}

int getNumOpciones(int odeIndex) {
  int count = 0;
  for (int i = 0; i < 5; i++) {
    if (strlen(subMenus[odeIndex][i]) > 0) count++;
  }
  return count;
}

void moverEnter() {
  if (menuInicial == 0) {
    if (menuIndex == 0) {
      ActivarFunc = true;
      enFuncionReles = true;
    } else if (menuIndex == 1) {
      enFuncionSense = true;
      conectadoSense = false;
      AvisoConexionSense = true;
    } else if (menuIndex == 2) {
      enFuncionSpark = true;
      conectadoSpark = false;
      AvisoConexionSpark = true;
      juegoIniciado = false;
      TerminoJuego = false;
      Rejugar = false;
      NivelJuego = 1;
    }
  } else if (menuInicial == 1) {
    int numOpciones = getNumOpciones(menuMemoria);

    if (menuIndex == numOpciones - 1) {
      if (menuMemoria == 1) {
        desconectarSense();
      } else {
        menuInicial = 0;
        menuIndex = 0;
        menuMemoria = -1;
      }
    } else if (menuMemoria == 1) {
      seleccionarSensorSense();
    }
  }
}

// PANTALLA PRINCIPAL ACTUALIZADA -----------------------------------------
void actualizarPantalla() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);

  if (menuInicial == 0) {
    mostrarMenuPrincipal();
  } else if (menuInicial == 1) {
    mostrarSubMenu(menuMemoria);
  }
  display.display();
}

void mostrarMenuPrincipal() {
  display.setCursor(0, 0);
  display.print("SELECCIONE ODE:");
  
  String odeSenseNombre = "ODE SENSE";
  if (funcionIoTDesbloqueada) {
    odeSenseNombre = "ODE SENSE!!";
  } else if (funcionAutoDesbloqueada) {
    odeSenseNombre = "ODE SENSE!";
  }

  for (int i = 0; i < CANT_OPCIONES_PRINCIPAL; i++) {
    display.setCursor(5, 15 + i * 15);
    display.print((i == menuIndex) ? "> " : "  ");
    
    if (i == 1) {
      display.print(odeSenseNombre);
    } else {
      display.print(menuPrincipal[i]);
    }
  }
}

void mostrarSubMenu(int odeIndex) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);

  display.setCursor(0, 0);
  display.print("ODE Sense");
  display.print(":");

  int numOpciones = getNumOpciones(odeIndex);
  int spacing = 10;

  for (int i = 0; i < numOpciones; i++) {
    display.setCursor(5, 15 + i * spacing);
    display.print((i == menuIndex) ? "> " : "  ");
    display.print(subMenus[odeIndex][i]);
  }
}

// CALLBACK ACTUALIZADO CON DESBLOQUEO -----------------------------------
void onDataRecv(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
  if (len == 1) {
    uint8_t codigo = data[0];
    
    if (codigo == 2) {
      digitalWrite(ledRojo, LOW);
      digitalWrite(ledVerde, HIGH);
      conectado = true;
      tiempoConexion = millis();
    } else if (codigo == 6) {
      digitalWrite(ledRojo, LOW);
      digitalWrite(ledVerde, HIGH);
      conectadoSpark = true;
      AvisoConexionSpark = true;
    } else if (codigo == 7) {
      NivelJuego += 1;
    } else if (codigo == 8) {//ODIE SPARK
      TerminoJuego = true;
      victoriasSpark++;
      actualizarDesbloqueos();
      
      display.clearDisplay();
      display.setTextSize(2);
      display.setCursor(0, 0);
      display.println("VICTORIA!");
      display.setTextSize(1);
      display.setCursor(0, 30);
      display.print("Victorias: ");
      display.println(victoriasSpark);
      display.display();
      delay(3000);
    } else if (codigo == 9) {
      Rejugar = true;
      menuIndex = 0;
      enFuncionSpark = true;
      lastEnterState = digitalRead(B_ENTER);
      r_Enter = millis();
      r_Arriba = r_Abajo = millis();
    } else if (codigo == 13) {
      digitalWrite(ledRojo, LOW);
      digitalWrite(ledVerde, HIGH);
      conectadoSense = true;
      AvisoConexionSense = true;
    }
  } else if (len == sizeof(struct_message)) {
    memcpy(&sensorData, data, sizeof(struct_message));
    ultimaActualizacionMediciones = millis();
  }
}

// FUNCION SENSE ----------------------------------------------------------
void Funcion_Sense() {
  unsigned long ahora = millis();

  if (!conectadoSense && (ahora - lastAttempt > intervalo)) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("Conectando con");
    display.println("ODE SENSE...");
    display.display();

    msg = 1;
    esp_now_send(MACSENSE, &msg, sizeof(msg));
    lastAttempt = ahora;
  }

  if (conectadoSense && AvisoConexionSense) {
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(0, 0);
    display.println("Conexion");
    display.println("exitosa!");
    display.display();
    delay(2000);

    AvisoConexionSense = false;
    
    mostrarMenuSense = true;
    enFuncionSense = false;
    menuIndex = 0;
  }
}

void mostrarMenuSenseFunc() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  
  display.setCursor(0, 0);
  display.println("SENSORES:");
  
  int numOpciones = getNumOpciones(1);
  for (int i = 0; i < numOpciones; i++) {
    display.setCursor(5, 12 + i * 10);
    
    if (i == numOpciones - 1) {
      display.print((i == menuIndex) ? "> " : "  ");
      display.print("Volver");
    } else {
      display.print((i == menuIndex) ? "> " : "  ");
      display.print(subMenus[1][i]);
      
      bool sensorActivo = false;
      switch(i) {
        case 0: sensorActivo = sensorDHT22Activo; break;
        case 1: sensorActivo = sensorTSDActivo; break;
        case 2: sensorActivo = sensorPIRActivo; break;
        case 3: sensorActivo = sensorMSV2Activo; break;
      }
      if (sensorActivo) {
        display.print(" A");
      }
    }
  }
  
  display.display();
}

void leerBotonesSense() {
  unsigned long ahora = millis();

  if (digitalRead(B_ARRIBA) == LOW && (ahora - r_Arriba > delayRebote)) {
    r_Arriba = ahora;
    menuIndex = (menuIndex > 0) ? menuIndex - 1 : getNumOpciones(1) - 1;
  }

  if (digitalRead(B_ABAJO) == LOW && (ahora - r_Abajo > delayRebote)) {
    r_Abajo = ahora;
    menuIndex = (menuIndex < getNumOpciones(1) - 1) ? menuIndex + 1 : 0;
  }

  int estadoEnter = digitalRead(B_ENTER);
  if (estadoEnter == LOW && lastEnterState == HIGH && (ahora - r_Enter > delayRebote)) {
    r_Enter = ahora;
    procesarSeleccionSense();
  }
  lastEnterState = estadoEnter;
}

void procesarSeleccionSense() {
  int numOpciones = getNumOpciones(1);
  
  if (menuIndex == numOpciones - 1) {
    desconectarSense();
  } else {
    seleccionarSensorSense();
  }
}

void seleccionarSensorSense() {
  switch(menuIndex) {
    case 0:
      toggleSensorDHT22();
      break;
    case 1:
      toggleSensorTDS();
      break;
    case 2:
      toggleSensorPIR();
      break;
    case 3:
      toggleSensorMSV2();
      break;
  }
}

void toggleSensorDHT22() {
  sensorDHT22Activo = !sensorDHT22Activo;
  msg = 2;
  esp_now_send(MACSENSE, &msg, sizeof(msg));
  enMediciones = true;
  mostrarMenuSense = false;
}

void toggleSensorTDS() {
  sensorTSDActivo = !sensorTSDActivo;
  msg = 3;
  esp_now_send(MACSENSE, &msg, sizeof(msg));
  enMediciones = true;
  mostrarMenuSense = false;
}

void toggleSensorPIR() {
  sensorPIRActivo = !sensorPIRActivo;
  msg = 4;
  esp_now_send(MACSENSE, &msg, sizeof(msg));
  enMediciones = true;
  mostrarMenuSense = false;
}

void toggleSensorMSV2() {
  sensorMSV2Activo = !sensorMSV2Activo;
  msg = 5;
  esp_now_send(MACSENSE, &msg, sizeof(msg));
  enMediciones = true;
  mostrarMenuSense = false;
}

// PANTALLA DE MEDICIONES ACTUALIZADA -------------------------------------
void mostrarMediciones() {
  unsigned long ahora = millis();
  
  if (ahora - ultimaActualizacionMediciones >= 1000) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    
    int line = 0;
    
    if (sensorDHT22Activo) {
      display.setCursor(0, line * 10);
      display.print("Temp: ");
      display.print(sensorData.temperature, 1);
      display.print("C");
      line++;
      
      display.setCursor(0, line * 10);
      display.print("Hum:  ");
      display.print(sensorData.humidity, 1);
      display.print("%");
      line++;
    }
    
    if (sensorTSDActivo) {
      display.setCursor(0, line * 10);
      display.print("TDS:  ");
      display.print(sensorData.tdsValue, 1);
      display.print("ppm");
      line++;
    }
    
    if (sensorPIRActivo) {
      display.setCursor(0, line * 10);
      display.print("PIR:  ");
      display.print(sensorData.pirState == 1 ? "DETECTADO" : "NO DETECTADO");
      line++;
    }
    
    if (sensorMSV2Activo) {
      display.setCursor(0, line * 10);
      display.print("Suelo:");
      display.print(sensorData.soilMoisture, 1);
      display.print("%");
      line++;
    }
    
    if (autoActivado || funcionIoTActiva) {
      display.setCursor(0, line * 10);
      display.print("FUNC: ");
      if (autoActivado) display.print("AUTO ");
      if (funcionIoTActiva) display.print("IoT");
      line++;
    }
    
    display.setCursor(0, 55);
    if (funcionAutoDesbloqueada || funcionIoTDesbloqueada) {
      display.println("ARRIBA:IoT ABAJO:Auto");
    } else {
      display.println("ENTER: Volver");
    }
    
    display.display();
  }
  
  leerBotonesFuncionesEspeciales();
}

// BOTONES FUNCIONES ESPECIALES EN PANTALLA DE MEDICIONES ----------------
void leerBotonesFuncionesEspeciales() {
  unsigned long ahora = millis();
  
  // BOTÓN ARRIBA - IoT (requiere 2 victorias)
  if (digitalRead(B_ARRIBA) == LOW && (ahora - r_Arriba > delayRebote)) {
    r_Arriba = ahora;
    
    if (funcionIoTDesbloqueada) {
      activarIoT();
      mostrarConfirmacionFuncion("IoT", funcionIoTActiva);
    } else {
      desactivarIoT();
      mostrarMensajeBloqueoTemporal("IoT BLOQUEADO", "Gana 2 en SPARK");
    }
  }
  
  // BOTÓN ABAJO - Automatización (requiere 1 victoria)
  if (digitalRead(B_ABAJO) == LOW && (ahora - r_Abajo > delayRebote)) {
    r_Abajo = ahora;
    
    if (funcionAutoDesbloqueada) {
      if (!autoActivado) {
        activarAutomatizacion();
        mostrarConfirmacionFuncion("AUTO", true);
      } else {
        desactivarAutomatizacion();
        mostrarConfirmacionFuncion("AUTO", false);
      }
    } else {
      mostrarMensajeBloqueoTemporal("AUTO BLOQUEADO", "Gana 1 en SPARK");
    }
  }
  
  int estadoEnter = digitalRead(B_ENTER);
  if (estadoEnter == LOW && lastEnterState == HIGH && (ahora - r_Enter > delayRebote)) {
    r_Enter = ahora;
    enMediciones = false;
    mostrarMenuSense = true;
    lastEnterState = digitalRead(B_ENTER);
  }
  lastEnterState = estadoEnter;
}

void mostrarMensajeBloqueoTemporal(const char* titulo, const char* mensaje) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("FUNCION BLOQUEADA");
  display.setTextSize(2);
  display.setCursor(0, 20);
  display.println(titulo);
  display.setTextSize(1);
  display.setCursor(0, 45);
  display.println(mensaje);
  display.display();
  delay(2000);
  ultimaActualizacionMediciones = 0;
}

void mostrarConfirmacionFuncion(const char* funcion, bool activa) {
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.print(funcion);
  display.println(activa ? " ON" : " OFF");
  display.display();
  delay(1500);
  ultimaActualizacionMediciones = 0;
}

void desconectarSense() {
  if (autoActivado) {
    desactivarAutomatizacion();
  }
  if (funcionIoTActiva) {
    desactivarIoT();
  }
  
  uint8_t msgSense = 8;
  esp_now_send(MACSENSE, &msgSense, sizeof(msgSense));

  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(0, 0);
  display.println("Desconectando");
  display.display();
  delay(1500);

  digitalWrite(ledRojo, HIGH);
  digitalWrite(ledVerde, LOW);
  digitalWrite(ledAzul, LOW);

  conectadoSense = false;
  AvisoConexionSense = true;
  enFuncionSense = false;
  mostrarMenuSense = false;
  enMediciones = false;
  sensorDHT22Activo = false;
  sensorTSDActivo = false;
  sensorPIRActivo = false;
  sensorMSV2Activo = false;

  menuInicial = 0;
  menuIndex = 0;
  menuMemoria = -1;
}

// FUNCION RELES ----------------------------------------------------------
void Funcion_Reles() {
  unsigned long ahora = millis();

  if (!conectado && (ahora - lastAttempt > intervalo)) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("Conectando con ODE ACTION");
    display.display();

    msg = 1;
    esp_now_send(MACACTION, &msg, sizeof(msg));
    lastAttempt = ahora;
  }

  if (conectado && AvisoConexion) {
    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(0, 0);
    display.println("Conexion exitosa");
    display.display();
    delay(1500);
    AvisoConexion = false;
  }

  if (enFuncionReles && conectado) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println(rele1Activo ? "RELE 1 ACTIVO" : "RELE 1 INACTIVO");
    display.setCursor(0, 16);
    display.println(rele2Activo ? "RELE 2 ACTIVO" : "RELE 2 INACTIVO");
    display.display();

    if (digitalRead(B_ARRIBA) == LOW) {
      uint8_t msg = 2;
      esp_now_send(MACACTION, &msg, sizeof(msg));
      delay(200);
      rele1Activo = !rele1Activo;
    }

    if (digitalRead(B_ABAJO) == LOW) {
      uint8_t msg = 3;
      esp_now_send(MACACTION, &msg, sizeof(msg));
      delay(200);
      rele2Activo = !rele2Activo;
    }

    if (digitalRead(B_ENTER) == LOW) {
      uint8_t msg = 4;
      esp_now_send(MACACTION, &msg, sizeof(msg));
      delay(200);

      display.clearDisplay();
      display.setTextSize(2);
      display.setCursor(0, 0);
      display.println("Desconectando");
      display.display();
      delay(1500);

      digitalWrite(ledRojo, HIGH);
      digitalWrite(ledVerde, LOW);
      digitalWrite(ledAzul, LOW);

      enFuncionReles = false;
      ActivarFunc = false;
      conectado = false;
      AvisoConexion = true;
      menuInicial = 0;
      menuIndex = 0;
    }
  }
}

// FUNCION SPARK ----------------------------------------------------------
void Funcion_Spark() {
  unsigned long ahora = millis();

  if (!conectadoSpark && (ahora - lastAttempt > intervalo)) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("Conectando con ODE SPARK");
    display.display();

    msg = 1;
    esp_now_send(MACSPARK, &msg, sizeof(msg));
    lastAttempt = ahora;
  }

  if (conectadoSpark && AvisoConexionSpark) {

    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(0, 0);
    display.println("Conexion exitosa");
    display.display();
    delay(3000);

    display.clearDisplay();
    display.setTextSize(2);
    display.setCursor(0, 0);
    display.println("Iniciando juego");
    display.display();
    delay(2000);

    uint8_t msg = 2;
    esp_now_send(MACSPARK, &msg, sizeof(msg));

    AvisoConexionSpark = false;
    juegoIniciado = true;
  }

  if (juegoIniciado) {
    if (NivelJuego < 6 && !TerminoJuego) {
      display.clearDisplay();
      display.setTextSize(2);
      display.setCursor(20, 25);
      display.print("Nivel ");
      display.println(NivelJuego);
      display.display();
      if (digitalRead(B_ENTER) == LOW) {
        TerminoJuego = true;
      }
    } else {
      display.clearDisplay();
      display.setTextSize(2);
      display.setCursor(20, 25);
      display.print("Juego Terminado");
      display.display();
      delay(3000);

      uint8_t msg = 3;
      esp_now_send(MACSPARK, &msg, sizeof(msg));

      display.clearDisplay();
      display.setTextSize(2);
      display.setCursor(0, 0);
      display.println("Desconectando");
      display.display();
      delay(3000);

      digitalWrite(ledRojo, HIGH);
      digitalWrite(ledVerde, LOW);
      digitalWrite(ledAzul, LOW);

      conectadoSpark = false;
      AvisoConexionSpark = true;
      enFuncionSpark = false;
      juegoIniciado = false;
      TerminoJuego = false;
      menuInicial = 0;
      menuIndex = 0;
      NivelJuego = 1;
    }
  }
}

void mostrarMenuRejugar() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SH110X_WHITE);
  display.setCursor(0, 0);
  display.print("Juego terminado:");

  for (int i = 0; i < 2; i++) {
    display.setCursor(5, 15 + i * 15);
    display.print((i == menuIndex) ? "> " : "  ");
    display.print(subMenus[0][i]);
  }
  display.display();
}

void leerBotonesRejugar() {
  unsigned long ahora = millis();

  if (digitalRead(B_ARRIBA) == LOW && (ahora - r_Arriba > delayRebote)) {
    r_Arriba = ahora;
    menuIndex = (menuIndex > 0) ? menuIndex - 1 : 1;
  }

  if (digitalRead(B_ABAJO) == LOW && (ahora - r_Abajo > delayRebote)) {
    r_Abajo = ahora;
    menuIndex = (menuIndex < 1) ? menuIndex + 1 : 0;
  }

  int estadoEnter = digitalRead(B_ENTER);
  if (estadoEnter == LOW && lastEnterState == HIGH && (ahora - r_Enter > delayRebote)) {
    r_Enter = ahora;

    if (menuIndex == 0) {
      uint8_t msg = 1;
      esp_now_send(MACSPARK, &msg, sizeof(msg));
      Rejugar = false;
      conectadoSpark = true;
      AvisoConexionSpark = true;
      enFuncionSpark = true;
      juegoIniciado = false;
      TerminoJuego = false;
      NivelJuego = 1;
      lastEnterState = digitalRead(B_ENTER);
      r_Enter = millis();
    } else {
      uint8_t msg = 3;
      esp_now_send(MACSPARK, &msg, sizeof(msg));

      display.clearDisplay();
      display.setTextSize(2);
      display.setCursor(0, 0);
      display.println("Desconectando");
      display.display();
      delay(1000);

      digitalWrite(ledRojo, HIGH);
      digitalWrite(ledVerde, LOW);
      digitalWrite(ledAzul, LOW);

      Rejugar = false;
      conectadoSpark = false;
      AvisoConexionSpark = true;
      enFuncionSpark = false;
      juegoIniciado = false;
      TerminoJuego = false;
      NivelJuego = 1;

      menuInicial = 0;
      menuIndex = 0;

      lastEnterState = digitalRead(B_ENTER);
      r_Enter = millis();
      r_Arriba = r_Abajo = millis();
    }
  }
  lastEnterState = estadoEnter;
}