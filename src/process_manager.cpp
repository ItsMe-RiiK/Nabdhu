#include "process_manager.h"
#include <dirent.h>
#include <fstream>
#include <iostream>
#include <pwd.h>
#include <signal.h>
#include <sstream>
#include <sys/types.h>
#include <unistd.h>

ProcessManager::ProcessManager() : prev_uptime(0), first_global_cpu_run(true), first_process_run(true)
{
  hertz = sysconf(_SC_CLK_TCK);
}

ProcessManager::~ProcessManager() {}

double ProcessManager::get_uptime_internal()
{
  std::ifstream file("/proc/uptime");
  if (file.is_open())
  {
    std::string line;
    if (std::getline(file, line))
    {
      std::stringstream ss(line);
      double uptime;
      ss >> uptime;
      return uptime;
    }
  }
  return 0.0;
}

long ProcessManager::get_system_uptime()
{
  return static_cast<long>(get_uptime_internal());
}

long ProcessManager::get_hertz() const
{
  return hertz;
}

std::string ProcessManager::get_user_from_uid(int uid)
{
  struct passwd *pw = getpwuid(uid);
  if (pw != nullptr)
  {
    return std::string(pw->pw_name);
  }
  return std::to_string(uid);
}

std::string ProcessManager::translate_state(const std::string &state_char)
{
  if (state_char == "R")
  {
    return "Running";
  }
  if (state_char == "S")
  {
    return "Sleeping";
  }
  if (state_char == "D")
  {
    return "Disk Sleep";
  }
  if (state_char == "Z")
  {
    return "Zombie";
  }
  if (state_char == "T")
  {
    return "Stopped";
  }
  if (state_char == "t")
  {
    return "Tracing Stop";
  }
  if (state_char == "X" || state_char == "x")
  {
    return "Dead";
  }
  if (state_char == "K")
  {
    return "Wakekill";
  }
  if (state_char == "W")
  {
    return "Waking";
  }
  if (state_char == "P")
  {
    return "Parked";
  }
  if (state_char == "I")
  {
    return "Idle";
  }
  return state_char;
}

GlobalCpuData ProcessManager::read_global_cpu_data()
{
  GlobalCpuData data;
  std::ifstream file("/proc/stat");
  if (file.is_open())
  {
    std::string line;
    if (std::getline(file, line))
    {
      if (line.substr(0, 3) == "cpu")
      {
        std::stringstream ss(line.substr(3));
        ss >> data.user >> data.nice >> data.system >> data.idle >> data.iowait >> data.irq >> data.softirq >> data.steal;
      }
    }
  }
  return data;
}

double ProcessManager::get_global_cpu_usage()
{
  GlobalCpuData current_cpu = read_global_cpu_data();
  double usage = 0.0;

  if (!first_global_cpu_run)
  {
    unsigned long long prev_total = prev_global_cpu.total();
    unsigned long long curr_total = current_cpu.total();
    unsigned long long prev_idle = prev_global_cpu.idle_all();
    unsigned long long curr_idle = current_cpu.idle_all();

    unsigned long long total_diff = curr_total - prev_total;
    unsigned long long idle_diff = curr_idle - prev_idle;

    if (total_diff > 0)
    {
      usage = 100.0 * (total_diff - idle_diff) / (double)total_diff;
    }
  }

  prev_global_cpu = current_cpu;
  first_global_cpu_run = false;

  return usage;
}

GlobalMemData ProcessManager::get_global_memory()
{
  GlobalMemData data;
  std::ifstream file("/proc/meminfo");
  std::string line;
  while (std::getline(file, line))
  {
    if (line.compare(0, 8, "MemTotal") == 0)
    {
      sscanf(line.c_str(), "MemTotal: %lld kB", &data.total_kb);
    }
    else if (line.compare(0, 7, "MemFree") == 0)
    {
      sscanf(line.c_str(), "MemFree: %lld kB", &data.free_kb);
    }
    else if (line.compare(0, 12, "MemAvailable") == 0)
    {
      sscanf(line.c_str(), "MemAvailable: %lld kB", &data.available_kb);
    }
    else if (line.compare(0, 6, "Cached") == 0)
    {
      sscanf(line.c_str(), "Cached: %lld kB", &data.cached_kb);
    }
    else if (line.compare(0, 9, "SwapTotal") == 0)
    {
      sscanf(line.c_str(), "SwapTotal: %lld kB", &data.swap_total_kb);
    }
    else if (line.compare(0, 8, "SwapFree") == 0)
    {
      sscanf(line.c_str(), "SwapFree: %lld kB", &data.swap_free_kb);
    }
  }
  return data;
}

MemHardwareInfo ProcessManager::get_memory_hardware_info()
{
  MemHardwareInfo info;
  FILE *fp = popen("/usr/bin/dmidecode -t memory 2>/dev/null", "r");
  if (fp == nullptr)
  {
    fp = popen("dmidecode -t memory 2>/dev/null", "r");
  }
  if (fp == nullptr)
  {
    return info;
  }

  char buffer[256];
  bool in_device = false;
  bool device_has_size = false;

  while (fgets(buffer, sizeof(buffer), fp) != NULL)
  {
    std::string line(buffer);
    if (!line.empty() && line.back() == '\n')
    {
      line.pop_back();
    }

    if (line == "Memory Device")
    {
      info.slots_total++;
      in_device = true;
      device_has_size = false;
    }
    else if (in_device)
    {
      if (line.empty())
      {
        in_device = false;
        if (device_has_size)
          info.slots_used++;
        continue;
      }

      if (line.find("Size:") != std::string::npos)
      {
        if (line.find("No Module Installed") == std::string::npos && line.find("Unknown") == std::string::npos)
        {
          device_has_size = true;
        }
      }
      else if (line.find("Form Factor:") != std::string::npos && device_has_size)
      {
        size_t pos = line.find(":");
        if (pos != std::string::npos)
        {
          std::string val = line.substr(pos + 1);
          val.erase(0, val.find_first_not_of(" \t"));
          if (val != "Unknown" && val != "None")
            info.form_factor = val;
        }
      }
      else if (line.find("Type:") != std::string::npos && device_has_size && line.find("Type Detail") == std::string::npos)
      {
        size_t pos = line.find(":");
        if (pos != std::string::npos)
        {
          std::string val = line.substr(pos + 1);
          val.erase(0, val.find_first_not_of(" \t"));
          if (val != "Unknown" && val != "None")
            info.type = val;
        }
      }
      else if (line.find("Speed:") != std::string::npos && device_has_size && line.find("Configured") == std::string::npos)
      {
        size_t pos = line.find(":");
        if (pos != std::string::npos)
        {
          std::string val = line.substr(pos + 1);
          val.erase(0, val.find_first_not_of(" \t"));
          if (val != "Unknown" && val != "None")
            info.speed = val;
        }
      }
    }
  }
  if (in_device && device_has_size)
  {
    info.slots_used++;
  }
  pclose(fp);
  return info;
}

int ProcessManager::get_cpu_threads_count()
{
  int count = 0;
  std::ifstream file("/proc/cpuinfo");
  if (file.is_open())
  {
    std::string line;
    while (std::getline(file, line))
    {
      if (line.rfind("processor", 0) == 0)
      {
        count++;
      }
    }
  }
  return count > 0 ? count : 1;
}

bool ProcessManager::check_is_app(int pid)
{
  std::string env_path = "/proc/" + std::to_string(pid) + "/environ";
  std::ifstream env_file(env_path);
  if (env_file.is_open())
  {
    std::string env_vars;
    std::stringstream buffer;
    buffer << env_file.rdbuf();
    env_vars = buffer.str();

    size_t pos = 0;
    while (pos < env_vars.length())
    {
      size_t end_pos = env_vars.find('\0', pos);
      if (end_pos == std::string::npos)
        end_pos = env_vars.length();

      std::string var = env_vars.substr(pos, end_pos - pos);
      if (var.rfind("DISPLAY=", 0) == 0 || var.rfind("WAYLAND_DISPLAY=", 0) == 0)
      {
        return true;
      }
      pos = end_pos + 1;
    }
  }
  return false;
}

std::vector<ProcessInfo> ProcessManager::get_processes()
{
  std::vector<ProcessInfo> current_processes;
  DIR *dir = opendir("/proc");
  if (dir == nullptr)
  {
    return current_processes;
  }

  double uptime = get_uptime_internal();
  std::map<int, CpuData> current_cpu_map;

  struct dirent *ent;
  while ((ent = readdir(dir)) != nullptr)
  {
    if (ent->d_type == DT_DIR)
    {
      std::string dir_name(ent->d_name);
      bool is_pid = true;
      for (char c : dir_name)
      {
        if (!isdigit(c))
        {
          is_pid = false;
          break;
        }
      }

      if (is_pid)
      {
        int pid = std::stoi(dir_name);
        ProcessInfo info;
        info.pid = pid;
        info.cpu_usage = 0.0;
        info.is_app = check_is_app(pid);

        std::string status_path = "/proc/" + dir_name + "/status";
        std::ifstream status_file(status_path);
        int uid = -1;
        info.memory_kb = 0;

        if (status_file.is_open())
        {
          std::string line;
          while (std::getline(status_file, line))
          {
            if (line.rfind("Name:", 0) == 0)
            {
              info.name = line.substr(6);
              size_t start = info.name.find_first_not_of(" \t");
              if (start != std::string::npos)
                info.name = info.name.substr(start);
            }
            else if (line.rfind("State:", 0) == 0)
            {
              size_t start = line.find_first_not_of(" \t", 6);
              if (start != std::string::npos)
              {
                std::string state_char = line.substr(start, 1);
                info.state = translate_state(state_char);
              }
            }
            else if (line.rfind("Uid:", 0) == 0)
            {
              std::stringstream ss(line.substr(5));
              ss >> uid;
            }
            else if (line.rfind("PPid:", 0) == 0)
            {
              std::stringstream ss(line.substr(5));
              ss >> info.ppid;
            }
            else if (line.rfind("VmRSS:", 0) == 0)
            {
              std::stringstream ss(line.substr(7));
              ss >> info.memory_kb;
            }
          }
        }

        if (uid != -1)
        {
          info.user = get_user_from_uid(uid);
        }

        // Read /proc/[pid]/cmdline for better name
        std::string cmdline_path = "/proc/" + dir_name + "/cmdline";
        std::ifstream cmdline_file(cmdline_path);
        if (cmdline_file.is_open())
        {
          std::string cmdline;
          std::getline(cmdline_file, cmdline, '\0');
          if (!cmdline.empty())
          {
            size_t pos = cmdline.find_last_of('/');
            std::string cmd_name = (pos != std::string::npos) ? cmdline.substr(pos + 1) : cmdline;

            // If the name is generic like "exe" or "AppRun", try to use the
            // parent directory name
            if (cmd_name == "exe" || cmd_name == "AppRun" || cmd_name == "chrome_crashpad_handler" || info.name == "Main" ||
                info.name == "Worker" || info.name == "Zygote")
            {
              if (pos != std::string::npos && pos > 0)
              {
                size_t parent_pos = cmdline.find_last_of('/', pos - 1);
                if (parent_pos != std::string::npos)
                {
                  std::string parent_dir = cmdline.substr(parent_pos + 1, pos - parent_pos - 1);
                  if (!parent_dir.empty() && parent_dir != "bin" && parent_dir != "usr")
                  {
                    cmd_name = parent_dir;
                  }
                }
              }
            }

            // Only overwrite info.name if the new name is longer and starts
            // with the old name (this restores untruncated names without
            // breaking descriptive thread names like "Web Content") Or if the
            // original name was literally "exe"
            if (info.name == "exe" || info.name == "Main" || info.name == "Worker" || cmd_name.rfind(info.name, 0) == 0)
            {
              info.name = cmd_name;
            }
          }
        }

        // Read /proc/[pid]/stat for CPU usage
        std::string stat_path = "/proc/" + dir_name + "/stat";
        std::ifstream stat_file(stat_path);
        if (stat_file.is_open())
        {
          std::string line;
          if (std::getline(stat_file, line))
          {
            size_t rp = line.find_last_of(')');
            if (rp != std::string::npos)
            {
              std::string rest = line.substr(rp + 2);
              std::stringstream ss(rest);
              std::string dummy, dummy_state;
              unsigned long long utime, stime;
              ss >> dummy_state >> info.ppid;
              for (int i = 0; i < 9; ++i)
                ss >> dummy;
              ss >> utime >> stime;

              current_cpu_map[pid] = {utime, stime};

              if (!first_process_run && prev_process_cpu.count(pid) > 0)
              {
                double uptime_diff = uptime - prev_uptime;
                if (uptime_diff > 0)
                {
                  unsigned long long prev_utime = prev_process_cpu[pid].utime;
                  unsigned long long prev_stime = prev_process_cpu[pid].stime;

                  unsigned long long total_time_diff = (utime + stime) - (prev_utime + prev_stime);
                  info.cpu_usage = (100.0 * ((double)total_time_diff / hertz) / uptime_diff) / get_cpu_threads_count();
                }
              }
            }
          }
        }

        current_processes.push_back(info);
      }
    }
  }
  closedir(dir);

  prev_process_cpu = current_cpu_map;
  prev_uptime = uptime;
  first_process_run = false;

  return current_processes;
}

bool ProcessManager::kill_process(int pid)
{
  return kill(pid, SIGTERM) == 0;
}
