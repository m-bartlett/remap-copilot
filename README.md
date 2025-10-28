# Copilot Key Remapper

Remap the Windows Copilot key (Meta+Shift+F23) to Right Control on Linux.

## Features

- Remaps Windows Copilot key combination to Right Control
- Configurable release delay (default 300ms) to allow key combinations
- Auto-detects keyboard device or accepts manual specification
- Minimal dependencies (libevdev only)
- Clean signal handling and resource cleanup

## Requirements

- Linux with evdev support
- libevdev library
- Root privileges (for device access)

### Install Dependencies

**Debian/Ubuntu:**
```bash
sudo apt install libevdev-dev build-essential
```

## Build


### Use a different key than right control

Change the defined value of the `COPILOT_REPLACE_KEY` macro. This can be done either in the source code or supplying a value for the `USERDEFINES` Make variable that will be passed to the compiler, for example to use the right alt key instead one could do:

```sh
make USERDEFINES="-DCOPILOT_REPLACE_KEY=KEY_RIGHTALT"
```