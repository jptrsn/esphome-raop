#pragma once

#include "esphome/core/component.h"
#include "esphome/components/media_player/media_player.h"
#include "esphome/components/i2s_audio/i2s_audio.h"

extern "C" {
#include "raop_core/raop.h"
#include "raop_core/raop_sink.h"
#include "raop_core/audio_buffer.h"
}

namespace esphome {
namespace raop_media_player {

class RAOPMediaPlayer : public media_player::MediaPlayer, public Component {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::LATE; }

  void set_i2s_audio_parent(i2s_audio::I2SAudioComponent *parent) { this->i2s_parent_ = parent; }
  void set_dout_pin(uint8_t pin) { this->dout_pin_ = pin; }
  void set_buffer_frames(uint32_t frames) { this->buffer_frames_ = frames; }

  // MediaPlayer control methods
  media_player::MediaPlayerTraits get_traits() override;
  bool is_muted() const override { return this->muted_; }
  void control(const media_player::MediaPlayerCall &call) override;

  // Called from C callbacks
  bool handle_raop_command(raop_event_t event, va_list args);
  void handle_raop_data(const uint8_t *data, size_t len, uint32_t playtime);
  void write_audio_data(const uint8_t *data, size_t len);

 protected:
  void start_raop_();
  void stop_raop_();
  bool try_lock_i2s_();
  void unlock_i2s_();
  void apply_volume_(uint8_t *data, size_t len);

  i2s_audio::I2SAudioComponent *i2s_parent_{nullptr};
  struct raop_ctx_s *raop_ctx_{nullptr};

  uint8_t dout_pin_;
  uint32_t buffer_frames_{1024};
  float volume_{1.0f};
  bool muted_{false};
  bool i2s_locked_{false};
  bool stream_active_{false};
};

// Global instance pointer for C callbacks
extern RAOPMediaPlayer *g_raop_instance;

}  // namespace raop_media_player
}  // namespace esphome