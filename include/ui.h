#pragma once
#include "disk_manager.h"
#include "network_manager.h"
#include "process_manager.h"
#include "renderer.h"
#include "service_manager.h"

#include <string>
#include <vector>

class UIManager
{
public:
  UIManager();
  ~UIManager();

  void run(int argc, char *argv[]);

private:
  void refresh_data();
  void draw();
  void handle_input();
  void perform_action();

  ProcessManager process_manager;
  ServiceManager service_manager;
  NetworkManager network_manager;
  DiskManager disk_manager;

  std::vector<double> cpu_history;
  std::vector<double> mem_history;
  std::vector<double> net_rx_history;
  std::vector<double> net_tx_history;
  int max_history = 50;

  enum class SortBy
  {
    CPU,
    Name,
    PID
  };
  SortBy current_sort = SortBy::CPU;

  bool running = true;
  renderer::Renderer render;

  int tab_selected = 0; // 0 = Processes, 1 = Services
  int proc_selected = 0;
  int proc_scroll = 0;
  int svc_selected = 0;
  int svc_scroll = 0;

  std::string search_query = "";
  bool in_search_mode = false;
  bool show_context_menu = false;
  int context_menu_selected = 0;

  bool show_main_menu = false;
  int main_menu_selected = 0;

  std::vector<ProcessInfo> raw_procs;
  std::vector<ServiceInfo> raw_svcs;
  std::vector<const ProcessInfo *> filtered_procs;
  std::vector<const ServiceInfo *> filtered_svcs;

  void apply_filter();

  std::vector<double> core_usages;
  int uptime_s = 0;
  int refresh_rate_ms = 2000;
  int refresh_warn_frames = 0;

  BatteryInfo battery_info;
  std::vector<GpuInfo> gpu_infos;

  bool show_cpu = true;
  bool show_mem = true;
  bool show_disk = true;
  bool show_net = true;
  bool show_proc = true;
  CpuHardwareInfo cpu_hw;
  GlobalMemData global_mem;
  std::vector<DiskInfo> disks;
  std::vector<NetworkInterfaceInfo> networks;

  double max_rx_kbps = 100.0;
  double max_tx_kbps = 100.0;

  std::string cpu_usage_str;
  std::vector<std::string> core_usage_strs;
  std::vector<std::string> core_temp_strs;
  std::string mem_usage_str;
  std::vector<std::string> disk_strs;
  std::string net_usage_str;
};