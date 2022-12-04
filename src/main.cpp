#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "secrets.h"

WiFiClient espClient;
PubSubClient client(espClient);

// NETPIE Configuration
const char* mqtt_server = "broker.netpie.io";
String serverPath = "https://ds.netpie.io/feed/api/v1/datapoints/query";
const int mqtt_port = 1883;
long lastMsg = 0; char msg[50]; int value = 0;

void sendPIR( void * parameter );
void reconnect();

void setup() {
  Serial.begin(115200);

  // WiFi Connection
  WiFi.begin(ssid, password);
  Serial.println("Connecting to WiFi");
  while ( WiFi.status() != WL_CONNECTED )
  {
    delay(100);
  }
  Serial.println("WiFi Connected");

  // MQTT Connection
  Serial.println("connecting to MQTT");
  client.setServer(mqtt_server, mqtt_port);
  client.connect(clientID, token, secret);
  delay(100);

  xTaskCreate(sendPIR, "sendPIR", 4000, NULL, 1, NULL);
}

void loop() {
}

void sendPIR( void * parameter )
{
  for(;;)
  {
    int sensorValue = (random(0,2));
    Serial.print("Sensor Value: ");
    Serial.println(sensorValue);

    String sent_data = "{\"data\":{\"PIR\":";
    sent_data += String(sensorValue);
    sent_data += "}}";

    if (sent_data.length() > 1)
    {
      while (!client.connected()) 
      {
        reconnect();
      }
      
      client.publish("@shadow/data/update", sent_data.c_str());
      Serial.print("Sent data: ");
      Serial.println(sent_data);
    }
    vTaskDelay(1000/portTICK_PERIOD_MS);
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(clientID, token, secret)) {
      Serial.println("connected");
      // client.subscribe("esp32/output");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}