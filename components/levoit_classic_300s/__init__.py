import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import (
    binary_sensor,
    fan,
    light,
    number,
    select,
    sensor,
    text_sensor,
    uart,
)
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_HUMIDITY,
    DEVICE_CLASS_PROBLEM,
    DEVICE_CLASS_TEMPERATURE,
    ENTITY_CATEGORY_CONFIG,
    ENTITY_CATEGORY_DIAGNOSTIC,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
    UNIT_PERCENT,
)

CODEOWNERS = ["@MikeG3D2"]
DEPENDENCIES = ["uart"]
AUTO_LOAD = [
    "binary_sensor",
    "fan",
    "light",
    "number",
    "select",
    "sensor",
    "text_sensor",
]

CONF_COMMAND_INTERVAL = "command_interval"
CONF_HUMIDIFIER = "humidifier"
CONF_MODE = "mode"
CONF_TARGET_HUMIDITY = "target_humidity"
CONF_NIGHT_LIGHT = "night_light"
CONF_CURRENT_HUMIDITY = "current_humidity"
CONF_TEMPERATURE = "temperature"
CONF_TANK_LIFTED = "tank_lifted"
CONF_RAW_STATUS = "raw_status"

levoit_ns = cg.esphome_ns.namespace("levoit_classic_300s")
LevoitClassic300S = levoit_ns.class_(
    "LevoitClassic300S", cg.PollingComponent, uart.UARTDevice
)
LevoitHumidifierFan = levoit_ns.class_("LevoitHumidifierFan", fan.Fan)
LevoitModeSelect = levoit_ns.class_("LevoitModeSelect", select.Select)
LevoitTargetHumidityNumber = levoit_ns.class_(
    "LevoitTargetHumidityNumber", number.Number
)
LevoitNightLight = levoit_ns.class_("LevoitNightLight", light.LightOutput)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(LevoitClassic300S),
            cv.Optional(
                CONF_COMMAND_INTERVAL, default="100ms"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_HUMIDIFIER): fan.fan_schema(
                LevoitHumidifierFan,
                icon="mdi:air-humidifier",
                default_restore_mode="ALWAYS_OFF",
            ),
            cv.Optional(CONF_MODE): select.select_schema(
                LevoitModeSelect,
                icon="mdi:state-machine",
                entity_category=ENTITY_CATEGORY_CONFIG,
            ),
            cv.Optional(CONF_TARGET_HUMIDITY): number.number_schema(
                LevoitTargetHumidityNumber,
                icon="mdi:water-percent",
                unit_of_measurement=UNIT_PERCENT,
                entity_category=ENTITY_CATEGORY_CONFIG,
            ),
            cv.Optional(CONF_NIGHT_LIGHT): light.light_schema(
                LevoitNightLight,
                light.LightType.BRIGHTNESS_ONLY,
                icon="mdi:lightbulb-night",
                default_restore_mode="ALWAYS_OFF",
            ),
            cv.Optional(CONF_CURRENT_HUMIDITY): sensor.sensor_schema(
                unit_of_measurement=UNIT_PERCENT,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_HUMIDITY,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_TEMPERATURE): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_TANK_LIFTED): binary_sensor.binary_sensor_schema(
                device_class=DEVICE_CLASS_PROBLEM,
                icon="mdi:cup-water",
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
            cv.Optional(CONF_RAW_STATUS): text_sensor.text_sensor_schema(
                icon="mdi:code-brackets",
                entity_category=ENTITY_CATEGORY_DIAGNOSTIC,
            ),
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.polling_component_schema("30s"))
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "levoit_classic_300s",
    baud_rate=9600,
    require_tx=True,
    require_rx=True,
    data_bits=8,
    parity="NONE",
    stop_bits=1,
)


async def to_code(config):
    parent = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(parent, config)
    await uart.register_uart_device(parent, config)
    cg.add(parent.set_command_interval(config[CONF_COMMAND_INTERVAL]))

    if entity_config := config.get(CONF_HUMIDIFIER):
        entity = await fan.new_fan(entity_config, parent)
        cg.add(parent.set_humidifier(entity))

    if entity_config := config.get(CONF_MODE):
        entity = await select.new_select(
            entity_config, parent, options=["Auto", "Manual"]
        )
        cg.add(parent.set_mode_select(entity))

    if entity_config := config.get(CONF_TARGET_HUMIDITY):
        entity = await number.new_number(
            entity_config,
            parent,
            min_value=30,
            max_value=80,
            step=1,
        )
        cg.add(parent.set_target_humidity_number(entity))

    if entity_config := config.get(CONF_NIGHT_LIGHT):
        entity = await light.new_light(entity_config, parent)
        cg.add(parent.set_night_light(entity))

    if entity_config := config.get(CONF_CURRENT_HUMIDITY):
        entity = await sensor.new_sensor(entity_config)
        cg.add(parent.set_current_humidity_sensor(entity))

    if entity_config := config.get(CONF_TEMPERATURE):
        entity = await sensor.new_sensor(entity_config)
        cg.add(parent.set_temperature_sensor(entity))

    if entity_config := config.get(CONF_TANK_LIFTED):
        entity = await binary_sensor.new_binary_sensor(entity_config)
        cg.add(parent.set_tank_lifted_binary_sensor(entity))

    if entity_config := config.get(CONF_RAW_STATUS):
        entity = await text_sensor.new_text_sensor(entity_config)
        cg.add(parent.set_raw_status_text_sensor(entity))
