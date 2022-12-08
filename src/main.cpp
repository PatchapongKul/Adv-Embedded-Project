#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "secrets.h"

#define LEDpin 2
#define PIRpin 5
#define HoldOnMillis 2000
#define SentIntervalMillis 10000

WiFiClient espClient;
PubSubClient client(espClient);

hw_timer_t *timer0 = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
portMUX_TYPE synch = portMUX_INITIALIZER_UNLOCKED;

// NETPIE Configuration
const char* mqtt_server = "broker.netpie.io";
const int mqtt_port = 1883;
long lastMsg = 0; char msg[50]; int value = 0;

void sendPIR( void * parameter );
void reconnect();
void callback(char* topic, byte* payload, unsigned int length);
void IRAM_ATTR PIRtrig(void);
void IRAM_ATTR onTimer();

bool PIRdetected = false, stateChange = false;

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
  reconnect();

  pinMode(LEDpin, OUTPUT);
  pinMode(PIRpin, INPUT);
  attachInterrupt(PIRpin, PIRtrig, HIGH);

  timer0 = timerBegin(0, 8000, true);
  timerAttachInterrupt(timer0, &onTimer, true);
  timerAlarmWrite(timer0, HoldOnMillis * 10, false);
  timerAlarmEnable(timer0);

  xTaskCreate(sendPIR, "sendPIR", 8000, NULL, 1, NULL);
}

void loop() 
{
  if (client.connected()) {
    client.loop();
  } else {
    reconnect();
  }
}

void callback(char* topic, byte* payload, unsigned int length) 
{
  String msg;
  for (int i = 0; i < length; i++) {
    msg = msg + (char)payload[i];
  }
  
  if (String(topic) == "@msg/led") {
    if (msg == "on") {
      digitalWrite(LEDpin, LOW);
    } else if (msg == "off") {
      digitalWrite(LEDpin, HIGH);
    }
  }
}

void sendPIR( void * parameter )
{
  long lastSentMilllis = 0, CurrentMillis;

  for(;;)
  {
    CurrentMillis = millis();
    if ((CurrentMillis - lastSentMilllis) > SentIntervalMillis || stateChange)
    {
      // Serial.print("Sensor Value: ");
      // Serial.println(PIRdetected);

      String sent_data = "{\"data\":{\"PIR\":";
      sent_data += String(PIRdetected);
      sent_data += ",\"LED\":";
      sent_data += String(digitalRead(LEDpin));
      sent_data += "}}";
        
      while (!client.connected()) {
        reconnect();
      }
      
      if (client.publish("@shadow/data/update", sent_data.c_str())) {
        lastSentMilllis = CurrentMillis;  // reset timer
        stateChange = false;  // reset sent flag
      }
      Serial.print("Sent data: ");
      Serial.println(sent_data);
      // Serial.println(uxTaskGetStackHighWaterMark(NULL));
    }
    vTaskDelay(100/portTICK_PERIOD_MS);
  }
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect(clientID, token, secret)) {
      Serial.println("connected");
      client.subscribe("@msg/led");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 2 seconds");
      // Wait 2 seconds before retrying
      vTaskDelay(2000/portTICK_PERIOD_MS);
    }
  }
}

void IRAM_ATTR PIRtrig(void)
{
  portENTER_CRITICAL(&synch);
  PIRdetected = true;
  stateChange = true;
  digitalWrite(2, HIGH);
  timerRestart(timer0);
  timerAlarmEnable(timer0);
  portEXIT_CRITICAL(&synch); 
}

void IRAM_ATTR onTimer()
{
  portENTER_CRITICAL_ISR(&timerMux);
  PIRdetected = false;
  stateChange = true;
  digitalWrite(2, LOW);
  timerAlarmDisable(timer0);
  portEXIT_CRITICAL_ISR(&timerMux);
}
