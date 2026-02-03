from esphome import pins
import esphome.codegen as cg
from esphome.components import media_player
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.core import CORE

from esphome.components.i2s_audio import (
    CONF_I2S_AUDIO_ID,
    CONF_I2S_DOUT_PIN,
    I2SAudioComponent,
    I2SAudioOut,
    i2s_audio_ns,
    register_i2s_audio_component,
    i2s_audio_component_schema,
)

CODEOWNERS = ["@jptrsn"]
DEPENDENCIES = ["i2s_audio", "psram"]

raop_media_player_ns = cg.esphome_ns.namespace("raop_media_player")
RAOPMediaPlayer = raop_media_player_ns.class_(
    "RAOPMediaPlayer", cg.Component, media_player.MediaPlayer, I2SAudioOut
)

CONF_BUFFER_FRAMES = "buffer_frames"


def validate_esp_idf_framework(config):
    if CORE.using_arduino:
        raise cv.Invalid("RAOP media player requires ESP-IDF framework")
    return config


CONFIG_SCHEMA = cv.All(
    media_player.media_player_schema(RAOPMediaPlayer).extend(
        i2s_audio_component_schema(
            RAOPMediaPlayer,
            default_sample_rate=44100,
            default_channel="stereo",
            default_bits_per_sample="16bit",
        )
    )
    .extend(
        {
            cv.Required(CONF_I2S_DOUT_PIN): pins.internal_gpio_output_pin_number,
            cv.Optional(CONF_BUFFER_FRAMES, default=1024): cv.int_range(
                min=512, max=2048
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA),
    validate_esp_idf_framework,
)


async def to_code(config):
    var = await media_player.new_media_player(config)
    await cg.register_component(var, config)
    await register_i2s_audio_component(var, config)

    cg.add(var.set_dout_pin(config[CONF_I2S_DOUT_PIN]))
    cg.add(var.set_buffer_frames(config[CONF_BUFFER_FRAMES]))

    # Add raop_core to include path
    cg.add_build_flag("-I$PROJECT_DIR/src/esphome/components/raop_media_player")