import esphome.codegen as cg
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_TEMPERATURE,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
)

from .. import CONF_DS3234_ID, DS3234Component, ds3234_ns

DS3234TemperatureSensor = ds3234_ns.class_(
    "DS3234TemperatureSensor",
    sensor.Sensor,
    cg.PollingComponent,
    cg.Parented.template(DS3234Component),
)

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        DS3234TemperatureSensor,
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=2,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
        entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
    )
    .extend({cv.GenerateID(CONF_DS3234_ID): cv.use_id(DS3234Component)})
    .extend(cv.polling_component_schema("60s"))
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_DS3234_ID])
