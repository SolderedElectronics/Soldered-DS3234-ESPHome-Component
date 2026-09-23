/**
 * @file ds3234_binary_sensor.h
 * @brief Alarm binary sensor platform for the ds3234 ESPHome component
 * @author Soldered Electronics
 */

#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/core/component.h"
#include "../ds3234.h"

namespace esphome {
namespace ds3234 {

/**
 * @brief Polls the DS3234 alarm flags and reports each fired alarm as a short ON pulse.
 *
 * When an enabled alarm's flag is set, the matching binary sensor turns ON and the flag is cleared (which also
 * releases the SQW/INT pin); the next poll turns it back OFF. Alarms are configured with the ds3234.set_alarm_1 /
 * ds3234.set_alarm_2 actions.
 */
class DS3234AlarmBinarySensor : public PollingComponent, public Parented<DS3234Component> {
 public:
  void set_alarm_1_binary_sensor(binary_sensor::BinarySensor *sensor) { this->alarm_1_ = sensor; }
  void set_alarm_2_binary_sensor(binary_sensor::BinarySensor *sensor) { this->alarm_2_ = sensor; }

  void update() override;
  void dump_config() override;

 protected:
  binary_sensor::BinarySensor *alarm_1_{nullptr};
  binary_sensor::BinarySensor *alarm_2_{nullptr};
};

}  // namespace ds3234
}  // namespace esphome
