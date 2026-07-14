# esp-32-template

ESP32 / ESP-IDF v6.x template. C++20, no exceptions, no RTTI.

## Setup

1. Install **ESP-IDF v6.x** (Windows installer): https://dl.espressif.com/dl/esp-idf/
2. Install the **CP210x VCP driver** if your board uses a CP2102/CP2102N: https://www.silabs.com/developer-tools/usb-to-uart-bridge-vcp-drivers
3. In VS Code, install the **Espressif IDF** extension.
4. Edit `.vscode/settings.json` — set `idf.currentSetup` and `clangd.path` to your local installs.

## Run

- **Build**: Ctrl+Shift+B
- **Flash**: *Tasks: Run Task* → *Flash* → enter port (e.g. `COM3`)
- **Monitor**: *Tasks: Run Task* → *Monitor*. Exit with `Ctrl+]`.

## Hardcoded paths to update

The VS Code config has Windows paths that are install-specific and IDF-version-specific. Update them after cloning, and again after upgrading ESP-IDF.

**`.vscode/settings.json`**
- `idf.currentSetup` — ESP-IDF root, e.g. `C:\esp\v6.0.1\esp-idf`. Changes per IDF version.
- `clangd.path` — full path to `clangd.exe` shipped with esp-clang, e.g. `C:\Espressif\tools\esp-clang\esp-20.1.1_20250829\esp-clang\bin\clangd.exe`. The version folder bumps with each IDF release. Or delete this line to let the clangd extension auto-download a generic build.
- `terminal.integrated.profiles.windows."ESP-IDF PowerShell".args` — contains two paths:
  - Python venv activator, e.g. `C:\Users\<you>\.espressif\python_env\idf6.0_py3.11_env\Scripts\Activate.ps1`. The `idf6.0_py3.11_env` segment changes when IDF or its bundled Python bumps.
  - `export.ps1`, e.g. `C:\esp\v6.0.1\esp-idf\export.ps1`. Must match `idf.currentSetup`.

**`.vscode/tasks.json`**
- `options.env.IDF_PYENV` — same Python venv activator path as above. Keep in sync with the terminal profile.

After editing, reload the VS Code window (*Developer: Reload Window*) and open a fresh terminal so the new profile takes effect.
