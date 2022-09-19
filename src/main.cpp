#include <NTPClient.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <esp_task_wdt.h>
#include "secrets.h"

#define WDT_TIMEOUT 5

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

void updateNTP( void * parameter );
void displaySerial( void * parameter );

void setup(){
  esp_task_wdt_init(WDT_TIMEOUT, true); // Initialize the Task Watchdog Timer
  esp_task_wdt_add(NULL);               // Subscribe current task to the Task Watchdog Timer

  Serial.begin(115200);

  WiFi.begin(ssid, password);
  Serial.println("Connecting to WiFi");
  while ( WiFi.status() != WL_CONNECTED )
  {
    vTaskDelay(100/portTICK_PERIOD_MS);
    esp_task_wdt_reset();
  }
  Serial.println("WiFi Connected");
  
  timeClient.begin();
  timeClient.update();
  timeClient.setTimeOffset(7*3600); //In Thailand (UTC+7)
  Serial.println("NTP updated");

  // disconnect from WiFi but not erase AP config
  WiFi.disconnect(false, true);
  Serial.println("WiFi Disconnected");
  
  xTaskCreate(
  updateNTP,    /* Task function. */
  "updateNTP",  /* name of task. */
  10000,        /* Stack size of task */
  NULL,         /* parameter of the task */
  2,            /* priority of the task */
  NULL);        /* Task handle to keep track of created task */

  xTaskCreate(
  displaySerial,    /* Task function. */
  "displaySerial",  /* name of task. */
  10000,            /* Stack size of task */
  NULL,             /* parameter of the task */
  1,                /* priority of the task */
  NULL);            /* Task handle to keep track of created task */
  
}
  
void loop() 
{
  if (timeClient.getHours() == 18)  // if it's 6 PM
  {
    Serial.println("ESP is going to sleep");
    esp_sleep_enable_timer_wakeup(12*60*60*1e6);  // sleep for 12 hours
    esp_task_wdt_delete(NULL);
    esp_deep_sleep_start();
  }
  esp_task_wdt_reset();
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
      WiFi.disconnect();
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
      WiFi.disconnect(false, true);
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