#include <NTPClient.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include "secrets.h"

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP);

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