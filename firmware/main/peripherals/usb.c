#include <freertos/FreeRTOS.h>
#include <tinyusb.h>
#include <tusb_cdc_acm.h>
#include "usb.h"

static usb_data_callback_t usb_data_callback;

static void rx_callback(int itf, cdcacm_event_t*)
{
   // Read incoming data and send it to a callback for processing
   size_t data_len = 0;
   uint8_t data[CONFIG_TINYUSB_CDC_RX_BUFSIZE + 1];
   if ((tinyusb_cdcacm_read(itf, data, CONFIG_TINYUSB_CDC_RX_BUFSIZE, &data_len) == ESP_OK) && usb_data_callback)
      usb_data_callback(data, data_len);
}

void usb_initialize(bool self_powered)
{
   // Initialize all static variables
   usb_data_callback = NULL;

   // Configure a GPIO pin for VBUS monitoring
   if (self_powered)
   {
      const gpio_config_t vbus_gpio_config = {
         .pin_bit_mask = BIT64(VBUS_MONITORING_GPIO_NUM),
         .mode = GPIO_MODE_INPUT,
         .intr_type = GPIO_INTR_DISABLE,
         .pull_up_en = false,
         .pull_down_en = false,
      };
      ESP_ERROR_CHECK(gpio_config(&vbus_gpio_config));
   }

   // Install the TinyUSB driver
   const tinyusb_config_t usb_config = {
      .device_descriptor = NULL,
      .string_descriptor = NULL,
      .string_descriptor_count = 0,
      .external_phy = false,
      .configuration_descriptor = NULL,
      .self_powered = self_powered,
      .vbus_monitor_io = USB_VBUS_MONITOR_PIN
   };
   ESP_ERROR_CHECK(tinyusb_driver_install(&usb_config));

   // Initialize the CDC ACM class
   const tinyusb_config_cdcacm_t acm_cfg = {
      .usb_dev = TINYUSB_USBDEV_0,
      .cdc_port = TINYUSB_CDC_ACM_0,
      .callback_rx = &rx_callback,
      .callback_rx_wanted_char = NULL,
      .callback_line_state_changed = NULL,
      .callback_line_coding_changed = NULL
   };
   ESP_ERROR_CHECK(tusb_cdc_acm_init(&acm_cfg));
}

void usb_add_data_callback(usb_data_callback_t callback)
{
   // Store the USB data callback function
   usb_data_callback = callback;
}

void usb_write_data(const uint8_t *data, size_t data_len)
{
   // Send bytes to the USB queue until all bytes have been written
   size_t bytes_remaining = data_len;
   while (tud_cdc_connected() && bytes_remaining)
   {
      size_t bytes_written = tinyusb_cdcacm_write_queue(TINYUSB_CDC_ACM_0, data, bytes_remaining);
      bytes_remaining -= bytes_written;
      data += bytes_written;
   }
   if (bytes_remaining)
      tud_cdc_write_clear();
}

void usb_write_audio_packet(double timestamp, float lat, float lon, float height, const uint8_t *audio, size_t audio_len)
{
   // Write a packet delimiter, timestamp, and audio data to the USB queue
   static const uint8_t usb_packet_delimiter[] = USB_PACKET_DELIMITER;
   usb_write_data((const uint8_t*)&usb_packet_delimiter, sizeof(usb_packet_delimiter));
   usb_write_data((const uint8_t*)&timestamp, sizeof(timestamp));
   usb_write_data((const uint8_t*)&lat, sizeof(lat));
   usb_write_data((const uint8_t*)&lon, sizeof(lon));
   usb_write_data((const uint8_t*)&height, sizeof(height));
   usb_write_data(audio, audio_len);
}