#include <freertos/FreeRTOS.h>
#include <esp_event.h>
#include <esp_wifi.h>
#include <nvs_flash.h>
#include "logging.h"
#include "usb.h"

static uint16_t dummy_tx_data[65000];

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

   // Generate dummy audio data
   for (uint16_t i = 0; i < 65000; i++)
      dummy_tx_data[i] = i;

   // Start the main application task
   while (true)
   {
      // Write lots of USB dummy data once per second
      print("Writing USB Data...");
      usb_write_data((uint8_t*)dummy_tx_data, sizeof(dummy_tx_data));
      vTaskDelay(pdMS_TO_TICKS(1000));
   }
}
