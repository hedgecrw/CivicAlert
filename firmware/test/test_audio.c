#include <freertos/FreeRTOS.h>
#include <esp_event.h>
#include <esp_wifi.h>
#include <nvs_flash.h>
#include "audio.h"
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

   // Create the audio-processing task
   xTaskCreatePinnedToCore(audio_task, "audio_task", 2048, xTaskGetCurrentTaskHandle(), 10, NULL, 1);

   // Start the main application task
   gps_timestamp_t audio_timestamp;
   uint32_t audio_data_ptr;
   while (true)
   {
      xTaskNotifyWaitIndexed(0, 0, ULONG_MAX, &audio_timestamp.timestamp_parts[0], portMAX_DELAY);
      xTaskNotifyWaitIndexed(1, 0, ULONG_MAX, &audio_timestamp.timestamp_parts[1], portMAX_DELAY);
      xTaskNotifyWaitIndexed(2, 0, ULONG_MAX, &audio_data_ptr, portMAX_DELAY);
      int16_t *audio_data = (int16_t*)audio_data_ptr;
      print("Timestamp: %0.6f, Audio Data: %d %d %d %d %d %d %d %d", audio_timestamp.gps_timestamp,
            audio_data[0], audio_data[1], audio_data[2], audio_data[3], audio_data[4], audio_data[5], audio_data[6], audio_data[7]);
   }
}
