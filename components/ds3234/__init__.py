from esphome import automation
import esphome.codegen as cg
from esphome.components import spi, time
import esphome.config_validation as cv
from esphome.const import CONF_DAY, CONF_HOUR, CONF_ID, CONF_MINUTE, CONF_SECOND

CODEOWNERS = ["@SolderedElectronics"]
DEPENDENCIES = ["spi"]

CONF_DS3234_ID = "ds3234_id"
CONF_ALARM = "alarm"
CONF_DAY_OF_MONTH = "day_of_month"
CONF_DAY_OF_WEEK = "day_of_week"

ds3234_ns = cg.esphome_ns.namespace("ds3234")
DS3234Component = ds3234_ns.class_(
    "DS3234Component", time.RealTimeClock, spi.SPIDevice
)
WriteAction = ds3234_ns.class_("WriteAction", automation.Action)
ReadAction = ds3234_ns.class_("ReadAction", automation.Action)
SetAlarm1Action = ds3234_ns.class_("SetAlarm1Action", automation.Action)
SetAlarm2Action = ds3234_ns.class_("SetAlarm2Action", automation.Action)
DisableAlarmAction = ds3234_ns.class_("DisableAlarmAction", automation.Action)


def _validate_alarm_fields(fields):
    """The DS3234 can only mask alarm fields from the coarsest one down.

    `fields` runs from the finest field to the coarsest one: once a field is left out, no coarser field may be set.
    """

    def is_set(config, key):
        if key == CONF_DAY:
            return CONF_DAY_OF_MONTH in config or CONF_DAY_OF_WEEK in config
        return key in config

    def validator(config):
        for finer, coarser in zip(fields, fields[1:]):
            if is_set(config, coarser) and not is_set(config, finer):
                coarser_name = (
                    "day_of_month/day_of_week" if coarser == CONF_DAY else coarser
                )
                raise cv.Invalid(
                    f"'{coarser_name}' requires '{finer}' to be set as well, the DS3234 can only leave out "
                    "fields from the coarsest one down"
                )
        return config

    return validator


def _alarm_schema(fields):
    ranges = {
        CONF_SECOND: cv.int_range(min=0, max=59),
        CONF_MINUTE: cv.int_range(min=0, max=59),
        CONF_HOUR: cv.int_range(min=0, max=23),
    }
    schema = {cv.GenerateID(): cv.use_id(DS3234Component)}
    for key in fields:
        if key != CONF_DAY:
            schema[cv.Optional(key)] = cv.templatable(ranges[key])
    schema[cv.Exclusive(CONF_DAY_OF_MONTH, CONF_DAY)] = cv.templatable(
        cv.int_range(min=1, max=31)
    )
    schema[cv.Exclusive(CONF_DAY_OF_WEEK, CONF_DAY)] = cv.templatable(
        cv.int_range(min=1, max=7)
    )
    return cv.All(cv.Schema(schema), _validate_alarm_fields(fields))


async def _set_alarm_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    for key in (CONF_SECOND, CONF_MINUTE, CONF_HOUR):
        if key in config:
            templ = await cg.templatable(config[key], args, cg.int_)
            cg.add(getattr(var, f"set_{key}")(templ))
    if CONF_DAY_OF_MONTH in config:
        templ = await cg.templatable(config[CONF_DAY_OF_MONTH], args, cg.int_)
        cg.add(var.set_day(templ))
    if CONF_DAY_OF_WEEK in config:
        templ = await cg.templatable(config[CONF_DAY_OF_WEEK], args, cg.int_)
        cg.add(var.set_day(templ))
        cg.add(var.set_day_is_weekday(True))
    return var


@automation.register_action(
    "ds3234.write_time",
    WriteAction,
    automation.maybe_simple_id({cv.GenerateID(): cv.use_id(DS3234Component)}),
    synchronous=True,
)
async def ds3234_write_time_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


@automation.register_action(
    "ds3234.read_time",
    ReadAction,
    automation.maybe_simple_id({cv.GenerateID(): cv.use_id(DS3234Component)}),
    synchronous=True,
)
async def ds3234_read_time_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    return var


automation.register_action(
    "ds3234.set_alarm_1",
    SetAlarm1Action,
    _alarm_schema([CONF_SECOND, CONF_MINUTE, CONF_HOUR, CONF_DAY]),
    synchronous=True,
)(_set_alarm_to_code)

automation.register_action(
    "ds3234.set_alarm_2",
    SetAlarm2Action,
    _alarm_schema([CONF_MINUTE, CONF_HOUR, CONF_DAY]),
    synchronous=True,
)(_set_alarm_to_code)


@automation.register_action(
    "ds3234.disable_alarm",
    DisableAlarmAction,
    cv.Schema(
        {
            cv.GenerateID(): cv.use_id(DS3234Component),
            cv.Required(CONF_ALARM): cv.int_range(min=1, max=2),
        }
    ),
    synchronous=True,
)
async def ds3234_disable_alarm_to_code(config, action_id, template_arg, args):
    var = cg.new_Pvariable(action_id, template_arg)
    await cg.register_parented(var, config[CONF_ID])
    cg.add(var.set_alarm(config[CONF_ALARM]))
    return var
