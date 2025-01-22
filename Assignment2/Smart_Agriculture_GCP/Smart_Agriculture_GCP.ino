#include <PubSubClient.h>
#include <WiFi.h>
#include "DHT.h"
#include <ESP32Servo.h>

// Moisture predefined value
int MinMoistureValue = 4095;
int MaxMoistureValue = 1800;
int MinMoisture = 0;
int MaxMoisture = 100;
int Moisture = 0;

// Servo predefined value
Servo servoMotor;

// Wi-FI and MQTT setup
const char* WIFI_SSID = "Jun Lin's HONOR 70"; // Your WiFi SSID
const char* WIFI_PASSWORD = "020519TommyYuan"; // Your WiFi password
const char* MQTT_SERVER = "34.68.255.77"; // Your VM instance public IP address
const char* MQTT_TOPIC = "iot"; // MQTT topic for subscription
const int MQTT_PORT = 1883; // Non-TLS communication port

// MQTT client
char buffer[128] = "";
WiFiClient espClient;
PubSubClient client(espClient);

//Used Pins
const int redLedPin = 10;        // Connect Red LED to A0 pin
const int greenLedPin = 9;       // Connect Green LED to A1 pin
const int moisturePin = A2;      // Middle Maker Port (A2)
const int servoPin = 5;          // Connect servo to A4 pin
const int rainPin = 7;           // Left side Maker Port (A5)
const int relayPin = 39;         // Connect relay ro d39 pin
const int dht11Pin = 42;         // Right side Maker Port

// Define variables to store actuator states
bool redLEDState = false;
bool greenLEDState = false;
bool relayPumpState = false;
int servoAngleState = 0;

//input sensor
#define DHTTYPE DHT11
DHT dht(dht11Pin, DHTTYPE);

//last message time
unsigned long lastMsgTime = 0;

void setup_wifi() {
    delay(10);
    Serial.println();
    Serial.print("Connecting to ");
    Serial.println(WIFI_SSID);

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }

    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
}

void setup() {
    // Initialize serial communication with a higher baud rate
    Serial.begin(115200);
    Serial.setTimeout(1);
    
    // Setup for GCP connection
    setup_wifi();
    client.setServer(MQTT_SERVER, MQTT_PORT);

    // Sensor Initialization
    dht.begin();
    pinMode(rainPin, INPUT);
    pinMode(moisturePin, INPUT);
    pinMode(redLedPin, OUTPUT);
    pinMode(greenLedPin, OUTPUT);
    pinMode(relayPin, OUTPUT);

    // Initialize servo
    servoMotor.attach(servoPin); 
    servoMotor.write(0);
    
    // Set initial states
    digitalWrite(redLedPin, redLEDState);
    digitalWrite(greenLedPin, greenLEDState);
    digitalWrite(relayPin, relayPumpState);
}

void reconnect() {
  while (!client.connected())
  {
    Serial.println("Attempting MQTT connection...");
    if(client.connect("ESP32Client")) {
      Serial.println("Connected to MQTT server");
    }
    else {
      Serial.print("Failed, rc=");
      Serial.print(client.state());
      Serial.println(" Retrying in 5 seconds...");
      delay(5000);
    }
  }
}

void loop() {
  
  
  if(!client.connected()) {
    reconnect();
  }
  client.loop();
  delay(5000);

  // Handle Python's servo control commands
  if (Serial.available() > 0) {
      int angle = Serial.parseInt();  // Read the incoming byte

      String status = Serial.readStringUntil('\n');  // Read the status sent from Python
      status.trim();  // Remove any trailing whitespace or newline characters

      if (status == "Healthy") {
        servoMotor.write(0);  // Servo at 0 degrees
      } else if (status == "Powdery" || status == "Rust") {
        servoMotor.write(90);  // Servo at 90 degrees
        delay(5000);        // Hold position for 5 seconds (adjust as needed)
        servoMotor.write(0);   // Reset to default position
      }
      Serial.println("Condition received: " + status);  // Debug print
      
      // Publish servo state to MQTT
      String commandStr = "{\"Servo\":" + String(angle) + "}";
      client.publish(MQTT_TOPIC, commandStr.c_str());
      
      // Send confirmation back to Python
      Serial.println(angle);
  }

  unsigned long cur = millis();
  if (cur - lastMsgTime > 2000) { // Using 2000ms interval
      lastMsgTime = cur;

      // DHT11 Data 
      float h = dht.readHumidity();
      int t = dht.readTemperature();

      // Initialize JSON payloadObject for DHT11 Sensor data 
      sprintf(buffer, "Temperature: %d", t);
      Serial.println(buffer);
      client.publish(MQTT_TOPIC, buffer);

      sprintf(buffer, "Temperature: %d", h);
      Serial.println(buffer);
      client.publish(MQTT_TOPIC, buffer);

      // Rain sensor data
      int raining = !digitalRead(rainPin);
      sprintf(buffer, "Raining: %d", raining);
      Serial.println(buffer);
      client.publish(MQTT_TOPIC, buffer);
      
      // Moisture sensor data
      int sensorValue = analogRead(moisturePin);
      Moisture = map(sensorValue, MinMoistureValue, MaxMoistureValue, MinMoisture, MaxMoisture);
      sprintf(buffer, "Soil moisture: %d", Moisture);
      Serial.println(buffer);
      client.publish(MQTT_TOPIC, buffer);

      if ((raining == 0) && (Moisture < 20) && (t > 15 && t < 35) && (h < 80)) {
          // Update LED states
          bool newRedLEDState = true;
          bool newGreenLEDState = false;
          bool newRelayPumpState = true;

          // Only update and publish if states have changed
          if (redLEDState != newRedLEDState) {
              redLEDState = newRedLEDState;
              digitalWrite(redLedPin, redLEDState);
              String commandStr = "{\"LED1\":" + String(redLEDState ? "true" : "false") + "}";
              client.publish(MQTT_TOPIC, commandStr.c_str());
          }

          if (greenLEDState != newGreenLEDState) {
              greenLEDState = newGreenLEDState;
              digitalWrite(greenLedPin, greenLEDState);
              String commandStr = "{\"LED2\":" + String(greenLEDState ? "true" : "false") + "}";
              client.publish(MQTT_TOPIC, commandStr.c_str());
          }

          if (relayPumpState != newRelayPumpState) {
              relayPumpState = newRelayPumpState;
              digitalWrite(relayPin, relayPumpState);
              String commandStr = "{\"Relay\":" + String(relayPumpState ? "true" : "false") + "}";
              client.publish(MQTT_TOPIC, commandStr.c_str());
          }
          
      } else {
          // Update LED states for normal conditions
          bool newRedLEDState = false;
          bool newGreenLEDState = true;
          bool newRelayPumpState = false;

          // Only update and publish if states have changed
          if (redLEDState != newRedLEDState) {
              redLEDState = newRedLEDState;
              digitalWrite(redLedPin, redLEDState);
              String commandStr = "{\"LED1\":" + String(redLEDState ? "true" : "false") + "}";
              client.publish(MQTT_TOPIC, commandStr.c_str());
          }

          if (greenLEDState != newGreenLEDState) {
              greenLEDState = newGreenLEDState;
              digitalWrite(greenLedPin, greenLEDState);
              String commandStr = "{\"LED2\":" + String(greenLEDState ? "true" : "false") + "}";
              client.publish(MQTT_TOPIC, commandStr.c_str());
              
          }

          if (relayPumpState != newRelayPumpState) {
              relayPumpState = newRelayPumpState;
              digitalWrite(relayPin, relayPumpState);
              String commandStr = "{\"Relay\":" + String(relayPumpState ? "true" : "false") + "}";
              client.publish(MQTT_TOPIC, commandStr.c_str());
              
          }
      }

      // Debug Output
      Serial.println("\n--- Current States ---");
      Serial.print("Humidity: "); Serial.println(h);
      Serial.print("Temperature: "); Serial.println(t);
      Serial.print("Raining: "); Serial.println(raining);
      Serial.print("Moisture Level: "); Serial.println(Moisture);
      Serial.print("Red LED: "); Serial.println(redLEDState ? "ON" : "OFF");
      Serial.print("Green LED: "); Serial.println(greenLEDState ? "ON" : "OFF");
      Serial.print("Pump: "); Serial.println(relayPumpState ? "ON" : "OFF");
      Serial.println("-------------------\n");
  }
}