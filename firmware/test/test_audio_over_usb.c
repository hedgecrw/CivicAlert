#include <freertos/FreeRTOS.h>
#include <esp_event.h>
#include <esp_wifi.h>
#include <nvs_flash.h>
#include "audio.h"
#include "gps.h"
#include "logging.h"
#include "usb.h"

static void usb_data_received(const uint8_t *data, size_t data_len)
{
   print("USB Data Received (%u bytes) - First 10 bytes: [0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X, 0x%02X]",
      data_len, data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7], data[8], data[9]);
}

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

   // Initialize the USB peripheral and data callback
   usb_initialize(USB_SELF_POWERED);
   usb_add_data_callback(usb_data_received);

   // Create the audio-processing task
   xTaskCreatePinnedToCore(audio_task, "audio_task", 2048, xTaskGetCurrentTaskHandle(), 10, NULL, 1);

   // Start the main application task
   float lat, lon, height;
   gps_timestamp_t audio_timestamp;
   uint32_t audio_data_ptr;
   while (true)
   {
      // Wait to receive the next second of audio
      xTaskNotifyWaitIndexed(0, 0, ULONG_MAX, &audio_timestamp.timestamp_parts[0], portMAX_DELAY);
      xTaskNotifyWaitIndexed(1, 0, ULONG_MAX, &audio_timestamp.timestamp_parts[1], portMAX_DELAY);
      xTaskNotifyWaitIndexed(2, 0, ULONG_MAX, &audio_data_ptr, portMAX_DELAY);
      gps_get_llh(&lat, &lon, &height);

      // Write the audio data out over USB
      print("[%0.6f]: Writing audio packet from <%0.6f, %0.6f, %0.3f> over USB...", audio_timestamp.gps_timestamp, lat, lon, height);
      usb_write_audio_packet(audio_timestamp.gps_timestamp, lat, lon, height, (uint8_t*)audio_data_ptr, AUDIO_PACKET_SIZE_BYTES);
   }
}
