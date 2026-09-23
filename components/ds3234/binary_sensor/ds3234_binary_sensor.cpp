/**
 * @file ds3234_binary_sensor.cpp
 * @brief Alarm binary sensor platform for the ds3234 ESPHome component
 * @author Soldered Electronics
 */

#include "ds3234_binary_sensor.h"
#include "esphome/core/log.h"

namespace esphome {
namespace ds3234 {

static const char *const TAG = "ds3234.binary_sensor";

void DS3234AlarmBinarySensor::update() {
  bool alarm_1, alarm_2;
  if (!this->parent_->read_and_clear_alarm_flags(alarm_1, alarm_2)) {
    this->status_set_warning();
    return;
  }
  if (this->alarm_1_ != nullptr)
    this->alarm_1_->publish_state(alarm_1);
  if (this->alarm_2_ != nullptr)
    this->alarm_2_->publish_state(alarm_2);
}

void DS3234AlarmBinarySensor::dump_config() {
  ESP_LOGCONFIG(TAG, "DS3234 Alarms:");
  LOG_UPDATE_INTERVAL(this);
  LOG_BINARY_SENSOR("  ", "Alarm 1", this->alarm_1_);
  LOG_BINARY_SENSOR("  ", "Alarm 2", this->alarm_2_);
}

}  // namespace ds3234
}  // namespace esphome
