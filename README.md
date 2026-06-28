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
- **Modern UI**: Full Dark Mode support and elegant rounded corners perfectly integrated with modern Linux Desktop Environments (Cinnamon, GNOME).

## 🛠️ Prerequisites

To compile and run this project, you will need the following installed on your system:

- GCC / G++ (supporting C++17)
- GTK+ 3.0 development files
- Make
- `dmidecode` (Optional, but highly recommended for reading RAM hardware details)
- `polkit` (Optional, for seamless GUI root access)

### Installation on Arch Linux / Manjaro
```bash
sudo pacman -S base-devel gtk3 dmidecode polkit
```

### Installation on Ubuntu / Debian
```bash
sudo apt install build-essential libgtk-3-dev dmidecode policykit-1
```

## 🚀 Building the App

Compile using the provided Makefile:

```bash
git clone 
cd MyTaskManager
make
```

To clean the compiled binaries:
```bash
make clean
```

## 🏃 Running the Application

You can start the task manager normally by running:
```bash
./taskmanager
```

### Running with Root Access (Recommended)
Because Linux's security model protects deep hardware information (like RAM Speed or Motherboard slots) and the ability to start/stop system services, **this application is best run as Root**.

Instead of using `sudo` from the terminal every time, you can integrate it seamlessly into your Desktop Environment using Polkit!

1. **Enable the Wrapper**: Make the included wrapper script executable:
   ```bash
   chmod +x run_taskmanager.sh
   ```
2. **Install the Desktop Shortcut**:
   Copy the `mytaskmanager.desktop` file to your applications folder:
   ```bash
   cp mytaskmanager.desktop ~/.local/share/applications/
   ```
   Now you can launch **My Linux Task Manager** directly from your Application Menu! It will securely prompt for your password via a GUI.

3. **(Optional) Bypass Password Prompt:**
   If you want the Task Manager to open instantly without asking for a password every time, you can add a Polkit rule:
   ```bash
   sudo tee /etc/polkit-1/rules.d/99-mytaskmanager.rules <<EOF
   polkit.addRule(function(action, subject) {
       if (action.id == "org.freedesktop.policykit.exec" &&
           action.lookup("program") == "$PWD/run_taskmanager.sh" &&
           subject.user == "$USER") {
           return polkit.Result.YES;
       }
   });
   EOF
   ```

## ⌨️ Controls

- **Search**: Just start typing! The search bar will automatically capture your keystrokes.
- **Context Menu**: Right-click on any process to reveal options like **End Task** (Kill) or **Open File Location**.
- **Services**: Use the buttons at the bottom of the Services tab to toggle active SystemD units.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.