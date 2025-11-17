
//AUTORES: LSSV - BSSC
//PROPOSITO: Juego de luces con secuencia aleatoria 
//UNIVERSIDAD DE SAN BUENAVENTURA CALI
//SISTEMA ODIE, ODIE SPARK 

#include <esp_now.h>
#include <WiFi.h>

// ---------------- CONFIGURACIÓN PINES----------------
const int NUM_LEDS = 4;
const int ledPins[NUM_LEDS] = { 18, 5, 4, 2 };
const int buttonPins[NUM_LEDS] = { 32, 33, 26, 27 };
const int buzzerPin = 14;
//DEFINICION DE NOTAS 
#define NOTE_C4 262
#define NOTE_E4 330
#define NOTE_G4 392
#define NOTE_C5 523

int currentLed = 0;
int buttonPressCount = 0;
int currentLevel = 1;
unsigned long levelStartTime;
bool iniciarJuego = false;
bool awaitingSelection = false; // espera tras game over o victoria

const int levelTargets[5] = { 20, 30, 35, 40, 50 };
const unsigned long levelTimes[5] = {
  30000,  // Nivel 1
  60000,  // Nivel 2
  60000,  // Nivel 3
  60000,  // Nivel 4
  60000   // Nivel 5
};

// ---------------- MAC DEL MAESTRO ----------------
uint8_t masterAddress[] = { 0xCC, 0xDB, 0xA7, 0x3D, 0xF6, 0xD4 };
esp_now_peer_info_t peerInfo;

// ---------------- Flags / debounce ----------------
volatile uint8_t pendingCmd = 0; // se establece en el callback, se procesa en loop
unsigned long lastButtonDebounce = 0;
const unsigned long buttonDebounceDelay = 150;

// ---------------- SETUP ----------------
void setup() {
  pinMode(buzzerPin, OUTPUT);
  digitalWrite(buzzerPin, LOW);

  for (int i = 0; i < NUM_LEDS; i++) {
    pinMode(ledPins[i], OUTPUT);
    digitalWrite(ledPins[i], LOW);
  }
  for (int i = 0; i < NUM_LEDS; i++) {
    pinMode(buttonPins[i], INPUT_PULLUP);
  }

  // mejor semilla
  randomSeed((uint32_t)esp_random());

  // WiFi + ESP-NOW
  WiFi.mode(WIFI_STA);
  if (esp_now_init() != ESP_OK) {
    return;
  }

  memcpy(peerInfo.peer_addr, masterAddress, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    // handle if needed
  }

  // CALLBACK: solo guarda la orden en pendingCmd (nada de delays ni restart aquí)
  esp_now_register_recv_cb([](const esp_now_recv_info_t *info, const uint8_t *data, int len) {
    if (len == 1) {
      pendingCmd = data[0];
    }
  });

  
  digitalWrite(ledPins[1], HIGH);
}

// ---------------- LOOP ----------------
void loop() {
  // Procesar comandos recibidos por ESP-NOW (se ejecutan EN EL LOOP, no en el callback)
  if (pendingCmd != 0) {
    uint8_t cmd = pendingCmd;
    pendingCmd = 0;

    if (cmd == 1) { // Maestro pide conectar/handshake
      digitalWrite(ledPins[1], LOW);
      digitalWrite(ledPins[0], HIGH);

      uint8_t msg = 6; // confirmación
      esp_now_send(masterAddress, &msg, sizeof(msg));
    }
    else if (cmd == 2) { // Maestro pide iniciar juego
      iniciarJuego = true;
      awaitingSelection = false;
      startLevel(1);
    }
    else if (cmd == 3) { // Maestro pide reiniciar la placa
      // reproducion del tono de error y reinicio controlado
      tone(buzzerPin, NOTE_C5, 400);
      delay(450);
      tone(buzzerPin, NOTE_C4, 600);
      delay(650);
      noTone(buzzerPin);
      esp_restart(); // reinicio cuando el maestro lo solicita
    }
  }

  // Si estamos esperando selección (tras game over/victoria), no procesamos el juego
  if (!iniciarJuego) {
    // solo idle - esperaremos instrucciones del maestro (cmd 1 o 2)
    delay(10);
    return;
  }

  // Lógica del juego cuando está activo
  unsigned long currentTime = millis();

  // tiempo agotado?
  if (currentTime - levelStartTime > levelTimes[currentLevel - 1]) {
    gameOver();
    return;
  }

  // lectura con debounce del botón asociado al LED actual
  int buttonState = digitalRead(buttonPins[currentLed]);
  if (buttonState == LOW && (millis() - lastButtonDebounce) > buttonDebounceDelay) {
    lastButtonDebounce = millis();

    // esperar liberación para evitar múltiples cuentas por rebote
    while (digitalRead(buttonPins[currentLed]) == LOW) {
      delay(10);
    }

    // procesamos el acierto
    digitalWrite(ledPins[currentLed], LOW);
    buttonPressCount++;

    if (buttonPressCount >= levelTargets[currentLevel - 1]) {
      uint8_t msg = 7; // nivel completado
      esp_now_send(masterAddress, &msg, sizeof(msg));
      successSequence();

      if (currentLevel < 5) {
        startLevel(currentLevel + 1);
      } else {
        victoryMelody();
        delay(2000);
        endGame();
      }
      return;
    }

    // seleccionar un LED distinto al actual
    int nextLed;
    int prev = currentLed;
    do {
      nextLed = random(NUM_LEDS);
    } while (nextLed == prev);

    currentLed = nextLed;
    digitalWrite(ledPins[currentLed], HIGH);
    delay(150); // pequeña pausa para evitar lecturas muy rápidas
  }
}

// ---------------- FUNCIONES AUXILIARES ----------------

void victoryMelody() {
  int melody[] = { NOTE_C4, NOTE_E4, NOTE_G4, NOTE_C5, NOTE_G4, NOTE_C5 };
  int noteDurations[] = { 300, 300, 300, 500, 300, 600 };

  for (int i = 0; i < 6; i++) {
    tone(buzzerPin, melody[i], noteDurations[i]);
    delay(noteDurations[i] * 1.1);
    noTone(buzzerPin);
  }
}

void startLevel(int level) {

  currentLevel = level;
  buttonPressCount = 0;
  levelStartTime = millis();

  tone(buzzerPin, NOTE_C4, 300);
  delay(250);
  tone(buzzerPin, NOTE_E4, 300);
  delay(250);
  tone(buzzerPin, NOTE_G4, 400);
  delay(300);
  noTone(buzzerPin);

  for (int t = 0; t < 4; t++) {
    for (int i = 0; i < NUM_LEDS; i++) digitalWrite(ledPins[i], HIGH);
    delay(150);
    for (int i = 0; i < NUM_LEDS; i++) digitalWrite(ledPins[i], LOW);
    delay(150);
  }

  int prevLed = currentLed;
  do {
    currentLed = random(NUM_LEDS);
  } while (currentLed == prevLed);

  digitalWrite(ledPins[currentLed], HIGH);
}

void successSequence() {
  tone(buzzerPin, 1000, 200);
  delay(240);
  tone(buzzerPin, 1500, 200);
  delay(240);
  noTone(buzzerPin);

  for (int t = 0; t < 4; t++) {
    for (int i = 0; i < NUM_LEDS; i++) digitalWrite(ledPins[i], HIGH);
    delay(150);
    for (int i = 0; i < NUM_LEDS; i++) digitalWrite(ledPins[i], LOW);
    delay(150);
  }
}

void gameOver() {
  tone(buzzerPin, NOTE_C5, 300);
  delay(350);
  tone(buzzerPin, NOTE_C4, 500);
  delay(500);
  noTone(buzzerPin);

  // Encender LED0 mientras se muestra el submenú
  for (int t = 0; t < 3; t++) {
    for (int i = 0; i < NUM_LEDS; i++) digitalWrite(ledPins[i], HIGH);
    delay(150);
    for (int i = 0; i < NUM_LEDS; i++) digitalWrite(ledPins[i], LOW);
    delay(150);
  }
  for (int i = 0; i < NUM_LEDS; i++) digitalWrite(ledPins[i], LOW);
  digitalWrite(ledPins[0], HIGH);

  uint8_t msg = 9;
  esp_now_send(masterAddress, &msg, sizeof(msg));

  iniciarJuego = false;
  awaitingSelection = true;
}

void endGame() {

  uint8_t msg = 8;
  esp_now_send(masterAddress, &msg, sizeof(msg));
  // apagar LEDs y quedamos a la espera del maestro (no reiniciamos automáticamente)

  iniciarJuego = false;
  awaitingSelection = true;
}
