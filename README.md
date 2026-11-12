# MotionScope

This project turns an ESP32 and an MPU6050 into a local, phone-friendly motion dashboard. The ESP32 creates its own Wi-Fi network and serves a 3D board view, live orientation, raw values, and scrolling graphs at `http://192.168.4.1`.

## Wiring

| MPU6050 | ESP32 |
| --- | --- |
| VCC | 3.3V |
| GND | GND |
| SDA | GPIO 21 |
| SCL | GPIO 22 |

Use a 3.3 V-compatible MPU6050 breakout. Do not connect a bare MPU6050 directly to 5 V logic. Most common GY-521 boards accept 3.3 V power and include I2C pull-ups; if yours does not, add suitable pull-ups to 3.3 V.

## Arduino IDE setup

This version needs **zero libraries from Library Manager**. `WiFi`, `WebServer`, and `Wire` are included with the **esp32 by Espressif Systems** board package; they appear automatically when that board package is installed.

1. In **File → Preferences**, add the Espressif ESP32 Boards Manager URL if ESP32 boards are not already installed: `https://espressif.github.io/arduino-esp32/package_esp32_index.json`.
2. In **Tools → Board → Boards Manager**, install **esp32 by Espressif Systems**, then select your ESP32 board and its serial port.
3. Open `ESP32_MPU6050_Visualizer.ino` and upload it. Do **not** install or include any MPU6050 library.
4. Open Serial Monitor at **115200 baud**. After reset, leave the sensor completely still for about two seconds while it finds gyro bias.
5. Connect a phone or computer to Wi-Fi network **Your Wi-Fi Name** using password **12345678**, then browse to `http://192.168.4.1`.

The access-point IP is normally `192.168.4.1`; it is also printed in Serial Monitor. The dashboard polls a lightweight JSON endpoint about 13 times per second, so no WebSocket library is required.

## Startup flow and LED

1. Upload the sketch and power or reset the ESP32.
2. The onboard LED blinks while the MPU6050 is calibrated (about 1.5 seconds). Keep the board still during this time.
3. The LED stays on when the dashboard is ready. The ESP32 creates its **own** Wi-Fi access point; it does not join or depend on your home Wi-Fi.
4. Join **Your Wi-Fi Name**, enter `12345678`, and open `http://192.168.4.1`.

Serial Monitor at **115200 baud** prints startup, MPU6050 detection, calibration bias, access-point details, and roll/pitch/yaw once per second. If the MPU6050 cannot be detected, the LED blinks quickly and the monitor asks you to check wiring. `LED_BUILTIN` normally maps to GPIO 2 on ESP32 DevKit boards; if your board’s onboard LED is different, change that one definition near the top of the sketch.

## MPU6050 address detection

Before it tries to configure the sensor, the sketch scans every I2C address and prints each responding device plus its `WHO_AM_I` value. It accepts MPU6050 (`0x68`), MPU6500 (`0x70`), and MPU6515 (`0x74`) compatible IMUs. This supports either valid I2C address: `0x68` (AD0 tied low, the usual default) or `0x69` (AD0 tied high), with no code edits. If no device appears in the scan, the problem is electrical or the SDA/SCL pins are not GPIO 21/22 on that particular ESP32 board.

## Calibration and use

The startup routine averages 500 still samples and subtracts their gyro offsets. For best results, put the board on a stable, level surface before powering or resetting it. Reset whenever its resting orientation is changed. Roll and pitch use a complementary filter: the gyro responds quickly while gravity from the accelerometer corrects slow error.

## Why yaw drifts

An MPU6050 has an accelerometer and gyro, but no magnetometer. Gravity gives an absolute reference for roll and pitch; it gives no reference for rotation around the vertical axis. Yaw is therefore the Z-axis gyro rate integrated over time. Any small gyro bias, temperature change, vibration, or timing error accumulates into an angle error, even after the startup bias calibration.

For a stable compass heading, use a magnetometer-equipped IMU such as an MPU9250/ICM-20948, calibrate its hard/soft-iron distortion, and fuse magnetometer data with gyro and accelerometer data. This firmware intentionally shows the MPU6050-only yaw estimate and should not be used as an absolute heading source.

## Notes

- The model axes assume the usual breakout-board orientation. If your physical board is mounted differently, rotate it physically or remap the axes in `sampleMotion()`.
- The visualizer is for educational and UI use, not safety-critical navigation or control.
- The access point is open to any nearby device that knows its simple password; change `AP_PASSWORD` before using it outside a private setting.
