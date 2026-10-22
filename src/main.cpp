#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DHT.h>
#include <LiquidCrystal_I2C.h>

ESP8266WebServer server(80);

const char *ssid = "";
const char *password = "";

enum States
{
  CONNECTING,
  RUNNING,
  FAILED
};

States cur_state = CONNECTING;

unsigned long last_time_connecting = 0;
unsigned long startAttemptTime = 0;

#define DHT_pin D7
#define DHT_type DHT11

DHT dht(DHT_pin, DHT_type);

LiquidCrystal_I2C display(0x27, 16, 2);

void setup()
{
  Serial.begin(115200);
  delay(1000);

  Serial.print("\nConnecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  // Фіксуємо час початку спроби підключення
  startAttemptTime = millis();
  last_time_connecting = millis();

  dht.begin();
  display.init();
  display.backlight();
}

void loop()
{
  unsigned long cur_time = millis();

  switch (cur_state)
  {
  case CONNECTING:
    // Виводимо крапку кожні 500 мс без блокування
    if (cur_time - last_time_connecting >= 500)
    {
      last_time_connecting = cur_time;
      Serial.print(".");
    }

    // Перевіряємо, чи з'явилося підключення
    if (WiFi.status() == WL_CONNECTED)
    {
      Serial.println("");
      Serial.println("WiFi connected successfully!");
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());

      server.begin();
      Serial.println("HTTP server started");

      cur_state = RUNNING; // Переходимо до штатного режиму роботи
    }
    // Якщо минуло більше 30 секунд — переходимо до стану помилки
    else if (cur_time - startAttemptTime >= 30000)
    {
      Serial.println("");
      Serial.println("Connection FAILED! Timeout reached.");
      cur_state = FAILED;
      startAttemptTime = cur_time; // Зберігаємо час для паузи перед рестартом
    }
    break;

  case RUNNING:
  {
    // Штатна робота вебсервера має оброблятися миттєво
    server.handleClient();

    // Опціонально: якщо зв'язок втрачено під час роботи
    if (WiFi.status() != WL_CONNECTED)
    {
      Serial.println("WiFi connection lost! Reconnecting...");
      WiFi.reconnect();
      cur_state = CONNECTING;
      startAttemptTime = cur_time;
    }

    // Оновлюємо дані з датчика та екран раз на 2 секунди (2000 мс)
    static unsigned long last_dht_time = 0;
    if (cur_time - last_dht_time >= 2000)
    {
      last_dht_time = cur_time;

      short temp = dht.readTemperature();
      short hum = dht.readHumidity();

      // Перевірка на помилки читання датчика
      if (!isnan(temp) && !isnan(hum))
      {
        display.setCursor(0, 0);
        display.print("Temp: ");
        display.print(temp);
        display.print(" C   "); // Пробіли щоб затирати старі символи

        display.setCursor(0, 1);
        display.print("Humidity: ");
        display.print(hum);
        display.print(" %   ");
      }
    }

    break;
  }

  case FAILED:
    // Чекаємо 5 секунд перед перезавантаженням без використання delay()
    if (cur_time - startAttemptTime >= 5000)
    {
      Serial.println("Restarting ESP8266...");
      ESP.restart();
    }
    break;

  default:
    break;
  }
}