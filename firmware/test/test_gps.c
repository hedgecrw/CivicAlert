#include <freertos/FreeRTOS.h>
#include <esp_event.h>
#include <esp_wifi.h>
#include <nvs_flash.h>
#include "gps.h"
#include "logging.h"

void app_main(void)
{
   // Initialize the main flash partition
   esp_err_t ret = nvs_flash_init();
   if ((ret == ESP_ERR_NVS_NO_FREE_PAGES) || (ret == ESP_ERR_NVS_NEW_VERSION_FOUND))
   {
      // Partition was truncated and needs to be erased
      ESP_ERROR_CHECK(nvs_flash_erase());
      ESP_ERROR_CHECK(nvs_flash_init());
   }

   // Create the GPS processing task
   xTaskCreatePinnedToCore(gps_task, "gps_task", 2048, NULL, 8, NULL, 0);

   // Start the main application task
   gps_timestamp_t gps_timestamp;
   float lat, lon, height;
   while (true)
   {
      gps_timestamp = gps_request_timestamp();
      gps_get_llh(&lat, &lon, &height);
      print("Timestamp: %0.6f, Latitude: %0.6f, Longitude: %0.6f, Height: %0.3f", gps_timestamp, lat, lon, height);
      vTaskDelay(pdMS_TO_TICKS(1000));
   }
}
