#include "raop_media_player.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"
#include "esp_mac.h"
#include "esp_netif.h"

namespace esphome {
namespace raop_media_player {

static const char *const TAG = "raop_media_player";

// Global instance for C callbacks
RAOPMediaPlayer *g_raop_instance = nullptr;

// C callback wrappers
extern "C" {

static bool raop_cmd_callback_wrapper(raop_event_t event, ...) {
  if (!g_raop_instance)
    return false;

  va_list args;
  va_start(args, event);
  bool result = g_raop_instance->handle_raop_command(event, args);
  va_end(args);
  return result;
}

static void raop_data_callback_wrapper(const uint8_t *data, size_t len, uint32_t playtime) {
  if (g_raop_instance) {
    g_raop_instance->handle_raop_data(data, len, playtime);
  }
}

static void audio_output_callback_wrapper(const uint8_t *data, size_t len) {
  if (g_raop_instance) {
    g_raop_instance->write_audio_data(data, len);
  }
}

}  // extern "C"

void RAOPMediaPlayer::setup() {
  ESP_LOGCONFIG(TAG, "Setting up RAOP Media Player...");

  g_raop_instance = this;

  // Start RAOP receiver
  this->start_raop_();
}

void RAOPMediaPlayer::loop() {
  // Minimal loop - RAOP handles everything via callbacks
}

void RAOPMediaPlayer::dump_config() {
  ESP_LOGCONFIG(TAG, "RAOP Media Player:");
  ESP_LOGCONFIG(TAG, "  I2S DOUT Pin: GPIO%d", this->dout_pin_);
  ESP_LOGCONFIG(TAG, "  Buffer Frames: %d", this->buffer_frames_);
}

media_player::MediaPlayerTraits RAOPMediaPlayer::get_traits() {
  auto traits = media_player::MediaPlayerTraits();
  traits.set_supports_pause(true);
  traits.set_supports_volume(true);
  traits.set_supports_mute(true);
  return traits;
}

void RAOPMediaPlayer::control(const media_player::MediaPlayerCall &call) {
  if (call.get_volume().has_value()) {
    this->volume_ = call.get_volume().value();
    ESP_LOGD(TAG, "Volume set to %.2f", this->volume_);
  }

  if (call.get_command().has_value()) {
    switch (call.get_command().value()) {
      case media_player::MEDIA_PLAYER_COMMAND_STOP:
        if (this->stream_active_) {
          ESP_LOGI(TAG, "Stop requested");
          // Note: RAOP doesn't support explicit stop from receiver side
          // Stream will stop when sender disconnects
        }
        break;
      case media_player::MEDIA_PLAYER_COMMAND_MUTE:
        this->muted_ = true;
        ESP_LOGD(TAG, "Muted");
        break;
      case media_player::MEDIA_PLAYER_COMMAND_UNMUTE:
        this->muted_ = false;
        ESP_LOGD(TAG, "Unmuted");
        break;
      default:
        break;
    }
  }
}

void RAOPMediaPlayer::start_raop_() {
  ESP_LOGI(TAG, "Starting RAOP receiver...");

  // Get MAC address
  uint8_t mac[6];
  esp_efuse_mac_get_default(mac);

  // Get local IP
  uint32_t ip = 0;
  esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
  if (netif) {
    esp_netif_ip_info_t ip_info;
    if (esp_netif_get_ip_info(netif, &ip_info) == ESP_OK) {
      ip = ip_info.ip.addr;
    }
  }

  if (ip == 0) {
    ESP_LOGE(TAG, "No IP address available, RAOP not started");
    return;
  }

  // Create device name from component name
  const char *device_name = this->get_name().c_str();

  ESP_LOGI(TAG, "Starting AirPlay receiver: %s on IP: %d.%d.%d.%d",
           device_name,
           (int)(ip & 0xFF), (int)((ip >> 8) & 0xFF),
           (int)((ip >> 16) & 0xFF), (int)((ip >> 24) & 0xFF));

  // Create RAOP context with 88200 frames latency (2 seconds at 44.1kHz)
  this->raop_ctx_ = raop_create(ip, (char *)device_name, mac, 88200,
                                raop_cmd_callback_wrapper, raop_data_callback_wrapper);

  if (this->raop_ctx_) {
    ESP_LOGI(TAG, "AirPlay receiver started successfully");
    this->state = media_player::MEDIA_PLAYER_STATE_IDLE;
  } else {
    ESP_LOGE(TAG, "Failed to start AirPlay receiver");
  }
}

void RAOPMediaPlayer::stop_raop_() {
  if (this->raop_ctx_) {
    ESP_LOGI(TAG, "Stopping RAOP receiver...");
    raop_delete(this->raop_ctx_);
    this->raop_ctx_ = nullptr;
  }

  if (this->i2s_locked_) {
    this->unlock_i2s_();
  }

  this->stream_active_ = false;
  this->state = media_player::MEDIA_PLAYER_STATE_IDLE;
}

bool RAOPMediaPlayer::try_lock_i2s_() {
  if (this->i2s_locked_)
    return true;

  if (this->parent_ && this->parent_->try_lock()) {
    this->i2s_locked_ = true;
    ESP_LOGD(TAG, "I2S locked");
    return true;
  }

  ESP_LOGW(TAG, "Failed to lock I2S - may be in use by another component");
  return false;
}

void RAOPMediaPlayer::unlock_i2s_() {
  if (this->i2s_locked_ && this->parent_) {
    this->parent_->unlock();
    this->i2s_locked_ = false;
    ESP_LOGD(TAG, "I2S unlocked");
  }
}

void RAOPMediaPlayer::setup_i2s_tx_() {
  if (this->tx_handle_ != nullptr) {
    ESP_LOGW(TAG, "I2S TX already configured");
    return;
  }

  // Get I2S port from parent
  i2s_port_t port = this->parent_->get_port();

  // Configure I2S TX channel
  i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(port, I2S_ROLE_MASTER);

  esp_err_t err = i2s_new_channel(&chan_cfg, &this->tx_handle_, nullptr);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to create I2S TX channel: %s", esp_err_to_name(err));
    this->tx_handle_ = nullptr;
    return;
  }

  // Configure I2S standard mode for PCM5102A (44.1kHz, 16-bit, stereo)
  i2s_std_config_t std_cfg = {
      .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(44100),
      .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
      .gpio_cfg = {
          .mclk = I2S_GPIO_UNUSED,
          .bclk = static_cast<gpio_num_t>(this->parent_->get_bclk_pin()),
          .ws = static_cast<gpio_num_t>(this->parent_->get_lrclk_pin()),
          .dout = static_cast<gpio_num_t>(this->dout_pin_),
          .din = I2S_GPIO_UNUSED,
          .invert_flags = {
              .mclk_inv = false,
              .bclk_inv = false,
              .ws_inv = false,
          },
      },
  };

  err = i2s_channel_init_std_mode(this->tx_handle_, &std_cfg);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to initialize I2S standard mode: %s", esp_err_to_name(err));
    i2s_del_channel(this->tx_handle_);
    this->tx_handle_ = nullptr;
    return;
  }

  err = i2s_channel_enable(this->tx_handle_);
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "Failed to enable I2S channel: %s", esp_err_to_name(err));
    i2s_del_channel(this->tx_handle_);
    this->tx_handle_ = nullptr;
    return;
  }

  ESP_LOGI(TAG, "I2S TX channel configured successfully");
}

void RAOPMediaPlayer::cleanup_i2s_tx_() {
  if (this->tx_handle_ != nullptr) {
    i2s_channel_disable(this->tx_handle_);
    i2s_del_channel(this->tx_handle_);
    this->tx_handle_ = nullptr;
    ESP_LOGD(TAG, "I2S TX channel cleaned up");
  }
}

bool RAOPMediaPlayer::handle_raop_command(raop_event_t event, va_list args) {
  switch (event) {
    case RAOP_SETUP: {
      ESP_LOGI(TAG, "RAOP: Setup - audio stream starting");

      // Try to lock I2S
      if (!this->try_lock_i2s_()) {
        ESP_LOGE(TAG, "Cannot start stream - I2S unavailable");
        return false;
      }

      // Setup I2S TX channel
      this->setup_i2s_tx_();
      if (this->tx_handle_ == nullptr) {
        ESP_LOGE(TAG, "Failed to setup I2S TX channel");
        this->unlock_i2s_();
        return false;
      }

      // Allocate RTP buffer in PSRAM
      uint8_t **buffer = va_arg(args, uint8_t **);
      size_t *size = va_arg(args, size_t *);

      *size = 352 * 4 * 1024;  // ~1.4MB for RTP buffer
      *buffer = (uint8_t *)heap_caps_malloc(*size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);

      if (*buffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate RTP buffer!");
        this->cleanup_i2s_tx_();
        this->unlock_i2s_();
        return false;
      }

      ESP_LOGI(TAG, "Allocated %zu byte RTP buffer in PSRAM", *size);

      // Initialize audio buffer with our callback
      audio_buffer_init(audio_output_callback_wrapper);

      this->stream_active_ = true;
      this->state = media_player::MEDIA_PLAYER_STATE_PLAYING;
      this->publish_state();
      break;
    }

    case RAOP_STREAM:
      ESP_LOGI(TAG, "RAOP: Stream started");
      break;

    case RAOP_STOP:
      ESP_LOGI(TAG, "RAOP: Stream stopped");
      audio_buffer_flush();
      audio_buffer_deinit();
      this->cleanup_i2s_tx_();
      this->unlock_i2s_();
      this->stream_active_ = false;
      this->state = media_player::MEDIA_PLAYER_STATE_IDLE;
      this->publish_state();
      break;

    case RAOP_FLUSH:
      ESP_LOGI(TAG, "RAOP: Flush requested");
      audio_buffer_flush();
      break;

    case RAOP_VOLUME: {
      float volume = va_arg(args, double);
      this->volume_ = volume;
      ESP_LOGI(TAG, "RAOP: Volume changed to %.2f", volume);
      this->publish_state();
      break;
    }

    case RAOP_TIMING: {
      if (!audio_buffer_is_ready())
        break;

      // Timing sync for multi-room
      uint32_t frames_buffered, head_playtime;
      audio_buffer_get_timing(&frames_buffered, &head_playtime);

      if (frames_buffered == 0)
        break;

      uint32_t now = (uint32_t)(esp_timer_get_time() / 1000ULL);
      uint32_t buffer_duration_ms = (frames_buffered * 352 * 1000) / 44100;
      uint32_t local_head_time = now + buffer_duration_ms;
      int32_t error = (int32_t)(head_playtime - local_head_time);

      ESP_LOGV(TAG, "Timing: buffered=%u frames, error=%d ms", frames_buffered, error);

      // Correct if drift > 50ms
      if (error < -50) {
        uint32_t skip = (-error * 44100) / (352 * 1000);
        audio_buffer_skip_frames(skip);
        ESP_LOGD(TAG, "Skipping %u frames (ahead by %d ms)", skip, -error);
      } else if (error > 50) {
        uint32_t pause = (error * 44100) / (352 * 1000);
        audio_buffer_pause_frames(pause);
        ESP_LOGD(TAG, "Pausing %u frames (behind by %d ms)", pause, error);
      }
      break;
    }

    case RAOP_METADATA: {
      char *artist = va_arg(args, char *);
      char *album = va_arg(args, char *);
      char *title = va_arg(args, char *);
      ESP_LOGI(TAG, "RAOP: Metadata - Artist: %s, Album: %s, Title: %s",
               artist ? artist : "N/A", album ? album : "N/A", title ? title : "N/A");
      // TODO: Future enhancement - expose to Home Assistant
      break;
    }

    default:
      ESP_LOGV(TAG, "RAOP: Unhandled event %d", event);
      break;
  }

  return true;
}

void RAOPMediaPlayer::handle_raop_data(const uint8_t *data, size_t len, uint32_t playtime) {
  if (!audio_buffer_write(data, len, playtime)) {
    ESP_LOGW(TAG, "Failed to buffer audio frame");
  }
}

void RAOPMediaPlayer::write_audio_data(const uint8_t *data, size_t len) {
  if (!this->i2s_locked_ || this->tx_handle_ == nullptr)
    return;

  const uint8_t *output_data = data;
  uint8_t temp_buffer[len];

  // Apply volume if needed
  if (this->volume_ < 1.0f || this->muted_) {
    memcpy(temp_buffer, data, len);
    this->apply_volume_(temp_buffer, len);
    output_data = temp_buffer;
  }

  size_t bytes_written = 0;
  esp_err_t err = i2s_channel_write(this->tx_handle_, output_data, len, &bytes_written, portMAX_DELAY);

  if (err != ESP_OK) {
    ESP_LOGE(TAG, "I2S write failed: %s", esp_err_to_name(err));
  } else if (bytes_written != len) {
    ESP_LOGW(TAG, "I2S partial write: %zu/%zu bytes", bytes_written, len);
  }
}

void RAOPMediaPlayer::apply_volume_(uint8_t *data, size_t len) {
  float multiplier = this->muted_ ? 0.0f : this->volume_;
  int16_t *samples = (int16_t *)data;
  size_t sample_count = len / 2;  // 16-bit samples

  for (size_t i = 0; i < sample_count; i++) {
    samples[i] = (int16_t)(samples[i] * multiplier);
  }
}

}  // namespace raop_media_player
}  // namespace esphome