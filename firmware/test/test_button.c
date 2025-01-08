#include <freertos/FreeRTOS.h>
#include <esp_event.h>
#include <esp_wifi.h>
#include <nvs_flash.h>
#include "button.h"
#include "logging.h"

static void setup_button_pressed(void)
{
   print("Button was long-pressed");
}

static void button_down_cb(void)
{
   print("Button was pressed");
}

static void button_up_cb(void)
{
   print("Button was released");
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

   // Initialize the button press handler
   button_handle_t setup_mode_button = button_initialize(BUTTON_SETUP_MODE_PIN, BUTTON_SETUP_MODE_ACTIVE_LEVEL);
   button_add_event_callback(setup_mode_button, BUTTON_CALLBACK_LONG_PRESS, setup_button_pressed, SETUP_MODE_BUTTON_PRESS_SECONDS);
   button_add_event_callback(setup_mode_button, BUTTON_CALLBACK_PRESS, button_down_cb, 0);
   button_add_event_callback(setup_mode_button, BUTTON_CALLBACK_RELEASE, button_up_cb, 0);

   // Start the main application task
   while (true)
   {
      print("Hello World!");
      vTaskDelay(pdMS_TO_TICKS(1000));
   }
}
