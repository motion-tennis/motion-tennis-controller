# zero_blink

Zephyr test firmware for the **Waveshare ESP32-S3-Zero** in the Motion Tennis controller
evaluation. It is used to try out individual building blocks of the controller before they move
into the controller firmware.

**Status:** test and evaluation code. It builds for `esp32s3_zero/esp32s3/procpu` with Zephyr
v4.4.0. The Protobuf encoding was checked on a host. Wi-Fi connection, UDP delivery and the real
MPU-6050 have not yet been verified on the device.

## What the firmware does

| Function | Source | Details |
| --- | --- | --- |
| Status LED | `src/main.c` | The onboard WS2812 RGB LED (GPIO21) cycles red → green → blue every 500 ms, showing that the firmware runs |
| IMU acquisition | `src/imu.c` | A thread reads one sample per sampling boundary from an MPU-6050 over I²C, or generates test data without a sensor |
| Protobuf encoding | `src/motion_codec.c` | Every sample becomes one `motion.v03.Envelope { controller_input }` (`proto/motion.proto`), encoded with nanopb, 133 bytes |
| UDP sending | `src/udp_sender.c` | A second thread sends every encoded message once to the receiver (UDP port 41000) |
| Wi-Fi station | `src/wifi.c` | WPA2-PSK, IPv4 via DHCP, power save disabled, automatic reconnect after 5 s |

```text
k_timer ──> IMU thread ──> queue (16) ──> sender thread ──> nanopb ──> UDP :41000 ──> receiver
            (MPU-6050 or simulated)                          Envelope{controller_input}
```

The console (USB, `/dev/ttyACM0`) shows Wi-Fi status and one line per sample:

```text
<inf> wifi: Connecting to "my-network"...
<inf> wifi: Connected, waiting for DHCP
<inf> wifi: IPv4 address: 192.168.1.57
#12 SIM 133 B | a   2.94   0.00   9.81 | g  1.18  0.00  0.00 | sent=12 not_sent=0 overload=0
```

| Field | Meaning |
| --- | --- |
| `#12` | sample ID (also `request_seq`), starts at 1 after every boot |
| `SIM` / `IMU` | generated data or MPU-6050 |
| `133 B` | size of the encoded message |
| `a`, `g` | acceleration in m/s² (including gravity), angular rate in rad/s |
| `SAT` | shown if any axis is at ≥ 98 % of the full scale |
| `sent` / `not_sent` | sent messages / encoded but not sent (no network or `sendto` error) |
| `overload` | samples dropped because the queue was full |

### Message content

`proto/motion.proto` and `proto/motion.options` are copies of `documentation/spec/motion.proto` and
`documentation/scripts/contract/motion.options` (contract `motion.v03`, proposed). nanopb generates
`motion.pb.c/.h` during the build. Keep the copies identical to the documentation repository.

| Field | Value |
| --- | --- |
| `major` / `minor` | 0 / 3 |
| `sender_id`, `transaction.controller_id` | `APP_CONTROLLER_ID` |
| `sender_session_id`, `transaction.session_id` | 16 random bytes, new at every boot |
| `message_seq` | 1, 2, 3, … per session |
| `sent_time_us` | local monotonic time at encoding |
| `clock_id`, `capture.clock_id` | `APP_CLOCK_ID` |
| `run_id` | 16 zero bytes (placeholder) |
| `transaction.request_seq`, `sample_id` | sample counter from 1 |
| `t0_us` | sampling boundary (timer wake-up), local monotonic µs |
| `capture` | read time, `DRIVER_FRAME`, uncertainty 5000 µs, `timing_valid = true` |
| `specific_force_m_s2`, `angular_velocity_rad_s` | sensor values in SI units |
| `sensor_saturated` | any axis ≥ 98 % of the full scale |
| `original_sensor_ticks`, `association_version`, `calibration_id`, `battery_millivolts` | 0 (not available yet) |

### Limitations

- Sampling uses a timer, not the MPU-6050 data-ready interrupt; `t0` is the timer wake-up.
- No `hello`, association or clock exchange; the placeholder fields above would be rejected by a
  receiver that validates them according to `documentation/docs/protocol.md`.
- A failed sensor read or a missed timer period is logged, not sent as a failure record.
- 2.4 GHz Wi-Fi only (ESP32-S3).

## Configuration

| File | Setting | Default |
| --- | --- | --- |
| `src/wifi_credentials.h` | `WIFI_SSID`, `WIFI_PASSWORD` | see [Wi-Fi credentials](#wi-fi-credentials) |
| `src/app_config.h` | `APP_RECEIVER_IPV4` – IPv4 address of the receiver | `"192.168.1.100"` |
| | `APP_RECEIVER_PORT` | `41000` |
| | `APP_CONTROLLER_ID` – nonzero, unique per controller | `1` |
| | `APP_CLOCK_ID` | `1` |
| `src/imu.c` | `IMU_SAMPLE_RATE_HZ` – 200 per specification, 10 for readable console tests | `10` |
| | `IMU_USE_SIMULATED_DATA` – `1` generated data, `0` MPU-6050 | `1` |
| `src/udp_sender.c` | `SENDER_PRINT_DIVIDER` – console line every N samples (e.g. 20 at 200 Hz) | `1` |
| overlay | I²C0 pins, MPU-6050 ranges (±16 g, ±2000 °/s), `smplrt-div = 39` (200 Hz), `&wifi` enabled | |

Simulated data: Z = 9.81 m/s² (gravity), a 0.5 Hz sine on accel X (±5 m/s²) and gyro Z (±2 rad/s).
Without a connected MPU-6050 its driver only logs an initialisation error at boot.

After changing the overlay, `prj.conf` or `CMakeLists.txt`, run a **pristine** build.

### Wi-Fi credentials

The credentials live in `src/wifi_credentials.h`. This file is listed in `.gitignore` so that
passwords never end up in the repository; only the template `wifi_credentials.h.example` is
committed.

1. Create the file from the template (once per checkout):

   ```sh
   cp src/wifi_credentials.h.example src/wifi_credentials.h
   ```

2. Enter the network:

   ```c
   #define WIFI_SSID     "my-network"
   #define WIFI_PASSWORD "my-password" // WPA2-PSK, 8 to 63 characters
   ```

3. Set `APP_RECEIVER_IPV4` in `src/app_config.h` to the IPv4 address of the PC that runs the
   receiver (Windows: `ipconfig`, entry of the Wi-Fi adapter).

4. Build and flash. The console shows `IPv4 address: …` once the connection is up.

The network must offer 2.4 GHz and WPA2-PSK.
The build fails with `wifi_credentials.h: No such file or directory` if step 1 was skipped.

## Build and flash

Requirements (see `documentation/docs/zephyr-eval-setup.md` for the complete setup):

- Zephyr **v4.4.0** west workspace with the modules `hal_espressif`, `nanopb`, `mbedtls`,
  `tf-psa-crypto` and the Espressif blobs:

  ```sh
  west config manifest.project-filter -- '-.*,+hal_espressif,+nanopb,+mbedtls,+tf-psa-crypto'
  west update
  west blobs fetch hal_espressif
  ```

- `protoc` and the Python protobuf packages in the workspace venv (nanopb generator):

  ```sh
  sudo apt install protobuf-compiler
  pip install protobuf grpcio-tools
  ```

- Zephyr SDK 1.0.1 with `xtensa-espressif_esp32s3_zephyr-elf`, `esptool`
  (`west packages pip --install`) and the custom board definition in [`../board`](../board).

From the west workspace, with this repository cloned to `~/zephyrproject/motion-tennis-controller`:

```sh
E=$HOME/zephyrproject/motion-tennis-controller/evaluation/esp32s3-zero
west build -p -b esp32s3_zero/esp32s3/procpu $E/zero_blink -d build/zero_blink \
  -- -DBOARD_ROOT=$E/board
west flash -d build/zero_blink
python -m serial.tools.miniterm --exit-char 24 /dev/ttyACM0 115200   # quit with Ctrl+X
```

Under WSL the board must be attached with `usbipd attach --wsl --busid <BUSID> --auto-attach`
in an administrator PowerShell before flashing.

## Test receiver (`tools/udp_receiver.py`)

A Python script that receives the `controller_input` messages on a PC, so the firmware can be tested
without the Raspberry Pi.

### What it does

- Generates the Python Protobuf module from `proto/motion.proto` at start-up (no generated files
  in the repository).
- Decodes every datagram and prints one line per message:

  ```text
  192.168.1.57 ctrl=1 seq=12 sample=12 133 B | a   2.94   0.00   9.81 | g  1.18  0.00  0.00
  ```

- Marks problems on the packet line: `new session …` (controller rebooted), `missing 2: 10..11`,
  `late sample 30 (reordered)`, `duplicate sample 20`, `NOT CANONICAL` (bytes differ from the
  canonical Protobuf encoding required by the protocol), unexpected version.
- Keeps a status line pinned to the top of the terminal with statistics over a sliding window:

  ```text
  ctrl 1 [a0a1a2a3] |  10.0 Hz (sender  10.0) | dt  100.0±  3.8 ms [95.0..105.9] | jitter  2.4 ms p99   6.1 | loss 0/100 (0.0%) | total 100 lost 0 dup 0 ooo 0 | 133 B
  ```

  | Field | Meaning |
  | --- | --- |
  | `Hz (sender …)` | receive rate; sender rate derived from the `t0_us` spacing |
  | `dt mean±std [min..max]` | interval between received packets |
  | `jitter std p99` | spread of arrival time minus `t0` (relative one-way delay; the clocks are not synchronised, so no absolute latency) and its 99th percentile above the minimum |
  | `loss n/expected` | missing samples within the window |
  | `total lost dup ooo` | totals since the session started: received, lost, duplicates, reordered |
  | `B` | size of the last message |
  | `NO DATA x s`, `SAT n`, `NONCANON n` | shown only when they apply |

### Options

| Option | Effect |
| --- | --- |
| `--port N` | UDP port (default 41000) |
| `--window S` | statistics window in seconds (default 10) |
| `--raw` | also print the full decoded message |
| `--no-status-line` | print statistics as normal lines every 2 s (for terminals without ANSI support or when redirecting output) |

Quit with **Ctrl+C**.

### Python environment

The script needs Python 3.10 or newer with `protobuf` and `grpcio-tools`; Python 3.12 is
recommended. `grpcio-tools` brings its own `protoc`, so nothing else is required on Windows.
Create the environment **outside** cloud-synchronised folders such as Nextcloud.

**Option A: Python venv (PowerShell)**

Requires Python 3.12 from python.org (includes the `py` launcher).

```powershell
# Once: allow local scripts such as Activate.ps1
Set-ExecutionPolicy -Scope CurrentUser RemoteSigned

# Create and fill the environment
py -3.12 -m venv $env:USERPROFILE\venvs\motion-proto
& $env:USERPROFILE\venvs\motion-proto\Scripts\Activate.ps1
python -m pip install --upgrade pip
pip install protobuf grpcio-tools

# Every time
& $env:USERPROFILE\venvs\motion-proto\Scripts\Activate.ps1
python <path>\zero_blink\tools\udp_receiver.py
deactivate
```

Without activation: `& $env:USERPROFILE\venvs\motion-proto\Scripts\python.exe <path>\zero_blink\tools\udp_receiver.py`.
On Linux: `python3 -m venv ~/venvs/motion-proto` and `source ~/venvs/motion-proto/bin/activate`.

**Option B: Anaconda**

```powershell
# Once, in the Anaconda Prompt: make "conda activate" available in PowerShell
conda init powershell
# then reopen PowerShell (and run Set-ExecutionPolicy as above if scripts are blocked)

# Create and fill the environment
conda create -n motion-proto python=3.12 -y
conda activate motion-proto
pip install protobuf grpcio-tools

# Every time
conda activate motion-proto
python <path>\zero_blink\tools\udp_receiver.py
conda deactivate
```

Install the two packages with **pip**, not conda, so that `protobuf` matches the version
`grpcio-tools` requires.

Check either environment with:

```powershell
python -c "import grpc_tools, google.protobuf; print(google.protobuf.__version__)"
```

### Running it

1. Start the receiver on a PC in the same Wi-Fi network. Under Windows with WSL, run it on
   **Windows**, not in WSL: with the default NAT networking, WSL 2 is not reachable from the
   ESP32.
2. Allow Python through Windows Defender Firewall for **private networks** when prompted
   (UDP 41000).
3. Use Windows Terminal or the VS Code terminal for the pinned status line; otherwise add
   `--no-status-line`.

## MPU-6050 wiring

| MPU-6050 breakout | ESP32-S3-Zero |
| --- | --- |
| VCC | 3V3 |
| GND | GND |
| SDA | GPIO1 |
| SCL | GPIO2 |
| INT | GPIO4 (not used yet, needed for data-ready mode) |
| AD0 | open or GND → address 0x68 |
| XDA, XCL | not connected |

I²C0 runs at 400 kHz. Most breakouts (e.g. GY-521) have 4.7 kΩ pull-ups; without them, add 4.7 kΩ
from SDA and SCL to 3V3. The Zephyr driver does not configure the DLPF, so the sensor rate is
8 kHz / (1 + `smplrt-div`). Set `IMU_USE_SIMULATED_DATA` to `0` to read the sensor.

## Project structure

```text
zero_blink/
├── CMakeLists.txt               # sources, nanopb generation, default overlay
├── prj.conf                     # Kconfig: LED, I2C/sensor, nanopb, Wi-Fi/networking
├── overlays/
│   └── esp32s3_zero_esp32s3_procpu.overlay   # I2C0 + MPU-6050, Wi-Fi enabled
├── proto/
│   ├── motion.proto             # copy of documentation/spec/motion.proto
│   └── motion.options           # copy of documentation/scripts/contract/motion.options
├── src/
│   ├── main.c                   # LED, starts Wi-Fi
│   ├── imu.c / imu.h            # acquisition thread, simulated data, sample queue
│   ├── motion_codec.c / .h      # Envelope/ControllerInput encoding
│   ├── udp_sender.c             # sender thread
│   ├── wifi.c / wifi.h          # Wi-Fi station
│   ├── app_config.h             # receiver address, IDs
│   ├── wifi_credentials.h.example
│   └── wifi_credentials.h       # local only, ignored by git
└── tools/
    └── udp_receiver.py          # test receiver with statistics
```

The overlay is included automatically for this target by `CMakeLists.txt`;
`-DZEPHYR_UTILS_DEFAULT_OVERLAY=OFF` disables it.

## Troubleshooting

| Symptom | Cause / resolution |
| --- | --- |
| `wifi_credentials.h: No such file or directory` | Create it from `wifi_credentials.h.example` |
| `Connection failed (…)`, retrying | Wrong SSID/password, 5 GHz-only network or unsupported security mode |
| Connected but `not_sent` increases | No IPv4 address yet, or wrong `APP_RECEIVER_IPV4` |
| `sent` increases but the receiver shows nothing | Firewall, receiver in WSL instead of Windows, wrong IP or different network (guest Wi-Fi with client isolation) |
| `<err> imu: mpu6050@68 not ready …` | Check wiring, power and AD0. The driver accepts `WHO_AM_I` 0x68 (MPU-6050), 0x70 (MPU-6500), 0x71 (MPU-9250) and 0x19 (MPU-6880) |
| `Sample …: read failed (-5)` | I²C NACK or bus error: pull-ups, loose wires, SDA/SCL swapped |
| CMake: `'protoc' not found`, nanopb generator errors | Install `protobuf-compiler` and `pip install protobuf grpcio-tools` in the workspace venv |
| Kconfig `MBEDTLS … y-selected`, CMake `tf-psa-crypto … not an existing directory` | Missing west modules; extend the project filter as shown above |
| Status line garbled in the receiver | Terminal without ANSI support: use Windows Terminal or `--no-status-line` |
