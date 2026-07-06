#pragma once
#include <map>
#include <string>
#include <vector>

struct ProcessInfo
{
  int pid;
  int ppid;
  std::string user;
  std::string name;
  std::string state;
  long long memory_kb;
  double cpu_usage;
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

class ProcessManager
{
public:
  ProcessManager();
  ~ProcessManager();

  std::vector<ProcessInfo> get_processes();
  static bool kill_process(int pid);

  double get_global_cpu_usage();
  static GlobalMemData get_global_memory();
  static MemHardwareInfo get_memory_hardware_info();

  long get_system_uptime();
  static int get_cpu_threads_count();

private:
  long hertz;

  struct CpuData
  {
    unsigned long long utime;
    unsigned long long stime;
  };

  std::map<int, CpuData> prev_process_cpu;
  double prev_uptime;

  GlobalCpuData prev_global_cpu;
  bool first_global_cpu_run;
  bool first_process_run;

  static double get_uptime_internal();
  long get_hertz() const;
  static std::string get_user_from_uid(int uid);
  static std::string translate_state(const std::string &state_char);
  static GlobalCpuData read_global_cpu_data();
  static bool check_is_app(int pid);
};
