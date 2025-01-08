#include <freertos/FreeRTOS.h>
#include <esp_event.h>
#include <esp_wifi.h>
#include <nvs_flash.h>
#include <wifi_provisioning/manager.h>
#include <wifi_provisioning/scheme_ble.h>
#include "audio.h"
#include "button.h"
#include "gps.h"
#include "logging.h"
#include "network.h"
#include "usb.h"

// Static global variables
static bool provisioned = false;
static EventGroupHandle_t wifi_event_group;
static volatile bool wifi_connected;

// Setup-mode button press handler
static void setup_button_pressed(void)
{
   if (provisioned)
   {
      print("Resetting Wi-Fi provisioning to factory settings...");
      wifi_prov_mgr_reset_provisioning();
      provisioned = false;
   }
   esp_restart();  // TODO: Check whether wifi_prov_mgr_reset_provisioning() reboots, if so, put this in an else block
}

// Wi-Fi provisioning event handler
static void provisioning_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
   static int retries = 0;
   if (event_base == WIFI_PROV_EVENT)
   {
      switch (event_id)
      {
         case WIFI_PROV_START:
            print("Provisioning started");
            break;
         case WIFI_PROV_CRED_RECV:
         {
            wifi_sta_config_t *wifi_sta_cfg = (wifi_sta_config_t *)event_data;
            print("Received Wi-Fi credentials\n\tSSID     : %s\n\tPassword : XXXX", (const char *)wifi_sta_cfg->ssid);
            break;
         }
         case WIFI_PROV_CRED_FAIL:
         {
            wifi_prov_sta_fail_reason_t *reason = (wifi_prov_sta_fail_reason_t *)event_data;
            printe("Provisioning failed!\n\tReason : %s", (*reason == WIFI_PROV_STA_AUTH_ERROR) ? "Authentication failed" : "Access Point not found");
            if (!provisioned)
            {
               if (++retries >= WIFI_CONNECT_FAILURE_MAX_RETRIES) {
                  print("Failed to connect with specified credentials, resetting provisioned credentials");
                  wifi_prov_mgr_reset_sm_state_on_failure();
                  retries = 0;
               }
            }
            break;
         }
         case WIFI_PROV_CRED_SUCCESS:
            print("Provisioning successful");
            provisioned = true;
            retries = 0;
            break;
         case WIFI_PROV_END:
            wifi_prov_mgr_deinit();
            break;
         default:
            break;
      }
   }
   else if (event_base == WIFI_EVENT)
   {
      switch (event_id)
      {
         case WIFI_EVENT_STA_START:
            esp_wifi_connect();
            break;
         case WIFI_EVENT_STA_DISCONNECTED:
            print("Disconnected. Reconnecting to Wi-Fi...");
            wifi_connected = false;
            esp_wifi_connect();
            break;
         default:
            break;
      }
   }
   else if ((event_base == IP_EVENT) && (event_id == IP_EVENT_STA_GOT_IP))
   {
      wifi_connected = true;
      ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
      print("Connected with IP Address:" IPSTR, IP2STR(&event->ip_info.ip));
   }
   else if (event_base == PROTOCOMM_TRANSPORT_BLE_EVENT)
   {
      switch (event_id)
      {
         case PROTOCOMM_TRANSPORT_BLE_CONNECTED:
            print("BLE transport: Connected!");
            break;
         case PROTOCOMM_TRANSPORT_BLE_DISCONNECTED:
            print("BLE transport: Disconnected!");
            break;
         default:
            break;
      }
   }
   else if (event_base == PROTOCOMM_SECURITY_SESSION_EVENT)
   {
      switch (event_id)
      {
         case PROTOCOMM_SECURITY_SESSION_SETUP_OK:
            print("Secured session established!");
            break;
         case PROTOCOMM_SECURITY_SESSION_INVALID_SECURITY_PARAMS:
            printe("Received invalid security parameters for establishing secure session!");
            break;
         case PROTOCOMM_SECURITY_SESSION_CREDENTIALS_MISMATCH:
            printe("Received incorrect username and/or PoP for establishing secure session!");
            break;
         default:
            break;
      }
   }
}

// Application entry point
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

   // Initialize the setup-mode button press handler
   button_handle_t setup_mode_button = button_initialize(BUTTON_SETUP_MODE_PIN, BUTTON_SETUP_MODE_ACTIVE_LEVEL);
   button_add_event_callback(setup_mode_button, BUTTON_CALLBACK_LONG_PRESS, setup_button_pressed, SETUP_MODE_BUTTON_PRESS_SECONDS);

   // Initialize the TCP/IP networking interface and register event handlers
   ESP_ERROR_CHECK(esp_netif_init());
   ESP_ERROR_CHECK(esp_event_loop_create_default());
   wifi_event_group = xEventGroupCreate();
   ESP_ERROR_CHECK(esp_event_handler_register(WIFI_PROV_EVENT, ESP_EVENT_ANY_ID, &provisioning_event_handler, NULL));
   ESP_ERROR_CHECK(esp_event_handler_register(PROTOCOMM_TRANSPORT_BLE_EVENT, ESP_EVENT_ANY_ID, &provisioning_event_handler, NULL));
   ESP_ERROR_CHECK(esp_event_handler_register(PROTOCOMM_SECURITY_SESSION_EVENT, ESP_EVENT_ANY_ID, &provisioning_event_handler, NULL));
   ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &provisioning_event_handler, NULL));
   ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &provisioning_event_handler, NULL));

   // Initialize the Wi-Fi stack and provisioning manager
   esp_netif_create_default_wifi_sta();
   wifi_init_config_t wifi_config = WIFI_INIT_CONFIG_DEFAULT();
   ESP_ERROR_CHECK(esp_wifi_init(&wifi_config));
   ESP_ERROR_CHECK(wifi_prov_mgr_init((wifi_prov_mgr_config_t){
      .scheme = wifi_prov_scheme_ble,
      .scheme_event_handler = WIFI_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM
   }));

   // Check whether the device is already provisioned
   ESP_ERROR_CHECK(wifi_prov_mgr_is_provisioned(&provisioned));
   if (!provisioned)
   {
      print("Starting provisioning...");
      uint8_t eth_mac[6];
      char service_name[13];
      wifi_prov_security_t security = WIFI_PROV_SECURITY_1;
      const char *proof_of_possession = BLE_PROOF_OF_POSSESSION_KEY;
      wifi_prov_security1_params_t *sec_params = proof_of_possession;
      uint8_t ble_service_uuid[] = { 0xb4, 0xdf, 0x5a, 0x1c, 0x3f, 0x6b, 0xf4, 0xbf, 0xea, 0x4a, 0x82, 0x03, 0x04, 0x90, 0x1a, 0x02, };
      esp_wifi_get_mac(WIFI_IF_STA, eth_mac);
      snprintf(service_name, sizeof(service_name), "CIVIC_%02X%02X%02X", eth_mac[3], eth_mac[4], eth_mac[5]);
      wifi_prov_scheme_ble_set_service_uuid(ble_service_uuid);
      ESP_ERROR_CHECK(wifi_prov_mgr_start_provisioning(security, (const void *)sec_params, service_name, NULL));
   }
   else
   {
      print("Already provisioned, connecting to Wi-Fi...");
      wifi_prov_mgr_deinit();
      ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
      ESP_ERROR_CHECK(esp_wifi_start());
   }

   // Initialize the USB and networking peripherals
   usb_initialize(USB_SELF_POWERED);
   network_initialize();

   // Create the GPS and audio processing tasks
   xTaskCreatePinnedToCore(gps_task, "gps_task", 2048, NULL, 8, NULL, 0);
   xTaskCreatePinnedToCore(audio_task, "audio_task", 2048, xTaskGetCurrentTaskHandle(), 10, NULL, 1);

   // Start the main application loop
   float lat, lon, height;
   gps_timestamp_t audio_timestamp;
   uint32_t audio_data_ptr;
   while (true)
   {
      // Wait for the next second of audio data
      xTaskNotifyWaitIndexed(0, 0, ULONG_MAX, &audio_timestamp.timestamp_parts[0], portMAX_DELAY);
      xTaskNotifyWaitIndexed(1, 0, ULONG_MAX, &audio_timestamp.timestamp_parts[1], portMAX_DELAY);
      xTaskNotifyWaitIndexed(2, 0, ULONG_MAX, &audio_data_ptr, portMAX_DELAY);
      gps_get_llh(&lat, &lon, &height);

      // Write the audio data out over USB
      print("[%0.6f]: Writing audio packet from <%0.6f, %0.6f, %0.3f> over USB...", audio_timestamp.gps_timestamp, lat, lon, height);
      usb_write_audio_packet(audio_timestamp.gps_timestamp, lat, lon, height, (uint8_t*)audio_data_ptr, AUDIO_PACKET_SIZE_BYTES);
   }
}
