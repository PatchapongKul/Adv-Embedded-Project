#include <NTPClient.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <esp_task_wdt.h>
#include "secrets.h"

#define WDT_TIMEOUT 5

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

hw_timer_t *timer0 = NULL;
portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;
portMUX_TYPE synch = portMUX_INITIALIZER_UNLOCKED;

void updateNTP( void * parameter );
void displaySerial( void * parameter );
void IRAM_ATTR buttonPress(void);
void IRAM_ATTR onTimer();

void setup(){
  esp_task_wdt_init(WDT_TIMEOUT, true); // Initialize the Task Watchdog Timer
  esp_task_wdt_add(NULL);               // Subscribe current task to the Task Watchdog Timer

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