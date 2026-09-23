import esphome.codegen as cg
from esphome.components import spi, time
import esphome.config_validation as cv
from esphome.const import CONF_ID

from . import DS3234Component

DEPENDENCIES = ["spi"]

CONFIG_SCHEMA = time.TIME_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(DS3234Component),
    }
).extend(spi.spi_device_schema(cs_pin_required=True))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await spi.register_spi_device(var, config)
    await time.register_time(var, config)
