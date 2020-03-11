<a href="https://www.hardwario.com/"><img src="https://www.hardwario.com/ci/assets/hw-logo.svg" width="200" alt="HARDWARIO Logo" align="right"></a>

# Firmware for HARDWARIO Lora Climate Monitor + CO2 + PIR Motion detector

[![Travis](https://img.shields.io/travis/bigclownprojects/bcf-lora-climate-pir-co2/master.svg)](https://travis-ci.org/bigclownprojects/bcf-lora-climate-pir-co2)
[![Release](https://img.shields.io/github/release/bigclownprojects/bcf-lora-climate-pir-co2.svg)](https://github.com/bigclownprojects/bcf-lora-climate-pir-co2/releases)
[![License](https://img.shields.io/github/license/bigclownprojects/bcf-lora-climate-pir-co2.svg)](https://github.com/bigclownprojects/bcf-lora-climate-pir-co2/blob/master/LICENSE)
[![Twitter](https://img.shields.io/twitter/follow/hardwario_en.svg?style=social&label=Follow)](https://twitter.com/hardwario_en)

## Description

Unit measure temperature, relative humidity, illuminance and atmospheric pressure.
Values is sent every 15 minutes over LoRaWAN. Values are the arithmetic mean of the measured values since the last send.

Measure interval is 60s for temperature, relative humidity, illuminance, orientation. And 5minutes for atmospheric pressure and CO2.
The battery is measured during transmission.

## Buffer
big endian

| Byte    | Name        | Type   | multiple | unit
| ------: | ----------- | ------ | -------- | -------
|       0 | HEADER      | uint8  |          |
|       1 | BATTERY     | uint8  | 10       | V
|       2 | ORIENTATION | uint8  |          |
|  3 -  4 | TEMPERATURE | int16  | 10       | °C
|       5 | HUMIDITY    | uint8  | 2        | %
|  6 -  7 | ILLUMINANCE | uint16 |          | lux
|  8 -  9 | PRESSURE    | uint16 | 0.5      | Pa
| 10 - 13 | PIR MOTION  | uint32 |          |
| 14 - 15 | CO2         | uint16 |          | ppm

### Header

* 0 - bool
* 1 - update
* 2 - button click

## AT

```sh
picocom -b 115200 --omap crcrlf  --echo /dev/ttyUSB0
```

## CO2 Calibration

Calibration could be started by long pressing of the button on Core Module or by typing `AT$CALIBRATION` AT command. The LED starts to blink.

After the calibration starts, put the device outside to calibrate to the 400 ppm level by clean outside air. First 15 minutes the LED is blinking fast and this delay is used so the clean outdoor air can flow inside the CO2 sensor.

After initial 15 minutes, the LED starts to blink slower and is doing 32 measurements with 2 minute period between measurements. This second stage takes 64 minutes.

After 32 samples the device will switch to normal operation and LED will stop blinking.
You can watch the calibration proces over USB. In the AT console there are debug commands. However the device muset be outdoor for proper calibration.

Calibration could be interrupted by long pressing of the button or by typing `AT$CALIBRATION` AT command. The LED stops blinking.

## License

This project is licensed under the [MIT License](https://opensource.org/licenses/MIT/) - see the [LICENSE](LICENSE) file for details.

---

Made with &#x2764;&nbsp; by [**HARDWARIO s.r.o.**](https://www.hardwario.com/) in the heart of Europe.
