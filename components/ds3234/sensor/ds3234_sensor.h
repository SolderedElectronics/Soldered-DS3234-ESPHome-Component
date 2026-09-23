/**
 * @file ds3234_sensor.h
 * @brief Temperature sensor platform for the ds3234 ESPHome component
 * @author Soldered Electronics
 */

#pragma once

#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"
#include "../ds3234.h"

namespace esphome {
namespace ds3234 {

/**
 * @brief Publishes the DS3234 die temperature (0.25 degree resolution).
 *
 * The chip itself converts the temperature every 64 seconds for its TCXO, this only reads the latest result.
 */
class DS3234TemperatureSensor : public sensor::Sensor, public PollingComponent, public Parented<DS3234Component> {
 public:
  void update() override;
  void dump_config() override;
};

}  // namespace ds3234
}  // namespace esphome
