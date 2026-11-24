# ESP32-S3-Zero

Custom board `esp32s3_zero`, based on `esp32s3_matrix` from the selected Zephyr environment.

This is a starting port, not hardware validation. The reference clocks, memory, console, pin routing, peripherals and runner settings are retained. Review them against your schematic before using the board.

Board sources: `boards/waveshare/esp32s3_zero/`. All reference SoCs, variants and declared revisions are retained.

Register this directory as a board root and analyze the workspace. Create a minimal application for one of the targets below, then explicitly Configure or Build it. Build success does not establish hardware operation.

- `esp32s3_zero/esp32s3/procpu`
- `esp32s3_zero/esp32s3/appcpu`

For builds outside Utilities, pass this directory as `BOARD_ROOT`. Keep original copyright and SPDX notices; the original file hashes and checkout identity are recorded in `zephyr-board-origin.json`.
