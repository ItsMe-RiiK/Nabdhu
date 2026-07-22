#include "ui.h"

#include "input.h"
#include "terminal.h"

#include <algorithm>
#include <chrono>

namespace {
  bool compare_icase(const std::string& a, const std::string& b)
  {
    for (size_t i = 0; i < a.length() && i < b.length(); ++i) {
      if (std::tolower(a[i]) != std::tolower(b[i]))
        return std::tolower(a[i]) < std::tolower(b[i]);
    }
    return a.length() < b.length();
  }

  bool contains_icase(const std::string& str, const std::string& query)
  {
    if (query.empty())
      return true;
    auto it =
      std::search(str.begin(), str.end(), query.begin(), query.end(), [](char ch1, char ch2) {
        return std::tolower(ch1) == std::tolower(ch2);
      });
    return it != str.end();
  }

  std::string format_bytes(long long bytes)
  {
    const char* units[] = {"B", "KB", "MB", "GB", "TB"};
    int         i       = 0;
    double      size    = bytes;
    while (size >= 1024 && i < 4) {
      size /= 1024;
      i++;
    }
    char buf[32];
    snprintf(buf, sizeof(buf), "%.1f%s", size, units[i]);
    return std::string(buf);
  }
}  // namespace

UIManager::UIManager()
{
  cpu_history.resize(max_history, 0.0);
  mem_history.resize(max_history, 0.0);
  net_rx_history.resize(max_history, 0.0);
  net_tx_history.resize(max_history, 0.0);
}

UIManager::~UIManager() { }

void UIManager::refresh_data()
{
  double cpu  = process_manager.get_global_cpu_usage();
  core_usages = process_manager.get_core_cpu_usage();
  uptime_s    = process_manager.get_system_uptime();
  cpu_hw      = process_manager.get_cpu_hardware_info();

  static int slow_poll_counter = 0;
  if (slow_poll_counter++ % 10 == 0) {
    battery_info = process_manager.get_battery_info();
    gpu_infos    = process_manager.get_gpu_info();
  }

  global_mem         = process_manager.get_global_memory();
  double mem_percent = 0.0;
  if (global_mem.total_kb > 0) {
    double used = (global_mem.total_kb - global_mem.available_kb);
    mem_percent = (used / global_mem.total_kb) * 100.0;
  }

  cpu_history.push_back(cpu);
  cpu_history.erase(cpu_history.begin());
  mem_history.push_back(mem_percent);
  mem_history.erase(mem_history.begin());

  disks    = disk_manager.get_disk_info();
  networks = network_manager.get_network_info();

  double total_rx = 0.0;
  double total_tx = 0.0;
  for (const auto& net : networks) {
    total_rx += net.rx_speed_kbps;
    total_tx += net.tx_speed_kbps;
  }

  if (total_rx > max_rx_kbps)
    max_rx_kbps = total_rx;
  if (total_tx > max_tx_kbps)
    max_tx_kbps = total_tx;

  net_rx_history.push_back(total_rx);
  net_rx_history.erase(net_rx_history.begin());
  net_tx_history.push_back(total_tx);
  net_tx_history.erase(net_tx_history.begin());

  raw_procs = process_manager.get_processes();
  if (tab_selected == 1) {
    raw_svcs = service_manager.get_services();
  }

  apply_filter();

  // Pre-format hardware strings to avoid heap allocations during draw()
  cpu_usage_str = "CPU Usage: " + std::to_string((int) cpu_history.back()) + "%";
  core_usage_strs.clear();
  core_temp_strs.clear();
  for (size_t i = 0; i < core_usages.size(); i++) {
    core_usage_strs.push_back("C" + std::to_string(i));
    if (i < cpu_hw.core_temps.size()) {
      core_temp_strs.push_back(std::to_string(cpu_hw.core_temps[i]) + "C");
    }
  }

  mem_usage_str = "Memory: " + format_bytes((global_mem.total_kb - global_mem.available_kb) * 1024)
                + " / " + format_bytes(global_mem.total_kb * 1024);

  disk_strs.clear();
  for (const auto& d : disks) {
    disk_strs.push_back(d.mount_point.substr(0, 10));
  }

  net_usage_str = "Network (Rx: " + std::to_string((int) net_rx_history.back())
                + "kbps, Tx: " + std::to_string((int) net_tx_history.back()) + "kbps)";
}

void UIManager::apply_filter()
{
  filtered_procs.clear();
  for (const auto& p : raw_procs) {
    if (
      contains_icase(p.name, search_query) || contains_icase(std::to_string(p.pid), search_query)
      || contains_icase(p.state, search_query)
    ) {
      filtered_procs.push_back(&p);
    }
  }

  if (current_sort == SortBy::CPU) {
    std::stable_sort(
      filtered_procs.begin(), filtered_procs.end(), [](const ProcessInfo* a, const ProcessInfo* b) {
        return a->cumulative_cpu_time > b->cumulative_cpu_time;
      }
    );

    // CPU Lazy: push processes over 5.0% instantaneous CPU to the front
    double target = 5.0;
    for (size_t i = 0, offset = 0; i < filtered_procs.size(); i++) {
      if (filtered_procs[i]->cpu_usage > target) {
        std::rotate(
          filtered_procs.begin() + offset, filtered_procs.begin() + i,
          filtered_procs.begin() + i + 1
        );
        offset++;
      }
    }
  }
  else if (current_sort == SortBy::Name) {
    std::sort(
      filtered_procs.begin(), filtered_procs.end(),
      [](const ProcessInfo* a, const ProcessInfo* b) { return compare_icase(a->name, b->name); }
    );
  }
  else if (current_sort == SortBy::PID) {
    std::sort(
      filtered_procs.begin(), filtered_procs.end(),
      [](const ProcessInfo* a, const ProcessInfo* b) { return a->pid < b->pid; }
    );
  }
  if (proc_selected >= (int) filtered_procs.size())
    proc_selected = std::max(-1, (int) filtered_procs.size() - 1);

  filtered_svcs.clear();
  for (const auto& s : raw_svcs) {
    if (contains_icase(s.name, search_query))
      filtered_svcs.push_back(&s);
  }

  if (svc_selected >= (int) filtered_svcs.size())
    svc_selected = std::max(-1, (int) filtered_svcs.size() - 1);
}

void UIManager::data_collection_loop()
{
  auto last_data_refresh = std::chrono::steady_clock::now();
  auto last_clock_tick   = std::chrono::steady_clock::now();

  std::unique_lock<std::mutex> lock(data_mutex, std::defer_lock);

  while (running) {
    auto now = std::chrono::steady_clock::now();
    auto elapsed_data =
      std::chrono::duration_cast<std::chrono::milliseconds>(now - last_data_refresh).count();
    auto elapsed_clock =
      std::chrono::duration_cast<std::chrono::milliseconds>(now - last_clock_tick).count();

    int timeout_data  = refresh_rate_ms - elapsed_data;
    int timeout_clock = 1000 - elapsed_clock;

    if (timeout_data <= 0) {
      if (!show_main_menu) {
        lock.lock();
        refresh_data();
        lock.unlock();
      }
      last_data_refresh = std::chrono::steady_clock::now();
      timeout_data      = refresh_rate_ms;
      data_dirty        = true;
    }

    if (timeout_clock <= 0) {
      int new_uptime = process_manager.get_system_uptime();
      lock.lock();
      uptime_s = new_uptime;
      lock.unlock();
      last_clock_tick = std::chrono::steady_clock::now();
      timeout_clock   = 1000;
      data_dirty      = true;
    }

    if (data_dirty) {
      input::notify_main_thread();
    }

    int sleep_ms = std::min(timeout_data, timeout_clock);
    if (sleep_ms > 0 && running) {
      lock.lock();
      data_cv.wait_for(lock, std::chrono::milliseconds(sleep_ms), [this] {
        return !running.load();
      });
      lock.unlock();
    }
  }
}

void UIManager::run(int argc, char* argv[])
{
  input::init_event_system();
  terminal::init();

  {
    std::lock_guard<std::mutex> lock(data_mutex);
    refresh_data();
  }

  data_thread = std::thread(&UIManager::data_collection_loop, this);

  auto last_draw = std::chrono::steady_clock::now();
  bool dirty     = true;

  while (running) {
    if (data_dirty) {
      dirty      = true;
      data_dirty = false;
    }

    auto draw_now = std::chrono::steady_clock::now();
    if (
      dirty
      && std::chrono::duration_cast<std::chrono::milliseconds>(draw_now - last_draw).count() >= 33
    ) {
      {
        std::lock_guard<std::mutex> lock(data_mutex);
        draw();
      }
      last_draw = draw_now;
      dirty     = false;
    }

    int event_timeout = -1;  // Wait infinitely by default
    if (dirty) {
      int time_till_draw = 33
                         - std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::steady_clock::now() - last_draw
                         )
                             .count();
      if (time_till_draw < 0)
        time_till_draw = 0;
      event_timeout = time_till_draw;
    }

    if (input::wait_for_event(event_timeout)) {
      while (input::has_event()) {
        std::lock_guard<std::mutex> lock(data_mutex);
        handle_input();
        dirty = true;
      }
    }
  }

  data_cv.notify_all();
  if (data_thread.joinable())
    data_thread.join();

  terminal::restore();
  input::shutdown_event_system();
}
