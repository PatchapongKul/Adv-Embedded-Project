#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <AES.h>
#include <Crypto.h>
#include "secrets.h"

WiFiClient espClient;
PubSubClient client(espClient);
AES128 aes128;

union tempMSG
{
  float temp;
  byte msg[4];
};

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

  xTaskCreate(send2Server, "send2Server", 6000, NULL, 1, NULL);
  xTaskCreate(postHTTP, "postHTTP", 6000, NULL, 2, NULL);
}
  
void loop() 
{
  client.loop(); 
}

void send2Server( void * parameter )
{
  tempMSG encrypTemp;
  byte cypherByte[16] = {0};
  String cypherString;
  String sentData;

  for(;;)
  {
    encrypTemp.temp = random(300, 350) / 10.0;
    
    aes128.setKey(key, 16);
    aes128.encryptBlock(cypherByte, encrypTemp.msg);
    
    cypherString = "";
    for(uint8_t i = 0; i < sizeof(cypherByte); i++) {
      cypherString += (cypherByte[i] < 0x10 ? "0" : "") + String(cypherByte[i], HEX);
    }

    StaticJsonDocument<96> dataJSON;
    JsonObject data = dataJSON.createNestedObject("data");
    data["temp"] = encrypTemp.temp;
    data["temp_encryp"] = cypherString;

    sentData = "";
    serializeJson(dataJSON, sentData);
    
    while (!client.connected()) {
      reconnect();
    }
    
    client.publish("@shadow/data/update", sentData.c_str());
    Serial.print("Sent data: ");
    Serial.println(sentData);

    // Serial.println(uxTaskGetStackHighWaterMark(NULL));
    vTaskDelay(5000/portTICK_PERIOD_MS);
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
  StaticJsonDocument<768> jsonResp;
  StaticJsonDocument<384> jsonPost;
  String stringPost;

  tempMSG decrypTemp;
  byte tempEncrypByte[16] = {0};
  aes128.setKey(key, 16);   // Setting Key for AES

  JsonObject start_relative = jsonPost.createNestedObject("start_relative");
  start_relative["value"] = 1;
  start_relative["unit"] = "days";

  JsonObject metrics_0 = jsonPost["metrics"].createNestedObject();
  metrics_0["name"] = clientID;

  JsonArray metrics_0_tags_attr = metrics_0["tags"].createNestedArray("attr");
  metrics_0_tags_attr.add("temp");
  metrics_0_tags_attr.add("temp_encryp");
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

      JsonObject queries_0_results_1 = jsonResp["queries"][0]["results"][1];
      const char* cypherChar = queries_0_results_1["values"][0][1];

      Serial.print("timestamp: ");
      Serial.print(timeUTC);
      Serial.print("\ttemp: ");
      Serial.print(temp);
      Serial.print("\ttempEencrypChar: ");
      Serial.println(cypherChar);

      String tmpStr = String(cypherChar);
      byte tmpByte[2] = {0};
      for (uint8_t i = 0, k = 0; i < tmpStr.length(); i++)
      {
        if (tmpStr[i] >= '0' && tmpStr[i] <= '9') {
          tmpByte[k++] = tmpStr[i] - '0';
        }
        else if (tmpStr[i] >= 'a' && tmpStr[i] <= 'f') {
          tmpByte[k++] = tmpStr[i] - 'a' + 10;
        }
        if (k > 1) {
          tempEncrypByte[i/2] = (tmpByte[0] << 4) | tmpByte[1];
          memset(tmpByte, 0, 2);
          k = 0;
        }
      }

      // Serial.print("tempEencrypByte: ");
      // for(uint8_t i = 0; i < sizeof(tempEncrypByte); i++) {
      //   Serial.print((tempEncrypByte[i] < 0x10 ? "0":"") + String(tempEncrypByte[i], HEX));
      // }
      // Serial.println();

      aes128.decryptBlock(decrypTemp.msg, tempEncrypByte);

      Serial.print("Decrypted Temp: ");
      Serial.println(decrypTemp.temp);
    }
    else {
      Serial.print("Error code: ");
      Serial.println(httpResponseCode);
    }
    
    // Serial.println(uxTaskGetStackHighWaterMark(NULL));
    http.end();   // Free resources 
    vTaskDelay(10000);
  }
}
