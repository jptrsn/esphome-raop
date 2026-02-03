import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import media_player
from esphome.const import CONF_ID

CODEOWNERS = ["@yourgithubusername"]  # Replace with your GitHub username
DEPENDENCIES = ["i2s_audio"]

raop_media_player_ns = cg.esphome_ns.namespace("raop_media_player")
RAOPMediaPlayer = raop_media_player_ns.class_(
    "RAOPMediaPlayer", cg.Component, media_player.MediaPlayer
)

CONFIG_SCHEMA = media_player.MEDIA_PLAYER_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(RAOPMediaPlayer),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await media_player.register_media_player(var, config)