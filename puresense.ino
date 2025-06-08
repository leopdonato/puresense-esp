// =========================
// Bibliotecas
// =========================
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>
#include "DHT.h"
#include <Preferences.h>

// ====== Config Wi-Fi ======
const char* ssid = "";
const char* password = "";

// ====== Config AWS ======
const char* aws_endpoint = "xxxxx-ats.iot.sa-east-1.amazonaws.com";
const int port = 8883;
const char* mqtt_topic = "puresense/sensores";

// ====== Certificados AWS (exemplo em texto) ======
// 🔴 RECOMENDADO: Use arquivos externos (.pem) para segurança real.
const char* ca_cert = "-----BEGIN CERTIFICATE-----\n..."; // AmazonRootCA1
const char* client_cert = "-----BEGIN CERTIFICATE-----\n..."; // Cert device
const char* private_key = "-----BEGIN PRIVATE KEY-----\n...";

// // Caminhos para os certificados
// #define CERT_CA "/AmazonRootCA1.pem"
// #define CERT_CLIENT "/cert.pem.crt"
// #define CERT_KEY "/private.pem.key"

// =========================
// Definição dos pinos
// =========================
// Display
#define TFT_CS    5
#define TFT_RST   15
#define TFT_DC    2

#define TFT_SCLK  18
#define TFT_MOSI  23

// DHT11
#define DHTPIN 4
#define DHTTYPE DHT11

// MQ-7 e MQ-2
#define MQ7_PIN 34  // pino analógico
#define MQ2_PIN 35  // pino analógico

// =========================
// Inicializações
// =========================
DHT dht(DHTPIN, DHTTYPE);
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

Preferences prefs;

// ====== Objetos Wi-Fi e MQTT ======
WiFiClientSecure net;
PubSubClient client(net);

// Resistência de carga (verifique seu módulo, geralmente é 10K)
const float RL_MQ7 = 10.0;
const float RL_MQ2 = 10.0;

// Valores de calibração
float R0_MQ7 = 0;
float R0_MQ2 = 0;

// =========================
// Setup
// =========================
void setup() {
  Serial.begin(115200);
  connectWiFi();
  connectAWS();

  // Inicia sensores
  dht.begin();

  // Inicia Display
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1);
  tft.fillScreen(ST77XX_BLACK);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(1);
  tft.setCursor(10, 10);
  tft.println("PureSense");
  delay(2000);

  // Calibrar sensores MQ
  calibrarMQ();
}

// =========================
// Loop principal
// =========================
void loop() {
  if (!client.connected()) connectAWS();
  client.loop();

  // Leitura DHT11
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();

  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("Erro na leitura do DHT11");
  }

  // Leitura MQ-7
  float adc_mq7 = analogRead(MQ7_PIN);
  float volt_mq7 = adc_mq7 * (3.3 / 4095.0);
  float Rs_mq7 = (3.3 - volt_mq7) * RL_MQ7 / volt_mq7;
  float ratio_mq7 = Rs_mq7 / R0_MQ7;
  float co_ppm_mq7 = pow((ratio_mq7 / 99.042), (1.0 / -1.518)); // Exemplo curva CO

  // Leitura MQ-2
  float adc_mq2 = analogRead(MQ2_PIN);
  float volt_mq2 = adc_mq2 * (3.3 / 4095.0);
  float Rs_mq2 = (3.3 - volt_mq2) * RL_MQ2 / volt_mq2;
  float ratio_mq2 = Rs_mq2 / R0_MQ2;
  float gases_ppm_mq2 = pow((ratio_mq2 / 9.8), (1.0 / -2.0)); // Curva genérica fumaça

  // Debug Serial
  Serial.println("====== Leitura ======");
  Serial.print("Temperatura: "); Serial.print(temperature); Serial.println(" C");
  Serial.print("Umidade: "); Serial.print(humidity); Serial.println(" %");
  Serial.print("CO (MQ-7): "); Serial.print(co_ppm_mq7); Serial.println(" ppm");
  Serial.print("Gases (MQ-2): "); Serial.print(gases_ppm_mq2); Serial.println(" ppm");
  Serial.println("=====================");

  // Display
  atualizarDisplay(temperature, humidity, co_ppm_mq7, gases_ppm_mq2);

  // AWS
  sendToAWS(temperature, humidity, co_ppm_mq7, gases_ppm_mq2);

  delay(300000); // 5 minutos entre leituras
}

// =========================
// Função para calibração
// =========================
void calibrarMQ() {
  prefs.begin("calibracao", false);

  R0_MQ7 = prefs.getFloat("R0_MQ7", 0);
  R0_MQ2 = prefs.getFloat("R0_MQ2", 0);

  if (R0_MQ7 == 0 || R0_MQ2 == 0) {
    Serial.println("⚙️ Calibrando sensores MQ...");
    tft.fillScreen(ST77XX_BLACK);
    tft.setCursor(5, 30);
    tft.println("Calibrando MQ...");
    delay(5000);  // Estabiliza

    // MQ-7
    float adc7 = analogRead(MQ7_PIN);
    float volt7 = adc7 * (3.3 / 4095.0);
    float Rs7 = (3.3 - volt7) * RL_MQ7 / volt7;
    R0_MQ7 = Rs7;

    // MQ-2
    float adc2 = analogRead(MQ2_PIN);
    float volt2 = adc2 * (3.3 / 4095.0);
    float Rs2 = (3.3 - volt2) * RL_MQ2 / volt2;
    R0_MQ2 = Rs2;

    // Salvar na flash
    prefs.putFloat("R0_MQ7", R0_MQ7);
    prefs.putFloat("R0_MQ2", R0_MQ2);

    Serial.print("R0 MQ-7 calibrado: "); Serial.println(R0_MQ7);
    Serial.print("R0 MQ-2 calibrado: "); Serial.println(R0_MQ2);
  } else {
    Serial.println("📦 Valores de R0 carregados da memoria.");
    Serial.print("R0 MQ-7: "); Serial.println(R0_MQ7);
    Serial.print("R0 MQ-2: "); Serial.println(R0_MQ2);
  }

  prefs.end();
}

// =========================
// Atualizar Display
// =========================
void atualizarDisplay(float temperature, float humidity, float co_ppm_mq7, float gases_ppm_mq2) {
  tft.fillScreen(ST77XX_BLACK);

  tft.setCursor(5, 5);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(1);
  tft.println("PureSense Monitor");

  tft.setCursor(5, 25);
  tft.print("Temp: "); tft.print(temperature); tft.println(" C");

  tft.setCursor(5, 40);
  tft.print("Umid: "); tft.print(humidity); tft.println(" %");

  tft.setCursor(5, 55);
  tft.print("CO: "); tft.print(co_ppm_mq7); tft.println(" ppm");

  tft.setCursor(5, 70);
  tft.print("Gases: "); tft.print(gases_ppm_mq2); tft.println(" ppm");
}

void connectWiFi() {
  WiFi.begin(ssid, password);
  Serial.print("Conectando ao Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
  }
  Serial.println("\n✅ Wi-Fi conectado.");
}

void connectAWS() {
  net.setCACert(ca_cert);
  net.setCertificate(client_cert);
  net.setPrivateKey(private_key);
  client.setServer(aws_endpoint, port);
  while (!client.connected()) {
    Serial.print("Conectando à AWS IoT...");
    if (client.connect("ESP32_PureSense")) {
      Serial.println("✅ Conectado!");
    } else {
      Serial.print("Falha: ");
      Serial.println(client.state());
      delay(2000);
    }
  }
}

void sendToAWS(float temperature, float humidity, float co_ppm_mq7, float gases_ppm_mq2) {
  String payload = "{";
  payload += "\"temperature\":" + String(temperature, 1) + ",";
  payload += "\"humidity\":" + String(humidity, 1) + ",";
  payload += "\"co\":" + String(co_ppm_mq7, 2) + ",";
  payload += "\"gases\":" + String(gases_ppm_mq2, 2);
  payload += "}";

  client.publish(mqtt_topic, payload.c_str());
  Serial.println("📤 Enviado para AWS IoT:");
  Serial.println(payload);
}
