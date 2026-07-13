# Nabdhu

A fast, lightweight, and highly optimized terminal user interface for system resource monitoring and process management on Linux. Built natively in C++ using a custom zero-allocation terminal rendering engine.

## 🌟 Features

- **Process Management**: View all running processes with their CPU & Memory usage. Sort them instantly by Name or CPU usage by tapping the `[f]` keybind.
- **Interactive Performance Graphs**: 
  - Real-time line charts drawn natively using Braille characters.
  - Monitors global CPU utilization across all cores.
  - Contextually color-graded gauges for Memory and Disk usage.
  - Live Network monitoring with Upload and Download speeds.
- **Hardware Details**: Displays SysLoad, CPU model, core clocks, and core temps cleanly in the hardware info pane.
- **Service Manager**: Seamlessly view, start, stop, and restart `systemd` background services without leaving the UI.
- **Smart Search**: Filter processes and services instantly by typing in the search bar.
- **Modern UI**: Polished responsive flexbox-style design that gracefully dynamically adjusts whether you are in full screen or splitting your terminal in half.

## 🛠️ Prerequisites

To compile and run this project, you will need the following installed on your system:

- GCC / G++ (supporting C++17)
- CMake (>= 3.10)
- `xdotool` and `xprop` (Optional, required for seamless GUI window centering and single-instance enforcement when launched via Desktop environment)

### Installation on Arch Linux / Manjaro
```bash
sudo pacman -S base-devel cmake xdotool xorg-xprop
```

### Installation on Ubuntu / Debian
```bash
sudo apt install build-essential cmake xdotool x11-utils
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

Because Linux's security model protects deep hardware information and the ability to start/stop system services, **this application is best run as Root**.

We provide a system-wide installation that installs a wrapper script (`run_nabdhu`). This securely elevates privileges while preserving your current `$DISPLAY` and graphical settings, ensuring it works perfectly on both **X11** and **Wayland**.

To install the binary, icons, and Start Menu launcher system-wide, simply run:

```bash
sudo make -C build install
sudo update-desktop-database /usr/local/share/applications/
```

Once installed, you can launch **Nabdhu** directly from your Desktop Environment's Application Menu!

### Running Manually from Terminal
You can still start the task manager normally by running:
```bash
/usr/local/bin/nabdhu
```
Or for full features:
```bash
/usr/local/bin/run_nabdhu
```

## ⌨️ Controls

- **Search**: Just start typing! The search bar will automatically capture your keystrokes.
- **Context Menu**: Press `Enter` on any process or service to reveal action options (e.g. End Task, Start/Stop Service).
- **Navigation**: Use the Arrow Keys to scroll through the list. Press `Tab` to switch between Processes and Services panes.
- **Sorting**: Press `f` to toggle sorting between Name and CPU usage.
- **Quit**: Press `q` or `Esc` to safely exit the application.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.