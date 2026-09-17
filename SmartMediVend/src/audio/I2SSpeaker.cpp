#include "I2SSpeaker.h"

#include <array>

#ifdef ARDUINO
#include "../../HardwarePins.h"
#include <freertos/FreeRTOS.h>
#endif

namespace smv::audio {

bool I2SSpeaker::begin(uint32_t sampleRate) {
#ifdef ARDUINO
  if (channel_ != nullptr && sampleRate_ == sampleRate) return true;
  end();
  i2s_chan_config_t channelConfig =
      I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_1, I2S_ROLE_MASTER);
  channelConfig.dma_desc_num = 6;
  channelConfig.dma_frame_num = 240;
  if (i2s_new_channel(&channelConfig, &channel_, nullptr) != ESP_OK) {
    channel_ = nullptr;
    return false;
  }

  i2s_std_config_t config{};
  config.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(sampleRate);
  config.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(
      I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO);
  config.gpio_cfg.mclk = I2S_GPIO_UNUSED;
  config.gpio_cfg.bclk = static_cast<gpio_num_t>(pins::MAX98357_BCLK);
  config.gpio_cfg.ws = static_cast<gpio_num_t>(pins::MAX98357_LRC);
  config.gpio_cfg.dout = static_cast<gpio_num_t>(pins::MAX98357_DIN);
  config.gpio_cfg.din = I2S_GPIO_UNUSED;
  config.gpio_cfg.invert_flags.mclk_inv = false;
  config.gpio_cfg.invert_flags.bclk_inv = false;
  config.gpio_cfg.invert_flags.ws_inv = false;
  if (i2s_channel_init_std_mode(channel_, &config) != ESP_OK ||
      i2s_channel_enable(channel_) != ESP_OK) {
    end();
    return false;
  }
  sampleRate_ = sampleRate;
  return true;
#else
  (void)sampleRate;
  return false;
#endif
}

void I2SSpeaker::end() {
#ifdef ARDUINO
  if (channel_ != nullptr) {
    i2s_channel_disable(channel_);
    i2s_del_channel(channel_);
    channel_ = nullptr;
  }
#endif
  sampleRate_ = 0;
}

bool I2SSpeaker::write(const int16_t* monoSamples,
                       std::size_t sampleCount,
                       uint32_t timeoutMs) {
#ifdef ARDUINO
  if (channel_ == nullptr || monoSamples == nullptr) return false;
  std::array<int16_t, 256> stereo{};
  std::size_t consumed = 0;
  while (consumed < sampleCount) {
    const std::size_t chunk =
        (sampleCount - consumed) < 128U ? sampleCount - consumed : 128U;
    for (std::size_t i = 0; i < chunk; ++i) {
      stereo[i * 2U] = monoSamples[consumed + i];
      stereo[i * 2U + 1U] = monoSamples[consumed + i];
    }
    std::size_t written = 0;
    const std::size_t bytes = chunk * 2U * sizeof(int16_t);
    if (i2s_channel_write(channel_, stereo.data(), bytes, &written,
                          pdMS_TO_TICKS(timeoutMs)) != ESP_OK ||
        written != bytes) {
      return false;
    }
    consumed += chunk;
  }
  return true;
#else
  (void)monoSamples;
  (void)sampleCount;
  (void)timeoutMs;
  return false;
#endif
}

bool I2SSpeaker::active() const {
#ifdef ARDUINO
  return channel_ != nullptr;
#else
  return false;
#endif
}

}  // namespace smv::audio
