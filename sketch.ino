#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <ESP32Servo.h>

// ----- DISPLAY OLED -----
#define OLED_W 128
#define OLED_H 64
#define OLED_RST -1
Adafruit_SSD1306 tela(OLED_W, OLED_H, &Wire, OLED_RST);

// ----- SERVO -----
#define SERVO_PIN 23
Servo servoMotor;

// ----- DHT22 -----
#define DHTPIN 15
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// ----- MQ-2 -----
#define MQ2_AOUT 34   // Entrada analógica do MQ-2

// ----- LEDS DE TEMPERATURA -----
#define LED_TEMP_BAIXA 12
#define LED_TEMP_ALTA 14

// ----- LEDS DE UMIDADE -----
#define LED_UMID_BAIXA 2
#define LED_UMID_ALTA 5

// ----- BUZZER -----
#define BUZZER 25

// ----- WIFI E MQTT -----
const char* wifiSSID = "Wokwi-GUEST";
const char* wifiPASS = "";
const char* brokerMQTT = "test.mosquitto.org";

WiFiClient wifiClient;
PubSubClient mqtt(wifiClient);

// ----- LIMITES -----
float tempMin = 16.0;
float tempMax = 30.0;
float umidMin = 30.0;
float umidMax = 70.0;

// MQ-2 classificação
int limiteGasBaixo = 300;
int limiteGasMedio = 1200;
int limiteGasAlto  = 2000;
String nivelGas;

// =========================================================
//  WIFI
// =========================================================
void conectarWiFi() {
  Serial.println("Conectando ao WiFi...");
  WiFi.begin(wifiSSID, wifiPASS);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(300);
  }
  Serial.println("\nWiFi conectado!");
}

// =========================================================
//  MQTT
// =========================================================
void reconectarMQTT() {
  while (!mqtt.connected()) {
    Serial.print("MQTT >> Tentando reconectar...");
    if (mqtt.connect("AirQualityNode32")) {
      Serial.println(" conectado!");
    } else {
      Serial.print(" falhou. Codigo: ");
      Serial.println(mqtt.state());
      delay(2000);
    }
  }
}

// =========================================================
//  ALERTA SONORO
// =========================================================
void beepAlerta() {
  tone(BUZZER, 1200);
  delay(150);
  noTone(BUZZER);
  delay(100);
}

// =========================================================
//  RESETAR LEDs
// =========================================================
void resetarLeds() {
  digitalWrite(LED_TEMP_BAIXA, LOW);
  digitalWrite(LED_TEMP_ALTA, LOW);
  digitalWrite(LED_UMID_BAIXA, LOW);
  digitalWrite(LED_UMID_ALTA, LOW);
}

// =========================================================
//  SETUP
// =========================================================
void setup() {
  Serial.begin(115200);

  pinMode(LED_TEMP_BAIXA, OUTPUT);
  pinMode(LED_TEMP_ALTA, OUTPUT);
  pinMode(LED_UMID_BAIXA, OUTPUT);
  pinMode(LED_UMID_ALTA, OUTPUT);  // <<— CORRIGIDO
  pinMode(BUZZER, OUTPUT);

  dht.begin();
  servoMotor.write(0);
  servoMotor.attach(SERVO_PIN, 500, 2400);

  if (!tela.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("Erro no OLED");
    while (1);
  }

  tela.clearDisplay();
  tela.setTextSize(1);
  tela.setTextColor(SSD1306_WHITE);
  tela.setCursor(0, 20);
  tela.println("Monitor Qualidade do Ar");
  tela.display();
  delay(1500);

  resetarLeds();
  conectarWiFi();
  mqtt.setServer(brokerMQTT, 1883);
}

// =========================================================
//  LOOP
// =========================================================
void loop() {

  if (!mqtt.connected()) reconectarMQTT();
  mqtt.loop();

  float temp = dht.readTemperature();
  float hum = dht.readHumidity();
  int mq2Valor = analogRead(MQ2_AOUT);

  if (isnan(temp) || isnan(hum)) {
    Serial.println("Erro ao ler DHT!");
    return;
  }

  resetarLeds();

  // ----------- TEMPERATURA -----------
  String estadoTemp;
  if (temp <= tempMin) {
    estadoTemp = "Baixa";
    digitalWrite(LED_TEMP_BAIXA, HIGH);
  } 
  else if (temp >= tempMax) {
    estadoTemp = "Alta";
    digitalWrite(LED_TEMP_ALTA, HIGH);
  } 
  else {
    estadoTemp = "Normal";
  }

  // ----------- UMIDADE -----------
  String estadoUmid;
  if (hum <= umidMin) {
    estadoUmid = "Baixa";
    digitalWrite(LED_UMID_BAIXA, HIGH);
  } 
  else if (hum >= umidMax) {
    estadoUmid = "Alta";
    digitalWrite(LED_UMID_ALTA, HIGH);
  } 
  else {
    estadoUmid = "Normal";
  }

  // ----------- MQ-2 CLASSIFICAÇÃO -----------
  if (mq2Valor < limiteGasBaixo) nivelGas = "Baixo";
  else if (mq2Valor < limiteGasMedio) nivelGas = "Moderado";
  else if (mq2Valor < limiteGasAlto) nivelGas = "Alto";
  else nivelGas = "Perigoso!";

  // ----------- ACIONAR SERVO POR TEMPERATURA -----------
  if (temp >= tempMax) servoMotor.write(90);
  else servoMotor.write(0);

  // ----------- ALERTA SONORO -----------
  if (temp >= tempMax || hum <= umidMin || hum >= umidMax || nivelGas == "Alto" || nivelGas == "Perigoso!") {
    beepAlerta();
  }

  // ----------- DISPLAY OLED -----------
  tela.clearDisplay();
  tela.setCursor(0, 0);
  tela.println("Qualidade do Ar");
  tela.drawLine(0, 12, OLED_W, 12, SSD1306_WHITE);

  tela.setCursor(0, 16);
  tela.print("Temp: ");
  tela.print(temp);
  tela.print(" C (");
  tela.print(estadoTemp);
  tela.println(")");

  tela.setCursor(0, 28);
  tela.print("Umid: ");
  tela.print(hum);
  tela.print("% (");
  tela.print(estadoUmid);
  tela.println(")");

  tela.setCursor(0, 52);
  tela.print("Nivel Gas: ");
  tela.print(nivelGas);

  tela.display();

  // ----------- SERIAL MONITOR -----------
  Serial.print("Temp: ");
  Serial.print(temp);
  Serial.print("C | Umid: ");
  Serial.print(hum);
  Serial.print("% | Nivel Gas: ");
  Serial.println(nivelGas);

  // ----------- MQTT (sem mq2Valor) -----------
  char msgTemp[10];
  dtostrf(temp, 4, 1, msgTemp);
  mqtt.publish("monitor/ar/temperatura", msgTemp);

  char msgHum[10];
  dtostrf(hum, 4, 1, msgHum);
  mqtt.publish("monitor/ar/umidade", msgHum);

  mqtt.publish("monitor/ar/estadoTemp", estadoTemp.c_str());
  mqtt.publish("monitor/ar/estadoUmid", estadoUmid.c_str());
  mqtt.publish("monitor/ar/nivelGas", nivelGas.c_str());

  delay(1000);
}
