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

## 🚀 Installation

There are two ways to install Nabdhu: downloading a pre-compiled binary, or building it yourself from source.

### Option 1: Pre-Compiled Binary (Easiest)

1. Go to the [Releases](https://github.com/ItsMe-RiiK/Nabdhu/releases) page and download the latest `nabdhu-linux-x86_64.tar.gz`.
2. Extract the archive and enter the folder:
   ```bash
   tar -xzvf nabdhu-linux-x86_64.tar.gz
   cd release_package
   ```
3. Run the provided installer script to install the app and its Start Menu shortcut:
   ```bash
   sudo ./install.sh
   ```
*(If you ever want to remove it, just run `sudo ./uninstall.sh` from the same folder).*

### Option 2: Build From Source

If you prefer to compile Nabdhu natively on your system, you can build it using CMake:

```bash
git clone https://github.com/ItsMe-RiiK/Nabdhu.git
cd Nabdhu

# Generate build files and compile
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc)

# Install system-wide
sudo cmake --install build
```

## 💻 Running Nabdhu

Once installed (via either method), you can launch Nabdhu by clicking its icon in your **Start Menu**, or by typing its name in your terminal:

```bash
nabdhu
```

**Note on Privileges**: Because Linux's security model protects deep hardware information and the ability to start/stop system services, Nabdhu's advanced features require Root privileges. If you run `nabdhu` as a standard user, it will work perfectly but with limited access. To use the Service Manager, run `sudo nabdhu` manually.

## ⌨️ Controls

- **Search**: Just start typing! The search bar will automatically capture your keystrokes.
- **Context Menu**: Press `Enter` on any process or service to reveal action options (e.g. End Task, Start/Stop Service).
- **Navigation**: Use the Arrow Keys to scroll through the list. Press `Tab` to switch between Processes and Services panes.
- **Sorting**: Press `f` to toggle sorting between Name, CPU, and PID

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.