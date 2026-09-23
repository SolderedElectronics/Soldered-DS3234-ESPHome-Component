/**
 * @file ds3234.cpp
 * @brief Implementation for the ds3234 ESPHome component
 * @author Soldered Electronics
 */

#include "ds3234.h"
#include "esphome/core/log.h"

// Datasheet:
// - https://www.analog.com/media/en/technical-documentation/data-sheets/DS3234.pdf

namespace esphome {
namespace ds3234 {

static const char *const TAG = "ds3234";

static const uint8_t DS3234_REG_SECONDS = 0x00;
static const uint8_t DS3234_REG_A1_SECONDS = 0x07;
static const uint8_t DS3234_REG_A2_MINUTES = 0x0B;
static const uint8_t DS3234_REG_CONTROL = 0x0E;
static const uint8_t DS3234_REG_STATUS = 0x0F;
static const uint8_t DS3234_REG_TEMP_MSB = 0x11;

/// Set in the address byte of a transfer to write instead of read.
static const uint8_t DS3234_WRITE_BIT = 0x80;

static const uint8_t DS3234_HOUR_12H = 0x40;
static const uint8_t DS3234_HOUR_PM = 0x20;

static const uint8_t DS3234_CONTROL_EOSC = 0x80;
static const uint8_t DS3234_CONTROL_INTCN = 0x04;
static const uint8_t DS3234_CONTROL_A2IE = 0x02;
static const uint8_t DS3234_CONTROL_A1IE = 0x01;

static const uint8_t DS3234_STATUS_OSF = 0x80;
static const uint8_t DS3234_STATUS_A2F = 0x02;
static const uint8_t DS3234_STATUS_A1F = 0x01;

/// AxMx bit in every alarm register: 1 = field is left out of the alarm match.
static const uint8_t DS3234_ALARM_MASK = 0x80;
/// DY/DT bit in the alarm day register: 1 = day of week, 0 = day of month.
static const uint8_t DS3234_ALARM_DAY_OF_WEEK = 0x40;

static uint8_t bcd_to_bin(uint8_t bcd) { return (bcd >> 4) * 10 + (bcd & 0x0F); }
static uint8_t bin_to_bcd(uint8_t bin) { return ((bin / 10) << 4) | (bin % 10); }

void DS3234Component::setup() {
  this->spi_setup();

  // SPI has no ACK, so check for a bus that reads back all 0xFF (MISO floating high / no chip) or all 0x00. A real
  // DS3234 never reads all 0x00: the day register is 1 - 7 and the control register powers up as 0x1C.
  uint8_t regs[DS3234_REG_STATUS + 1];
  this->read_registers_(DS3234_REG_SECONDS, regs, sizeof(regs));
  bool all_ff = true;
  bool all_00 = true;
  for (uint8_t reg : regs) {
    all_ff &= reg == 0xFF;
    all_00 &= reg == 0x00;
  }
  if (all_ff || all_00) {
    ESP_LOGE(TAG, "No response from DS3234 (all registers read 0x%02X)", regs[0]);
    this->mark_failed();
    return;
  }

  // Sync right away instead of waiting for the first update(), so on_boot automations already see the RTC time.
  this->read_time();
}

void DS3234Component::update() { this->read_time(); }

void DS3234Component::dump_config() {
  ESP_LOGCONFIG(TAG, "DS3234:");
  LOG_PIN("  CS Pin: ", this->cs_);
  if (this->is_failed()) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
  }
  RealTimeClock::dump_config();
}

void DS3234Component::read_time() {
  if (this->is_failed()) {
    return;
  }

  if (this->read_register_(DS3234_REG_STATUS) & DS3234_STATUS_OSF) {
    ESP_LOGW(TAG, "Oscillator stop flag set (power lost or oscillator halted), not syncing to system clock. Use "
                  "ds3234.write_time to set the RTC.");
    return;
  }

  uint8_t regs[7];
  this->read_registers_(DS3234_REG_SECONDS, regs, sizeof(regs));

  uint8_t hour;
  if (regs[2] & DS3234_HOUR_12H) {
    // 12-hour mode, e.g. set by other firmware: 12 AM = 00, 1 - 11 PM = 13 - 23.
    hour = bcd_to_bin(regs[2] & 0x1F) % 12;
    if (regs[2] & DS3234_HOUR_PM)
      hour += 12;
  } else {
    hour = bcd_to_bin(regs[2] & 0x3F);
  }

  ESPTime rtc_time{
      .second = bcd_to_bin(regs[0] & 0x7F),
      .minute = bcd_to_bin(regs[1] & 0x7F),
      .hour = hour,
      .day_of_week = uint8_t(regs[3] & 0x07),
      .day_of_month = bcd_to_bin(regs[4] & 0x3F),
      .month = bcd_to_bin(regs[5] & 0x1F),  // bit 7 is the century bit, ignored
      .year = uint16_t(bcd_to_bin(regs[6]) + 2000),
  };
  ESP_LOGD(TAG, "Read  %04u-%02u-%02u %02u:%02u:%02u (weekday %u)", rtc_time.year, rtc_time.month,
           rtc_time.day_of_month, rtc_time.hour, rtc_time.minute, rtc_time.second, rtc_time.day_of_week);

  rtc_time.recalc_timestamp_utc(false);
  if (!rtc_time.is_valid(/*check_day_of_week=*/true, /*check_day_of_year=*/false)) {
    ESP_LOGE(TAG, "Invalid RTC time, not syncing to system clock.");
    return;
  }
  time::RealTimeClock::synchronize_epoch_(rtc_time.timestamp);
}

void DS3234Component::write_time() {
  if (this->is_failed()) {
    return;
  }

  auto now = time::RealTimeClock::utcnow();
  if (!now.is_valid() || now.year < 2000 || now.year > 2099) {
    ESP_LOGE(TAG, "Invalid system time, not syncing to RTC.");
    return;
  }

  // Always written in 24-hour mode (bit 6 of the hour register cleared), day of week 1 - 7 with Sunday = 1.
  uint8_t regs[7];
  regs[0] = bin_to_bcd(now.second);
  regs[1] = bin_to_bcd(now.minute);
  regs[2] = bin_to_bcd(now.hour);
  regs[3] = now.day_of_week;
  regs[4] = bin_to_bcd(now.day_of_month);
  regs[5] = bin_to_bcd(now.month);
  regs[6] = bin_to_bcd(now.year - 2000);
  this->write_registers_(DS3234_REG_SECONDS, regs, sizeof(regs));

  // Time is valid again: clear the oscillator stop flag and make sure the oscillator keeps running on battery.
  uint8_t status = this->read_register_(DS3234_REG_STATUS);
  if (status & DS3234_STATUS_OSF)
    this->write_register_(DS3234_REG_STATUS, status & ~DS3234_STATUS_OSF);
  uint8_t control = this->read_register_(DS3234_REG_CONTROL);
  if (control & DS3234_CONTROL_EOSC)
    this->write_register_(DS3234_REG_CONTROL, control & ~DS3234_CONTROL_EOSC);

  ESP_LOGD(TAG, "Write %04u-%02u-%02u %02u:%02u:%02u (weekday %u)", now.year, now.month, now.day_of_month, now.hour,
           now.minute, now.second, now.day_of_week);
}

bool DS3234Component::read_temperature(float &celsius) {
  if (this->is_failed()) {
    return false;
  }
  uint8_t regs[2];
  this->read_registers_(DS3234_REG_TEMP_MSB, regs, sizeof(regs));
  // MSB is the signed integer part, bits 7:6 of the LSB are the fraction in 0.25 degree steps.
  celsius = int8_t(regs[0]) + (regs[1] >> 6) * 0.25f;
  return true;
}

bool DS3234Component::read_and_clear_alarm_flags(bool &alarm_1, bool &alarm_2) {
  if (this->is_failed()) {
    return false;
  }
  uint8_t regs[2];
  this->read_registers_(DS3234_REG_CONTROL, regs, sizeof(regs));
  const uint8_t control = regs[0];
  const uint8_t status = regs[1];

  alarm_1 = (status & DS3234_STATUS_A1F) && (control & DS3234_CONTROL_A1IE);
  alarm_2 = (status & DS3234_STATUS_A2F) && (control & DS3234_CONTROL_A2IE);

  // Clearing the flag also releases the (active low) SQW/INT pin.
  uint8_t clear = (alarm_1 ? DS3234_STATUS_A1F : 0) | (alarm_2 ? DS3234_STATUS_A2F : 0);
  if (clear)
    this->write_register_(DS3234_REG_STATUS, status & ~clear);
  return true;
}

void DS3234Component::set_alarm_1(int second, int minute, int hour, int day, bool day_is_weekday) {
  const int values[4] = {second, minute, hour, day};
  this->write_alarm_(1, values, 4, day_is_weekday);
}

void DS3234Component::set_alarm_2(int minute, int hour, int day, bool day_is_weekday) {
  const int values[3] = {minute, hour, day};
  this->write_alarm_(2, values, 3, day_is_weekday);
}

void DS3234Component::write_alarm_(uint8_t alarm, const int *values, size_t count, bool day_is_weekday) {
  if (this->is_failed()) {
    return;
  }

  // values[] runs from the finest field to the coarsest one, the last entry is always the day.
  static const char *const NAMES_1[] = {"second", "minute", "hour", "day"};
  static const char *const NAMES_2[] = {"minute", "hour", "day"};
  static const int MAX_1[] = {59, 59, 23, 31};
  static const int MAX_2[] = {59, 23, 31};
  const char *const *names = alarm == 1 ? NAMES_1 : NAMES_2;
  const int *max_values = alarm == 1 ? MAX_1 : MAX_2;

  uint8_t regs[4];
  bool seen_dont_care = false;
  for (size_t i = 0; i < count; i++) {
    const bool is_day = i == count - 1;
    if (values[i] == DS3234_ALARM_DONT_CARE) {
      seen_dont_care = true;
      regs[i] = DS3234_ALARM_MASK;
      continue;
    }
    if (seen_dont_care) {
      ESP_LOGE(TAG, "Alarm %u: '%s' is set but a finer field is not, the DS3234 cannot match that", alarm, names[i]);
      return;
    }
    const int min_value = is_day ? 1 : 0;
    const int max_value = is_day && day_is_weekday ? 7 : max_values[i];
    if (values[i] < min_value || values[i] > max_value) {
      ESP_LOGE(TAG, "Alarm %u: %s %d out of range (%d - %d)", alarm, names[i], values[i], min_value, max_value);
      return;
    }
    // Hours are always written in 24-hour mode.
    regs[i] = bin_to_bcd(values[i]);
    if (is_day && day_is_weekday)
      regs[i] |= DS3234_ALARM_DAY_OF_WEEK;
  }

  this->write_registers_(alarm == 1 ? DS3234_REG_A1_SECONDS : DS3234_REG_A2_MINUTES, regs, count);

  // Drop a stale flag from an earlier match, then route the alarm to the SQW/INT pin. INTCN = 1 turns the square
  // wave output off.
  const uint8_t flag = alarm == 1 ? DS3234_STATUS_A1F : DS3234_STATUS_A2F;
  const uint8_t status = this->read_register_(DS3234_REG_STATUS);
  if (status & flag)
    this->write_register_(DS3234_REG_STATUS, status & ~flag);
  const uint8_t control = this->read_register_(DS3234_REG_CONTROL);
  this->write_register_(DS3234_REG_CONTROL,
                        control | DS3234_CONTROL_INTCN | (alarm == 1 ? DS3234_CONTROL_A1IE : DS3234_CONTROL_A2IE));

  ESP_LOGD(TAG, "Alarm %u set (registers 0x%02X 0x%02X 0x%02X 0x%02X)", alarm, regs[0], regs[1], regs[2],
           count == 4 ? regs[3] : 0);
}

void DS3234Component::disable_alarm(uint8_t alarm) {
  if (this->is_failed()) {
    return;
  }
  if (alarm != 1 && alarm != 2) {
    ESP_LOGE(TAG, "Invalid alarm %u, must be 1 or 2", alarm);
    return;
  }
  const uint8_t control = this->read_register_(DS3234_REG_CONTROL);
  this->write_register_(DS3234_REG_CONTROL, control & ~(alarm == 1 ? DS3234_CONTROL_A1IE : DS3234_CONTROL_A2IE));
  const uint8_t flag = alarm == 1 ? DS3234_STATUS_A1F : DS3234_STATUS_A2F;
  const uint8_t status = this->read_register_(DS3234_REG_STATUS);
  if (status & flag)
    this->write_register_(DS3234_REG_STATUS, status & ~flag);
  ESP_LOGD(TAG, "Alarm %u disabled", alarm);
}

void DS3234Component::read_registers_(uint8_t reg, uint8_t *data, size_t len) {
  this->enable();
  this->write_byte(reg);
  this->read_array(data, len);
  this->disable();
}

void DS3234Component::write_registers_(uint8_t reg, const uint8_t *data, size_t len) {
  this->enable();
  this->write_byte(reg | DS3234_WRITE_BIT);
  this->write_array(data, len);
  this->disable();
}

uint8_t DS3234Component::read_register_(uint8_t reg) {
  uint8_t value;
  this->read_registers_(reg, &value, 1);
  return value;
}

}  // namespace ds3234
}  // namespace esphome
