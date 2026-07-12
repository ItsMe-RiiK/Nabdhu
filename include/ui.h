#pragma once
#include "disk_manager.h"
#include "network_manager.h"
#include "process_manager.h"
#include "service_manager.h"

#include <vector>

class UIManager
{
public:
  UIManager();
  ~UIManager();

  void run(int argc, char *argv[]);

private:
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
    Name
  };
  SortBy current_sort = SortBy::CPU;
};
