<div>
  <picture>
    <source media="(prefers-color-scheme: dark)" srcset="https://github.com/user-attachments/assets/13984090-9eff-4e4e-94c3-e22282f621aa">
    <source media="(prefers-color-scheme: light)" srcset="https://github.com/user-attachments/assets/9498933e-4cfa-4deb-aebf-6e0df33db365">
    <img
      align="left"
      width="60"
      alt="nopilot"
      src="https://github.com/user-attachments/assets/9498933e-4cfa-4deb-aebf-6e0df33db365">
  </picture>
  <h3>
    <span><h3>
    <span><h3>
    <span><h3>
      <span><h3>Copilot Key Remapper</h3></span>
    </h3></span>
    </h3></span>
    </h3></span>
  </h3>
</div>

Substitute the Copilot key's hardwired shortcut with a different key.

The default substitution key is <kbd>RightControl</kbd>, but see [Use a different key than right control](#use-a-different-key-than-right-control) for info on sending an alternative key.

<br/>

## About

Chances are you stumbled on this repo because you have a god-forsaken copilot key on your keyboard.

In typical Microsoft fashion, this key was implemented in the most annoying way conceivable, namely:

* It does not send a single scan code, but is hard-wired to send <kbd>LeftMeta</kbd>+<kbd>LeftShift</kbd>+<kbd>F23</kbd>
* It does not track when you release the key, so it actually sends down events for <kbd>LeftMeta</kbd>+<kbd>LeftShift</kbd>+<kbd>F23</kbd>, immediately followed by up events for each.

These two qualities make the key almost entirely unusable for anything else other than some other sort of application launcher. 
This application is an attempt to use this key as a modifier key, given its placement where such a key would normally reside.

### How it works

Since the key sends the same key chord in the same order each time, and nearly instantly, we don't need much logic to track modifier key state.
Basically it watches for the key order <kbd>LeftMeta</kbd>&rarr;<kbd>LeftShift</kbd>&rarr;<kbd>F23</kbd> and if this sequence arrives in order,
it drops the keys and substitute a different key press in the device stream. Then it schedules a release event of that key in a few milliseconds
(default is 300, but this can be supplied as a command line argument). This is necessary because the copilot key itself does not react to when it is released,
it sends the key chord up events immediately after the keydown events so it's necessary to simulate a small delay so the user has a chance to press a different
key to combine with the substituted modifier key.

> [!NOTE]
> It's possible to simulate the behavior of this app using the [`evsieve`](https://github.com/KarsMulder/evsieve) tool.
> ```sh
> device='/dev/input/by-path/your-keyboard-device-path'
> copilot_seq='key:leftmeta key:leftshift key:f23'
> copilot_sub_key='key:rightctrl'
> evsieve \
>     --input "$device" grab \
>     --hook $copilot_seq sequential period=0.1 send-key=$copilot_sub_key \
>     --withhold \
>     --delay $copilot_sub_key:0 period=0.3 \
>     --output name="kbd-no-copilot" create-link="kbd-no-copilot"
> ```
> `evsieve` is more robust and enables arbitrary complex substitutions.
> However, I found that its virtual device circumvented keyboard mappings and rules in my desktop environment,
> for example it prevented **disabling the touchpad while typing** despite having this option explicitly configured.
> Hence, I opted to write this tool as a more useful solution.

<br/>

## Requirements

- Linux only, operates on evdev devices
- GNU Make
- `libevdev` library:

  Debian/Ubuntu:
  ```sh
  sudo apt install libevdev-dev build-essential
  ``` 
  
  Arch Linux:
  ```sh
  sudo pacman -S libevdev
  ```
  
  Fedora:
  ```sh
  sudo dnf install libevdev-devel
  ```

<br/>

## Build & Install

### Compile
```sh
make all
```
The built executable can be tested from `./build/remap-copilot`.

### Use a different key than right control

Change the defined value of the `COPILOT_REPLACE_KEY` macro.
This can be done either in the source code or supplying a value for the `USERDEFINES` Make variable that will be passed to the compiler.

For example, to use the right alt key instead:
```sh
make USERDEFINES="-DCOPILOT_REPLACE_KEY=KEY_RIGHTALT"
```

### Install
```sh
sudo make install
```
The default install prefix is `/usr/local/`. Change the `PREFIX` Make variable to alter this, for example: `sudo make install PREFIX=/usr` will install to `/usr/bin`.

### Systemd Service
```console
$ make install-systemd
Installed ~/.config/systemd/user/remap-copilot.service
Try:
        systemctl --user start remap-copilot
        systemctl --user enable --now remap-copilot
$ systemctl --user enable --now remap-copilot.service
Created symlink '~/.config/systemd/user/default.target.wants/remap-copilot.service'
```
Installs a user systemd service that ensures an instance of this program is always running.

### Uninstall
```sh
make uninstall
```
Will remove the executable from the installation directory, and also removes the user systemd service if it's detected.
