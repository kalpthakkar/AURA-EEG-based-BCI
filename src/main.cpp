#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
 
#define WIFISSID "Aura" // Put your WifiSSID here
#define PASSWORD "qwerty123" // Put your wifi password here
#define TOKEN "BBFF-RTWfptOrz5RQmofGsJQqGxoA3o4VDI" // Put your Ubidots' TOKEN
#define MQTT_CLIENT_NAME "auraproject" // MQTT client Name, please enter your own 8-12 alphanumeric character ASCII string; 
                                           //it should be a random and unique ascii string and different from all other devices
 
/****************************************
 * Define Constants
 ****************************************/
#define VARIABLE_LABEL "sensor" // Assing the variable label
#define DEVICE_LABEL "esp32" // Assig the device label
 
#define SENSOR A0 // Set the A0 as SENSOR
/*
Connection
38Pin ESP32: 	{GND:GND | 3.3V:3V3 | OUTPUT:SP | LO-:SD3 | LO+:SD2 | SDN:NULL}
Normal ESP32: 	{GND:GND | 3.3V:3V3 | OUTPUT:VP | LO-:D3 or D35 | LO+:D2 or D34 | SDN:NULL}
*/[

// #define LED_INDICATOR 2

/* Handler for Light indicator status updating FreeRTOS Task */
TaskHandle_t LightControlTask;
const int ledPin = 21;  // 21 corresponds to GPIO21 (G21 in ESP32)
/* setting PWM properties */
const int freq = 5000;
const int ledChannel = 0;
const int resolution = 8;
 
char mqttBroker[]  = "industrial.api.ubidots.com";
char payload[100];
char topic[150];
// Space to store values to send
char str_sensor[10];
 
/****************************************
 * Auxiliar Functions
 ****************************************/
WiFiClient ubidots;
PubSubClient client(ubidots);
 
void callback(char* topic, byte* payload, unsigned int length) {
  char p[length + 1];
  memcpy(p, payload, length);
  p[length] = NULL;
  Serial.write(payload, length);
  Serial.println(topic);
}

void LightController( void * pvParameters ){      // Light_Indicator handler using FreeRTOS 
  Serial.print("Light Controller Task is running on core ");
  Serial.println(xPortGetCoreID());
  /*
  *  => Reseting state (Glow-Low)
  *  => WiFi Connected & Working (ON)
  */
  for(;;){
    vTaskDelay(100);
    if (WiFi.status() != WL_CONNECTED) {
      // Serial.println("LightMode: Glow");
      // increase the LED brightness
      for(int dutyCycle = 0; dutyCycle <= 255; dutyCycle+=3){   
        // changing the LED brightness with PWM
        ledcWrite(ledChannel, dutyCycle);
        delay(15);
      }
      // decrease the LED brightness
      for(int dutyCycle = 255; dutyCycle >= 0; dutyCycle-=3){
        // changing the LED brightness with PWM
        ledcWrite(ledChannel, dutyCycle);   
        delay(15);
      }
      vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
    else {
      // Serial.print("LightMode: ON");
      ledcWrite(ledChannel, 255);
      vTaskDelay(2000 / portTICK_PERIOD_MS);
    }

  }
}

 
void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.println("Attempting MQTT connection...");
    
    // Attemp to connect
    if (client.connect(MQTT_CLIENT_NAME, TOKEN, "")) {
      Serial.println("Connected");
    } else {
      Serial.print("Failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 2 seconds");
      // Wait 2 seconds before retrying
      delay(2000);
    }
  }
}
 
/****************************************
 * Main Functions
 ****************************************/
void setup() {
  Serial.begin(9600);
  WiFi.begin(WIFISSID, PASSWORD);
 
  pinMode(SENSOR, INPUT); // Assigning the pin as INPUT

  // configure LED PWM functionalitites
  ledcSetup(ledChannel, freq, resolution);
  // attach the channel to the GPIO to be controlled
  ledcAttachPin(ledPin, ledChannel);

  xTaskCreatePinnedToCore(
    LightController,      /* Function to implement the task */
    "Light control",      /* Name of the task */
    1000,                 /* Stack size in words */
    NULL,                 /* Task input parameter */
    1,                    /* Priority of the task */
    &LightControlTask,    /* Task handle. */
    0);                   /* Core where the task should run */
 
  Serial.println();
  Serial.print("Waiting for WiFi...");
  
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  
  Serial.println("");
  Serial.println("WiFi Connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  client.setServer(mqttBroker, 1883);
  client.setCallback(callback);  
}
 
void loop() {
  if (!client.connected()) {
    reconnect();
  }
 
  sprintf(topic, "%s%s", "/v1.6/devices/", DEVICE_LABEL);
  sprintf(payload, "%s", ""); // Cleans the payload
  sprintf(payload, "{\"%s\":", VARIABLE_LABEL); // Adds the variable label
  
  float sensor = analogRead(SENSOR); 
  Serial.print("Sensor: ");
  Serial.println(sensor);
  
  /* 4 is mininum width, 2 is precision; float value is copied onto str_sensor*/
  dtostrf(sensor, 4, 2, str_sensor);
  
  sprintf(payload, "%s {\"value\": %s}}", payload, str_sensor); // Adds the value
  Serial.println("Publishing data to Ubidots Cloud");
  client.publish(topic, payload);
  client.loop();
  delay(500);
}