#include <NTPClient.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <esp_task_wdt.h>
#include "secrets.h"
#include <Wire.h>
#include <Arduino_HTS221.h>
#include <PubSubClient.h>

#define WDT_TIMEOUT 5

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

TwoWire sensor(1);
bool a = sensor.setPins(41, 40);
HTS221Class HTS1(sensor);

QueueHandle_t tempQueue, serverQueue;

hw_timer_t *timer0 = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
portMUX_TYPE synch = portMUX_INITIALIZER_UNLOCKED;

WiFiClient espClient;
PubSubClient client(espClient);

const char* mqtt_server = "192.168.71.100";
long lastMsg = 0;
char msg[50];
int value = 0;

void updateNTP( void * parameter );
void displaySerial( void * parameter );
void readTemp( void * parameter );
void displayTemp( void * parameter );
void send2Server( void * parameter );
void IRAM_ATTR buttonPress(void);
void IRAM_ATTR onTimer();
void reconnect();
void callback(char* topic, byte* message, unsigned int length);

void setup(){
  esp_task_wdt_init(WDT_TIMEOUT, true); // Initialize the Task Watchdog Timer
  // esp_task_wdt_add(NULL);               // Subscribe current task to the Task Watchdog Timer

  Serial.begin(115200);

  pinMode(2, OUTPUT);
  digitalWrite(2, HIGH);
  pinMode(0, INPUT_PULLUP);
  attachInterrupt(0, buttonPress, FALLING);

  timer0 = timerBegin(0, 80, true);
  timerAttachInterrupt(timer0, &onTimer, true);
  timerAlarmWrite(timer0, 1000000, true);
  timerAlarmEnable(timer0);

  WiFi.begin(ssid, password);
  Serial.println("Connecting to WiFi");
  while ( WiFi.status() != WL_CONNECTED )
  {
    vTaskDelay(100/portTICK_PERIOD_MS);
    esp_task_wdt_reset();
  }
  Serial.println("WiFi Connected");
  
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  if (!HTS1.begin()) {
    Serial.println("Failed to initialize humidity temperature sensor!");
    while (1);
  }
  Serial.println("Initialize Temperature Sensor ..... Done!");

  tempQueue = xQueueCreate( 10, sizeof(float));
  serverQueue = xQueueCreate( 10, sizeof(float));
  
  timeClient.begin();
  timeClient.update();
  timeClient.setTimeOffset(7*3600); //In Thailand (UTC+7)
  Serial.println("NTP updated");

  // disconnect from WiFi but not erase AP config
  // WiFi.disconnect(false, true);
  Serial.println("WiFi Disconnected");
  
  xTaskCreate(
    updateNTP,    /* Task function. */
    "updateNTP",  /* name of task. */
    4000,        /* Stack size of task */
    NULL,         /* parameter of the task */
    5,            /* priority of the task */
    NULL);        /* Task handle to keep track of created task */

  xTaskCreate(
    displaySerial,    /* Task function. */
    "displaySerial",  /* name of task. */
    4000,            /* Stack size of task */
    NULL,             /* parameter of the task */
    4,                /* priority of the task */
    NULL);            /* Task handle to keep track of created task */
  
  xTaskCreate(
    readTemp,    /* Task function. */
    "readTemp",  /* name of task. */
    4000,            /* Stack size of task */
    NULL,             /* parameter of the task */
    3,                /* priority of the task */
    NULL);            /* Task handle to keep track of created task */

  xTaskCreate(
    displayTemp,    /* Task function. */
    "displayTemp",  /* name of task. */
    4000,            /* Stack size of task */
    NULL,             /* parameter of the task */
    2,                /* priority of the task */
    NULL);            /* Task handle to keep track of created task */

  xTaskCreate(
    send2Server,    /* Task function. */
    "send2Server",  /* name of task. */
    4000,            /* Stack size of task */
    NULL,             /* parameter of the task */
    1,                /* priority of the task */
    NULL);            /* Task handle to keep track of created task */
}
  
void loop() 
{
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  if (timeClient.getHours() == 18)  // if it's 6 PM
  {
    Serial.println("ESP is going to sleep");
    esp_sleep_enable_timer_wakeup(12*60*60*1e6);  // sleep for 12 hours
    esp_task_wdt_delete(NULL);
    esp_deep_sleep_start();
  }
  // esp_task_wdt_reset();
}

void updateNTP( void * parameter )
{
  // Subscribe current task to the Task Watchdog Timer
  esp_task_wdt_add(NULL);

  uint32_t currentMillis = 0, lastUpdateMillis = 0;

  /* loop forever */
  for(;;)
  {
    currentMillis = millis();
    if (currentMillis - lastUpdateMillis > 60000)
    {
      // WiFi.disconnect();
      WiFi.begin(ssid, password);
      Serial.println("Connecting to WiFi");
      
      while ( WiFi.status() != WL_CONNECTED )
      {
        vTaskDelay(100/portTICK_PERIOD_MS);
        esp_task_wdt_reset();
      }
      Serial.println("WiFi Connected");

      timeClient.update();
      Serial.println("NTP updated");
      
      // disconnect from WiFi but not erase AP config
      // WiFi.disconnect(false, true);
      Serial.println("WiFi Disconnected");

      lastUpdateMillis = currentMillis;
    }
    vTaskDelay(100/portTICK_PERIOD_MS);
    esp_task_wdt_reset();
  }
}

void displaySerial( void * parameter )
{
  // Subscribe current task to the Task Watchdog Timer
  esp_task_wdt_add(NULL);
  
  /* loop forever */
  for(;;)
  {
    Serial.println(timeClient.getFormattedTime());
    vTaskDelay(1000/portTICK_PERIOD_MS);
    esp_task_wdt_reset();
  }
}

void readTemp( void * parameter )
{
  for(;;)
  {
  float temperature = HTS1.readTemperature();

  // String temp_json = "{\"temperature\":" + String(temperature) + ",\"time\":'" + formattedTime + "\"}";
  // String temp_json = String(temperature) + "," + formattedTime;
  // Serial.println(temp_json);

  xQueueSend( tempQueue, ( void * ) &temperature, ( TickType_t ) 0 );
  vTaskDelay(6000/portTICK_PERIOD_MS);
  }
}

void displayTemp( void * parameter )
{
  for(;;)
  {
    float display_temp;
    if( tempQueue != NULL )
      {
        while( xQueueReceive( tempQueue, &( display_temp ),( TickType_t ) 10 ) == pdPASS )
        {
          xQueueSend( serverQueue, ( void * ) &display_temp, ( TickType_t ) 0 );

          Serial.print("Data from queue ");
          Serial.println(display_temp);
          // Serial.println("°C");
        }
      }
    vTaskDelay(20000/portTICK_PERIOD_MS);
  }
}

void send2Server( void * parameter )
{
  for(;;)
  {
    float temp_data;
    String sent_data = "[";
    // char tempString[30];
    if( serverQueue != NULL )
    {
      while( xQueueReceive( serverQueue, &( temp_data ),( TickType_t ) 10 ) == pdPASS )
        {
          
          // Serial.print("Data is sending ---> ");
          
          sent_data += String(temp_data);
          sent_data += ',';
          // dtostrf(temp_data, 1, 2, tempString);
          // Serial.println(tempString);
          
        }
    }
    if (sent_data.length() > 1){
      sent_data = sent_data.substring(0,sent_data.length()-1);
      sent_data += ']';

      Serial.print("Sent data");
      Serial.println(sent_data);
      while (!client.connected()) 
      {
        reconnect();
      }

      // sent_data.toCharArray(tempString,30);
      client.publish("esp32/temperature", sent_data.c_str());
    
    } 
    
    vTaskDelay(20000/portTICK_PERIOD_MS);
  }
}

void IRAM_ATTR buttonPress(void)
{
  portENTER_CRITICAL(&synch);
  timerRestart(timer0);
  digitalWrite(2, LOW);
  portEXIT_CRITICAL(&synch); 
}

void IRAM_ATTR onTimer()
{
  portENTER_CRITICAL_ISR(&timerMux);
  digitalWrite(2, HIGH);
  portEXIT_CRITICAL_ISR(&timerMux);
}

void callback(char* topic, byte* message, unsigned int length) {
  Serial.print("Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  String messageTemp;
  
  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    messageTemp += (char)message[i];
  }
  Serial.println();

  // Feel free to add more if statements to control more GPIOs with MQTT
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Attempt to connect
    if (client.connect("ESP8266Client","topgear","sitpora")) {
      Serial.println("connected");
      // Subscribe
      client.subscribe("esp32/output");
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
