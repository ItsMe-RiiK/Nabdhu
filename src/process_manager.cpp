#include "process_manager.h"

#include <algorithm>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <fstream>
#include <iostream>
#include <pwd.h>
#include <signal.h>
#include <sstream>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

ProcessManager::ProcessManager() : last_cache_time(0.0), prev_uptime(0), first_global_cpu_run(true), first_process_run(true)
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

std::vector<GlobalCpuData> ProcessManager::read_all_cpu_data()
{
  std::vector<GlobalCpuData> data_list;
  std::ifstream file("/proc/stat");
  if (file.is_open())
  {
    std::string line;
    while (std::getline(file, line))
    {
      if (line.substr(0, 3) == "cpu")
      {
        GlobalCpuData data;
        std::stringstream ss(line);
        std::string cpu_label;
        ss >> cpu_label >> data.user >> data.nice >> data.system >> data.idle >> data.iowait >> data.irq >> data.softirq >> data.steal;
        data_list.push_back(data);
      }
      else
      {
        break; // cpu lines are always at the top
      }
    }
  }
  return data_list;
}

double ProcessManager::get_global_cpu_usage()
{
  std::vector<GlobalCpuData> current_cpu_data = read_all_cpu_data();
  if (current_cpu_data.empty())
    return 0.0;

  double global_usage = 0.0;
  std::vector<double> core_usages;

  if (!first_global_cpu_run && prev_cpu_data.size() == current_cpu_data.size())
  {
    for (size_t i = 0; i < current_cpu_data.size(); ++i)
    {
      unsigned long long prev_total = prev_cpu_data[i].total();
      unsigned long long curr_total = current_cpu_data[i].total();
      unsigned long long prev_idle = prev_cpu_data[i].idle_all();
      unsigned long long curr_idle = current_cpu_data[i].idle_all();

      unsigned long long total_diff = curr_total - prev_total;
      unsigned long long idle_diff = curr_idle - prev_idle;

      double usage = 0.0;
      if (total_diff > 0)
      {
        usage = 100.0 * (total_diff - idle_diff) / (double)total_diff;
      }

      if (i == 0)
      {
        global_usage = usage;
      }
      else
      {
        core_usages.push_back(usage);
      }
    }
  }

  prev_cpu_data = current_cpu_data;
  last_global_usage = global_usage;
  last_core_usages = core_usages;
  first_global_cpu_run = false;

  return last_global_usage;
}

std::vector<double> ProcessManager::get_core_cpu_usage()
{
  return last_core_usages;
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

CpuHardwareInfo ProcessManager::get_cpu_hardware_info()
{
  CpuHardwareInfo info;
  info.model_name = "Unknown";
  info.speed = "0.0 GHz";
  info.load_avg[0] = info.load_avg[1] = info.load_avg[2] = 0.0;

  std::ifstream loadavg_file("/proc/loadavg");
  if (loadavg_file.is_open())
  {
    loadavg_file >> info.load_avg[0] >> info.load_avg[1] >> info.load_avg[2];
  }

  std::ifstream cpuinfo_file("/proc/cpuinfo");
  if (cpuinfo_file.is_open())
  {
    std::string line;
    while (std::getline(cpuinfo_file, line))
    {
      if (line.find("model name") != std::string::npos && info.model_name == "Unknown")
      {
        size_t pos = line.find(":");
        if (pos != std::string::npos)
        {
          std::string val = line.substr(pos + 1);
          val.erase(0, val.find_first_not_of(" \t"));

          // Extract static speed from model name
          size_t at_pos = val.find("@ ");
          if (at_pos != std::string::npos)
          {
            info.speed = val.substr(at_pos + 2);
          }
          else
          {
            info.speed = ""; // Fallback
          }

          // Simplify Intel core names
          size_t ix = val.find("i3-");
          if (ix == std::string::npos)
            ix = val.find("i5-");
          if (ix == std::string::npos)
            ix = val.find("i7-");
          if (ix == std::string::npos)
            ix = val.find("i9-");
          if (ix != std::string::npos)
          {
            size_t space_pos = val.find(" ", ix);
            if (space_pos != std::string::npos)
              val = val.substr(ix, space_pos - ix);
            else
              val = val.substr(ix);
          }
          else
          {
            // Simplify AMD Ryzen names
            size_t rx = val.find("Ryzen");
            if (rx != std::string::npos)
            {
              // Try to grab "Ryzen X XXXXX"
              int spaces = 0;
              size_t end_pos = rx;
              while (end_pos < val.length() && spaces < 3)
              {
                if (val[end_pos] == ' ')
                  spaces++;
                if (spaces < 3)
                  end_pos++;
              }
              val = val.substr(rx, end_pos - rx);
            }
          }

          info.model_name = val;
        }
      }
    }
  }

  for (int i = 0; i < 10; ++i)
  {
    std::string hwmon_path = "/sys/class/hwmon/hwmon" + std::to_string(i);
    DIR *dir = opendir(hwmon_path.c_str());
    if (dir)
    {
      struct dirent *ent;
      while ((ent = readdir(dir)) != nullptr)
      {
        std::string name(ent->d_name);
        if (name.find("temp") == 0 && name.find("_input") != std::string::npos)
        {
          std::ifstream temp_file(hwmon_path + "/" + name);
          if (temp_file.is_open())
          {
            int millidegrees;
            if (temp_file >> millidegrees)
            {
              info.core_temps.push_back(millidegrees / 1000);
            }
          }
        }
      }
      closedir(dir);
    }
  }

  if (info.core_temps.empty())
  {
    for (int i = 0; i < 10; ++i)
    {
      std::string thermal_path = "/sys/class/thermal/thermal_zone" + std::to_string(i) + "/temp";
      std::ifstream temp_file(thermal_path);
      if (temp_file.is_open())
      {
        int millidegrees;
        if (temp_file >> millidegrees)
        {
          info.core_temps.push_back(millidegrees / 1000);
        }
      }
    }
  }

  std::sort(info.core_temps.begin(), info.core_temps.end(), std::greater<int>());

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
  int fd = open(env_path.c_str(), O_RDONLY);
  if (fd != -1)
  {
    char buf[4096];
    ssize_t bytes_read = read(fd, buf, sizeof(buf));
    close(fd);
    if (bytes_read > 0)
    {
      size_t pos = 0;
      while (pos < bytes_read)
      {
        size_t end_pos = pos;
        while (end_pos < bytes_read && buf[end_pos] != '\0')
          end_pos++;

        if (end_pos - pos >= 8 && strncmp(&buf[pos], "DISPLAY=", 8) == 0)
          return true;
        if (end_pos - pos >= 16 && strncmp(&buf[pos], "WAYLAND_DISPLAY=", 16) == 0)
          return true;

        pos = end_pos + 1;
      }
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
  std::unordered_map<int, CpuData> current_cpu_map;
  std::unordered_map<int, CachedProcessData> new_cache;

  bool update_cache = false;
  if (uptime - last_cache_time >= 2.0 || last_cache_time == 0.0)
  {
    update_cache = true;
    last_cache_time = uptime;
  }

  char stat_buf[1024];
  std::string line;
  line.reserve(512);

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
        info.memory_kb = 0;
        int uid = -1;

        std::string status_path = "/proc/" + dir_name + "/status";
        std::ifstream status_file(status_path);

        if (status_file.is_open())
        {
          line.clear();
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
              sscanf(line.c_str(), "Uid: %d", &uid);
            }
            else if (line.rfind("PPid:", 0) == 0)
            {
              sscanf(line.c_str(), "PPid: %d", &info.ppid);
            }
            else if (line.rfind("Threads:", 0) == 0)
            {
              sscanf(line.c_str(), "Threads: %d", &info.threads);
            }
            else if (line.rfind("VmRSS:", 0) == 0)
            {
              sscanf(line.c_str(), "VmRSS: %lld", &info.memory_kb);
            }
          }
        }

        if (uid != -1)
        {
          info.user = get_user_from_uid(uid);
        }

        bool found_in_cache = false;
        auto cache_it = process_cache.find(pid);
        if (cache_it != process_cache.end())
        {
          info.command = cache_it->second.command;
          info.name = cache_it->second.name;
          info.is_app = cache_it->second.is_app;
          found_in_cache = true;
          if (update_cache)
          {
            new_cache[pid] = cache_it->second;
          }
        }

        if (!found_in_cache)
        {
          info.is_app = check_is_app(pid);

          std::string cmdline_path = "/proc/" + dir_name + "/cmdline";
          int fd_cmd = open(cmdline_path.c_str(), O_RDONLY);
          if (fd_cmd != -1)
          {
            char cmd_buf[4096];
            ssize_t bytes_read = read(fd_cmd, cmd_buf, sizeof(cmd_buf) - 1);
            if (bytes_read > 0)
            {
              cmd_buf[bytes_read] = '\0';
              std::string full_cmd(cmd_buf, bytes_read);
              for (char &c : full_cmd)
              {
                if (c == '\0')
                  c = ' ';
              }
              if (!full_cmd.empty() && full_cmd.back() == ' ')
                full_cmd.pop_back();
              info.command = full_cmd;

              std::string cmdline(cmd_buf);
              if (!cmdline.empty())
              {
                size_t pos = cmdline.find_last_of('/');
                std::string cmd_name = (pos != std::string::npos) ? cmdline.substr(pos + 1) : cmdline;

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

                if (info.name == "exe" || info.name == "Main" || info.name == "Worker" || cmd_name.rfind(info.name, 0) == 0)
                {
                  info.name = cmd_name;
                }
              }
            }
            close(fd_cmd);
          }

          if (update_cache)
          {
            CachedProcessData new_cache_data;
            new_cache_data.command = info.command;
            new_cache_data.name = info.name;
            new_cache_data.is_app = info.is_app;
            new_cache[pid] = new_cache_data;
          }
        }

        std::string stat_path = "/proc/" + dir_name + "/stat";
        int fd_stat = open(stat_path.c_str(), O_RDONLY);
        if (fd_stat != -1)
        {
          ssize_t bytes_read = read(fd_stat, stat_buf, sizeof(stat_buf) - 1);
          if (bytes_read > 0)
          {
            stat_buf[bytes_read] = '\0';
            char *rp = strrchr(stat_buf, ')');
            if (rp != nullptr)
            {
              char dummy_state;
              unsigned long long utime, stime;

              char *rest = rp + 2;

              int dummy_ppid;
              unsigned long dummy_uval;

              int items =
                  sscanf(rest, "%c %d %d %d %d %d %lu %lu %lu %lu %lu %llu %llu", &dummy_state, &info.ppid, &dummy_ppid, &dummy_ppid,
                         &dummy_ppid, &dummy_ppid, &dummy_uval, &dummy_uval, &dummy_uval, &dummy_uval, &dummy_uval, &utime, &stime);

              if (items == 13)
              {
                current_cpu_map[pid] = { utime, stime };

                if (!first_process_run)
                {
                  auto prev_it = prev_process_cpu.find(pid);
                  if (prev_it != prev_process_cpu.end())
                  {
                    double uptime_diff = uptime - prev_uptime;
                    if (uptime_diff > 0)
                    {
                      unsigned long long prev_utime = prev_it->second.utime;
                      unsigned long long prev_stime = prev_it->second.stime;

                      unsigned long long total_time_diff = (utime + stime) - (prev_utime + prev_stime);
                      info.cpu_usage = (100.0 * ((double)total_time_diff / hertz) / uptime_diff) / get_cpu_threads_count();
                    }
                  }
                }
              }
            }
          }
          close(fd_stat);
        }

        current_processes.push_back(info);
      }
    }
  }
  closedir(dir);

  if (update_cache)
  {
    process_cache = std::move(new_cache);
  }
  else
  {
    // If we didn't update cache this frame, we need to retain process_cache for the next frame
    // We prune dead pids from it based on current pids
    for (auto it = process_cache.begin(); it != process_cache.end();)
    {
      if (current_cpu_map.find(it->first) == current_cpu_map.end())
      {
        it = process_cache.erase(it);
      }
      else
      {
        ++it;
      }
    }
  }

  prev_process_cpu = std::move(current_cpu_map);
  prev_uptime = uptime;
  first_process_run = false;

  return current_processes;
}

bool ProcessManager::kill_process(int pid)
{
  return kill(pid, SIGTERM) == 0;
}
