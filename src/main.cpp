#include "secrets.h"
#include "DHT.h"
#include <WiFiClientSecure.h>
#include <MQTTClient.h>
#include <ArduinoJson.h>
#include "WiFi.h"

// DHT wiring notes at the bottom
#define DHTPIN 4     // Digital pin connected to the DHT sensor
// DHTTYPE:
// - DHT11 : DHT 11
// - DHT22 : DHT 22 - AM2302, AM2321
// - DHT21 : DHT 21 - AM2301
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);
WiFiClientSecure net = WiFiClientSecure();
MQTTClient client = MQTTClient(256);

uint8_t numPublishFails = 0;
uint8_t publishFailsAlertLevel = 10;
#define LEDPIN 15

void connectWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.println("Connecting to Wi-Fi");

  while (WiFi.status() != WL_CONNECTED){
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.print("Connected to ");
  Serial.println(WIFI_SSID);
}

void connectAWS() {
  // https://github.com/aws-samples/aws-iot-esp32-arduino-examples/blob/master/examples/basic-pubsub/basic-pubsub.ino

  // Configure WiFiClientSecure to use the AWS IoT device credentials
  net.setCACert(AWS_CERT_CA);
  net.setCertificate(AWS_CERT_CRT);
  net.setPrivateKey(AWS_CERT_PRIVATE);

  // Connect to the MQTT broker on the AWS endpoint we defined earlier
  client.begin(AWS_IOT_ENDPOINT, 8883, net);

  Serial.print("Connecting to AWS IOT");
  while (!client.connect(THINGNAME)) {
    Serial.println("Retrying AWS IOT connection...");
    delay(1000);
  }

  if(!client.connected()) {
    Serial.println("AWS IoT Timeout!");
    return;
  }

  Serial.println("AWS IoT Connected!");
}

bool publishMessage() {
  StaticJsonDocument<200> doc; // https://arduinojson.org/v6/api/staticjsondocument/

  doc["sensor"] = "DHT22";

  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)

  bool isFahrenheit = true; // false == Celsius
  float temp = dht.readTemperature(isFahrenheit);
  if (isnan(temp)) {
    Serial.println("Error reading temp");
    return false;
  }
  doc["tempf"] = temp;

  float humidity = dht.readHumidity();
  if (!isnan(humidity)) {
    doc["humidity"] = humidity;
  }

  char jsonBuffer[512];
  serializeJson(doc, jsonBuffer); // print to client

  std::string str;
  str = "home/thing/";
  str += THINGNAME;
  str += "/climate";
  const char *topic = str.c_str();
  bool result = client.publish(topic, jsonBuffer);
  Serial.print(jsonBuffer);
  Serial.print(" Success: ");
  Serial.println(result);
  return result;
}

void handlePublishStatus(bool isSuccess) {
  numPublishFails = isSuccess ? 0 : std::max(numPublishFails++, publishFailsAlertLevel);

  if (numPublishFails < publishFailsAlertLevel) {
    // turn LED off
    digitalWrite(LEDPIN, LOW);
  } else {
    // turn LED onn
    digitalWrite(LEDPIN, HIGH);
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("Initiating temperature monitor");

  pinMode(LEDPIN, OUTPUT);
  connectWifi();
  connectAWS();

  dht.begin();
}

void loop() {
  bool isPublishSuccess = publishMessage();
  handlePublishStatus(isPublishSuccess);
  client.loop();
  for (int d = 0; d < 30; d++) {
    delay(1000);
    client.loop();
  }
  // delay(30000);
}



/*
void printStuff() {
  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  float h = dht.readHumidity();
  // Read temperature as Celsius (the default)
  float t = dht.readTemperature();
  // Read temperature as Fahrenheit (isFahrenheit = true)
  float f = dht.readTemperature(true);

  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(t) || isnan(f)) {
    Serial.println(F("Failed to read from DHT sensor!"));
    return;
  }

  // Compute heat index in Fahrenheit (the default)
  float hif = dht.computeHeatIndex(f, h);
  // Compute heat index in Celsius (isFahreheit = false)
  float hic = dht.computeHeatIndex(t, h, false);

  Serial.print(F("Humidity: "));
  Serial.print(h);
  Serial.print(F("%  Temperature: "));
  Serial.print(t);
  Serial.print(F("°C "));
  Serial.print(f);
  Serial.print(F("°F  Heat index: "));
  Serial.print(hic);
  Serial.print(F("°C "));
  Serial.print(hif);
  Serial.println(F("°F"));
}
*/

// DHT Wiring Notes:
// Connect pin 1 (on the left) of the sensor to +5V
// NOTE: If using a board with 3.3V logic like an Arduino Due connect pin 1
// to 3.3V instead of 5V!
// Connect pin 2 of the sensor to whatever your DHTPIN is
// Connect pin 4 (on the right) of the sensor to GROUND
// Connect a 10K resistor from pin 2 (data) to pin 1 (power) of the sensor