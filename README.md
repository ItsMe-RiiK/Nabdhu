# Nabdhu

A modern, fast, and lightweight Task Manager for Linux, built natively with C++ and FTXUI. Inspired by the sleek design of modern system monitors, this tool provides an elegant interface for managing processes, monitoring system performance, and controlling `systemd` services natively in your terminal.

## 🌟 Features

- **Process Management**: View all running processes with their CPU & Memory usage. Sort them instantly by Name or CPU usage by tapping the `[f]` keybind.
- **Interactive Performance Graphs**: 
  - Beautiful real-time line charts drawn natively in the terminal.
  - Monitors global CPU utilization across all cores.
  - Detailed Memory and Disk usage panes.
  - Live Network monitoring with Upload and Download speeds.
- **Hardware Details**: Displays SysLoad, CPU model, core clocks, and core temps cleanly in the hardware info pane.
- **Service Manager**: Seamlessly view, start, stop, and restart `systemd` background services without leaving the UI.
- **Smart Search**: Filter processes and services instantly by simply typing in the search bar.
- **Modern UI**: Polished responsive flexbox design. The UI gracefully re-flows and dynamically adjusts whether you are in full screen or splitting your terminal in half.

## 🛠️ Prerequisites

To compile and run this project, you will need the following installed on your system:

- GCC / G++ (supporting C++17)
- CMake (>= 3.10)
- `xdotool` and `xprop` (required for seamless GUI window centering and single-instance enforcement)
- `dmidecode` (Optional, but highly recommended for reading RAM hardware details)
- `polkit` (Optional, for seamless GUI root access via Start Menu)

### Installation on Arch Linux / Manjaro
```bash
sudo pacman -S base-devel cmake dmidecode polkit xdotool xorg-xprop
```

### Installation on Ubuntu / Debian
```bash
sudo apt install build-essential cmake dmidecode policykit-1 xdotool x11-utils
```

## 🚀 Building the App

This project uses CMake for an easy, out-of-source build configuration.

```bash
git clone https://github.com/ItsMe-RiiK/Nabdhu.git
cd Nabdhu

# Generate build files
cmake -B build

# Compile the application
cmake --build build
```

## 💻 System-Wide Installation (Recommended)

Because Linux's security model protects deep hardware information (like RAM Speed or Motherboard slots) and the ability to start/stop system services, **this application is best run as Root**.

We provide a seamless system-wide installation that installs a secure Polkit wrapper (`run_nabdhu`). This securely elevates privileges while preserving your current `$DISPLAY` and graphical settings, ensuring it works perfectly on both **X11** and **Wayland**.

To install the binary, icons, and Start Menu launcher system-wide, simply run:

```bash
sudo make -C build install
sudo update-desktop-database /usr/local/share/applications/
```

Once installed, you can launch **Nabdhu** directly from your Desktop Environment's Application Menu! It will securely prompt for your password via your native graphical prompt (e.g. GNOME PolicyKit dialog) and launch properly.

### Running Manually from Terminal
You can still start the task manager normally by running:
```bash
/usr/local/bin/nabdhu
```
Or for full features:
```bash
sudo /usr/local/bin/run_nabdhu
```

## 🧹 Code Quality and Formatting

The project ships with strict code formatting configurations.
- Run `clang-format` based on the `.clang-format` (LLVM Style).
- Static analysis is enabled via the configured `.clang-tidy` which ensures high code quality while intelligently ignoring UI-framework specific macros.

## ⌨️ Controls

- **Search**: Just start typing! The search bar will automatically capture your keystrokes.
- **Context Menu**: Right-click on any process to reveal options like **End Task** (Kill) or **Open File Location**.
- **Services**: Use the buttons at the bottom of the Services tab to toggle active SystemD units.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.