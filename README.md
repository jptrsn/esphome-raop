# ESPHome RAOP (AirPlay) Media Player

AirPlay audio receiver component for ESPHome with multi-room synchronization support.

## Features

- **AirPlay Audio Streaming**: Full AirPlay 1 protocol support with authentication
- **Multi-room Sync**: Synchronized playback across multiple AirPlay devices
- **ESPHome Integration**: Works alongside other ESPHome media players
- **Home Assistant Ready**: Automatic discovery and control
- **I²S Audio Output**: Support for external DACs via I²S

## Requirements

- **ESP32-WROVER** or **ESP32-S3** with PSRAM (minimum 4MB)
- **ESP-IDF framework** (ESPHome 2025.6.0+)
- **I²S DAC** (e.g., PCM5102A, MAX98357A)
- ~3.5MB PSRAM for audio buffering

## Installation

Add this to your ESPHome YAML configuration:
```yaml
external_components:
  - source: github://yourusername/esphome-raop
    components: [ raop_media_player ]

# Configure PSRAM (required)
psram:

# Configure I²S pins
i2s_audio:
  id: i2s_output
  i2s_lrclk_pin: GPIO25
  i2s_bclk_pin: GPIO26

# Add RAOP media player
media_player:
  - platform: raop_media_player
    name: "Living Room Speaker"
    i2s_dout_pin: GPIO22
    i2s_audio_id: i2s_output
```

## Quick Start

See [examples/basic-raop-player.yaml](examples/basic-raop-player.yaml) for a complete configuration example.

## Configuration Options

### Media Player

- **name** (*Required*, string): Name of the media player
- **i2s_dout_pin** (*Required*, pin): I²S data output pin
- **i2s_audio_id** (*Required*, ID): Reference to i2s_audio component
- **buffer_frames** (*Optional*, int): Audio buffer size in frames (default: 1024, ~23 seconds)

## How It Works

This component implements an AirPlay 1 (RAOP) receiver that:

1. Advertises via mDNS as an AirPlay target
2. Handles RTSP control and authentication
3. Receives RTP audio packets
4. Maintains timing-based buffer for multi-room sync
5. Outputs PCM audio via I²S

## Limitations

- AirPlay 1 only (AirPlay 2 not supported)
- No video support
- Requires PSRAM for audio buffering
- ESP32 classic or ESP32-S3 only

## Credits

Based on the excellent work of:
- [squeezelite-esp32](https://github.com/sle118/squeezelite-esp32) - RTP packet handling and audio buffering
- [shairport-sync](https://github.com/mikebrady/shairport-sync) - RTSP protocol and authentication

## License

GPL v3 - See [LICENSE](LICENSE) for details.

This license maintains compatibility with upstream GPL-licensed components.