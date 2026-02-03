# Quick Start Guide

## Prerequisites

1. **Hardware:**
   - ESP32-WROVER or ESP32-S3 with PSRAM (minimum 4MB)
   - I²S DAC module (e.g., PCM5102A, MAX98357A)
   - USB cable for programming

2. **Software:**
   - ESPHome 2025.6.0 or later
   - Home Assistant (optional, but recommended)

## Wiring

Connect your I²S DAC to the ESP32:
```
DAC Pin   →  ESP32 Pin
VIN       →  3.3V or 5V (check your DAC)
GND       →  GND
DIN/DOUT  →  GPIO22 (i2s_dout_pin)
BCK/BCLK  →  GPIO26 (i2s_bclk_pin)
LCK/LRCLK →  GPIO25 (i2s_lrclk_pin)
SCK       →  GND (leaving this floating may result in no audio output)
```

## Troubleshooting

**Device doesn't appear in AirPlay menu:**
- Check WiFi connection
- Verify mDNS is not blocked by firewall
- Check logs for "No IP address available"

**No sound:**
- Check I²S wiring
- Verify DAC power
- Check volume (may be muted or at 0%)
- Verify SCK pin on DAC is connected to GND

**Audio dropouts:**
- WiFi signal strength issues
- Check logs for "Buffer full" warnings
- Try increasing `buffer_frames` to 2048

**Build errors:**
- Ensure ESP-IDF framework is selected (not Arduino)
- Check ESPHome version >= 2025.6.0
- Verify PSRAM is enabled