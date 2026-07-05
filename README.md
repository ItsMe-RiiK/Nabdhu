# My Linux Task Manager

A modern, fast, and lightweight Task Manager for Linux, built natively with C++ and GTK3. Inspired by the sleek design of modern system monitors, this tool provides an elegant interface for managing processes, monitoring system performance, and controlling `systemd` services.

## 🌟 Features

- **Process Management**: View all running processes with their CPU & Memory usage. Uses dynamic color highlighting (green for low usage, yellow/orange for medium, red for heavy usage).
- **Interactive Performance Graphs**: 
  - Beautiful real-time line charts drawn natively with Cairo.
  - Monitors global CPU utilization across all cores.
  - Detailed Memory layout (In Use, Available, Cached, Swap).
- **Hardware Details**: Displays RAM Speed (MHz), Slots used, Type (DDR4/5), and Form Factor directly in the GUI (requires root).
- **Service Manager**: Seamlessly view, start, stop, and restart `systemd` background services without touching the terminal.
- **Smart Search**: Filter processes and services instantly by simply typing anywhere on the screen (no need to click the search bar!).
- **Modern UI**: Full Dark Mode support and elegant rounded corners perfectly integrated with modern Linux Desktop Environments (Cinnamon, GNOME). All graphical assets are embedded into a single portable binary.

## 🛠️ Prerequisites

To compile and run this project, you will need the following installed on your system:

- GCC / G++ (supporting C++17)
- CMake (>= 3.10)
- pkg-config
- GTK+ 3.0 development files (`libgtk-3-dev` / `gtk3`)
- `dmidecode` (Optional, but highly recommended for reading RAM hardware details)
- `polkit` (Optional, for seamless GUI root access via Start Menu)

### Installation on Arch Linux / Manjaro
```bash
sudo pacman -S base-devel cmake pkgconf gtk3 dmidecode polkit
```

### Installation on Ubuntu / Debian
```bash
sudo apt install build-essential cmake pkg-config libgtk-3-dev dmidecode policykit-1
```

## 🚀 Building the App

This project uses CMake for an easy, out-of-source build configuration.

```bash
git clone https://github.com/ItsMe-RiiK/MyTaskManager.git
cd MyTaskManager

# Generate build files
cmake -B build

# Compile the application
cmake --build build
```

## 💻 System-Wide Installation (Recommended)

Because Linux's security model protects deep hardware information (like RAM Speed or Motherboard slots) and the ability to start/stop system services, **this application is best run as Root**.

We provide a seamless system-wide installation that installs a secure Polkit wrapper (`run_taskmanager`). This securely elevates privileges while preserving your current `$DISPLAY` and graphical settings, ensuring it works perfectly on both **X11** and **Wayland**.

To install the binary, icons, and Start Menu launcher system-wide, simply run:

```bash
sudo make -C build install
sudo update-desktop-database /usr/local/share/applications/
```

Once installed, you can launch **TaskManager** directly from your Desktop Environment's Application Menu! It will securely prompt for your password via your native graphical prompt (e.g. GNOME PolicyKit dialog) and launch properly.

### Running Manually from Terminal
You can still start the task manager normally by running:
```bash
/usr/local/bin/taskmanager
```
Or for full features:
```bash
sudo /usr/local/bin/run_taskmanager
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