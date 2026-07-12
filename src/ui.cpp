#include "ui.h"

#include <algorithm>
#include <atomic>
#include <cctype>
#include <ftxui/component/component.hpp>
#include <ftxui/component/event.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>
#include <ftxui/screen/color.hpp>
#include <iostream>
#include <thread>
#include <unistd.h>

using namespace ftxui;

UIManager::UIManager()
{
  cpu_history.resize(max_history, 0.0);
  mem_history.resize(max_history, 0.0);
  net_rx_history.resize(max_history, 0.0);
  net_tx_history.resize(max_history, 0.0);
}

UIManager::~UIManager() {}

bool contains_icase(const std::string &str, const std::string &query)
{
  if (query.empty())
    return true;
  auto it = std::search(str.begin(), str.end(), query.begin(), query.end(),
                        [](char ch1, char ch2) { return std::toupper(ch1) == std::toupper(ch2); });
  return (it != str.end());
}

std::string format_bytes(unsigned long long bytes)
{
  const char *suffixes[] = { "B", "KB", "MB", "GB", "TB" };
  int s = 0;
  double count = bytes;
  while (count >= 1024 && s < 4)
  {
    s++;
    count /= 1024;
  }
  char buf[32];
  snprintf(buf, sizeof(buf), "%.2f %s", count, suffixes[s]);
  return std::string(buf);
}

std::string format_kbps(double kbps)
{
  if (kbps > 1024.0)
  {
    char buf[32];
    snprintf(buf, sizeof(buf), "%.2f Mbps", kbps / 1024.0);
    return std::string(buf);
  }
  char buf[32];
  snprintf(buf, sizeof(buf), "%.2f Kbps", kbps);
  return std::string(buf);
}

class InvertedGraph : public Node
{
public:
  InvertedGraph(std::function<std::vector<int>(int, int)> fn) : fn_(std::move(fn)) {}

  void ComputeRequirement() override
  {
    requirement_.min_x = 1;
    requirement_.min_y = 1;
    requirement_.flex_grow_x = 1;
    requirement_.flex_grow_y = 1;
  }

  void Render(Screen &screen) override
  {
    int width = (box_.x_max - box_.x_min + 1) * 2;
    int height = (box_.y_max - box_.y_min + 1) * 4;
    Canvas c(width, height);
    auto data = fn_(width, height);
    for (int x = 0; x < width && x < (int)data.size(); ++x)
    {
      int val = data[x];
      // Draw from top (0) down to val
      for (int y = 0; y < val; ++y)
      {
        c.DrawPointOn(x, y);
      }
    }
    auto el = canvas(std::move(c));
    el->ComputeRequirement();
    el->SetBox(box_);
    el->Render(screen);
  }

private:
  std::function<std::vector<int>(int, int)> fn_;
};

void UIManager::run(int argc, char *argv[])
{
  std::cout << "\x1b]0;Nabdhu\x07";
  auto screen = ScreenInteractive::Fullscreen();

  int tab_selected = 0;
  std::vector<std::string> tab_entries = { "Processes", "Services" };
  auto tab_menu = Menu(&tab_entries, &tab_selected, MenuOption::HorizontalAnimated());

  int proc_selected = 0;
  int svc_selected = 0;
  std::vector<std::string> proc_entries;
  std::vector<std::string> svc_entries;
  std::string proc_header_str = "";

  std::vector<ProcessInfo> raw_procs;
  std::vector<ServiceInfo> raw_svcs;
  std::vector<ProcessInfo> filtered_procs;
  std::vector<ServiceInfo> filtered_svcs;

  std::vector<double> core_usages;
  GlobalMemData global_mem;
  std::vector<DiskInfo> disks;
  std::vector<NetworkInterfaceInfo> networks;
  long uptime_s = 0;
  CpuHardwareInfo cpu_hw;

  std::string search_query = "";
  std::string old_search_query = "";

  bool show_context_menu = false;
  int context_menu_selected = 0;

  double max_rx_kbps = 0.0;
  double max_tx_kbps = 0.0;

  std::string terminal_input_str;
  std::string svc_header_str = "";

  std::vector<std::string> terminal_output;
  std::vector<std::string> terminal_display_output;
  std::mutex terminal_mutex;
  int terminal_selected = 0;

  int disk_scroll_y = 0;
  bool disk_hovered = false;
  bool disks_dirty = true;
  Elements cached_disk_elements;

  bool left_pane_dirty = true;
  Element cached_top_pane;
  Element cached_mem_pane;
  Element cached_net_pane;

  bool terminal_pane_dirty = true;
  Element cached_terminal_pane;
  int last_terminal_selected = -1;
  std::string last_terminal_input = "";

  auto apply_filter = [&]()
  {
    if (tab_selected == 0)
    {
      filtered_procs.clear();
      proc_entries.clear();

      auto dim = Terminal::Size();
      int left_width = std::max(35, dim.dimx > 120 ? dim.dimx / 2 : dim.dimx / 2);
      int right_width = dim.dimx - left_width;
      int avail_w = std::max(10, right_width - 3);

      bool show_status = avail_w >= 85;
      bool show_user = avail_w >= 65;

      char header[512];
      if (show_status && show_user)
      {
        snprintf(header, sizeof(header), "%-6s | %-16s | %-10s | %-6s | %-8s | %-8s | %-6s", "PID", "Name", "Status", "Thread", "User",
                 "Mem", "CPU%");
      }
      else if (show_user)
      {
        snprintf(header, sizeof(header), "%-6s | %-16s | %-6s | %-8s | %-8s | %-6s", "PID", "Name", "Thread", "User", "Mem", "CPU%");
      }
      else
      {
        snprintf(header, sizeof(header), "%-6s | %-16s | %-8s | %-6s", "PID", "Name", "Mem", "CPU%");
      }

      std::string h_str = header;
      if ((int)h_str.length() > avail_w)
        h_str = h_str.substr(0, avail_w);
      proc_header_str = h_str;

      char svc_header[512];
      snprintf(svc_header, sizeof(svc_header), "%-35s | %-15s", "Service Name", "Status");
      std::string s_str = svc_header;
      if ((int)s_str.length() > avail_w)
        s_str = s_str.substr(0, avail_w);
      svc_header_str = s_str;

      // Filter
      for (const auto &p : raw_procs)
      {
        if (contains_icase(p.name, search_query) || contains_icase(std::to_string(p.pid), search_query) ||
            contains_icase(p.state, search_query))
        {
          filtered_procs.push_back(p);
        }
      }

      // Sort
      if (current_sort == SortBy::CPU)
      {
        std::sort(filtered_procs.begin(), filtered_procs.end(),
                  [](const ProcessInfo &a, const ProcessInfo &b) { return a.cpu_usage > b.cpu_usage; });
      }
      else
      {
        std::sort(filtered_procs.begin(), filtered_procs.end(),
                  [](const ProcessInfo &a, const ProcessInfo &b)
                  {
                    std::string a_l = a.name, b_l = b.name;
                    std::transform(a_l.begin(), a_l.end(), a_l.begin(), ::tolower);
                    std::transform(b_l.begin(), b_l.end(), b_l.begin(), ::tolower);
                    return a_l < b_l;
                  });
      }

      int proc_count = 0;
      for (const auto &p : filtered_procs)
      {
        if (proc_count++ >= 150)
          break;

        char buf[512];
        std::string mem_str = format_bytes(p.memory_kb * 1024);

        if (show_status && show_user)
        {
          snprintf(buf, sizeof(buf), "%-6d | %-16.16s | %-10.10s | %-6d | %-8.8s | %-8.8s | %5.1f%%", p.pid, p.name.c_str(),
                   p.state.c_str(), p.threads, p.user.c_str(), mem_str.c_str(), p.cpu_usage);
        }
        else if (show_user)
        {
          snprintf(buf, sizeof(buf), "%-6d | %-16.16s | %-6d | %-8.8s | %-8.8s | %5.1f%%", p.pid, p.name.c_str(), p.threads, p.user.c_str(),
                   mem_str.c_str(), p.cpu_usage);
        }
        else
        {
          snprintf(buf, sizeof(buf), "%-6d | %-16.16s | %-8.8s | %5.1f%%", p.pid, p.name.c_str(), mem_str.c_str(), p.cpu_usage);
        }

        std::string b_str = buf;
        if ((int)b_str.length() > avail_w)
          b_str = b_str.substr(0, avail_w);
        proc_entries.push_back(b_str);
      }
      if (proc_selected >= (int)proc_entries.size())
        proc_selected = std::max(0, (int)proc_entries.size() - 1);
    }
    else
    {
      filtered_svcs.clear();
      svc_entries.clear();

      auto dim = Terminal::Size();
      int avail_w = std::max(10, dim.dimx > 120 ? (dim.dimx / 2) - 6 : dim.dimx - 32);

      int svc_count = 0;
      for (const auto &s : raw_svcs)
      {
        if (contains_icase(s.name, search_query))
        {
          if (svc_count++ >= 150)
            continue;
          filtered_svcs.push_back(s);
          char buf[512];
          snprintf(buf, sizeof(buf), "%-35.35s | %-15.15s", s.name.c_str(), s.active_state.c_str());
          std::string b_str = buf;
          if ((int)b_str.length() > avail_w)
            b_str = b_str.substr(0, avail_w);
          svc_entries.push_back(b_str);
        }
      }
      if (svc_selected >= (int)svc_entries.size())
        svc_selected = std::max(0, (int)svc_entries.size() - 1);
    }
  };

  auto refresh_data = [&]()
  {
    double cpu = process_manager.get_global_cpu_usage();
    core_usages = process_manager.get_core_cpu_usage();
    uptime_s = process_manager.get_system_uptime();
    cpu_hw = process_manager.get_cpu_hardware_info();

    global_mem = process_manager.get_global_memory();
    double mem_percent = 0.0;
    if (global_mem.total_kb > 0)
    {
      double used = (global_mem.total_kb - global_mem.available_kb);
      mem_percent = (used / global_mem.total_kb) * 100.0;
    }

    cpu_history.push_back(cpu);
    cpu_history.erase(cpu_history.begin());
    mem_history.push_back(mem_percent);
    mem_history.erase(mem_history.begin());

    disks = disk_manager.get_disk_info();
    disks_dirty = true;
    networks = network_manager.get_network_info();
    left_pane_dirty = true;

    double total_rx = 0.0;
    double total_tx = 0.0;
    for (const auto &net : networks)
    {
      total_rx += net.rx_speed_kbps;
      total_tx += net.tx_speed_kbps;
    }
    net_rx_history.push_back(total_rx);
    net_rx_history.erase(net_rx_history.begin());
    net_tx_history.push_back(total_tx);
    net_tx_history.erase(net_tx_history.begin());

    if (tab_selected == 0)
    {
      raw_procs = process_manager.get_processes();
    }
    else
    {
      raw_svcs = service_manager.get_services();
    }

    apply_filter();
  };

  refresh_data();

  auto search_input = Input(&search_query, "Search...");

  auto perform_action = [&]()
  {
    show_context_menu = false;
    std::vector<std::string> context_entries_proc = { "End Task", "Open File Location", "Cancel" };
    std::vector<std::string> context_entries_svc = { "End Service", "Disable Service", "Open File Location", "Cancel" };
    const auto &list = (tab_selected == 1) ? context_entries_svc : context_entries_proc;
    if (context_menu_selected < 0 || context_menu_selected >= (int)list.size())
      return;
    std::string action = list[context_menu_selected];
    if (action == "Cancel")
      return;

    if (tab_selected == 0)
    {
      int real_idx = proc_selected;
      if (real_idx >= 0 && real_idx < (int)filtered_procs.size())
      {
        int pid = filtered_procs[real_idx].pid;
        if (action == "End Task")
          process_manager.kill_process(pid);
        else if (action == "Open File Location")
          system(("xdg-open $(dirname $(readlink /proc/" + std::to_string(pid) + "/exe)) >/dev/null 2>&1 &").c_str());
      }
    }
    else
    {
      if (svc_selected >= 0 && svc_selected < (int)filtered_svcs.size())
      {
        std::string sname = filtered_svcs[svc_selected].name;
        if (action == "End Service")
          service_manager.stop_service(sname);
        else if (action == "Disable Service")
          system(("echo 1234 | sudo -S sh -c 'systemctl stop " + sname + " && systemctl disable " + sname +
                  " && rm -f /etc/systemd/system/" + sname + " && systemctl daemon-reload'")
                     .c_str());
        else if (action == "Open File Location")
        {
          FILE *pipe = popen(("systemctl show -p FragmentPath " + sname).c_str(), "r");
          if (pipe)
          {
            char buffer[256];
            if (fgets(buffer, sizeof(buffer), pipe) != NULL)
            {
              std::string output = buffer;
              if (output.find("FragmentPath=") == 0)
              {
                std::string path = output.substr(13);
                path.erase(path.find_last_not_of(" \n\r\t") + 1);
                if (!path.empty())
                {
                  size_t pos = path.find_last_of('/');
                  if (pos != std::string::npos)
                    system(("xdg-open '" + path.substr(0, pos) + "' >/dev/null 2>&1 &").c_str());
                  else
                    system(("xdg-open '" + path + "' >/dev/null 2>&1 &").c_str());
                }
              }
            }
            pclose(pipe);
          }
        }
      }
    }
    refresh_data();
  };

  MenuOption proc_menu_option = MenuOption::Vertical();
  proc_menu_option.entries_option.transform = [](EntryState state)
  {
    Element e = text(state.label);
    if (state.focused)
      e = e | bgcolor(Color::Blue) | color(Color::White);
    else if (state.active)
      e = e | bgcolor(Color::Grey39) | color(Color::White);
    return e;
  };
  proc_menu_option.on_enter = [&]()
  {
    show_context_menu = true;
    context_menu_selected = 0;
  };
  auto proc_menu = Menu(&proc_entries, &proc_selected, proc_menu_option);

  MenuOption svc_menu_option = MenuOption::Vertical();
  svc_menu_option.entries_option.transform = [](EntryState state)
  {
    Element e = text(state.label);
    if (state.focused)
      e = e | bgcolor(Color::Blue) | color(Color::White);
    else if (state.active)
      e = e | bgcolor(Color::Grey39) | color(Color::White);
    return e;
  };
  svc_menu_option.on_enter = [&]()
  {
    show_context_menu = true;
    context_menu_selected = 0;
  };
  auto svc_menu = Menu(&svc_entries, &svc_selected, svc_menu_option);

  auto proc_menu_catch =
      CatchEvent(proc_menu,
                 [&](Event event)
                 {
                   if (tab_selected != 0 && event.is_mouse())
                     return false;

                   if (event.is_mouse() && event.mouse().button == Mouse::Right && event.mouse().motion == Mouse::Pressed)
                   {
                     if (proc_selected >= 0 && !proc_entries.empty())
                     {
                       show_context_menu = true;
                       context_menu_selected = 0;
                     }
                     return true;
                   }
                   return false;
                 });

  auto svc_menu_catch = CatchEvent(svc_menu,
                                   [&](Event event)
                                   {
                                     if (tab_selected != 1 && event.is_mouse())
                                       return false;
                                     if (event.is_mouse() && event.mouse().button == Mouse::Right && event.mouse().motion == Mouse::Pressed)
                                     {
                                       show_context_menu = true;
                                       context_menu_selected = 0;
                                       return true;
                                     }
                                     return false;
                                   });

  auto tab_container = Container::Tab({ proc_menu_catch, svc_menu_catch }, &tab_selected);

  int active_layout_child = 1;
  auto right_pane_container = Container::Vertical({ tab_menu, search_input, tab_container }, &active_layout_child);

  MenuOption term_menu_opt = MenuOption::Vertical();
  term_menu_opt.entries_option.transform = [](EntryState state) { return text(state.label); };
  auto terminal_output_comp = Menu(&terminal_display_output, &terminal_selected, term_menu_opt);

  InputOption terminal_opt;
  terminal_opt.on_enter = [&]
  {
    if (terminal_input_str.empty())
      return;
    std::string cmd = terminal_input_str;
    terminal_input_str = "";

    {
      std::lock_guard<std::mutex> lock(terminal_mutex);
      terminal_output.push_back("$ " + cmd);
    }

    std::thread(
        [cmd, &terminal_output, &terminal_mutex, &screen]()
        {
          FILE *pipe = popen((cmd + " 2>&1").c_str(), "r");
          if (!pipe)
          {
            std::lock_guard<std::mutex> lock(terminal_mutex);
            terminal_output.push_back("Error running command.");
            return;
          }
          char buffer[256];
          while (fgets(buffer, sizeof(buffer), pipe) != nullptr)
          {
            std::string line(buffer);
            if (!line.empty() && line.back() == '\n')
              line.pop_back();
            {
              std::lock_guard<std::mutex> lock(terminal_mutex);
              terminal_output.push_back(line);
              if (terminal_output.size() > 500)
                terminal_output.erase(terminal_output.begin());
            }
            screen.PostEvent(Event::Custom);
          }
          pclose(pipe);
        })
        .detach();
  };
  auto terminal_input_comp = Input(&terminal_input_str, "bash> ", terminal_opt);

  auto terminal_pane_container = Container::Vertical({ terminal_output_comp, terminal_input_comp });

  auto disks_pane_renderer = Renderer(
      [&]
      {
        if (disks_dirty)
        {
          cached_disk_elements.clear();
          for (const auto &d : disks)
          {
            char buf[128];
            snprintf(buf, sizeof(buf), "%-8s %s", d.mount_point.substr(0, 8).c_str(), format_bytes(d.total_bytes).c_str());
            cached_disk_elements.push_back(hbox({ text(buf) | flex }));

            double f_pct = 100.0 - d.used_percent;

            Elements used_blocks;
            Elements free_blocks;
            int total_blocks = 10;
            int used_count = (int)((d.used_percent / 100.0) * total_blocks);
            int free_count = total_blocks - used_count;

            for (int i = 0; i < total_blocks; ++i)
            {
              if (i < used_count)
                used_blocks.push_back(text("■") | color(Color::Red));
              else
                used_blocks.push_back(text("■") | color(Color::GrayDark));
            }

            for (int i = 0; i < total_blocks; ++i)
            {
              if (i < free_count)
                free_blocks.push_back(text("■") | color(Color::Green));
              else
                free_blocks.push_back(text("■") | color(Color::GrayDark));
            }

            char ubuf[64], fbuf[64];
            snprintf(ubuf, sizeof(ubuf), "Used: %3.0f%% ", d.used_percent);
            snprintf(fbuf, sizeof(fbuf), "Free: %3.0f%% ", f_pct);

            cached_disk_elements.push_back(hbox({ text(ubuf), hbox(std::move(used_blocks)) }));
            cached_disk_elements.push_back(hbox({ text(fbuf), hbox(std::move(free_blocks)) }));
            cached_disk_elements.push_back(text(" "));
          }
          disks_dirty = false;
        }

        if (disk_scroll_y > (int)cached_disk_elements.size() - 1)
          disk_scroll_y = std::max(0, (int)cached_disk_elements.size() - 1);
        if (disk_scroll_y < 0)
          disk_scroll_y = 0;

        Elements display_elements = cached_disk_elements;
        return window(text("disks ") | color(Color::RedLight),
                      vbox(std::move(display_elements)) | focusPosition(0, disk_scroll_y) | yframe | vscroll_indicator);
      });

  auto disks_pane_comp = CatchEvent(Hoverable(disks_pane_renderer, &disk_hovered),
                                    [&](Event e)
                                    {
                                      if (disk_hovered && e.is_mouse())
                                      {
                                        if (e.mouse().button == Mouse::WheelDown)
                                        {
                                          disk_scroll_y++;
                                          return true;
                                        }
                                        if (e.mouse().button == Mouse::WheelUp)
                                        {
                                          disk_scroll_y--;
                                          if (disk_scroll_y < 0)
                                            disk_scroll_y = 0;
                                          return true;
                                        }
                                      }
                                      return false;
                                    });

  auto left_interactive = Container::Vertical({ disks_pane_comp, terminal_pane_container });
  int main_active_child = 1; // Start with right pane selected
  auto top_level_container = Container::Horizontal({ left_interactive, right_pane_container }, &main_active_child);

  auto main_component = Renderer(
      top_level_container,
      [&]
      {
        if (left_pane_dirty)
        {
          // CPU Top Panel
          auto cpu_graph = graph(
                               [&](int width, int height)
                               {
                                 std::vector<int> out(width, 0);
                                 for (int i = 0; i < width; ++i)
                                 {
                                   int idx = (i * max_history) / width;
                                   if (idx < max_history)
                                     out[i] = (cpu_history[idx] * height) / 100.0;
                                 }
                                 return out;
                               }) |
                           color(Color::GreenLight);

          Elements core_gauges;
          for (size_t i = 0; i < core_usages.size(); ++i)
          {
            char buf[64];
            snprintf(buf, sizeof(buf), "C%zu %3.0f%%", i, core_usages[i]);

            std::string t_str = "";
            if (i < cpu_hw.core_temps.size())
            {
              t_str = std::to_string(cpu_hw.core_temps[i]) + "°C";
            }

            core_gauges.push_back(hbox({ text(buf) | size(WIDTH, EQUAL, 8),
                                         gauge(core_usages[i] / 100.0) | color(Color::GreenLight) | bgcolor(Color::Green) | flex,
                                         text(" " + t_str) | color(Color::Cyan) | size(WIDTH, EQUAL, 5) }));
          }

          int days = uptime_s / (24 * 3600);
          int uptime_rem = uptime_s % (24 * 3600);
          int hours = uptime_rem / 3600;
          int mins = (uptime_rem % 3600) / 60;
          char uptime_str[64];
          if (days > 0)
            snprintf(uptime_str, sizeof(uptime_str), " up %dd %02d:%02d ", days, hours, mins);
          else
            snprintf(uptime_str, sizeof(uptime_str), " up %02d:%02d ", hours, mins);

          char cpu_title[128];
          snprintf(cpu_title, sizeof(cpu_title), "cpu %3.0f%% |%s", cpu_history.back(), uptime_str);

          auto term_size = Terminal::Size();
          char load_str[128];
          if (term_size.dimx < 100)
          {
            snprintf(load_str, sizeof(load_str), "%.2f, %.2f, %.2f", cpu_hw.load_avg[0], cpu_hw.load_avg[1], cpu_hw.load_avg[2]);
          }
          else
          {
            snprintf(load_str, sizeof(load_str), "SysLoad (1m, 5m, 15m): %.2f %.2f %.2f", cpu_hw.load_avg[0], cpu_hw.load_avg[1],
                     cpu_hw.load_avg[2]);
          }

          auto cpu_box = vbox({ hbox({ text(cpu_hw.model_name) | bold, text(" @ "), text(cpu_hw.speed) | color(Color::RedLight) }),
                                separator(), vbox(std::move(core_gauges)) | yflex | vscroll_indicator, separator(), text(load_str) });

          // Match left_width calculation from main_component exactly to split width 50/50
          int left_width = std::max(35, term_size.dimx / 2);
          int half_usable = std::max(10, (left_width - 3) / 2);

          cached_top_pane = window(text(cpu_title) | color(Color::RedLight),
                                   hbox({ cpu_graph | flex, separator(), cpu_box | size(WIDTH, EQUAL, half_usable) })) |
                            size(HEIGHT, LESS_THAN, core_usages.size() + 6);

          // Mem Panel
          long long total_mem = global_mem.total_kb;
          long long used_mem = total_mem - global_mem.available_kb;
          long long cached_mem = global_mem.cached_kb;
          long long free_mem = global_mem.free_kb;

          cached_mem_pane =
              window(text("mem ") | color(Color::RedLight),
                     vbox({ hbox({ text("Total:"), text(" ") | flex, text(format_bytes(total_mem * 1024)) | bold }), text(" "),
                            hbox({ text("Used:"), text(" ") | flex, text(format_bytes(used_mem * 1024)) }),
                            gauge(total_mem > 0 ? (double)used_mem / total_mem : 0) | color(Color::RedLight) | bgcolor(Color::Red),
                            text(" "), hbox({ text("Cached:"), text(" ") | flex, text(format_bytes(cached_mem * 1024)) }),
                            gauge(total_mem > 0 ? (double)cached_mem / total_mem : 0) | color(Color::CyanLight) | bgcolor(Color::Cyan),
                            text(" "), hbox({ text("Free:"), text(" ") | flex, text(format_bytes(global_mem.available_kb * 1024)) }),
                            gauge(total_mem > 0 ? (double)global_mem.available_kb / total_mem : 0) | color(Color::GreenLight) |
                                bgcolor(Color::Green) }));
        }

        // Disks Panel
        auto disks_pane = disks_pane_comp->Render();

        if (left_pane_dirty)
        {
          std::string ip_title = "net";
          Elements net_elements;
          double total_rx = 0.0;
          double total_tx = 0.0;
          unsigned long long total_rx_bytes = 0;
          unsigned long long total_tx_bytes = 0;
          for (const auto &net : networks)
          {
            total_rx += net.rx_speed_kbps;
            total_tx += net.tx_speed_kbps;
            total_rx_bytes += net.rx_bytes;
            total_tx_bytes += net.tx_bytes;
            if (!net.ip_address.empty() && ip_title == "net")
              ip_title = "net [" + net.ip_address + "]";
          }

          if (total_rx > max_rx_kbps)
            max_rx_kbps = total_rx;
          if (total_tx > max_tx_kbps)
            max_tx_kbps = total_tx;

          char line_buf[256];
          snprintf(line_buf, sizeof(line_buf), "         %-10s | %-10s", "Download", "Upload");
          net_elements.push_back(text(line_buf) | color(Color::Magenta));

          snprintf(line_buf, sizeof(line_buf), "Current: %-10s | %-10s", format_kbps(total_rx).c_str(), format_kbps(total_tx).c_str());
          net_elements.push_back(text(line_buf));

          snprintf(line_buf, sizeof(line_buf), "Peak:    %-10s | %-10s", format_kbps(max_rx_kbps).c_str(),
                   format_kbps(max_tx_kbps).c_str());
          net_elements.push_back(text(line_buf));

          snprintf(line_buf, sizeof(line_buf), "Total:   %-10s | %-10s", format_bytes(total_rx_bytes).c_str(),
                   format_bytes(total_tx_bytes).c_str());
          net_elements.push_back(text(line_buf));

          cached_net_pane = window(text(ip_title) | color(Color::RedLight), vbox(std::move(net_elements)));
          left_pane_dirty = false;
        }

        {
          std::lock_guard<std::mutex> lock(terminal_mutex);
          bool was_at_bottom = terminal_selected >= (int)terminal_display_output.size() - 1;
          if (terminal_display_output.size() != terminal_output.size() ||
              (!terminal_output.empty() && terminal_display_output.back() != terminal_output.back()))
          {
            terminal_display_output = terminal_output;
            if ((was_at_bottom) && !terminal_display_output.empty())
            {
              terminal_selected = terminal_display_output.size() - 1;
            }
            terminal_pane_dirty = true;
          }
        }

        if (terminal_selected != last_terminal_selected || terminal_input_str != last_terminal_input || terminal_pane_dirty)
        {
          cached_terminal_pane = window(
              text("terminal") | color(Color::Green),
              vbox({ terminal_output_comp->Render() | yflex | yframe | vscroll_indicator, separator(), terminal_input_comp->Render() }));
          last_terminal_selected = terminal_selected;
          last_terminal_input = terminal_input_str;
          terminal_pane_dirty = false;
        }

        auto dim = Terminal::Size();
        int max_term_height = std::max(5, dim.dimy - (int)core_usages.size() - 27);

        auto left_pane = vbox(
            { cached_top_pane | size(HEIGHT, GREATER_THAN, 7) | yflex,
              hbox({ cached_mem_pane | flex, disks_pane | flex }) | size(HEIGHT, LESS_THAN, 15) | size(HEIGHT, GREATER_THAN, 6) | yflex,
              cached_terminal_pane | size(HEIGHT, LESS_THAN, max_term_height) | size(HEIGHT, GREATER_THAN, 5) | yflex,
              cached_net_pane | size(HEIGHT, EQUAL, 6) });

        int left_width = std::max(35, dim.dimx > 120 ? dim.dimx / 2 : dim.dimx / 2);

        std::string proc_title = tab_selected == 0 ? "proc" : "svc";

        auto filter_indicator = hbox({ text(" f") | color(Color::Red) | bold, text("ilter by: ") | color(Color::White),
                                       text(current_sort == SortBy::CPU ? "CPU" : "Name") | color(Color::White) | bold });

        Elements right_pane_elements = { hbox({ tab_menu->Render() | flex, search_input->Render() | size(WIDTH, LESS_THAN, 20) | border }) |
                                         size(HEIGHT, EQUAL, 3) };

        if (tab_selected == 0)
        {
          right_pane_elements.push_back(filter_indicator);
          right_pane_elements.push_back(text(proc_header_str) | bold | inverted);
        }
        else
        {
          right_pane_elements.push_back(text(svc_header_str) | bold | inverted);
        }

        right_pane_elements.push_back(tab_container->Render() | vscroll_indicator | yframe | flex);

        auto right_pane = window(text(proc_title) | color(Color::RedLight), vbox(std::move(right_pane_elements)));

        return hbox({ left_pane | size(WIDTH, EQUAL, left_width), right_pane | flex }) | bgcolor(Color::Black) | color(Color::White);
      });

  std::vector<std::string> context_entries_proc = { "End Task", "Open File Location", "Cancel" };
  std::vector<std::string> context_entries_svc = { "End Service", "Disable Service", "Open File Location", "Cancel" };

  MenuOption ctx_option;
  ctx_option.on_enter = perform_action;

  auto context_menu_proc_comp = Menu(&context_entries_proc, &context_menu_selected, ctx_option);
  auto context_menu_svc_comp = Menu(&context_entries_svc, &context_menu_selected, ctx_option);
  auto context_menu_comp = Container::Tab({ context_menu_proc_comp, context_menu_svc_comp }, &tab_selected);

  auto context_menu_renderer =
      Renderer(context_menu_comp,
               [&] { return window(text(" Action "), context_menu_comp->Render()) | bgcolor(Color::Black) | clear_under | center; });

  auto context_menu_modal = Modal(main_component, context_menu_renderer, &show_context_menu);

  std::atomic<bool> refresh_ui_continue = true;
  std::thread refresh_ui(
      [&]
      {
        while (refresh_ui_continue)
        {
          using namespace std::chrono_literals;
          std::this_thread::sleep_for(2.0s);
          screen.Post([&] { refresh_data(); });
          screen.PostEvent(Event::Custom);
        }
      });

  auto event_catch = CatchEvent(context_menu_modal,
                                [&](Event event)
                                {
                                  if (search_query != old_search_query)
                                  {
                                    old_search_query = search_query;
                                    apply_filter();
                                  }

                                  if (event == Event::Escape)
                                  {
                                    if (show_context_menu)
                                    {
                                      show_context_menu = false;
                                      return true;
                                    }
                                  }

                                  bool is_typing = (main_active_child == 0) || (main_active_child == 1 && active_layout_child == 1);

                                  if (event == Event::Character('q') && !show_context_menu && !is_typing)
                                  {
                                    screen.ExitLoopClosure()();
                                    return true;
                                  }

                                  if (event == Event::Character('f') && !show_context_menu && !is_typing)
                                  {
                                    current_sort = (current_sort == SortBy::CPU) ? SortBy::Name : SortBy::CPU;
                                    apply_filter();
                                    return true;
                                  }

                                  return false;
                                });

  screen.Loop(event_catch);
  refresh_ui_continue = false;
  refresh_ui.join();
}
