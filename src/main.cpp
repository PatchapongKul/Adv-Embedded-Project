#include <Wire.h>
#include <Arduino_HTS221.h>
#include <NTPClient.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include "secrets.h"

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

TwoWire sensor(1);
bool a = sensor.setPins(41, 40);
HTS221Class HTS1(sensor);

QueueHandle_t tempQueue;

void updateNTP( void * parameter );
void displaySerial( void * parameter );
void readTemp( void * parameter );
void displayTemp( void * parameter );

void setup(){
  Serial.begin(115200);

  WiFi.begin(ssid, password);

  while ( WiFi.status() != WL_CONNECTED ) {
    delay ( 500 );
    Serial.print ( "." );
  }
  Serial.println("");
  Serial.println("WiFi Connected");
  delay(500);
  
  if (!HTS1.begin()) {
    Serial.println("Failed to initialize humidity temperature sensor!");
    while (1);
  }
  Serial.println("Initialize Temperature Sensor ..... Done!");

  tempQueue = xQueueCreate( 10, sizeof(float));

  timeClient.begin();
  timeClient.update();
  timeClient.setTimeOffset(7*3600); //In Thailand (UTC+7)
  
  xTaskCreate(
  updateNTP,    /* Task function. */
  "updateNTP",  /* name of task. */
  10000,        /* Stack size of task */
  NULL,         /* parameter of the task */
  3,            /* priority of the task */
  NULL);        /* Task handle to keep track of created task */

  xTaskCreate(
  displaySerial,    /* Task function. */
  "displaySerial",  /* name of task. */
  10000,            /* Stack size of task */
  NULL,             /* parameter of the task */
  2,                /* priority of the task */
  NULL);            /* Task handle to keep track of created task */

  xTaskCreate(
  readTemp,    /* Task function. */
  "readTemp",  /* name of task. */
  10000,            /* Stack size of task */
  NULL,             /* parameter of the task */
  1,                /* priority of the task */
  NULL);            /* Task handle to keep track of created task */

  xTaskCreate(
  displayTemp,    /* Task function. */
  "displayTemp",  /* name of task. */
  10000,            /* Stack size of task */
  NULL,             /* parameter of the task */
  1,                /* priority of the task */
  NULL);            /* Task handle to keep track of created task */
  
}
  
void loop() {

}

void updateNTP( void * parameter )
{
  /* loop forever */
  for(;;)
  {
    timeClient.update();
    Serial.println("NTP updated");
    vTaskDelay(60000/portTICK_PERIOD_MS);
  }
}

void displaySerial( void * parameter )
{
  /* loop forever */
  for(;;)
  {
    Serial.println(timeClient.getFormattedTime());
    vTaskDelay(1000/portTICK_PERIOD_MS);
  }
}

void readTemp( void * parameter )
{
  for(;;)
  {
  float temperature = HTS1.readTemperature();
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
          Serial.print("Temperature = ");
          Serial.print(display_temp);
          Serial.println("°C");
        }
      }
    vTaskDelay(20000/portTICK_PERIOD_MS);
  }
}