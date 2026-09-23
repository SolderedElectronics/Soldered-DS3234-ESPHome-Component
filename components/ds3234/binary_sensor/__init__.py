import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import CONF_ID

from .. import CONF_DS3234_ID, DS3234Component, ds3234_ns

CONF_ALARM_1 = "alarm_1"
CONF_ALARM_2 = "alarm_2"

DS3234AlarmBinarySensor = ds3234_ns.class_(
    "DS3234AlarmBinarySensor",
    cg.PollingComponent,
    cg.Parented.template(DS3234Component),
)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(DS3234AlarmBinarySensor),
            cv.GenerateID(CONF_DS3234_ID): cv.use_id(DS3234Component),
            cv.Optional(CONF_ALARM_1): binary_sensor.binary_sensor_schema(),
            cv.Optional(CONF_ALARM_2): binary_sensor.binary_sensor_schema(),
        }
    ).extend(cv.polling_component_schema("1s")),
    cv.has_at_least_one_key(CONF_ALARM_1, CONF_ALARM_2),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_DS3234_ID])
    if alarm_1_config := config.get(CONF_ALARM_1):
        sens = await binary_sensor.new_binary_sensor(alarm_1_config)
        cg.add(var.set_alarm_1_binary_sensor(sens))
    if alarm_2_config := config.get(CONF_ALARM_2):
        sens = await binary_sensor.new_binary_sensor(alarm_2_config)
        cg.add(var.set_alarm_2_binary_sensor(sens))
