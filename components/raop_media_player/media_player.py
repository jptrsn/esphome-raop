import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import media_player, i2s_audio
from esphome.const import CONF_ID, CONF_MODE
from esphome.core import CORE

CODEOWNERS = ["@yourgithubusername"]  # Replace with your GitHub username
DEPENDENCIES = ["i2s_audio", "psram"]

raop_media_player_ns = cg.esphome_ns.namespace("raop_media_player")
RAOPMediaPlayer = raop_media_player_ns.class_(
    "RAOPMediaPlayer", cg.Component, media_player.MediaPlayer
)

CONF_I2S_DOUT_PIN = "i2s_dout_pin"
CONF_I2S_AUDIO_ID = "i2s_audio_id"
CONF_BUFFER_FRAMES = "buffer_frames"

CONFIG_SCHEMA = cv.All(
    media_player.MEDIA_PLAYER_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(RAOPMediaPlayer),
            cv.Required(CONF_I2S_DOUT_PIN): pins.internal_gpio_output_pin_number,
            cv.Optional(CONF_I2S_AUDIO_ID): cv.use_id(i2s_audio.I2SAudioComponent),
            cv.Optional(CONF_BUFFER_FRAMES, default=1024): cv.int_range(min=512, max=2048),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_with_esp_idf,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await media_player.register_media_player(var, config)

    if CONF_I2S_AUDIO_ID in config:
        i2s_audio_component = await cg.get_variable(config[CONF_I2S_AUDIO_ID])
        cg.add(var.set_i2s_audio_parent(i2s_audio_component))

    cg.add(var.set_dout_pin(config[CONF_I2S_DOUT_PIN]))
    cg.add(var.set_buffer_frames(config[CONF_BUFFER_FRAMES]))

    # Validate ESP-IDF framework
    if CORE.using_arduino:
        raise cv.Invalid("RAOP media player requires ESP-IDF framework")

    # Add include paths for raop_core
    cg.add_build_flag("-Icomponents/raop_media_player/raop_core")
    cg.add_build_flag("-Icomponents/raop_media_player/raop_core/codecs/alac")

    # Link ALAC library
    cg.add_build_flag("-Lcomponents/raop_media_player/raop_core/codecs/alac")
    cg.add_build_flag("-lalac")