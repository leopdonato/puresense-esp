// Bibliotecas do display
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <SPI.h>

// Biblioteca do DHT11
#include "DHT.h"

// ========================
// Definição dos pinos
// ========================
// Display
#define TFT_CS    5
#define TFT_RST   15
#define TFT_DC    2

#define TFT_SCLK  18
#define TFT_MOSI  23

// DHT11
#define DHTPIN 4
#define DHTTYPE DHT11

// MQ-7
#define MQ7_PIN 34  // pino analógico

// MQ-2
#define MQ2_PIN 35  // pino analógico

// ========================
// Inicializações
// ========================
DHT dht(DHTPIN, DHTTYPE);
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

void setup() {
  Serial.begin(115200);

  // Inicia DHT
  dht.begin();

  // Inicia Display
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(1);
  tft.fillScreen(ST77XX_BLACK);

  // Texto inicial
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(1);
  tft.setCursor(10, 10);
  tft.println("PureSense");
  delay(2000);
}

void loop() {
  // =========================
  // Leitura dos sensores
  // =========================
  float temp = dht.readTemperature();
  float umid = dht.readHumidity();

  if (isnan(umid) || isnan(temp)) {
    Serial.println("Failed to read from DHT sensor!");
  }

  int mq7_value = analogRead(MQ7_PIN);

  if (isnan(mq7_value)) {
    Serial.println("Failed to read from MQ7 sensor!");
  }

  int mq2_value = analogRead(MQ2_PIN);
  if (isnan(mq2_value)) {
    Serial.println("Failed to read from MQ2 sensor!");
  }

  // =========================
  // Conversao dos MQ (bruta)
  // =========================
  float mq7_ppm = map(mq7_value, 0, 4095, 0, 1000); // CO estimado
  float mq2_ppm = map(mq2_value, 0, 4095, 0, 1000); // Gás/fumaça estimado

  // =========================
  // Debug no Serial
  // =========================
  Serial.println("====== Leitura ======");
  Serial.print("Temperatura: "); Serial.print(temp); Serial.println(" ºC");
  Serial.print("Umidade: "); Serial.print(umid); Serial.println(" %");
  Serial.print("CO (MQ7): "); Serial.print(mq7_ppm); Serial.println(" ppm");
  Serial.print("Gases (MQ2): "); Serial.print(mq2_ppm); Serial.println(" ppm");
  Serial.println("=====================");
  Serial.println();

  // =========================
  // Atualiza Display
  // =========================
  tft.fillScreen(ST77XX_BLACK);

  tft.setCursor(5, 5);
  tft.setTextColor(ST77XX_WHITE);
  tft.setTextSize(1);
  tft.println("PureSense \n Monitor Ambiente");

  tft.setTextSize(1);
  tft.setCursor(5, 30);
  tft.print("Temperatura: "); tft.print(temp); tft.println(" C");

  tft.setCursor(5, 45);
  tft.print("Umidade: "); tft.print(umid); tft.println(" %");

  tft.setCursor(5, 60);
  tft.print("CO: "); tft.print(mq7_ppm); tft.println(" ppm");

  tft.setCursor(5, 75);
  tft.print("Gases: "); tft.print(mq2_ppm); tft.println(" ppm");

  delay(300000);
}

