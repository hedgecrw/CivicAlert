#include <freertos/FreeRTOS.h>
#include <driver/i2s_pdm.h>
#include "audio.h"
#include "logging.h"
#include "gps.h"

// Global shared audio buffer
static int16_t audio_buffer[2 * AUDIO_SAMPLE_RATE_HZ];

// Audio peripheral initialization
static i2s_chan_handle_t audio_init(void)
{
   // Configure the I2S RX channel to interrupt every 0.5ms to ensure timestamp triggering is at least that accurate
   i2s_chan_handle_t rx_channel;
   i2s_chan_config_t rx_channel_config = {
      .id = I2S_NUM_0,
      .role = I2S_ROLE_MASTER,
      .dma_desc_num = 6,
      .dma_frame_num = AUDIO_SAMPLE_RATE_HZ / 2000,
      .auto_clear_after_cb = false,
      .auto_clear_before_cb = false,
      .intr_priority = 3,
   };
   i2s_new_channel(&rx_channel_config, NULL, &rx_channel);

   // Configure the RX channel for PDM mode
   i2s_pdm_rx_config_t pdm_rx_config = {
      .clk_cfg = I2S_PDM_RX_CLK_DEFAULT_CONFIG(AUDIO_SAMPLE_RATE_HZ),
      .slot_cfg = I2S_PDM_RX_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO),
      .gpio_cfg = {
         .clk = MIC_CLOCK_PIN,
         .din = MIC_DATA_PIN,
         .invert_flags = { .clk_inv = false, },
      }
   };
   i2s_channel_init_pdm_rx_mode(rx_channel, &pdm_rx_config);
   return rx_channel;
}

// Audio task entry point
void audio_task(void *args)
{
   // Initialize the audio peripheral
   size_t bytes_read = 0;
   TaskHandle_t main_task = (TaskHandle_t)args;
   uint32_t audio_buffer_index = AUDIO_SAMPLE_RATE_HZ;
   i2s_chan_handle_t audio_channel = audio_init();

   // Enable the I2S RX channel and send the first timestamp request
   i2s_channel_enable(audio_channel);
   gps_timestamp_t audio_timestamp = gps_request_timestamp();

   // Read audio in a loop forever
   while (true)
   {
      // Read one second of audio and immediately request a new timestamp
      audio_buffer_index = (audio_buffer_index + AUDIO_SAMPLE_RATE_HZ) % (2 * AUDIO_SAMPLE_RATE_HZ);
      i2s_channel_read(audio_channel, audio_buffer + audio_buffer_index, 2 * AUDIO_SAMPLE_RATE_HZ, &bytes_read, 1000);
      audio_timestamp = gps_request_timestamp();

      // Send the audio data and timestamp to the main task for processing
      xTaskNotifyIndexed(main_task, 0, audio_timestamp.timestamp_parts[0], eSetValueWithOverwrite);
      xTaskNotifyIndexed(main_task, 1, audio_timestamp.timestamp_parts[1], eSetValueWithOverwrite);
      xTaskNotifyIndexed(main_task, 2, (uint32_t)(audio_buffer + audio_buffer_index), eSetValueWithOverwrite);
   }
}
