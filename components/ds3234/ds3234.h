/**
 * @file ds3234.h
 * @brief Public API for the ds3234 ESPHome component
 * @author Soldered Electronics
 */

#pragma once

#include "esphome/components/spi/spi.h"
#include "esphome/components/time/real_time_clock.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"

namespace esphome {
namespace ds3234 {

/// Value passed to set_alarm_1() / set_alarm_2() for a field that should not take part in the alarm match.
static const int DS3234_ALARM_DONT_CARE = -1;

/**
 * @brief DS3234 SPI real-time clock exposed as an ESPHome time source.
 *
 * Keeps the ESPHome system clock in sync with the RTC (read_time()) and can push the system clock back into the RTC
 * (write_time()), same as the upstream ds1307/pcf85063 components. The chip is always written in 24-hour mode; a
 * 12-hour mode left over from other firmware is still decoded correctly on read.
 *
 * The year register is interpreted as 2000 - 2099, the century bit is ignored (same as the Soldered DS3234 Arduino
 * library).
 */
class DS3234Component : public time::RealTimeClock,
                        public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_HIGH,
                                              spi::CLOCK_PHASE_TRAILING, spi::DATA_RATE_4MHZ> {
 public:
  void setup() override;
  void update() override;
  void dump_config() override;

  /// Read the RTC and synchronize the system clock to it. Skipped if the oscillator stop flag is set.
  void read_time();
  /// Write the current system clock to the RTC, clear the oscillator stop flag and enable the oscillator.
  void write_time();

  /**
   * @brief Read the on-chip temperature sensor.
   *
   * @param[out] celsius Temperature in degrees Celsius, 0.25 degree resolution.
   * @return true on success.
   */
  bool read_temperature(float &celsius);

  /**
   * @brief Read the alarm flags and clear the ones that are set.
   *
   * A flag only counts as fired if its alarm interrupt is enabled (set_alarm_1() / set_alarm_2() enable it,
   * disable_alarm() disables it), so a disabled alarm never reports as fired.
   *
   * @param[out] alarm_1 true if alarm 1 fired since the last call.
   * @param[out] alarm_2 true if alarm 2 fired since the last call.
   * @return true on success.
   */
  bool read_and_clear_alarm_flags(bool &alarm_1, bool &alarm_2);

  /**
   * @brief Configure and enable alarm 1 (seconds resolution).
   *
   * Fields set to DS3234_ALARM_DONT_CARE are masked out of the match. The chip only supports masking from the
   * coarsest field down: if a field is set, every finer field must be set as well.
   *
   * @param second 0 - 59, or DS3234_ALARM_DONT_CARE.
   * @param minute 0 - 59, or DS3234_ALARM_DONT_CARE.
   * @param hour 0 - 23, or DS3234_ALARM_DONT_CARE.
   * @param day Day of month (1 - 31) or day of week (1 - 7, Sunday = 1), or DS3234_ALARM_DONT_CARE.
   * @param day_is_weekday true if @p day is a day of week, false if it is a day of month.
   */
  void set_alarm_1(int second, int minute, int hour, int day, bool day_is_weekday);

  /**
   * @brief Configure and enable alarm 2 (minute resolution, fires at second 00).
   *
   * Same masking rules as set_alarm_1().
   */
  void set_alarm_2(int minute, int hour, int day, bool day_is_weekday);

  /**
   * @brief Disable an alarm's interrupt and clear its flag.
   *
   * @param alarm 1 or 2.
   */
  void disable_alarm(uint8_t alarm);

 protected:
  void read_registers_(uint8_t reg, uint8_t *data, size_t len);
  void write_registers_(uint8_t reg, const uint8_t *data, size_t len);
  uint8_t read_register_(uint8_t reg);
  void write_register_(uint8_t reg, uint8_t value) { this->write_registers_(reg, &value, 1); }
  void write_alarm_(uint8_t alarm, const int *values, size_t count, bool day_is_weekday);
};

template<typename... Ts> class WriteAction : public Action<Ts...>, public Parented<DS3234Component> {
 public:
  void play(const Ts &...x) override { this->parent_->write_time(); }
};

template<typename... Ts> class ReadAction : public Action<Ts...>, public Parented<DS3234Component> {
 public:
  void play(const Ts &...x) override { this->parent_->read_time(); }
};

template<typename... Ts> class SetAlarm1Action : public Action<Ts...>, public Parented<DS3234Component> {
 public:
  TEMPLATABLE_VALUE(int, second)
  TEMPLATABLE_VALUE(int, minute)
  TEMPLATABLE_VALUE(int, hour)
  TEMPLATABLE_VALUE(int, day)
  void set_day_is_weekday(bool day_is_weekday) { this->day_is_weekday_ = day_is_weekday; }

  void play(const Ts &...x) override {
    this->parent_->set_alarm_1(this->second_.has_value() ? this->second_.value(x...) : DS3234_ALARM_DONT_CARE,
                               this->minute_.has_value() ? this->minute_.value(x...) : DS3234_ALARM_DONT_CARE,
                               this->hour_.has_value() ? this->hour_.value(x...) : DS3234_ALARM_DONT_CARE,
                               this->day_.has_value() ? this->day_.value(x...) : DS3234_ALARM_DONT_CARE,
                               this->day_is_weekday_);
  }

 protected:
  bool day_is_weekday_{false};
};

template<typename... Ts> class SetAlarm2Action : public Action<Ts...>, public Parented<DS3234Component> {
 public:
  TEMPLATABLE_VALUE(int, minute)
  TEMPLATABLE_VALUE(int, hour)
  TEMPLATABLE_VALUE(int, day)
  void set_day_is_weekday(bool day_is_weekday) { this->day_is_weekday_ = day_is_weekday; }

  void play(const Ts &...x) override {
    this->parent_->set_alarm_2(this->minute_.has_value() ? this->minute_.value(x...) : DS3234_ALARM_DONT_CARE,
                               this->hour_.has_value() ? this->hour_.value(x...) : DS3234_ALARM_DONT_CARE,
                               this->day_.has_value() ? this->day_.value(x...) : DS3234_ALARM_DONT_CARE,
                               this->day_is_weekday_);
  }

 protected:
  bool day_is_weekday_{false};
};

template<typename... Ts> class DisableAlarmAction : public Action<Ts...>, public Parented<DS3234Component> {
 public:
  void set_alarm(uint8_t alarm) { this->alarm_ = alarm; }
  void play(const Ts &...x) override { this->parent_->disable_alarm(this->alarm_); }

 protected:
  uint8_t alarm_{1};
};

}  // namespace ds3234
}  // namespace esphome
