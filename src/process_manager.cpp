#include "process_manager.h"

#include <algorithm>
#include <chrono>
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
  int fd = open("/proc/uptime", O_RDONLY);
  if (fd != -1)
  {
    char buf[64];
    ssize_t bytes = read(fd, buf, sizeof(buf) - 1);
    close(fd);
    if (bytes > 0)
    {
      buf[bytes] = '\0';
      return std::atof(buf);
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

std::string ProcessManager::translate_state(char state_char)
{
  switch (state_char)
  {
  case 'R':
    return "Running";
  case 'S':
    return "Sleeping";
  case 'D':
    return "Disk Sleep";
  case 'Z':
    return "Zombie";
  case 'T':
    return "Stopped";
  case 't':
    return "Tracing Stop";
  case 'X':
  case 'x':
    return "Dead";
  case 'K':
    return "Wakekill";
  case 'W':
    return "Waking";
  case 'P':
    return "Parked";
  case 'I':
    return "Idle";
  default:
    return std::string(1, state_char);
  }
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
  int fd = open("/proc/meminfo", O_RDONLY);
  if (fd == -1)
    return data;
  char buf[4096];
  ssize_t bytes = read(fd, buf, sizeof(buf) - 1);
  close(fd);
  if (bytes > 0)
  {
    buf[bytes] = '\0';
    char *ptr = buf;
    while (*ptr)
    {
      if (strncmp(ptr, "MemTotal:", 9) == 0)
        data.total_kb = atoll(ptr + 9);
      else if (strncmp(ptr, "MemFree:", 8) == 0)
        data.free_kb = atoll(ptr + 8);
      else if (strncmp(ptr, "MemAvailable:", 13) == 0)
        data.available_kb = atoll(ptr + 13);
      else if (strncmp(ptr, "Cached:", 7) == 0)
        data.cached_kb = atoll(ptr + 7);
      else if (strncmp(ptr, "SwapTotal:", 10) == 0)
        data.swap_total_kb = atoll(ptr + 10);
      else if (strncmp(ptr, "SwapFree:", 9) == 0)
        data.swap_free_kb = atoll(ptr + 9);

      while (*ptr && *ptr != '\n')
        ptr++;
      if (*ptr == '\n')
        ptr++;
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
  static std::string cached_model = "Unknown";
  static std::string cached_speed = "0.0 GHz";
  static bool cpu_static_cached = false;

  CpuHardwareInfo info;
  info.model_name = cached_model;
  info.speed = cached_speed;
  info.load_avg[0] = info.load_avg[1] = info.load_avg[2] = 0.0;

  std::ifstream loadavg_file("/proc/loadavg");
  if (loadavg_file.is_open())
  {
    loadavg_file >> info.load_avg[0] >> info.load_avg[1] >> info.load_avg[2];
  }

  if (!cpu_static_cached)
  {
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

            size_t at_pos = val.find("@ ");
            if (at_pos != std::string::npos)
            {
              info.speed = val.substr(at_pos + 2);
            }
            else
            {
              info.speed = ""; // Fallback
            }

            size_t intel_pos = val.find("Intel(R) Core(TM) ");
            if (intel_pos != std::string::npos)
            {
              val.replace(intel_pos, 18, "Intel ");
              size_t cpu_pos = val.find(" CPU @");
              if (cpu_pos != std::string::npos)
              {
                val = val.substr(0, cpu_pos);
              }
            }
            else
            {
              size_t rx = val.find("Ryzen ");
              if (rx != std::string::npos)
              {
                size_t end_pos = rx + 6;
                while (end_pos < val.length() && (isdigit(val[end_pos]) || val[end_pos] == 'X'))
                {
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
    cached_model = info.model_name;
    cached_speed = info.speed;
    cpu_static_cached = true;
  }

  // Real-time speed (throttled)
  static int mhz_counter = 0;
  static std::string cached_real_speed = "";

  if (++mhz_counter >= 5 || cached_real_speed.empty())
  {
    mhz_counter = 0;
    int fd = open("/proc/cpuinfo", O_RDONLY);
    if (fd != -1)
    {
      char buf[16384];
      ssize_t bytes = read(fd, buf, sizeof(buf) - 1);
      close(fd);
      if (bytes > 0)
      {
        buf[bytes] = '\0';
        double sum_mhz = 0.0;
        int count = 0;
        char *ptr = buf;
        while (*ptr)
        {
          if (strncmp(ptr, "cpu MHz", 7) == 0)
          {
            while (*ptr && *ptr != ':')
              ptr++;
            if (*ptr == ':')
            {
              ptr++;
              sum_mhz += std::atof(ptr);
              count++;
            }
          }
          while (*ptr && *ptr != '\n')
            ptr++;
          if (*ptr == '\n')
            ptr++;
        }
        if (count > 0)
        {
          double avg_mhz = sum_mhz / count;
          char out_buf[64];
          if (avg_mhz >= 1000.0)
          {
            snprintf(out_buf, sizeof(out_buf), "%.2f GHz", avg_mhz / 1000.0);
          }
          else
          {
            snprintf(out_buf, sizeof(out_buf), "%d MHz", (int)avg_mhz);
          }
          cached_real_speed = out_buf;
        }
        else
        {
          cached_real_speed = info.speed;
        }
      }
    }
  }
  info.real_speed = cached_real_speed;

  info.core_temps.clear();

  static std::vector<std::string> cached_temp_paths;
  static bool temp_paths_found = false;

  if (!temp_paths_found)
  {
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
            cached_temp_paths.push_back(hwmon_path + "/" + name);
          }
        }
        closedir(dir);
      }
    }
    if (cached_temp_paths.empty())
    {
      for (int i = 0; i < 10; ++i)
      {
        std::string thermal_path = "/sys/class/thermal/thermal_zone" + std::to_string(i) + "/temp";
        struct stat st;
        if (stat(thermal_path.c_str(), &st) == 0)
        {
          cached_temp_paths.push_back(thermal_path);
        }
      }
    }
    temp_paths_found = true;
  }

  info.core_temps.clear();
  for (const auto &path : cached_temp_paths)
  {
    int fd = open(path.c_str(), O_RDONLY);
    if (fd != -1)
    {
      char buf[32];
      ssize_t bytes = read(fd, buf, sizeof(buf) - 1);
      close(fd);
      if (bytes > 0)
      {
        buf[bytes] = '\0';
        info.core_temps.push_back(std::atoi(buf) / 1000);
      }
    }
  }

  std::sort(info.core_temps.begin(), info.core_temps.end(), std::greater<int>());

  return info;
}

int ProcessManager::get_cpu_threads_count()
{
  static int cached_count = 0;
  if (cached_count == 0)
  {
    cached_count = sysconf(_SC_NPROCESSORS_ONLN);
    if (cached_count <= 0)
      cached_count = 1;
  }
  return cached_count;
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

  long page_size = sysconf(_SC_PAGESIZE);
  char stat_buf[1024];

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
        info.threads = 1;

        bool found_in_cache = false;
        auto cache_it = process_cache.find(pid);
        if (cache_it != process_cache.end())
        {
          info.command = cache_it->second.command;
          info.name = cache_it->second.name;
          info.user = cache_it->second.user;
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

          std::string proc_dir = "/proc/" + dir_name;
          struct stat st;
          if (stat(proc_dir.c_str(), &st) == 0)
          {
            info.user = get_user_from_uid(st.st_uid);
          }

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
            new_cache_data.user = info.user;
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
              if (info.name.empty() || info.name == "self" || info.name == "Self" || info.name == "exe")
              {
                char *lp = strchr(stat_buf, '(');
                if (lp && lp < rp)
                {
                  info.name = std::string(lp + 1, rp - lp - 1);
                  if (update_cache)
                  {
                    new_cache[pid].name = info.name;
                  }
                }
              }

              if (info.name.empty() || info.name == "self" || info.name == "Self" || info.name == "exe")
              {
                if (!info.command.empty())
                {
                  size_t space_pos = info.command.find(' ');
                  std::string first_cmd = (space_pos != std::string::npos) ? info.command.substr(0, space_pos) : info.command;
                  size_t slash_pos = first_cmd.find_last_of('/');
                  if (slash_pos != std::string::npos)
                    first_cmd = first_cmd.substr(slash_pos + 1);
                  if (!first_cmd.empty())
                  {
                    info.name = first_cmd;
                    if (update_cache)
                      new_cache[pid].name = info.name;
                  }
                }
              }
              if (info.name.empty())
              {
                info.name = "Unknown";
              }

              char state_char;
              unsigned long long utime, stime;
              int ppid, threads;
              char *rest = rp + 2;

              int dummy;
              unsigned long ldummy;
              int items = sscanf(rest, "%c %d %d %d %d %d %lu %lu %lu %lu %lu %llu %llu %lu %lu %lu %lu %d", &state_char, &info.ppid,
                                 &dummy, &dummy, &dummy, &dummy, &ldummy, &ldummy, &ldummy, &ldummy, &ldummy, &utime, &stime, &ldummy,
                                 &ldummy, &ldummy, &ldummy, &info.threads);

              if (items >= 13)
              {
                info.state = translate_state(state_char);
                current_cpu_map[pid] = { utime, stime };
                info.cumulative_cpu_time = utime + stime;

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

        std::string statm_path = "/proc/" + dir_name + "/statm";
        int fd_statm = open(statm_path.c_str(), O_RDONLY);
        if (fd_statm != -1)
        {
          char statm_buf[256];
          ssize_t bytes_read = read(fd_statm, statm_buf, sizeof(statm_buf) - 1);
          if (bytes_read > 0)
          {
            statm_buf[bytes_read] = '\0';
            long long size, resident;
            if (sscanf(statm_buf, "%lld %lld", &size, &resident) == 2)
            {
              info.memory_kb = (resident * page_size) / 1024;
            }
          }
          close(fd_statm);
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

BatteryInfo ProcessManager::get_battery_info()
{
  BatteryInfo info;
  for (int i = 0; i < 2; i++)
  {
    std::string path = "/sys/class/power_supply/BAT" + std::to_string(i);
    std::ifstream cap_file(path + "/capacity");
    if (cap_file.is_open())
    {
      info.present = true;
      cap_file >> info.capacity;
      cap_file.close();

      std::ifstream stat_file(path + "/status");
      if (stat_file.is_open())
      {
        std::getline(stat_file, info.status);
      }
      break;
    }
  }
  return info;
}

std::vector<GpuInfo> ProcessManager::get_gpu_info()
{
  std::vector<GpuInfo> gpus;
  return gpus; // Disabled to save CPU

  // NVIDIA
  FILE *pipe = popen(
      "nvidia-smi --query-gpu=name,utilization.gpu,memory.used,memory.total,temperature.gpu,power.draw --format=csv,noheader,nounits 2>/dev/null",
      "r");
  if (pipe)
  {
    char buffer[256];
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
    {
      std::string line(buffer);
      GpuInfo info;
      size_t pos = 0;
      auto get_next = [&]()
      {
        size_t next = line.find(',', pos);
        std::string val = (next == std::string::npos) ? line.substr(pos) : line.substr(pos, next - pos);
        if (next != std::string::npos)
          pos = next + 1;
        while (!val.empty() && val.front() == ' ')
          val.erase(0, 1);
        while (!val.empty() && (val.back() == '\n' || val.back() == '\r'))
          val.pop_back();
        return val;
      };

      info.name = get_next();
      try
      {
        info.utilization = std::stod(get_next());
      }
      catch (...)
      {
      }
      try
      {
        info.memory_used_mb = std::stod(get_next());
      }
      catch (...)
      {
      }
      try
      {
        info.memory_total_mb = std::stod(get_next());
      }
      catch (...)
      {
      }
      try
      {
        info.temperature = std::stod(get_next());
      }
      catch (...)
      {
      }
      try
      {
        info.power_watts = std::stod(get_next());
      }
      catch (...)
      {
      }

      if (!info.name.empty())
      {
        gpus.push_back(info);
      }
    }
    pclose(pipe);
  }

  // Fallbacks for AMD/Intel
  if (gpus.empty())
  {
    static unsigned long long prev_rc6 = 0;
    static double prev_time = 0.0;
    for (int i = 0; i < 4; i++)
    {
      std::string path = "/sys/class/drm/card" + std::to_string(i);
      struct stat st;
      if (stat(path.c_str(), &st) == 0)
      {
        // check if Intel
        std::string rc6_path = path + "/gt/gt0/rc6_residency_ms";
        if (stat(rc6_path.c_str(), &st) == 0)
        {
          std::ifstream rc6_file(rc6_path);
          unsigned long long rc6 = 0;
          if (rc6_file >> rc6)
          {
            auto now = std::chrono::steady_clock::now();
            double current_time = std::chrono::duration<double>(now.time_since_epoch()).count();
            if (prev_time > 0.0)
            {
              double time_diff = current_time - prev_time;
              double rc6_diff = (rc6 - prev_rc6) / 1000.0; // ms to seconds
              double util = 100.0 * (1.0 - (rc6_diff / time_diff));
              if (util < 0.0)
                util = 0.0;
              if (util > 100.0)
                util = 100.0;

              static std::string cached_intel_name = "";
              if (cached_intel_name.empty())
              {
                cached_intel_name = "Intel GPU";
                FILE *fp = popen("lspci | grep -i 'vga\\|3d\\|display' | grep -i intel", "r");
                if (fp)
                {
                  char buf[256];
                  if (fgets(buf, sizeof(buf), fp) != nullptr)
                  {
                    std::string line(buf);
                    size_t pos = line.find("Intel Corporation ");
                    if (pos != std::string::npos)
                    {
                      cached_intel_name = "Intel " + line.substr(pos + 18);
                      size_t rev_pos = cached_intel_name.find(" (rev");
                      if (rev_pos != std::string::npos)
                        cached_intel_name = cached_intel_name.substr(0, rev_pos);
                      size_t bracket_pos = cached_intel_name.find(" [");
                      if (bracket_pos != std::string::npos)
                        cached_intel_name = cached_intel_name.substr(0, bracket_pos);
                      while (!cached_intel_name.empty() && (cached_intel_name.back() == '\n' || cached_intel_name.back() == '\r'))
                        cached_intel_name.pop_back();
                    }
                  }
                  pclose(fp);
                }
              }
              GpuInfo info;
              info.name = cached_intel_name;
              info.utilization = util;
              gpus.push_back(info);
            }
            prev_rc6 = rc6;
            prev_time = current_time;
          }
          if (!gpus.empty())
            break;
        }
        // check AMD
        std::string busy_path = path + "/device/gpu_busy_percent";
        if (stat(busy_path.c_str(), &st) == 0)
        {
          std::ifstream busy_file(busy_path);
          int busy = 0;
          if (busy_file >> busy)
          {
            static std::string cached_amd_name = "";
            if (cached_amd_name.empty())
            {
              cached_amd_name = "AMD GPU";
              FILE *fp = popen("lspci | grep -i 'vga\\|3d\\|display' | grep -i amd", "r");
              if (fp)
              {
                char buf[256];
                if (fgets(buf, sizeof(buf), fp) != nullptr)
                {
                  std::string line(buf);
                  size_t pos = line.find(": ");
                  if (pos != std::string::npos)
                  {
                    cached_amd_name = line.substr(pos + 2);
                    size_t rev_pos = cached_amd_name.find(" (rev");
                    if (rev_pos != std::string::npos)
                      cached_amd_name = cached_amd_name.substr(0, rev_pos);
                    while (!cached_amd_name.empty() && (cached_amd_name.back() == '\n' || cached_amd_name.back() == '\r'))
                      cached_amd_name.pop_back();
                  }
                }
                pclose(fp);
              }
            }
            GpuInfo info;
            info.name = cached_amd_name;
            info.utilization = busy;
            gpus.push_back(info);
          }
          if (!gpus.empty())
            break;
        }
      }
    }
  }

  return gpus;
}
