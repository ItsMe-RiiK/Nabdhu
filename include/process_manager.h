#pragma once
#include <string>
#include <unordered_map>
#include <vector>

struct CachedProcessData
{
  std::string command;
  std::string name;
  std::string user;
  bool is_app;
};

struct ProcessInfo
{
  int pid;
  int ppid;
  std::string name;
  std::string command;
  std::string user;
  std::string state;
  int threads;
  long long memory_kb;
  double cpu_usage;
  unsigned long long cumulative_cpu_time;
  bool is_app;
};

struct GlobalCpuData
{
  unsigned long long user = 0, nice = 0, system = 0, idle = 0, iowait = 0, irq = 0, softirq = 0, steal = 0;

  unsigned long long total() const
  {
    return user + nice + system + idle + iowait + irq + softirq + steal;
  }

  unsigned long long idle_all() const
  {
    return idle + iowait;
  }
};

struct GlobalMemData
{
  long long total_kb = 0;
  long long free_kb = 0;
  long long available_kb = 0;
  long long cached_kb = 0;
  long long swap_total_kb = 0;
  long long swap_free_kb = 0;
};

struct MemHardwareInfo
{
  std::string speed = "-";
  std::string type = "-";
  std::string form_factor = "-";
  int slots_used = 0;
  int slots_total = 0;
};

struct CpuHardwareInfo
{
  std::string model_name;
  std::string speed;
  std::string real_speed;
  double load_avg[3];
  std::vector<int> core_temps;
};

struct BatteryInfo
{
  bool present = false;
  int capacity = 0; // 0-100%
  std::string status; // Charging, Discharging, Full
};

struct GpuInfo
{
  std::string name;
  double utilization = 0.0;
  double memory_used_mb = 0.0;
  double memory_total_mb = 0.0;
  double temperature = 0.0;
  double power_watts = 0.0;
};

class ProcessManager
{
public:
  ProcessManager();
  ~ProcessManager();

  std::vector<ProcessInfo> get_processes();
  static bool kill_process(int pid);

  double get_global_cpu_usage();
  std::vector<double> get_core_cpu_usage();
  static GlobalMemData get_global_memory();
  static MemHardwareInfo get_memory_hardware_info();
  static CpuHardwareInfo get_cpu_hardware_info();
  static BatteryInfo get_battery_info();
  static std::vector<GpuInfo> get_gpu_info();

  long get_system_uptime();
  static int get_cpu_threads_count();

private:
  long hertz;

  struct CpuData
  {
    unsigned long long utime;
    unsigned long long stime;
  };

  std::unordered_map<int, CpuData> prev_process_cpu;
  std::unordered_map<int, CachedProcessData> process_cache;
  double last_cache_time;
  double prev_uptime;

  std::vector<GlobalCpuData> prev_cpu_data;
  std::vector<double> last_core_usages;
  double last_global_usage;
  bool first_global_cpu_run;
  bool first_process_run;

  static double get_uptime_internal();
  long get_hertz() const;
  static std::string get_user_from_uid(int uid);
  static std::string translate_state(char state_char);
  static std::vector<GlobalCpuData> read_all_cpu_data();
  static bool check_is_app(int pid);
};
