#include "I2SMicrophone.h"

#ifdef ARDUINO
#include "../../HardwarePins.h"
#include <freertos/FreeRTOS.h>
#endif

namespace smv::audio {

bool I2SMicrophone::begin(uint32_t sampleRate) {
#ifdef ARDUINO
  if (channel_ != nullptr) return true;
  i2s_chan_config_t channelConfig =
      I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_0, I2S_ROLE_MASTER);
  channelConfig.dma_desc_num = 6;
  channelConfig.dma_frame_num = 240;
  if (i2s_new_channel(&channelConfig, nullptr, &channel_) != ESP_OK) {
    channel_ = nullptr;
    return false;
  }

  i2s_std_config_t config{};
  config.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(sampleRate);
  config.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
      I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO);
  config.slot_cfg.slot_mask = I2S_STD_SLOT_LEFT;
  config.gpio_cfg.mclk = I2S_GPIO_UNUSED;
  config.gpio_cfg.bclk = static_cast<gpio_num_t>(pins::INMP441_SCK);
  config.gpio_cfg.ws = static_cast<gpio_num_t>(pins::INMP441_WS);
  config.gpio_cfg.dout = I2S_GPIO_UNUSED;
  config.gpio_cfg.din = static_cast<gpio_num_t>(pins::INMP441_SD);
  config.gpio_cfg.invert_flags.mclk_inv = false;
  config.gpio_cfg.invert_flags.bclk_inv = false;
  config.gpio_cfg.invert_flags.ws_inv = false;
  if (i2s_channel_init_std_mode(channel_, &config) != ESP_OK ||
      i2s_channel_enable(channel_) != ESP_OK) {
    end();
    return false;
  }
  return true;
#else
  (void)sampleRate;
  return false;
#endif
}

void I2SMicrophone::end() {
#ifdef ARDUINO
  if (channel_ != nullptr) {
    i2s_channel_disable(channel_);
    i2s_del_channel(channel_);
    channel_ = nullptr;
  }
#endif
}

std::size_t I2SMicrophone::read(int32_t* samples,
                                std::size_t maximumSamples,
                                uint32_t timeoutMs) {
#ifdef ARDUINO
  if (channel_ == nullptr || samples == nullptr || maximumSamples == 0) {
    return 0;
  }
  std::size_t bytesRead = 0;
  const TickType_t timeout = pdMS_TO_TICKS(timeoutMs);
  if (i2s_channel_read(channel_, samples,
                       maximumSamples * sizeof(int32_t), &bytesRead,
                       timeout) != ESP_OK) {
    return 0;
  }
  return bytesRead / sizeof(int32_t);
#else
  (void)samples;
  (void)maximumSamples;
  (void)timeoutMs;
  return 0;
#endif
}

bool I2SMicrophone::active() const {
#ifdef ARDUINO
  return channel_ != nullptr;
#else
  return false;
#endif
}

}  // namespace smv::audio
