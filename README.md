# Soldered DS3234 ESPHome Component

| ![DS3234 RTC Breakout](https://cms.soldered.com/products/333358/media/333358_featured-photo_122004.jpg) |
| :----------------------------------------------------------------------------------------------------: |
|                             [DS3234 RTC Breakout](https://www.solde.red/333358)                             |

Breakout board for the Analog Devices (Maxim) DS3234, an extremely accurate SPI real-time clock with an integrated
temperature-compensated crystal oscillator (TCXO, ±2 ppm from 0 °C to +40 °C). It keeps seconds through year with
leap-year compensation up to 2099, has two programmable alarms on an active-low SQW/INT pin, an on-chip temperature
sensor and a CR2032 battery holder that keeps the time running through power loss.

External ESPHome component for the Soldered DS3234 RTC breakout board. It is a port of the
[Soldered DS3234 RTC Arduino library](https://github.com/SolderedElectronics/Soldered-DS3234-RTC-Arduino-Library)
and follows the upstream ESPHome [DS1307](https://esphome.io/components/time/ds1307/) /
[PCF85063](https://esphome.io/components/time/pcf85063/) components: the DS3234 is a
[time](https://esphome.io/components/time/) platform that syncs the ESPHome system clock on boot, and can be written
back from a network time source such as SNTP or Home Assistant. It also exposes the die temperature as a `sensor` and
the two alarms as `binary_sensor`s.

## Repository Contents

- **components/** - the ESPHome external component (Python config + C++ implementation)
- **examples/** - example YAML configs showing how to use the component

## Usage

Reference this repo directly from your own ESPHome YAML (no need to clone it locally):

```yaml
external_components:
  - source: github://SolderedElectronics/Soldered-DS3234-ESPHome-Component
    components: [ds3234]

spi:
  clk_pin: GPIO12
  mosi_pin: GPIO11
  miso_pin: GPIO13

time:
  - platform: ds3234
    id: rtc
    cs_pin: GPIO10
  - platform: sntp
    on_time_sync:
      then:
        - ds3234.write_time: rtc

sensor:
  - platform: ds3234
    name: "RTC Temperature"

binary_sensor:
  - platform: ds3234
    alarm_1:
      name: "RTC Alarm 1"
```

See [`examples/basic.yaml`](examples/basic.yaml) for a full working example.

The RTC always stores UTC; the `timezone` option only affects how ESPHome presents local time, same as every other
time platform. On read, the component checks the DS3234's oscillator stop flag: if the oscillator was stopped (e.g.
the board was without both main power and battery), the time is not trusted and the system clock is left alone until
`ds3234.write_time` runs. `ds3234.write_time` always writes 24-hour mode, clears that flag and makes sure the
oscillator keeps running on battery. The year is stored as 2000 - 2099.

### Configuration variables

#### Time platform

- **id** (*Optional*, [ID](https://esphome.io/guides/configuration-types#config-id)): ID of the RTC, needed for the
  actions below and for the `sensor`/`binary_sensor` platforms if you have more than one DS3234.
- **cs_pin** (**Required**, [Pin](https://esphome.io/guides/configuration-types#config-pin)): chip-select pin of the
  DS3234.
- **spi_id** (*Optional*, [ID](https://esphome.io/guides/configuration-types#config-id)): the SPI bus to use, if you
  have more than one.
- **data_rate** (*Optional*): SPI clock. Defaults to `4MHz`, the DS3234's maximum. The chip runs in SPI mode 3.
- **update_interval** (*Optional*, [Time](https://esphome.io/guides/configuration-types#config-time)): how often to
  re-sync the system clock from the RTC. Defaults to `15min`.
- All other options from [Time](https://esphome.io/components/time/#base-time-configuration) (`timezone`,
  `on_time`, `on_time_sync`, ...).

#### Sensor platform

- **ds3234_id** (*Optional*, [ID](https://esphome.io/guides/configuration-types#config-id)): the DS3234 time
  platform to read from.
- **update_interval** (*Optional*, [Time](https://esphome.io/guides/configuration-types#config-time)): defaults to
  `60s`. The chip converts the temperature every 64 seconds on its own.
- All other options from [Sensor](https://esphome.io/components/sensor/#config-sensor). Publishes the die
  temperature in °C with 0.25 °C resolution.

#### Binary sensor platform

- **ds3234_id** (*Optional*, [ID](https://esphome.io/guides/configuration-types#config-id)): the DS3234 time
  platform to read from.
- **alarm_1** (*Optional*, [Binary Sensor](https://esphome.io/components/binary_sensor/#config-binary-sensor)):
  turns ON when alarm 1 fires.
- **alarm_2** (*Optional*, [Binary Sensor](https://esphome.io/components/binary_sensor/#config-binary-sensor)):
  turns ON when alarm 2 fires.
- **update_interval** (*Optional*, [Time](https://esphome.io/guides/configuration-types#config-time)): how often the
  alarm flags are polled. Defaults to `1s`.

At least one of `alarm_1`/`alarm_2` is required. The alarm flags are polled over SPI, so the SQW/INT pin does not need
to be wired for this. When a fired alarm is seen, its binary sensor turns ON and the flag is cleared (which also
releases the SQW/INT pin); the next poll turns it back OFF, so use `on_press` to react to it. A disabled alarm never
reports as fired.

### Actions

#### `ds3234.write_time`

Writes the current system time to the RTC. Typically used from a network time source's `on_time_sync`.

```yaml
- ds3234.write_time: rtc
```

#### `ds3234.read_time`

Reads the RTC and syncs the system clock to it (the same thing that happens every `update_interval`).

```yaml
- ds3234.read_time: rtc
```

#### `ds3234.set_alarm_1` / `ds3234.set_alarm_2`

Configures an alarm and enables it on the SQW/INT pin. Alarm 1 has seconds resolution; alarm 2 has minute
resolution and fires at second 00. Every field is optional and templatable; a field that is left out does not take
part in the match. The DS3234 can only leave fields out from the coarsest one down, so e.g. `hour` requires `minute`
(and `second` for alarm 1) to be set as well.

- **id** (*Optional*, [ID](https://esphome.io/guides/configuration-types#config-id)): the DS3234 to configure.
- **second** (*Optional*, int, `0` - `59`, alarm 1 only)
- **minute** (*Optional*, int, `0` - `59`)
- **hour** (*Optional*, int, `0` - `23`, in the RTC's time, i.e. UTC)
- **day_of_month** (*Optional*, int, `1` - `31`) **or** **day_of_week** (*Optional*, int, `1` - `7`, Sunday = 1)

```yaml
# Every minute at second 00
- ds3234.set_alarm_1:
    id: rtc
    second: 0

# Every day at 07:30:00 UTC
- ds3234.set_alarm_1:
    id: rtc
    second: 0
    minute: 30
    hour: 7

# Every Monday at 06:00 UTC
- ds3234.set_alarm_2:
    id: rtc
    minute: 0
    hour: 6
    day_of_week: 2

# Every hour (alarm 2 with no fields set fires every minute)
- ds3234.set_alarm_2:
    id: rtc
    minute: 0
```

Setting an alarm switches the SQW/INT pin to interrupt mode (the square-wave output is turned off). The alarm
registers are battery-backed, so a configured alarm survives a reboot; the SQW/INT pin can be used with
[deep_sleep](https://esphome.io/components/deep_sleep/)'s `wakeup_pin` to wake the ESP32 on an alarm.

#### `ds3234.disable_alarm`

Disables an alarm's interrupt and clears its flag.

```yaml
- ds3234.disable_alarm:
    id: rtc
    alarm: 1
```

### Hardware design

You can find hardware design for this board in the [DS3234 RTC Breakout hardware repository](https://github.com/SolderedElectronics/DS3234-RTC-Breakout-hardware-design).

### Documentation

Access library documentation [here](https://docs.soldered.com/).

### About Soldered

<img src="https://raw.githubusercontent.com/SolderedElectronics/Soldered-Generic-Arduino-Library/dev/extras/Soldered-logo-color.png" alt="soldered-logo" width="500"/>

At Soldered, we design and manufacture a wide selection of electronic products to help you turn your ideas into acts and bring you one step closer to your final project. Our products are intented for makers and crafted in-house by our experienced team in Osijek, Croatia. We believe that sharing is a crucial element for improvement and innovation, and we work hard to stay connected with all our makers regardless of their skill or experience level. Therefore, all our products are open-source. Finally, we always have your back. If you face any problem concerning either your shopping experience or your electronics project, our team will help you deal with it, offering efficient customer service and cost-free technical support anytime. Some of those might be useful for you:

- [Web Store](https://www.soldered.com/shop)
- [Tutorials & Projects](https://soldered.com/learn)
- [Documentation](https://docs.soldered.com)

### Open-source license

Soldered invests vast amounts of time into hardware & software for these products, which are all open-source. Please support future development by buying one of our products.

Check license details in the LICENSE file. Long story short, use these open-source files for any purpose you want to, as long as you apply the same open-source licence to it and disclose the original source. No warranty - all designs in this repository are distributed in the hope that they will be useful, but without any warranty. They are provided "AS IS", therefore without warranty of any kind, either expressed or implied. The entire quality and performance of what you do with the contents of this repository are your responsibility. In no event, Soldered (TAVU) will be liable for your damages, losses, including any general, special, incidental or consequential damage arising out of the use or inability to use the contents of this repository.

## Have fun!

And thank you from your fellow makers at Soldered Electronics.
