#include "network.h"

//static QueueHandle_t data_queue;

// INIT: data_queue = xQueueCreate(5, sizeof(usb_data_t));
// TO SEND: xQueueSend(data_queue, &packet, 0);
// TO RECEIVE: usb_data_t packet;packet.data_len = 0;if (xQueueReceive(usb_data_queue, &packet, portMAX_DELAY) && packet.data_len)

void network_initialize(void)
{
  ;
}
