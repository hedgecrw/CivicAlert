#include <freertos/FreeRTOS.h>
#include <esp_timer.h>
#include "button.h"

// Button configuration structure
typedef struct
{
   uint8_t gpio_num;
   uint8_t active_level;
   TickType_t press_interval, long_press_interval;
   button_callback_t event_callback[BUTTON_NUM_CALLBACK_TYPES];
   esp_timer_handle_t timer_handle;
   bool timer_running;
   int tick_count;
} button_dev_t;

static void button_gpio_isr_handler(void* arg)
{
   // Start a timer upon a new button press event
   button_dev_t* button = (button_dev_t*)arg;
   if (!button->timer_running)
   {
      button->tick_count = 0;
      button->timer_running = true;
      esp_timer_start_periodic(button->timer_handle, portTICK_PERIOD_MS * 1000U);
   }
   gpio_intr_disable(button->gpio_num);
   gpio_wakeup_disable(button->gpio_num);
}

static void button_timer_callback(void *arg)
{
   // Determine whether the button is still pressed
   button_dev_t* button = (button_dev_t*)arg;
   if (button->active_level == gpio_get_level(button->gpio_num))  // Button Pressed
   {
      button->tick_count++;
      if (button->event_callback[BUTTON_CALLBACK_PRESS] && (button->tick_count == button->press_interval))
         button->event_callback[BUTTON_CALLBACK_PRESS]();
      if (button->event_callback[BUTTON_CALLBACK_LONG_PRESS] && (button->tick_count == button->long_press_interval))
         button->event_callback[BUTTON_CALLBACK_LONG_PRESS]();
   }
   else  // Button Released
   {
      button->timer_running = false;
      esp_timer_stop(button->timer_handle);
      if (button->event_callback[BUTTON_CALLBACK_RELEASE])
         button->event_callback[BUTTON_CALLBACK_RELEASE]();
      gpio_intr_enable(button->gpio_num);
      gpio_wakeup_enable(button->gpio_num, button->active_level ? GPIO_INTR_HIGH_LEVEL : GPIO_INTR_LOW_LEVEL);
   }
}

button_handle_t button_initialize(gpio_num_t gpio_num, button_active_t active_level)
{
   // Initialize a new button device structure
   button_dev_t* button_handle = (button_dev_t*)calloc(1, sizeof(button_dev_t));
   memset(button_handle, 0, sizeof(button_dev_t));
   button_handle->gpio_num = gpio_num;
   button_handle->active_level = active_level;
   button_handle->press_interval = 50 / portTICK_PERIOD_MS;

   // Initialize a button press timer
   const esp_timer_create_args_t button_timer_config = {
      .callback = button_timer_callback,
      .arg = button_handle,
      .dispatch_method = ESP_TIMER_TASK,
      .name = "button_press_timer",
      .skip_unhandled_events = false
   };
   esp_timer_create(&button_timer_config, &button_handle->timer_handle);

   // Initialize the GPIO pin and install the ISR service
   const gpio_config_t gpio_configuration = {
      .mode = GPIO_MODE_INPUT,
      .intr_type = active_level ? GPIO_INTR_HIGH_LEVEL : GPIO_INTR_LOW_LEVEL,
      .pin_bit_mask = (1ULL << gpio_num),
      .pull_down_en = active_level ? GPIO_PULLDOWN_ENABLE : GPIO_PULLDOWN_DISABLE,
      .pull_up_en = active_level ? GPIO_PULLUP_DISABLE : GPIO_PULLUP_ENABLE
   };
   gpio_config(&gpio_configuration);
   gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
   gpio_isr_handler_add(gpio_num, button_gpio_isr_handler, button_handle);
   gpio_intr_enable(gpio_num);
   gpio_wakeup_enable(gpio_num, active_level ? GPIO_INTR_HIGH_LEVEL : GPIO_INTR_LOW_LEVEL);
   return button_handle;
}

void button_add_event_callback(button_handle_t button_handle, button_callback_type_t callback_type, button_callback_t callback, uint32_t press_seconds)
{
   // Store the event callback and create a timer to debounce the button press
   button_dev_t *button = (button_dev_t*)button_handle;
   button->event_callback[callback_type] = callback;
   if (callback_type == BUTTON_CALLBACK_LONG_PRESS)
      button->long_press_interval = press_seconds * 1000 / portTICK_PERIOD_MS;
}
