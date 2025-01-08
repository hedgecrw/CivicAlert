#ifndef __USB_HEADER_H__
#define __USB_HEADER_H__

#include "app_config.h"

typedef void (*usb_data_callback_t)(const uint8_t *data, size_t data_len);

void usb_initialize(bool self_powered);
void usb_add_data_callback(usb_data_callback_t callback);
void usb_write_data(const uint8_t *data, size_t data_len);
void usb_write_audio_packet(double timestamp, float lat, float lon, float height, const uint8_t *audio, size_t audio_len);

#endif // __USB_HEADER_H__
