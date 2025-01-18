#include "VOneMqttClient.h"
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

// Define start and end angles for synchronized motion 
const int startAngle = 0; 
const int endAngle = 90; 

//define device id
const char* DHT11Sensor = "746feabd-7198-438d-8710-e42a2cb55fcc";   
const char* RainSensor = "aaf2723c-9dfe-4e59-ba0d-675fb074b542";      
const char* MoistureSensor = "23e84acc-fa1a-4a6e-a5e9-92bf40c22e8e";  
const char* RedLED = "935e2084-d94b-42ff-93e2-bb286c10d611";
const char* GreenLED = "b3d0530d-0064-4fbb-b8ae-b2b90e111b98";
const char* RelayWaterPump = "a327624c-8e5f-4418-9038-c48b60b937e5";
const char* Servo = "273c4e20-2313-4d79-af70-e46de89d9f23";

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

// Create an instance of VOneMqttClient
VOneMqttClient voneClient;

//last message time
unsigned long lastMsgTime = 0;

// Helper function to publish actuator state
void publishActuatorState(const char* deviceId, bool state) {
    String commandStr = "{\"state\":" + String(state ? "true" : "false") + "}";
    voneClient.publishActuatorStatusEvent(deviceId, commandStr.c_str(), state);
}

// Helper function for servo state
// void publishServoState(int angle) {
//     String commandStr = "{\"angle\":" + String(angle) + "}";
//     voneClient.publishActuatorStatusEvent(Servo, commandStr.c_str(), state);
// }

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

void triggerActuator_callback(const char* actuatorDeviceId, const char* actuatorCommand) {
    Serial.print("Main received callback: ");
    Serial.print(actuatorDeviceId);
    Serial.print(" : ");
    Serial.println(actuatorCommand);

    String errorMsg = "";

    JSONVar commandObject = JSON.parse(actuatorCommand);
    JSONVar keys = commandObject.keys();

    String key = "";
    for (int i = 0; i < keys.length(); i++) {
        key = (const char*)keys[i];
    }

    if (String(actuatorDeviceId) == RedLED) {
        redLEDState = (bool)commandObject[key];
        digitalWrite(redLedPin, redLEDState);
        Serial.println(redLEDState ? "Red LED ON" : "Red LED OFF");
        voneClient.publishActuatorStatusEvent(actuatorDeviceId, actuatorCommand, true);
    } 
    else if (String(actuatorDeviceId) == GreenLED) {
        greenLEDState = (bool)commandObject[key];
        digitalWrite(greenLedPin, greenLEDState);
        Serial.println(greenLEDState ? "Green LED ON" : "Green LED OFF");
        voneClient.publishActuatorStatusEvent(actuatorDeviceId, actuatorCommand, true);
    } 
    else if (String(actuatorDeviceId) == RelayWaterPump) {
        relayPumpState = (bool)commandObject[key];
        digitalWrite(relayPin, relayPumpState);
        Serial.println(relayPumpState ? "Relay Water Pump ON" : "Relay Water Pump OFF");
        voneClient.publishActuatorStatusEvent(actuatorDeviceId, actuatorCommand, true);
        delay(1000);
    } 
    else if (String(actuatorDeviceId) == Servo) {
        int servoAngle = (int)commandObject[key];
        servoMotor.write(servoAngle);    
        voneClient.publishActuatorStatusEvent(actuatorDeviceId, actuatorCommand, true);//publish actuator status
    }
    else
    {
        Serial.print(" No actuator found : ");
        Serial.println(actuatorDeviceId);
        errorMsg = "No actuator found";
        voneClient.publishActuatorStatusEvent(actuatorDeviceId, actuatorCommand, false);//publish actuator status
    } 
}

void setup() {
    // Initialize serial communication with a higher baud rate
    Serial.begin(115200);
    
    // Setup for V-One connection
    setup_wifi();
    voneClient.setup();

    // Register actuators callback
    voneClient.registerActuatorCallback(triggerActuator_callback);

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

void loop() {
  if (!voneClient.connected()) {
      Serial.println("Lost connection - attempting to reconnect...");
      voneClient.reconnect();
      
      // Publish all device statuses at once
      const char* devices[] = {DHT11Sensor, RainSensor, MoistureSensor, 
                              RedLED, GreenLED, RelayWaterPump, Servo};
      for(const char* device : devices) {
          voneClient.publishDeviceStatusEvent(device, true);
      }
  }
  
  voneClient.loop();

  unsigned long cur = millis();
  if (cur - lastMsgTime > 2000) { // Using 2000ms interval
      lastMsgTime = cur;

      // DHT11 Data 
      float h = dht.readHumidity();
      int t = dht.readTemperature();

      // Initialize JSON payloadObject for DHT11 Sensor data 
      JSONVar payloadObject;
      payloadObject["Humidity"] = h;
      payloadObject["Temperature"] = t;
      voneClient.publishTelemetryData(DHT11Sensor, payloadObject);

      // Rain sensor data
      int raining = !digitalRead(rainPin);
      voneClient.publishTelemetryData(RainSensor, "Raining", raining);
      
      // Moisture sensor data
      int sensorValue = analogRead(moisturePin);
      Moisture = map(sensorValue, MinMoistureValue, MaxMoistureValue, MinMoisture, MaxMoisture);
      voneClient.publishTelemetryData(MoistureSensor, "Soil moisture", Moisture);

      if ((raining == 0) && (Moisture < 20) && (t > 15 && t < 35) && (h < 80)) {
          // Update states
          redLEDState = true;
          greenLEDState = false;
          relayPumpState = true;

          // Set physical pins
          digitalWrite(redLedPin, redLEDState);
          digitalWrite(greenLedPin, greenLEDState);
          digitalWrite(relayPin, relayPumpState);

          // Publish Actuators Telemary Data 
          publishActuatorState(RedLED, redLEDState);
          publishActuatorState(GreenLED, greenLEDState);
          publishActuatorState(RelayWaterPump, relayPumpState);
          
      } else {
          // Update states
          redLEDState = false;
          greenLEDState = true;
          relayPumpState = false;

          // Set physical pins
          digitalWrite(redLedPin, redLEDState);
          digitalWrite(greenLedPin, greenLEDState);
          digitalWrite(relayPin, relayPumpState);

          // Publish Actuators Telemary Data 
          publishActuatorState(RedLED, redLEDState);
          publishActuatorState(GreenLED, greenLEDState);
          publishActuatorState(RelayWaterPump, relayPumpState);
      }

      // Condition to check the python code to ensure that it can call it from python
      if (Serial.available() > 0) {
        String status = Serial.readStringUntil('\n');  // Read status from Python
        
        // Process the received status and control the servo
        if (status == "Healthy") {
          servoMotor.write(0);  // Position servo to 0 degrees
        } else if (status == "Powdery" || status == "Rust") {
          servoMotor.write(90);  // Position servo to 90 degrees
        }
        
        delay(500);  // Small delay for stability
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