#include <sdkconfig.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <driver/gpio.h>

#define WIFI_CONNECT_FAILURE_MAX_RETRIES     5
#define BLE_PROOF_OF_POSSESSION_KEY          "sj$a8@;^0fcz"

#define SETUP_MODE_BUTTON_PRESS_SECONDS      5

#define AUDIO_SAMPLE_RATE_HZ                 48000
#define AUDIO_PACKET_SIZE_BYTES              (AUDIO_SAMPLE_RATE_HZ * sizeof(int16_t))

#define BUTTON_SETUP_MODE_PIN                GPIO_NUM_15
#define BUTTON_SETUP_MODE_ACTIVE_LEVEL       BUTTON_ACTIVE_LOW

#define MIC_DATA_PIN                         GPIO_NUM_17
#define MIC_CLOCK_PIN                        GPIO_NUM_18

#define GPS_RX_PIN                           GPIO_NUM_4
#define GPS_TX_PIN                           GPIO_NUM_5
#define GPS_EXTINT_PIN                       GPIO_NUM_6
#define GPS_RESET_PIN                        GPIO_NUM_7
#define GPS_TIMEPULSE_PIN                    GPIO_NUM_8

#define USB_VBUS_MONITOR_PIN                 GPIO_NUM_1
#define USB_SELF_POWERED                     false  // TODO: Change to true for actual HW
#define USB_PACKET_DELIMITER                 { 0x7E, 0x6F, 0x50, 0x11 }
