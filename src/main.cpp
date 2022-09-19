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

  while ( WiFi.status() != WL_CONNECTED ) {
    delay ( 500 );
    Serial.print ( "." );
  }
  Serial.println("");
  Serial.println("WiFi Connected");
  delay(500);
  
  timeClient.begin();
  timeClient.update();
  timeClient.setTimeOffset(7*3600); //In Thailand (UTC+7)
  
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
  
void loop() {
  esp_task_wdt_reset();
}

void updateNTP( void * parameter )
{
  // Subscribe current task to the Task Watchdog Timer
  esp_task_wdt_add(NULL);

  /* loop forever */
  for(;;)
  {
    timeClient.update();
    Serial.println("NTP updated");
    vTaskDelay(4000/portTICK_PERIOD_MS);
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