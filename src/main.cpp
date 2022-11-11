#include <Arduino.h>
#include <WiFi.h>
#include "secrets.h"
#include <PubSubClient.h>

WiFiClient espClient;
PubSubClient client(espClient);

const char* mqtt_server = "broker.netpie.io";
long lastMsg = 0;
char msg[50];
int value = 0;

void send2Server( void * parameter );
void reconnect();

void setup()
{
  Serial.begin(115200);

  WiFi.begin(ssid, password);
  Serial.println("Connecting to WiFi");
  while ( WiFi.status() != WL_CONNECTED )
  {
    vTaskDelay(100/portTICK_PERIOD_MS);
  }
  Serial.println("WiFi Connected");
  
  client.setServer(mqtt_server, 1883);

  xTaskCreate(send2Server, "send2Server", 4000, NULL, 1, NULL);
}
  
void loop() 
{
  client.loop(); 
}

void send2Server( void * parameter )
{
  for(;;)
  {
    float temp_data = random(300, 350) / 10.0;
    String sent_data = "{\"data\":{\"temp\":";
    
    sent_data += String(temp_data);
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
    
    vTaskDelay(2000/portTICK_PERIOD_MS);
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
