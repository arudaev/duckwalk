# 00 · Install

[Docs home](README.md) · [Next: User guide →](guide.md)

Duckwalk comes in two parts. **Part 1** puts Harold on your Flipper Zero, and that is all you need for him to keep your computer awake. **Part 2**, the desktop app, is optional: it tells Harold when you're at the computer, so he lets go of the pointer while you work.

## Before you start

- **A Flipper Zero on the official firmware, 1.5 or newer.** qFlipper (on your computer) and the Flipper mobile app both show the firmware version and update it. Custom firmware isn't supported.
- **A USB-C cable that carries data.** Some cables only charge; with one of those the computer doesn't see the Flipper at all.

## Part 1: the Flipper app

v0.1 isn't released yet, so for now you build the app yourself. It takes about ten minutes the first time, most of it a one-off download.

### 1. Install Python and ufbt

[ufbt](https://github.com/flipperdevices/flipperzero-ufbt) is Flipper's official tool for building apps. It needs Python 3.8 or newer.

**Windows**

1. Install Python from [python.org](https://www.python.org/downloads/). In the installer, tick **Add python.exe to PATH**. (If typing `python` opens the Microsoft Store, Python isn't installed yet; that's only a shortcut.)
2. In a new terminal:
   ```powershell
   py -m pip install --upgrade ufbt
   ```

**macOS**

1. Install [Homebrew](https://brew.sh) if you don't have it. It installs Apple's command line tools too, which takes a while.
2. Then:
   ```bash
   brew install pipx
   pipx install ufbt
   pipx ensurepath
   ```
3. Open a new terminal, so it finds `ufbt`.

**Linux**

1. Install pipx with your package manager: `sudo apt install pipx` (Debian, Ubuntu), `sudo dnf install pipx` (Fedora) or `sudo pacman -S python-pipx` (Arch).
2. Then:
   ```bash
   pipx install ufbt
   pipx ensurepath
   ```
3. Open a new terminal.

Plain `pip install` on macOS and most Linux systems stops with *externally-managed-environment*; pipx is the way round it. If a terminal says it can't find `ufbt` on Windows, use `py -m ufbt` instead of `ufbt` in the steps below.

### 2. Get the code

On the [Duckwalk page on GitHub](https://github.com/arudaev/duckwalk), choose **Code > Download ZIP** and unpack it. You get a folder called `duckwalk-main`. With Git, `git clone https://github.com/arudaev/duckwalk.git` gives you a folder called `duckwalk` instead.

### 3. Linux only: let ufbt reach the Flipper

On Linux, only some users may open the Flipper's serial port. Flipper's own rule gives that right to whoever is logged in, for Flipper devices only:

```bash
curl -fsSL https://raw.githubusercontent.com/flipperdevices/qFlipper/dev/installer-assets/udev/42-flipperzero.rules -o 42-flipperzero.rules
cat 42-flipperzero.rules
sudo cp 42-flipperzero.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules && sudo udevadm trigger
```

Then unplug the Flipper and plug it back in. If you already use qFlipper, its installer may have added this rule for you.

Or join the serial group instead: `sudo usermod -aG dialout $USER` (`uucp` on Arch), then log out and in.

### 4. Build and start it

1. Close qFlipper and any Flipper Lab tab in your browser. They hold the Flipper's port.
2. Plug in the Flipper and unlock it.
3. In a terminal, go to the app folder and start it:
   ```bash
   cd Downloads/duckwalk-main/app
   ufbt launch
   ```
   (Or `cd duckwalk/app` if you cloned it.) The first run downloads the Flipper SDK and its compiler into a `.ufbt` folder in your home folder: a few hundred MB, so give it a few minutes. Later runs take seconds.
4. Harold's welcome screen opens on the Flipper. Press **Left** for USB or **Right** for Bluetooth; Bluetooth walks you through pairing on the Flipper's screen ([more in the user guide](guide.md#connecting-over-bluetooth)).

Once you choose USB, Duckwalk takes over the Flipper's USB port, so `ufbt launch` may end with an error about the port (`ClearCommError` on Windows). That's expected. Press **Back** on the Flipper before you run `ufbt launch` again.

From now on Duckwalk is on the Flipper under **Apps > USB > Duckwalk**; you don't need the computer to start it.

### Once v0.1 is out

You won't need to build it. Install **Duckwalk** from the **USB** category of the Flipper mobile app or [Flipper Lab](https://lab.flipper.net/apps). Or download `duckwalk.fap` from the [releases](https://github.com/arudaev/duckwalk/releases) and copy it to `apps/USB/` on the Flipper's SD card with qFlipper.

## Part 2: the desktop app (optional)

With the desktop app running, Harold lets go of the pointer the moment you touch your own mouse, keyboard, touchpad or pen, and walks only while you're away. It notices *that* you used a device, never what you typed or where you pointed; [what it does, exactly](../desktop/app/README.md#what-it-does-exactly). Without it, Harold walks on his timer ([how to manage without it](guide.md#without-the-desktop-app)).

It starts at login after its first start; Harold's menu in the tray turns that off.

### Windows

Until v0.1, build it with Rust:

1. Install Rust with [rustup](https://rustup.rs). Its installer offers to add Microsoft's C++ build tools, which Rust needs on Windows; that's a large download.
2. In a new terminal:
   ```powershell
   cargo install --git https://github.com/arudaev/duckwalk duckwalk
   duckwalk
   ```

Once v0.1 is out, download `duckwalk-desktop-windows-x64.exe` (most PCs) or `duckwalk-desktop-windows-arm64.exe` from the [releases](https://github.com/arudaev/duckwalk/releases) and open it. Windows may call it an unknown app: **More info**, then **Run anyway** ([why it isn't signed yet](code-signing.md)).

Either way, the first start installs it for your user: it copies itself to `%LOCALAPPDATA%\Programs\Duckwalk`, adds **Duckwalk** to the Start menu and puts that folder on your PATH. No admin rights needed.

### macOS

With [Homebrew](https://brew.sh), which builds it on your Mac so macOS doesn't flag it:

```bash
brew install --HEAD arudaev/tap/duckwalk
duckwalk
```

The build takes a few minutes. Once v0.1 is out, it's `brew install arudaev/tap/duckwalk`.

#### Allow Input Monitoring

On its first start the app asks for **Input Monitoring**. Allow it in **System Settings > Privacy & Security > Input Monitoring** (started from a terminal, the entry may carry the terminal's name), then quit Duckwalk from Harold's menu and start it again. Without it, Harold walks on his timer.

### Linux

1. Install Rust with [rustup](https://rustup.rs) (or your distribution's `rustup` package), and the libraries listed under [Build](../desktop/app/README.md#build). The names there are Debian and Ubuntu's; Fedora and Arch call them a little differently.
2. In a new terminal:
   ```bash
   cargo install --git https://github.com/arudaev/duckwalk duckwalk
   duckwalk
   ```

Once v0.1 is out, you can download `duckwalk-desktop-linux-x64` from the [releases](https://github.com/arudaev/duckwalk/releases) instead: move it to a folder where it can stay (it starts from there at every login) and make it executable with `chmod +x`.

On GNOME without the [AppIndicator extension](https://extensions.gnome.org/extension/615/appindicator-support/) (Fedora, for example), Harold doesn't show in the top bar, but the app still runs.

The Linux build hasn't been tested over USB yet; if it can't reach the Flipper there, use Bluetooth, and please [tell us](https://github.com/arudaev/duckwalk/issues/new/choose).

#### Join the `input` group

The app sees your input through `/dev/input/`, which only the `input` group may read:

```bash
sudo usermod -aG input $USER
```

Then log out and in again. Be aware that every program you run can then read your keyboard and mouse; to undo it later, `sudo gpasswd -d $USER input`. Without the group, the app says it can't read any input device, and Harold walks on his timer.

### Check it works

With Duckwalk open on the Flipper, the corner shows **USB+PC** or **BT+PC**. Touch your mouse: the status line says **You're working**.

## When it doesn't work

| What you see | What to do |
|---|---|
| `ufbt launch` can't open the port, or finds no Flipper | Close qFlipper and Flipper Lab. If Duckwalk is running in USB mode, press **Back** on the Flipper. Check the cable carries data. On Linux, see step 3 |
| The Flipper says **Error: Outdated App** | The SDK is older than your firmware: run `ufbt update`, then `ufbt launch` again |
| The Flipper says **Error: Outdated Firmware** or **Error: Missing Imports** | Your firmware is older than the SDK: update it to the latest release with qFlipper or the Flipper mobile app, then launch again |
| No **+PC** in the Flipper's corner | Start the desktop app; only one copy runs at a time. Over Bluetooth, the computer must be paired with "Harold" and not only with the Flipper |
| The Flipper never says **You're working** | macOS: allow Input Monitoring and restart the app. Linux: join the `input` group and log out and in |

Still stuck? [Open an issue](https://github.com/arudaev/duckwalk/issues/new/choose) with your system, the Flipper's firmware version and what you tried.

## Remove it

**From the Flipper:** with qFlipper's file manager, delete `apps/USB/duckwalk.fap` and the folder `apps_data/duckwalk/` (settings and Bluetooth keys). If you installed it from the catalog, you can remove it in the Flipper mobile app or Flipper Lab instead. Then remove "Harold" from the computer's Bluetooth devices, if you paired him.

**The desktop app:** untick **Start at login** in Harold's menu and choose **Quit Duckwalk**. Then:

- **Windows:** delete the folder `%LOCALAPPDATA%\Programs\Duckwalk`, the Start menu entry **Duckwalk**, and that folder in your user PATH (search Windows for *environment variables*). If you built it with Rust, also `cargo uninstall duckwalk`.
- **macOS:** `brew uninstall duckwalk`, and remove Duckwalk from Input Monitoring.
- **Linux:** `cargo uninstall duckwalk`, or delete the file you downloaded; leave the `input` group with `sudo gpasswd -d $USER input`.

If Start at login was still on, its entry stays behind: the **Duckwalk** value under `HKCU\Software\Microsoft\Windows\CurrentVersion\Run` on Windows, `~/Library/LaunchAgents/Duckwalk.plist` on macOS, `~/.config/autostart/Duckwalk.desktop` on Linux. The app also leaves a small `Duckwalk` folder in your settings folder (`%APPDATA%`, `~/Library/Application Support`, `~/.config`).

**The build tools:** `pipx uninstall ufbt` (or `py -m pip uninstall ufbt` on Windows), the `.ufbt` folder in your home folder, and Rust with `rustup self uninstall` if you don't need it.

---

[Docs home](README.md) · [Next: User guide →](guide.md)
