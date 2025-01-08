#ifndef __BUTTON_HEADER_H__
#define __BUTTON_HEADER_H__

#include "app_config.h"

typedef void (*button_callback_t)(void);
typedef void* button_handle_t;

typedef enum {
   BUTTON_ACTIVE_LOW = 0,
   BUTTON_ACTIVE_HIGH
} button_active_t;

typedef enum {
   BUTTON_CALLBACK_PRESS = 0,
   BUTTON_CALLBACK_RELEASE,
   BUTTON_CALLBACK_LONG_PRESS,
   BUTTON_NUM_CALLBACK_TYPES,
} button_callback_type_t;

button_handle_t button_initialize(gpio_num_t gpio_num, button_active_t active_level);
void button_add_event_callback(button_handle_t button_handle, button_callback_type_t callback_type, button_callback_t callback, uint32_t press_seconds);

#endif  //__BUTTON_HEADER_H__
