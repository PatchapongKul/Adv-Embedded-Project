#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "secrets.h"

WiFiClient espClient;
PubSubClient client(espClient);

const char* mqtt_server = "broker.netpie.io";

String serverPath = "https://ds.netpie.io/feed/api/v1/datapoints/query";

void postHTTP( void * parameter );
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
  xTaskCreate(postHTTP, "postHTTP", 6000, NULL, 2, NULL);
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

void postHTTP( void * parameter )
{
  HTTPClient http;
  StaticJsonDocument<512> jsonResp;
  StaticJsonDocument<384> jsonPost;
  String stringPost;

  JsonObject start_relative = jsonPost.createNestedObject("start_relative");
  start_relative["value"] = 1;
  start_relative["unit"] = "days";

  JsonObject metrics_0 = jsonPost["metrics"].createNestedObject();
  metrics_0["name"] = clientID;
  metrics_0["tags"]["attr"] = "temp";
  metrics_0["limit"] = 1;

  JsonObject metrics_0_aggregators_0 = metrics_0["aggregators"].createNestedObject();
  metrics_0_aggregators_0["name"] = "first";

  JsonObject metrics_0_aggregators_0_sampling = metrics_0_aggregators_0.createNestedObject("sampling");
  metrics_0_aggregators_0_sampling["value"] = "1";
  metrics_0_aggregators_0_sampling["unit"] = "minutes";

  serializeJson(jsonPost, stringPost);
  // Serial.println(stringPost);

  String authen = "Device " + String(clientID) + ":" + String(token);

  for(;;)
  {
    http.begin(serverPath.c_str());
    http.addHeader("Authorization", authen);
    http.addHeader("Content-Type", "application/json");
    int httpResponseCode = http.POST(stringPost);
    
    if (httpResponseCode > 0) 
    {
      Serial.print("HTTP Response code: ");
      Serial.println(httpResponseCode);
      String payload = http.getString();
      // Serial.println(payload);

      DeserializationError error = deserializeJson(jsonResp, payload);

      if (error) {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.c_str());
        vTaskDelay(1000);
        continue;
      }

      JsonObject queries_0_results_0 = jsonResp["queries"][0]["results"][0];

      int64_t timeUTC = queries_0_results_0["values"][0][0];
      float temp = queries_0_results_0["values"][0][1];

      Serial.print("timestamp: ");
      Serial.print(timeUTC);
      Serial.print("\ttemp: ");
      Serial.println(temp);
    }
    else {
      Serial.print("Error code: ");
      Serial.println(httpResponseCode);
    }
    
    // Serial.println(uxTaskGetStackHighWaterMark(NULL));
    http.end();   // Free resources 
    vTaskDelay(5000);
  }
}