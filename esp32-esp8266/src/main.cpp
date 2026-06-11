#include <Arduino.h>
#include "esp_task_wdt.h"
#include <SPI.h>
#include <SD.h>
#include <WiFi.h>
#include <PubSubClient.h>

/*
ligacao map gol mi
pino 1 terra
pino 2 sinal temperatura do ar
pino 3 +5v
pino 4 sinal MAP

pinos leds:
d21
d22
d23

pinos sensor:
d32
d33

pinos pot:
d34
d35
*/

#define WDT_TIMEOUT 5  // segundos

const char* WIFI_SSID = "uaifai-tiradentes";
const char* WIFI_PASSWORD = "bemvindoaocesar";
const char* MQTT_SERVER = "172.26.70.113";
const int MQTT_PORT = 1883;
const char* MQTT_TOPIC = "esp32/trafego/dados";

WiFiClient espClient;
PubSubClient mqttClient(espClient);

/* ================== SD (VSPI) ================== */
const int SD_CS   = 5;
const int SD_SCK  = 18;
const int SD_MISO = 19;
const int SD_MOSI = 23;

bool sdOk = false;

/* ================== PINOS ================== */
const int pinLedIni       = 22;   // LED verde virou LED de status (GPIO23 agora é MOSI do SD)
const int pinLedVermelho  = 21;   // opcional: pode usar como “pulso do eixo”, ou deixar desconectado

const int pinTemp         = 33;
const int pinPress        = 32;

const int pinPotVerde     = 35;   // vai controlar apenas x
const int pinPotVermelho  = 34;   // pode ficar sem usar (ou reaproveitar depois)

/* ================== CONSTANTES ================== */
const float VMIN_OK = 1.8;
const float VMAX_OK = 2.6;

const unsigned long TEMPO_INICIAL_MS   = 5000;
const unsigned long TEMPO_ESTAVEL_MS   = 10000;
const unsigned long TEMPO_COLETA_MS    = 30000;

const unsigned long BLINK_RAPIDO_MS     = 100;
const unsigned long TEMPO_FORA_FAIXA_MS = 500;

const unsigned long INTERVALO_MEDIA_MS  = 300000; // 5 minutos
const unsigned long SERIAL_INTERVAL_MS  = 1000;

const unsigned long TEMPO_VMIN_OPERACAO_MS = 30000; // 30s abaixo de VMIN

/* evento eixo */
const float HISTERESIS_EVENTO = 0.010;            // 10 mV
const unsigned long MIN_INTERVAL_EVENTO_MS = 80;  // anti-repique simples (ajuste se necessário)

/* ================== VARIÁVEIS ================== */
enum Estado {
  INIT,
  AGUARDANDO_PRESSAO,
  ESTAVEL,
  COLETA_MEDIA,
  OPERACAO
};

Estado estadoAtual = INIT;

unsigned long tempoEstado = 0;
unsigned long ultimoBlinkRapido = 0;
unsigned long ultimoSerial = 0;
unsigned long ultimoRecalculoMedia = 0;

/* timers separados */
unsigned long tempoForaFaixa = 0;        // USADO APENAS EM ESTAVEL
unsigned long tempoVminOperacao = 0;     // USADO APENAS EM OPERACAO

float somaPressao = 0.0;
unsigned long contLeituras = 0;
float pressaoMedia = 0.0;

/* controle do pisca duplo */
int blinkDuploContador = 0;
bool blinkDuploAtivo = false;

/* contagem de eixos */
unsigned long eixos = 0;
bool acimaX = false;
unsigned long ultimoEvento = 0;

/* ================== FUNÇÕES ================== */

float lerPressaoV() {
  return analogRead(pinPress) * (3.3 / 4095.0);
}

float lerPotVerdeV() {
  return analogRead(pinPotVerde) * (3.3 / 4095.0);
}

const char* nomeEstado() {
  switch (estadoAtual) {
    case INIT: return "INIT";
    case AGUARDANDO_PRESSAO: return "AGUARDANDO";
    case ESTAVEL: return "ESTAVEL";
    case COLETA_MEDIA: return "COLETA_MEDIA";
    case OPERACAO: return "OPERACAO";
  }
  return "DESCONHECIDO";
}

void sdLogLine(const String& line) {
  if (!sdOk) return;
  File f = SD.open("/log.csv", FILE_APPEND);
  if (!f) { sdOk = false; return; }
  f.println(line);
  f.flush();   // mais seguro pra campo (desligamento abrupto)
  f.close();
}

void sdLogHeaderIfNeeded() {
  if (!sdOk) return;
  if (!SD.exists("/log.csv")) {
    File f = SD.open("/log.csv", FILE_WRITE);
    if (f) {
      // adicionamos potV e potN (normalizado) no log
      f.println("ms,estado,pressaoV,pmedia,potV,potN,x,eixos,evento");
      f.close();
    }
  }
}

void setupWiFi() {
  Serial.print("\nConectando ao WiFi: ");
  Serial.println(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void reconnectMQTT() {
  if (WiFi.status() == WL_CONNECTED && !mqttClient.connected()) {
    Serial.print("Tentando conectar ao MQTT...");
    if (mqttClient.connect("ESP32_Trafego")) {
      Serial.println("conectado ao MQTT!");
    } else {
      Serial.print("falhou, rc=");
      Serial.println(mqttClient.state());
    }
  }
}

void publishMqtt(const char* estado, float pressaoV, float pressaoMedia, float potVerdeV, float potVerdeN, float x, unsigned long eixos, const char* evento) {
  if (mqttClient.connected()) {
    String payload = "{\"estado\":\"" + String(estado) + "\",\"pressaoV\":" + String(pressaoV, 3) + 
                     ",\"pmedia\":" + String(pressaoMedia, 3) + ",\"potV\":" + String(potVerdeV, 3) + 
                     ",\"potN\":" + String(potVerdeN, 3) + ",\"x\":" + String(x, 3) + 
                     ",\"eixos\":" + String(eixos) + ",\"evento\":\"" + String(evento) + "\"}";
    mqttClient.publish(MQTT_TOPIC, payload.c_str());
  }
}

/* ================== SETUP ================== */

void setup() {
  Serial.begin(115200);

  pinMode(pinLedIni, OUTPUT);
  pinMode(pinLedVermelho, OUTPUT);

  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  digitalWrite(pinLedIni, HIGH);
  tempoEstado = millis();

  Serial.println("Sistema iniciado");

  /* ===== SD ===== */
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  if (SD.begin(SD_CS, SPI)) {
    sdOk = true;
    sdLogHeaderIfNeeded();
    sdLogLine(String(millis()) + ",BOOT,,,,,,0,BOOT");
    Serial.println("SD OK");
  } else {
    sdOk = false;
    Serial.println("SD FALHOU");
  }

  /* ===== WATCHDOG ===== */
  esp_task_wdt_init(WDT_TIMEOUT, true);
  esp_task_wdt_add(NULL);

  setupWiFi();
  mqttClient.setServer(MQTT_SERVER, MQTT_PORT);
}

/* ================== LOOP ================== */

void loop() {
  if (WiFi.status() == WL_CONNECTED) {
    if (!mqttClient.connected()) {
      reconnectMQTT();
    }
    mqttClient.loop();
  }

  unsigned long agora = millis();

  float pressaoV = lerPressaoV();

  // lê potenciômetro UMA vez por loop (pra log/serial e pra x)
  float potVerdeV = lerPotVerdeV();     // 0–3.3 V
  float potVerdeN = potVerdeV / 3.3;    // 0–1

  /* x (apenas na OPERACAO faz sentido) */
  float x = 0.0;
  if (estadoAtual == OPERACAO) {
    //x = pressaoMedia + potVerdeN * 0.25;
    x = pressaoMedia + 0.03;
  }

  /* ===== SERIAL + SD a cada 1s ===== */
  if (agora - ultimoSerial >= SERIAL_INTERVAL_MS) {
    ultimoSerial = agora;

    Serial.print("Estado: ");
    Serial.print(nomeEstado());
    Serial.print(" | Pressao: ");
    Serial.print(pressaoV, 3);
    Serial.print(" V | Pmedia: ");
    Serial.print(pressaoMedia, 3);

    // sempre mostra o potenciômetro
    Serial.print(" | PotV: ");
    Serial.print(potVerdeV, 3);
    Serial.print(" V | PotN: ");
    Serial.print(potVerdeN, 3);

    if (estadoAtual == OPERACAO) {
      Serial.print(" | x: ");
      Serial.print(x, 3);
      Serial.print(" | eixos: ");
      Serial.print(eixos);
    }
    Serial.println();

    // log contínuo 1 Hz
    String line = String(agora) + "," + nomeEstado() + "," +
                  String(pressaoV, 3) + "," + String(pressaoMedia, 3) + "," +
                  String(potVerdeV, 3) + "," + String(potVerdeN, 3) + "," +
                  (estadoAtual == OPERACAO ? String(x, 3) : "") + "," +
                  String(eixos) + ",SAMPLE";
    sdLogLine(line);
    publishMqtt(nomeEstado(), pressaoV, pressaoMedia, potVerdeV, potVerdeN, x, eixos, "SAMPLE");
  }

  /* ===== QUEDA ABAIXO DE VMIN EM OPERAÇÃO (30s) ===== */
  if (estadoAtual == OPERACAO) {
    if (pressaoV < VMIN_OK) {
      if (tempoVminOperacao == 0) tempoVminOperacao = agora;

      if (agora - tempoVminOperacao >= TEMPO_VMIN_OPERACAO_MS) {
        Serial.println("Pressao abaixo de VMIN por 30s! Reiniciando ciclo...");
        sdLogLine(String(agora) + "," + nomeEstado() + "," + String(pressaoV, 3) + "," +
                  String(pressaoMedia, 3) + "," + String(potVerdeV, 3) + "," + String(potVerdeN, 3) + "," +
                  String(x, 3) + "," + String(eixos) + ",VMIN_30S_RESET");
        publishMqtt(nomeEstado(), pressaoV, pressaoMedia, potVerdeV, potVerdeN, x, eixos, "VMIN_30S_RESET");

        estadoAtual = AGUARDANDO_PRESSAO;
        tempoEstado = agora;
        tempoVminOperacao = 0;
        digitalWrite(pinLedIni, LOW);
        acimaX = false;
        esp_task_wdt_reset();
        return;
      }
    } else {
      tempoVminOperacao = 0;
    }
  }

  switch (estadoAtual) {

    case INIT:
      if (agora - tempoEstado >= TEMPO_INICIAL_MS) {
        digitalWrite(pinLedIni, LOW);
        estadoAtual = AGUARDANDO_PRESSAO;
      }
      break;

    case AGUARDANDO_PRESSAO:
      if (pressaoV >= VMIN_OK && pressaoV <= VMAX_OK) {
        digitalWrite(pinLedIni, HIGH);
        tempoEstado = agora;
        estadoAtual = ESTAVEL;
      } else {
        if (agora - ultimoBlinkRapido >= BLINK_RAPIDO_MS) {
          digitalWrite(pinLedIni, !digitalRead(pinLedIni));
          ultimoBlinkRapido = agora;
        }
      }
      break;

    case ESTAVEL:
      if (pressaoV < VMIN_OK || pressaoV > VMAX_OK) {

        if (tempoForaFaixa == 0) tempoForaFaixa = agora;

        if (agora - tempoForaFaixa >= TEMPO_FORA_FAIXA_MS) {
          estadoAtual = AGUARDANDO_PRESSAO;
          tempoForaFaixa = 0;
        }

      } else {
        tempoForaFaixa = 0;

        if (agora - tempoEstado >= TEMPO_ESTAVEL_MS) {
          estadoAtual = COLETA_MEDIA;
          tempoEstado = agora;
          somaPressao = 0;
          contLeituras = 0;
        }
      }
      break;

    case COLETA_MEDIA:
      somaPressao += pressaoV;
      contLeituras++;

      if (agora - tempoEstado >= TEMPO_COLETA_MS) {
        pressaoMedia = somaPressao / contLeituras;

        blinkDuploAtivo = true;
        blinkDuploContador = 0;
        ultimoBlinkRapido = agora;

        estadoAtual = OPERACAO;
        ultimoRecalculoMedia = agora;

        acimaX = false;
        ultimoEvento = 0;

        Serial.print("Nova Pmedia: ");
        Serial.println(pressaoMedia, 3);

        sdLogLine(String(agora) + ",OPERACAO,," + String(pressaoMedia, 3) + "," +
                  String(potVerdeV, 3) + "," + String(potVerdeN, 3) + ",," +
                  String(eixos) + ",NEW_PMEDIA");
        publishMqtt("OPERACAO", pressaoV, pressaoMedia, potVerdeV, potVerdeN, x, eixos, "NEW_PMEDIA");
      }
      break;

    case OPERACAO: {

      /* pisca duplo ao entrar em operacao */
      if (blinkDuploAtivo) {
        if (agora - ultimoBlinkRapido >= BLINK_RAPIDO_MS) {
          digitalWrite(pinLedIni, !digitalRead(pinLedIni));
          ultimoBlinkRapido = agora;
          blinkDuploContador++;
          if (blinkDuploContador >= 4) {
            digitalWrite(pinLedIni, LOW);
            blinkDuploAtivo = false;
          }
        }
      }

      /* recalcula pressão média a cada 5 minutos */
      if (agora - ultimoRecalculoMedia >= INTERVALO_MEDIA_MS) {
        Serial.println("Recalibrando pressao media...");
        sdLogLine(String(agora) + "," + nomeEstado() + "," + String(pressaoV, 3) + "," +
                  String(pressaoMedia, 3) + "," + String(potVerdeV, 3) + "," + String(potVerdeN, 3) + "," +
                  String(x, 3) + "," + String(eixos) + ",RECALIBRAR");
        publishMqtt(nomeEstado(), pressaoV, pressaoMedia, potVerdeV, potVerdeN, x, eixos, "RECALIBRAR");

        estadoAtual = COLETA_MEDIA;
        tempoEstado = agora;
        somaPressao = 0;
        contLeituras = 0;
        esp_task_wdt_reset();
        return;
      }

      /* ===== DETECÇÃO DE EIXO (evento) ===== */
      // arm/disarm com histerese e intervalo mínimo
      if (!acimaX && pressaoV > x + HISTERESIS_EVENTO) {
        if (agora - ultimoEvento >= MIN_INTERVAL_EVENTO_MS) {
          eixos++;
          ultimoEvento = agora;

          // opcional: piscar o vermelho como “marca de eixo”
          digitalWrite(pinLedVermelho, HIGH);

          sdLogLine(String(agora) + "," + nomeEstado() + "," + String(pressaoV, 3) + "," +
                    String(pressaoMedia, 3) + "," + String(potVerdeV, 3) + "," + String(potVerdeN, 3) + "," +
                    String(x, 3) + "," + String(eixos) + ",AXLE");
          publishMqtt(nomeEstado(), pressaoV, pressaoMedia, potVerdeV, potVerdeN, x, eixos, "AXLE");
        }
        acimaX = true;
      }

      if (acimaX && pressaoV < x - HISTERESIS_EVENTO) {
        acimaX = false;
        digitalWrite(pinLedVermelho, LOW);
      }

      break;
    }
  }

  esp_task_wdt_reset();
}
