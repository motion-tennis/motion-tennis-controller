# VS Code configuration template

Template for developing `zero_blink` with VS Code on Windows through the WSL extension. The files belong in the `.vscode/` folder of the **west workspace** (`~/zephyrproject/.vscode/`), not of this repository: all paths are relative to the workspace root.

| File | Content |
| --- | --- |
| `extensions.json` | Recommends C/C++, Python, DeviceTree and Kconfig; marks CMake Tools as unwanted because it conflicts with west |
| `settings.json` | IntelliSense compile database, workspace venv for Python and terminals, file associations, watcher exclusions |
| `c_cpp_properties.json` | IntelliSense configuration `Zephyr ESP32-S3` with the SDK cross compiler |
| `tasks.json` | Build, pristine build, flash, monitor and build + flash + monitor |

The tasks expect the application under `apps/<name>` and the board root under `hardware/esp32s3_zero` in the workspace. With this repository cloned to `~/zephyrproject/motion-tennis-controller`, create links instead of copies so that edits go straight into the repository:

```bash
cd ~/zephyrproject
E=motion-tennis-controller/evaluation/esp32s3-zero
mkdir -p apps hardware .vscode
ln -s ../$E/zero_blink apps/zero_blink
ln -s ../$E/board hardware/esp32s3_zero
cp $E/vscode/*.json .vscode/
code .
```

`ln -s` fails if `apps/zero_blink` or `hardware/esp32s3_zero` already exist; rename or remove the old copies first. Then:

1. Install the recommended extensions for **WSL: Ubuntu**.
2. Create `apps/zero_blink/src/wifi_credentials.h` from the `.example` file.
3. Run **Zephyr: Build (pristine)** (Ctrl+Shift+B uses the incremental build).
4. Select **C/C++: Select a Configuration…** → `Zephyr ESP32-S3` once; IntelliSense works after the first build.

Flashing and the monitor need the board attached to WSL: run `usbipd attach --wsl --busid <BUSID> --auto-attach` in an administrator PowerShell yourself; VS Code does not do this.

These files are an evaluation template. Once the controller firmware has its own west manifest in this repository, a shared `.vscode/` at the repository root with repository-relative paths should replace it.
