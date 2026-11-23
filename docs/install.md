# 00 · Install

[Docs home](README.md) · [Next: User guide →](guide.md)

Duckwalk comes in two parts. **Part 1** puts Harold on your Flipper Zero, and that is all he needs to keep your computer awake. **Part 2**, the desktop app, is optional: it tells Harold when you're at the computer, so he lets go of the pointer while you work.

## Before you start

- **A Flipper Zero on the latest official firmware** (1.5.1 or newer). qFlipper on your computer and the Flipper mobile app both show the version and update it. Custom firmware isn't supported.
- **A USB-C cable that carries data.** Some cables only charge; with one of those the computer doesn't see the Flipper at all.
- **A terminal,** to paste a few commands into. Windows: right-click **Start**, then **Terminal**. macOS: **Terminal** is in Applications > Utilities. Linux: your desktop's terminal app.

## Part 1: the Flipper app

v0.1 isn't released yet, so for now you build the app yourself. It takes 10 to 20 minutes the first time, most of it a one-off download.

### 1. Install Python and ufbt

[ufbt](https://github.com/flipperdevices/flipperzero-ufbt) is Flipper's official tool for building apps. It needs Python 3.8 or newer.

**Windows**

1. Install Python from [python.org](https://www.python.org/downloads/). If the installer shows **Add python.exe to PATH**, tick it, and leave the rest as it is. (If typing `python` opens the Microsoft Store, Python isn't installed yet; that's only a shortcut.) On Windows on ARM, any of python.org's Windows installers works; ufbt's compiler runs under Windows' x64 emulation.
2. In a new terminal:
   ```powershell
   py -m pip install --upgrade ufbt
   ```
3. If pip warns that a folder isn't on PATH, or later `ufbt` isn't found, type `py -m ufbt` wherever this page says `ufbt`.

**macOS** (plain `pip install` stops with *externally-managed-environment* here; pipx is the way round it)

1. Install [Homebrew](https://brew.sh) if you don't have it. It asks for your Mac's password (nothing shows while you type it) and installs Apple's command line tools, which takes a while. When it finishes, run the commands it prints under **Next steps**.
2. In a new terminal:
   ```bash
   brew install pipx
   pipx install ufbt
   pipx ensurepath
   ```
3. Open a new terminal, so it finds `ufbt`.

**Linux** (pipx, for the same reason as on macOS)

1. Install pipx and curl with your package manager: `sudo apt install pipx curl` (Debian, Ubuntu), `sudo dnf install pipx` (Fedora) or `sudo pacman -S python-pipx` (Arch).
2. Then:
   ```bash
   pipx install ufbt
   pipx ensurepath
   ```
3. Open a new terminal.

### 2. Get the code

On the [Duckwalk page on GitHub](https://github.com/arudaev/duckwalk), choose **Code > Download ZIP** and unpack it. Inside is a folder called `duckwalk-main` with an `app` folder in it; Windows' **Extract All** puts it one level deeper, in `duckwalk-main\duckwalk-main\app`. (With Git: `git clone https://github.com/arudaev/duckwalk.git`.)

### 3. Linux only: let ufbt reach the Flipper

On Linux, only some users may open the Flipper's serial port. Flipper's own rule gives that right to whoever is logged in, for the Flipper and the chip's update mode only:

```bash
curl -fsSL https://raw.githubusercontent.com/flipperdevices/qFlipper/dev/installer-assets/udev/42-flipperzero.rules -o 42-flipperzero.rules
cat 42-flipperzero.rules
sudo cp 42-flipperzero.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules && sudo udevadm trigger
```

`cat` shows what you're installing: it should mention only Flipper devices and STMicroelectronics (the Flipper's chip maker). Then unplug the Flipper and plug it back in. If you use qFlipper, its installer may have added this rule already.

### 4. Build and start it

1. Close qFlipper, and [Flipper Lab](https://lab.flipper.net) if it's open in your browser: only one program can talk to the Flipper at a time.
2. Plug in the Flipper and unlock it. If your Mac (Apple silicon) asks whether to allow the accessory to connect, choose **Allow**.
3. Open a terminal in the `app` folder. Windows: open the folder in File Explorer, right-click an empty spot, **Open in Terminal**. macOS and Linux: `cd` into it, for example `cd ~/Downloads/duckwalk-main/app`.
4. Run:
   ```bash
   ufbt launch
   ```
   (Windows, if `ufbt` isn't found: `py -m ufbt launch`.) The first run downloads the Flipper SDK and its compiler into a `.ufbt` folder in your home folder: about 1 GB on disk, so give it time. Later runs take seconds.
5. Harold's welcome screen opens on the Flipper. Press **Left** for USB if the Flipper stays plugged in, or **Right** for Bluetooth to keep it cable-free; Bluetooth walks you through pairing on the Flipper's screen ([more in the user guide](guide.md#connecting-over-bluetooth)).

   With USB, Duckwalk takes over the Flipper's USB port, so the terminal may now show an error about the port (`ClearCommError` on Windows). That's fine: the app is on the Flipper, and you can close the terminal. Press **Back** on the Flipper before you run `ufbt launch` again.

**Check it worked:** Harold sits at his desk and his status line counts down to the next walk. When it reaches zero he moves your pointer a little and back; that's him working. **OK** puts him to sleep, **Back** leaves the app. From now on Duckwalk is on the Flipper under **Apps > USB > Duckwalk**; you don't need the computer to start it. That's all Harold needs. Part 2 is optional, and the [user guide](guide.md) shows what he can do.

### Once v0.1 is out

You won't need to build it. Install **Duckwalk** from the **USB** category of the Flipper mobile app or [Flipper Lab](https://lab.flipper.net/apps). Or download `duckwalk.fap` from the [releases](https://github.com/arudaev/duckwalk/releases) and copy it to `apps/USB/` on the Flipper's SD card with qFlipper.

## Part 2: the desktop app (optional)

With the desktop app running, Harold lets go of the pointer the moment you touch your own mouse, keyboard, touchpad or pen, and walks only while you're away. It notices *that* you used a device, never what you typed or where you pointed ([details](../desktop/app/README.md#what-it-does-exactly)). Without it, Harold walks on his timer ([how to manage without it](guide.md#without-the-desktop-app)).

Until v0.1 is out, it's built from source with Rust: one to two GB of downloads, and 10 to 20 minutes. If that's more than you want, Harold works without it; come back once v0.1 is out.

### Windows

1. Install Rust with [rustup](https://rustup.rs). It offers to install Microsoft's C++ build tools, which Rust needs on Windows; accept the defaults.
2. In a new terminal:
   ```powershell
   cargo install --locked --git https://github.com/arudaev/duckwalk duckwalk
   duckwalk
   ```

Once v0.1 is out, download `duckwalk-desktop-windows-x64.exe` (most PCs) or `duckwalk-desktop-windows-arm64.exe` (Windows on ARM) from the [releases](https://github.com/arudaev/duckwalk/releases) and open it instead.

Either way, the first start installs it for your user: it copies itself to `%LOCALAPPDATA%\Programs\Duckwalk`, adds **Duckwalk** to the Start menu and puts that folder on your PATH, so you can delete the download. No admin rights needed. Harold sits in the tray; Windows may hide him behind the **^** next to the clock.

### macOS

With [Homebrew](https://brew.sh), which builds it on your Mac so macOS doesn't flag it:

```bash
brew install --HEAD arudaev/tap/duckwalk
duckwalk
```

The build takes a few minutes. Harold appears in the menu bar. From then on the app starts when you log in. Once v0.1 is out, there are also ready-made files on the [releases](https://github.com/arudaev/duckwalk/releases) page: `duckwalk-desktop-macos-arm64` (Apple silicon) or `-macos-x64` (Intel); make the file executable with `chmod +x`, keep it in a folder where it can stay, and run it. macOS refuses it the first time: choose **Open Anyway** under **System Settings > Privacy & Security**, then run it again.

#### Allow Input Monitoring

On its first start the app asks for **Input Monitoring**. Allow it in **System Settings > Privacy & Security > Input Monitoring**, then quit Duckwalk from Harold's menu and start it again. If you started Duckwalk from Terminal, macOS may list **Terminal** instead; allowing that gives everything you run in Terminal the same access. Without Input Monitoring, Harold walks on his timer.

### Linux

1. Install Rust with [rustup](https://rustup.rs) (when it asks how to install, press **Enter** for the default), and the libraries and a C compiler:
   - Debian, Ubuntu: `sudo apt install build-essential libudev-dev libdbus-1-dev pkg-config libgtk-3-dev libayatana-appindicator3-dev`
   - Fedora: `sudo dnf install gcc systemd-devel dbus-devel pkgconf-pkg-config gtk3-devel libayatana-appindicator-gtk3-devel`
   - Arch: `sudo pacman -S base-devel systemd-libs dbus gtk3 libayatana-appindicator`
2. In a new terminal:
   ```bash
   cargo install --locked --git https://github.com/arudaev/duckwalk duckwalk
   duckwalk
   ```

Once v0.1 is out, you can download `duckwalk-desktop-linux-x64` from the [releases](https://github.com/arudaev/duckwalk/releases) instead: move it to a folder where it can stay (it starts from there at every login) and make it executable with `chmod +x`.

On GNOME without the [AppIndicator extension](https://extensions.gnome.org/extension/615/appindicator-support/) (Fedora, for example), Harold doesn't show in the top bar, but the app still runs. On Fedora: `sudo dnf install gnome-shell-extension-appindicator`, turn it on in the Extensions app, then log out and in.

#### Join the `input` group

The app sees your input through `/dev/input/`, which only the `input` group may read:

```bash
sudo usermod -aG input $USER
```

Every program you run can then read your keyboard and mouse; on a shared or work computer, ask first. To undo it, `sudo gpasswd -d $USER input`. Without the group, the app says it can't read any input device, and Harold walks on his timer.

#### Over USB: let the app reach Duckwalk

Over USB, the app talks to Duckwalk through `/dev/hidraw`, which on most systems only root may open. This rule lets whoever is logged in open Duckwalk's own device (USB ID `1209:d0c4`) and nothing else:

```bash
echo 'SUBSYSTEM=="hidraw", ATTRS{idVendor}=="1209", ATTRS{idProduct}=="d0c4", TAG+="uaccess"' | sudo tee /etc/udev/rules.d/70-duckwalk.rules
sudo udevadm control --reload-rules && sudo udevadm trigger
```

It hasn't been tested on Linux yet: if the Flipper never shows **USB+PC**, use Bluetooth, which doesn't need it, and [tell us](https://github.com/arudaev/duckwalk/issues/new/choose).

When you're done with both, log out and in again, then unplug the Flipper and plug it back in.

### Check it works

With Duckwalk open on the Flipper, the corner of its screen shows **USB+PC** or **BT+PC**. Touch your mouse: the status line says **You're working**. From now on the app starts when you log in; to stop that, untick **Start at login** in Harold's menu.

## When it doesn't work

| What you see | What to do |
|---|---|
| `ufbt launch` can't open the port, or finds no Flipper | Close qFlipper and Flipper Lab. If Duckwalk is running in USB mode, press **Back** on the Flipper. Unlock the Flipper, and check the cable carries data. On Linux, see [step 3](#3-linux-only-let-ufbt-reach-the-flipper) |
| The Flipper says **Error: Outdated App**, **Outdated Firmware** or **Missing Imports** | The SDK and the firmware are out of step. Outdated App: run `ufbt update`, then `ufbt launch` again. The other two: update the firmware to the latest release with qFlipper or the Flipper mobile app, then launch again |
| Windows calls the desktop app an unknown app | **More info**, then **Run anyway**. The download isn't signed yet; [here's why](code-signing.md) |
| No **+PC** on the Flipper | Start the desktop app; only one copy runs at a time. Over Bluetooth, pair the computer with **Harold** (the name starts with Harold), not only with the Flipper. On Linux over USB, see [the rule above](#over-usb-let-the-app-reach-duckwalk) |
| The Flipper never says **You're working** | macOS: [allow Input Monitoring](#allow-input-monitoring). Linux: [join the `input` group](#join-the-input-group) |

Still stuck? [Open an issue](https://github.com/arudaev/duckwalk/issues/new/choose) with your system, the Flipper's firmware version and what you tried.

## Remove it

**From the Flipper:** with qFlipper's file manager, delete `apps/USB/duckwalk.fap` and the folder `apps_data/duckwalk/` (settings and Bluetooth keys). If you installed it from the catalog, you can remove it in the Flipper mobile app or Flipper Lab instead. Then remove the Bluetooth device whose name starts with **Harold** from the computer, if you paired it.

**The desktop app:** untick **Start at login** in Harold's menu and choose **Quit Duckwalk**. Then:

- **Windows:** delete the folder `%LOCALAPPDATA%\Programs\Duckwalk`, the Start menu entry **Duckwalk**, and that folder in your user PATH (search Windows for *environment variables*). If you built it with Rust, also `cargo uninstall duckwalk`.
- **macOS:** `brew uninstall duckwalk` (or delete the file you downloaded), and remove Duckwalk from Input Monitoring.
- **Linux:** `cargo uninstall duckwalk`, or delete the file you downloaded. Leave the `input` group with `sudo gpasswd -d $USER input`, and delete `/etc/udev/rules.d/70-duckwalk.rules` (and `42-flipperzero.rules`, unless you use qFlipper).

The app also leaves a small `Duckwalk` folder in your settings folder (`%APPDATA%`, `~/Library/Application Support` or `~/.config`). If you skipped unticking **Start at login**, its entry stays too: the **Duckwalk** value under `HKCU\Software\Microsoft\Windows\CurrentVersion\Run` on Windows, `~/Library/LaunchAgents/Duckwalk.plist` on macOS, `~/.config/autostart/Duckwalk.desktop` on Linux.

**The build tools:** `pipx uninstall ufbt` (or `py -m pip uninstall ufbt` on Windows), the `.ufbt` folder in your home folder, and Rust with `rustup self uninstall` if you don't need it.

---

[Docs home](README.md) · [Next: User guide →](guide.md)
