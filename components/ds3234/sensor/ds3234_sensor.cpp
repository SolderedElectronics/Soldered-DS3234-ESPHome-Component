/**
 * @file ds3234_sensor.cpp
 * @brief Temperature sensor platform for the ds3234 ESPHome component
 * @author Soldered Electronics
 */

#include "ds3234_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ds3234 {

static const char *const TAG = "ds3234.sensor";

void DS3234TemperatureSensor::update() {
  float celsius;
  if (this->parent_->read_temperature(celsius)) {
    this->publish_state(celsius);
  } else {
    this->status_set_warning();
  }
}

void DS3234TemperatureSensor::dump_config() {
  LOG_SENSOR("", "DS3234 Temperature", this);
  LOG_UPDATE_INTERVAL(this);
}

}  // namespace ds3234
}  // namespace esphome
