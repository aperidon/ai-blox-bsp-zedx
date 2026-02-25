BMI SDK (C / C++) — README

Overview
- Small SDK and example consumers for the BMI SPSC driver used in this repo.
- The SDK exposes range/scale selection for accelerometer and gyroscope but
  intentionally does NOT expose sampling-frequency (ODR) changes — those are
  driver/daemon controlled.

Sysfs class path
- Device class: `/sys/class/bmi_spsc`
- Example device node: `/sys/class/bmi_spsc/spsc_bmi0`

Important sysfs files (readable/writable by driver):
- `accel_available_ranges`  — list of supported accel range indexes
- `gyro_available_ranges`   — list of supported gyro range indexes
- `accel_range`             — current accel range index (write to change)
- `gyro_range`              — current gyro range index (write to change)
- `accel_frequency` / `gyro_frequency` — driver-controlled, currently fixed to 200Hz (not exposed)

Range index mapping (index → physical scale)
- Accelerometer (`accel_range` indices):
  - `0` → ±3 g
  - `1` → ±6 g
  - `2` → ±12 g  (default)
  - `3` → ±24 g

- Gyroscope (`gyro_range` indices):
  - `0` → ±2000 °/s
  - `1` → ±1000 °/s  (default)
  - `2` → ±500 °/s
  - `3` → ±250 °/s
  - `4` → ±125 °/s

Notes
- Defaults on this platform are `accel_range = 2` (±12 g) and `gyro_range = 1` (±1000 °/s).
- Use `accel_available_ranges` / `gyro_available_ranges` to confirm supported indices on other platforms.

Build (C)
1. cd tools/bmi/sdk/c
2. mkdir -p build && cd build
3. cmake .. && make
4. Example run: `./bmi_consumer_c spsc_bmi0`

Build (C++)
1. cd tools/bmi/sdk/cpp
2. mkdir -p build && cd build
3. cmake .. && make
4. Example run: `./bmi_consumer_cpp spsc_bmi0`

New CLI options (both C and C++)
- `--accel-range <idx>`    — set accelerometer range index before starting
- `--gyro-range <idx>`     — set gyroscope range index before starting
- `--csv <filename>`       — set output CSV filename (default: `bmi_data_*.csv`)

Full CLI synopsis (C and C++ consumer)
- `./bmi_consumer_{c,cpp} [spsc_bmiN] [--accel-range IDX] [--gyro-range IDX] [--csv FILE]`
  - `spsc_bmiN` is the optional device name (positional), e.g. `spsc_bmi0`.
  - `--accel-range IDX` sets the accel range index via the imu daemon.
  - `--gyro-range IDX` sets the gyro range index via the imu daemon.
  - `--csv FILE` writes samples to `FILE` instead of the default name.

Notes & examples
- Read available ranges:
  cat /sys/class/bmi_spsc/spsc_bmi0/accel_available_ranges
  cat /sys/class/bmi_spsc/spsc_bmi0/gyro_available_ranges

- Set range via sysfs (same effect as SDK option):
  echo 1 | sudo tee /sys/class/bmi_spsc/spsc_bmi0/accel_range

- SDK example using the new flags:
  cd tools/bmi/sdk/cpp/build
  ./bmi_consumer_cpp spsc_bmi0 --accel-range 1 --gyro-range 0

Analyze CSV output (Python)
- Script: `tools/bmi/sdk/analyze_consumer_test.py`
- Usage: `python3 analyze_consumer_test.py <input_csv>`
- Purpose: reads the timestamp (column index 1) and reports inter-sample delta
  statistics (mean/stddev/min/max) and counts within 5±0.1 ms and 5±0.5 ms.

Example run & possible outcome:
```
$ python3 tools/bmi/sdk/analyze_consumer_test.py bmi_data_cpp.csv
File: bmi_data_cpp.csv
Deltas count: 13471
Mean delta: 4.988088 ms
Stddev (population): 0.069719 ms
Min delta: 4.662916 ms, Max delta: 5.922230 ms

Within 5±0.1 ms: 12135/13471 (90.08%)
Within 5±0.5 ms: 13467/13471 (99.97%)

Sample of first 10 deltas (ms):
1: 4.954909 ms
2: 4.954909 ms
3: 4.954909 ms
4: 4.999003 ms
5: 4.999003 ms
6: 4.999003 ms
7: 4.999003 ms
8: 5.359908 ms
9: 5.359908 ms
10: 5.359908 ms
```

Why frequency is not exposed
- The kernel/daemon manage sampling timers; changing ODR at runtime may not
  be supported for all IMU implementations used by this driver. To avoid
  unsafe configurations the SDK intentionally omits user-facing frequency
  flags. If you need runtime ODR control, modify the driver/daemon first.

Troubleshooting
- If a range change fails, confirm the daemon is running, the device exists
  under `/sys/class/bmi_spsc/`, and `timer_status` is `stopped` before
  attempting a change.
