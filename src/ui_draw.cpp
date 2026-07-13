#include "terminal.h"
#include "ui.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <fmt/core.h>
#include <fmt/format.h>
#include <string>

namespace
{
  std::string format_bytes(long long bytes)
  {
    const char *units[] = { "B", "KB", "MB", "GB", "TB" };
    int i = 0;
    double size = bytes;
    while (size >= 1024 && i < 4)
    {
      size /= 1024;
      i++;
    }
    return fmt::format("{:.1f}{}", size, units[i]);
  }

  int get_temp_color(int temp)
  {
    if (temp <= 40)
      return renderer::rgb(0, 255, 0);
    if (temp >= 85)
      return renderer::rgb(255, 0, 0);

    if (temp <= 65)
    {
      float t = (temp - 40) / 25.0f;
      int r = (int)(t * 255);
      return renderer::rgb(r, 255, 0);
    }

    float t = (temp - 65) / 20.0f;
    int g = (int)((1.0f - t) * 255);
    return renderer::rgb(255, g, 0);
  }

  int get_bat_color(int capacity)
  {
    if (capacity >= 50)
      return renderer::rgb(0, 255, 0);
    if (capacity <= 20)
      return renderer::rgb(255, 0, 0);

    float t = (capacity - 20) / 30.0f;
    int r = (int)((1.0f - t) * 255);
    return renderer::rgb(r, 255, 0);
  }
} // namespace

void UIManager::draw()
{
  int w = terminal::get_width();
  int h = terminal::get_height();
  render.resize(w, h);
  render.clear();

  if (w < 80 || h < 25)
  {
    if (w >= 33 && h >= 4)
    {
      int box_x = w / 2 - 16;
      int box_y = h / 2 - 2;
      render.draw_window(box_x, box_y, 33, 4, "Warning", 31, 31);
      render.draw_text(w / 2 - 12, box_y + 1, "Window to small", 31, 49, true);
      render.draw_text(w / 2 - 11, box_y + 2, fmt::format("Size: {}x{} (Min 80x25)", w, h), 33, 49, true);
    }
    else
    {
      render.draw_text(std::max(0, w / 2 - 4), h / 2, "Too small", 31, 49, true);
    }
    render.render();
    return;
  }

  bool left_visible = show_cpu || show_mem || show_disk || show_net;
  int left_w = 0, right_w = 0;
  if (!left_visible)
  {
    left_w = 0;
    right_w = w;
  }
  else if (!show_proc)
  {
    left_w = w;
    right_w = 0;
  }
  else
  {
    int right_min = 45;
    left_w = w / 2;
    if (w - left_w < right_min)
      left_w = w - right_min;
    if (left_w < 35)
      left_w = 35;
    right_w = w - left_w;
  }

  int avail_h = h - 1; // Top bar takes 1 line
  bool show_mid = show_mem || show_disk;
  int num_left_rows = (show_cpu ? 1 : 0) + (show_mid ? 1 : 0) + (show_net ? 1 : 0);
  int cpu_h = 0, mid_h = 0, net_h = 0;

  if (num_left_rows == 3)
  {
    net_h = std::max(6, avail_h / 4);
    mid_h = std::max((int)(2 + disks.size() * 3), (avail_h - net_h) / 2);
    // don't let mid_h starve cpu_h completely
    if (avail_h - net_h - mid_h < 6)
      mid_h = avail_h - net_h - 6;
    if (mid_h < 4)
      mid_h = 4;
    cpu_h = avail_h - net_h - mid_h;
  }
  else if (num_left_rows == 2)
  {
    if (!show_cpu)
    {
      net_h = std::max(6, avail_h / 3);
      mid_h = avail_h - net_h;
    }
    else if (!show_mid)
    {
      net_h = std::max(6, avail_h / 3);
      cpu_h = avail_h - net_h;
    }
    else if (!show_net)
    {
      mid_h = avail_h / 2;
      cpu_h = avail_h - mid_h;
    }
  }
  else if (num_left_rows == 1)
  {
    if (show_cpu)
      cpu_h = avail_h;
    if (show_mid)
      mid_h = avail_h;
    if (show_net)
      net_h = avail_h;
  }

  int y_cursor = 1;

  // --- 0. Top Bar ---
  // Background
  render.draw_hline(0, 0, w, 37, 49);
  // Draw string formatting logic
  auto utf8_len = [](const std::string &s)
  {
    int count = 0;
    for (char c : s)
      if ((c & 0xC0) != 0x80)
        count++;
    return count;
  };

  auto draw_top = [&](int x, const std::string &pre, char key, const std::string &post)
  {
    render.draw_text(x, 0, pre, 37);
    x += utf8_len(pre);
    if (key != ' ')
    {
      render.draw_text(x, 0, std::string(1, key), 91); // Red key
      x += 1;
    }
    render.draw_text(x, 0, post, 37);
    return x + utf8_len(post);
  };

  // Header dynamic spacing
  int num_items = battery_info.present ? 4 : 3;

  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);
  struct tm *tm_info = localtime(&time_t);
  char time_buf[64];
  strftime(time_buf, sizeof(time_buf), "Time %H:%M:%S", tm_info);
  std::string time_txt = time_buf;

  std::string bat_txt = "";
  if (battery_info.present)
  {
    bat_txt = "BAT:           " + std::to_string(battery_info.capacity) + "%";
  }

  std::string ms_txt = "- " + std::to_string(refresh_rate_ms) + " MS +";

  int pos[4];
  pos[0] = 3; // Menu
  if (num_items == 4)
  {
    pos[1] = (w / 2) - (time_txt.length() / 2); // Perfectly center time
    pos[2] = (3 * w / 4) - (bat_txt.length() / 2); // Center BAT in right half
    pos[3] = w - ms_txt.length() - 3;
  }
  else
  {
    pos[1] = (w / 2) - (time_txt.length() / 2); // Perfectly center time
    pos[2] = w - ms_txt.length() - 3;
    pos[3] = 0;
  }

  // 1. Menu
  render.draw_text(pos[0] - 1, 0, " ", 37);
  render.draw_text(pos[0], 0, "M", 91);
  render.draw_text(pos[0] + 1, 0, "enu ", 37);

  // 2. Time
  render.draw_text(pos[1] - 1, 0, " " + time_txt + " ", 37);

  // 3. Bat (if present)
  int ms_idx = 2;
  if (battery_info.present)
  {
    int bat_col = get_bat_color(battery_info.capacity);
    render.draw_text(pos[2] - 1, 0, " BAT: ", 37);
    render.draw_block_gauge(pos[2] + 5, 0, 10, battery_info.capacity / 100.0, renderer::Gradient::RedToGreen, bat_col, 90);
    render.draw_text(pos[2] + 16, 0, std::to_string(battery_info.capacity) + "% ", bat_col);
    ms_idx = 3;
  }

  // 4. MS
  int tx = pos[ms_idx];
  render.draw_text(tx - 1, 0, " ", 37);
  render.draw_text(tx, 0, "-", 91);
  std::string just_ms = " " + std::to_string(refresh_rate_ms) + " MS ";
  if (refresh_warn_frames > 0)
  {
    render.draw_text(tx + 1, 0, just_ms, 91, 49, true);
    refresh_warn_frames--;
  }
  else
  {
    render.draw_text(tx + 1, 0, just_ms, 37);
  }
  render.draw_text(tx + 1 + just_ms.length(), 0, "+", 91);
  render.draw_text(tx + 1 + just_ms.length() + 1, 0, " ", 37);

  // --- 1. CPU Box ---
  if (show_cpu)
  {
    render.draw_window(0, y_cursor, left_w, cpu_h, "", 37);
    render.draw_text(2, y_cursor, "\xC2\xB9", 91);
    render.draw_text(
        3, y_cursor,
        "CPU \xE2\x94\x80\xE2\x94\x80 Uptime " +
            [this]()
            {
              int h_up = uptime_s / 3600;
              int m_up = (uptime_s % 3600) / 60;
              int s_up = uptime_s % 60;
              return fmt::format("{:02d}h:{:02d}m:{:02d}s", h_up, m_up, s_up);
            }() +
            " ",
        37);
    if (cpu_h > 2)
    {
      std::string clean_name = cpu_hw.model_name;
      std::string fluff[] = { "Intel(R)",     "Core(TM)",    "CPU",     "Processor", "14th Gen", "13th Gen", "12th Gen",
                              "11th Gen",     "10th Gen",    "9th Gen", "8th Gen",   "7th Gen",  "AMD ",     "with Radeon Graphics",
                              "AuthenticAMD", "GenuineIntel" };
      for (const auto &f : fluff)
      {
        size_t pos;
        while ((pos = clean_name.find(f)) != std::string::npos)
        {
          clean_name.erase(pos, f.length());
        }
      }
      size_t at_pos = clean_name.find('@');
      if (at_pos != std::string::npos)
      {
        clean_name = clean_name.substr(0, at_pos);
      }
      std::string final_name;
      bool in_space = true;
      for (char c : clean_name)
      {
        if (c == ' ' || c == '\t')
        {
          if (!in_space)
          {
            final_name += ' ';
            in_space = true;
          }
        }
        else
        {
          final_name += c;
          in_space = false;
        }
      }
      if (!final_name.empty() && final_name.back() == ' ')
        final_name.pop_back();
      if (!final_name.empty() && final_name.front() == ' ')
        final_name.erase(0, 1);

      render.draw_text(2, y_cursor + 1, final_name + " @ " + cpu_hw.speed, 37, 49, false, false, left_w - 3);
      double total_pct = cpu_history.back();

      int pkg_temp = cpu_hw.core_temps.empty() ? 0 : cpu_hw.core_temps[0];
      int temp_col = get_temp_color(pkg_temp);

      std::string temp_str = "";
      if (pkg_temp > 0)
      {
        temp_str = std::to_string(pkg_temp) + "\xC2\xB0"
                                              "C";
      }

      std::string speed_str = fmt::format("{:3d}% @ {} ", (int)total_pct, cpu_hw.real_speed);

      render.draw_text(2, y_cursor + 2, "CPU", 37);
      int graph_x = 6;
      int total_len = speed_str.length() + temp_str.length();
      int graph_len = left_w - graph_x - total_len - 2;
      render.draw_block_gauge(graph_x, y_cursor + 2, graph_len, total_pct / 100.0, renderer::Gradient::GreenToRed);

      int text_start = graph_x + graph_len + 1;
      render.draw_text(text_start, y_cursor + 2, speed_str, 37);
      if (!temp_str.empty())
      {
        render.draw_text(text_start + speed_str.length(), y_cursor + 2, temp_str, temp_col);
      }
    }
    if (cpu_h > 4)
    {
      int cores_y = y_cursor + 3;
      int cores_h = cpu_h - 4;

      if (!gpu_infos.empty() && cores_h > 2)
        cores_h -= 2; // save space for GPU

      int cols = (left_w > 50) ? 3 : 2;
      int col_w = (left_w - 4) / cols;

      for (size_t i = 0; i < core_usages.size(); i++)
      {
        int r = i / cols;
        int c = i % cols;
        if (r >= cores_h)
          break;

        int cx = 2 + c * col_w;
        int cy = cores_y + r;

        int temp_val = (i < cpu_hw.core_temps.size()) ? cpu_hw.core_temps[i] : 0;
        std::string temp_str = (temp_val > 0) ? (std::to_string(temp_val) + "\xC2\xB0"
                                                                            "C") :
                                                "";
        int temp_col = get_temp_color(temp_val);

        std::string core_name = "C" + std::to_string(i);
        render.draw_text(cx, cy, core_name, 36);
        render.draw_block_gauge(cx + 3, cy, col_w - 8, core_usages[i] / 100.0, renderer::Gradient::GreenToRed);
        if (!temp_str.empty())
          render.draw_text(cx + col_w - 5, cy, temp_str, temp_col);
      }

      // GPU
      if (!gpu_infos.empty() && cpu_h > cores_h + 4)
      {
        int gpu_y = y_cursor + cpu_h - 2;
        const auto &gpu = gpu_infos[0];

        render.draw_text(2, gpu_y - 1, gpu.name, 37, 49, false, false, left_w - 3);
        render.draw_text(2, gpu_y, "GPU", 37);

        std::string ginfo;
        if (gpu.power_watts > 0)
        {
          ginfo = fmt::format("{:3.0f}% {:3.0f}w", gpu.utilization, gpu.power_watts);
        }
        else
        {
          ginfo = fmt::format("{:3.0f}%", gpu.utilization);
        }

        int graph_x = 6;
        int graph_len = left_w - graph_x - ginfo.length() - 2;
        render.draw_block_gauge(graph_x, gpu_y, graph_len, gpu.utilization / 100.0, renderer::Gradient::GreenToRed);
        render.draw_text(graph_x + graph_len + 1, gpu_y, ginfo, 37);
      }
    }
    y_cursor += cpu_h;
  }

  // --- 2. Memory / Disk Box ---
  if (show_mid)
  {
    int m_w = show_mem ? (show_disk ? left_w / 2 : left_w) : 0;
    int d_w = show_disk ? (show_mem ? left_w - m_w : left_w) : 0;

    if (show_mem)
    {
      render.draw_window(0, y_cursor, m_w, mid_h, "", 37);
      render.draw_text(2, y_cursor, "\xC2\xB2", 91);
      render.draw_text(3, y_cursor, "Memory ", 37);
      if (mid_h > 4)
      {
        double total = global_mem.total_kb;
        double used = total - global_mem.available_kb;
        double cached = global_mem.cached_kb;
        double free = global_mem.free_kb;

        render.draw_text(2, y_cursor + 1, "Total:", 37);
        render.draw_text(m_w - 10, y_cursor + 1, format_bytes(total * 1024), 37);

        render.draw_text(2, y_cursor + 2, "Used:", 37);
        render.draw_text(m_w - 10, y_cursor + 2, format_bytes(used * 1024), 37);
        if (m_w > 4)
          render.draw_block_gauge(2, y_cursor + 3, m_w - 4, used / total, renderer::Gradient::DarkRedToRed, 31);

        if (mid_h > 6)
        {
          render.draw_text(2, y_cursor + 4, "Cached:", 37);
          render.draw_text(m_w - 10, y_cursor + 4, format_bytes(cached * 1024), 37);
          if (m_w > 4)
            render.draw_block_gauge(2, y_cursor + 5, m_w - 4, cached / total, renderer::Gradient::DarkCyanToCyan, 36);
        }
        if (mid_h > 8)
        {
          render.draw_text(2, y_cursor + 6, "Free:", 37);
          render.draw_text(m_w - 10, y_cursor + 6, format_bytes(free * 1024), 37);
          if (m_w > 4)
            render.draw_block_gauge(2, y_cursor + 7, m_w - 4, free / total, renderer::Gradient::DarkGreenToGreen, 32);
        }
      }
    }

    if (show_disk)
    {
      int d_x = show_mem ? m_w : 0;
      render.draw_window(d_x, y_cursor, d_w, mid_h, "", 37);
      render.draw_text(d_x + 2, y_cursor, "D", 91);
      render.draw_text(d_x + 3, y_cursor, "isk ", 37);
      if (mid_h > 2)
      {
        int d_list = std::min((int)disks.size(), (mid_h - 1) / 3);
        for (int i = 0; i < d_list; i++)
        {
          int dy = y_cursor + 1 + (i * 3);
          render.draw_text(d_x + 2, dy, disks[i].mount_point, 37);
          render.draw_text(d_x + d_w - 10, dy, format_bytes(disks[i].total_bytes), 37);

          double used_pct = (double)disks[i].used_bytes / disks[i].total_bytes;
          render.draw_text(d_x + 2, dy + 1, fmt::format("U: {:.1f}%", used_pct * 100.0), 37);
          if (d_w > 12)
            render.draw_block_gauge(d_x + 9, dy + 1, d_w - 11, used_pct, renderer::Gradient::DarkRedToRed, 31);

          double free_pct = (double)(disks[i].total_bytes - disks[i].used_bytes) / disks[i].total_bytes;
          render.draw_text(d_x + 2, dy + 2, fmt::format("F: {:.1f}%", free_pct * 100.0), 37);
          if (d_w > 12)
            render.draw_block_gauge(d_x + 9, dy + 2, d_w - 11, free_pct, renderer::Gradient::DarkGreenToGreen, 32);
        }
      }
    }
    y_cursor += mid_h;
  }

  // --- 3. Net Box ---
  if (show_net)
  {
    render.draw_window(0, y_cursor, left_w, net_h, "", 37);
    render.draw_text(2, y_cursor, "\xC2\xB3", 91);
    render.draw_text(3, y_cursor, "Net ", 37);
    if (net_h > 2)
    {
      int text_x = 2;
      render.draw_text(text_x, y_cursor + 1, "        Down   | Up", 37);

      char nbuf[128];
      int smooth_count = std::min((int)net_rx_history.size(), std::max(1, 1000 / refresh_rate_ms));
      double curr_rx = 0, curr_tx = 0;
      for (int i = 0; i < smooth_count; i++)
      {
        curr_rx += net_rx_history[net_rx_history.size() - 1 - i];
        curr_tx += net_tx_history[net_tx_history.size() - 1 - i];
      }
      curr_rx /= smooth_count;
      curr_tx /= smooth_count;

      std::string nbuf_curr = fmt::format("Curr: {:8.2f} | {:8.2f} KB/s", curr_rx, curr_tx);
      render.draw_text(text_x, y_cursor + 2, nbuf_curr, 37);
      std::string nbuf_peak = fmt::format("Peak: {:8.2f} | {:8.2f} KB/s", max_rx_kbps, max_tx_kbps);
      render.draw_text(text_x, y_cursor + 3, nbuf_peak, 37);

      if (net_h > 4)
      {
        unsigned long long total_rx = 0, total_tx = 0;
        for (const auto &net : networks)
        {
          if (net.name != "lo")
          { // exclude loopback
            total_rx += net.rx_bytes;
            total_tx += net.tx_bytes;
          }
        }
        std::string t_rx = format_bytes(total_rx);
        std::string t_tx = format_bytes(total_tx);
        std::string tbuf = fmt::format("Totl: {:8} | {:8}", t_rx, t_tx);
        render.draw_text(text_x, y_cursor + 4, tbuf, 37);
      }
    }
    y_cursor += net_h;
  }

  // --- 4. Process Box ---
  if (show_proc)
  {
    render.draw_window(left_w, 1, right_w, avail_h, "", 37);

    // Custom title for Process Box
    int tx = left_w + 2;
    render.draw_text(tx, 1, "\xE2\x81\xB4", 91);
    tx += 1;
    std::string title_text = (tab_selected == 0) ? "Processes " : "Services  ";
    render.draw_text(tx, 1, title_text, 37);
    tx += 10;

    // Tab indicator
    render.draw_text(tx, 1, " \xE2\x86\xB9 ", 91); // Tab symbol
    tx += 3;

    // Sort indicator (draw on right edge first)
    std::string sort_text = (current_sort == SortBy::CPU) ? "ort(CPU) " : "ort(Name)";
    int sort_x = left_w + right_w - sort_text.length() - 2;
    render.draw_text(sort_x, 1, "S", 91);
    render.draw_text(sort_x + 1, 1, sort_text, 37);

    // Filter indicator
    render.draw_text(tx, 1, " F", 91);
    tx += 2;
    int filter_max = sort_x - tx - 1; // space left for filter
    if (in_search_mode)
    {
      std::string f_text = "ilter: " + search_query + "_";
      if (f_text.length() > filter_max && filter_max > 0)
      {
        f_text = f_text.substr(f_text.length() - filter_max);
      }
      render.draw_text(tx, 1, f_text, 33, 49, true, false, filter_max);
    }
    else
    {
      std::string f_text = search_query.empty() ? "ilter" : "ilter: " + search_query;
      if (f_text.length() > filter_max && filter_max > 0)
      {
        f_text = f_text.substr(f_text.length() - filter_max);
      }
      render.draw_text(tx, 1, f_text, 37, 49, false, false, filter_max);
    }

    int list_x = left_w;
    int list_start_y = 3; // header on 2, list on 3
    int list_h = std::max(0, avail_h - 3);

    if (tab_selected == 0)
    {
      int target_len = right_w - 3;
      int fixed_len = 38;
      int prog_len = std::max(4, target_len - fixed_len);

      std::string header =
          fmt::format(" {:<5}   {:<{}.{}}   {:<7}   {:<7}   {:>6}", "PID", "Program", prog_len, prog_len, "User", "Mem", "CPU%");
      if ((int)header.length() < target_len)
        header.append(target_len - header.length(), ' ');
      if (avail_h > 3)
        render.draw_text(list_x + 2, 2, header, 37, 49, true, true, target_len);

      for (int i = 0; i < list_h && i + proc_scroll < (int)filtered_procs.size(); i++)
      {
        int idx = i + proc_scroll;
        const auto p = filtered_procs[idx];
        std::string formatted = fmt::format(" {:<5d}   {:<{}.{}}   {:<7.7}   {:<7.7}   {:5.1f}%", p->pid, p->name, prog_len, prog_len, p->user,
                                            format_bytes(p->memory_kb * 1024), p->cpu_usage);
        if ((int)formatted.length() < target_len)
          formatted.append(target_len - formatted.length(), ' ');

        if (idx == proc_selected)
        {
          render.draw_text(list_x + 2, list_start_y + i, formatted, 30, 47, false, false, target_len);
        }
        else
        {
          render.draw_text(list_x + 2, list_start_y + i, formatted, 39, 49, false, false, target_len);
        }
      }

      // Footer Right-Aligned (Process)
      int bottom_y = h - 1;
      for (int k = 0; k < right_w - 2; k++)
      {
        render.draw_text(left_w + 1 + k, bottom_y, "\xE2\x94\x80", 37, 49);
      }

      int r_edge = left_w + right_w - 2;

      std::string counts_str;
      if (proc_selected == -1)
      {
        counts_str = fmt::format(" -/{} ", (int)filtered_procs.size());
      }
      else
      {
        counts_str = fmt::format(" {}/{} ", proc_selected + 1, (int)filtered_procs.size());
      }
      r_edge -= counts_str.length();
      render.draw_text(r_edge, bottom_y, counts_str, 37, 49);

      r_edge -= 4;
      render.draw_text(r_edge, bottom_y, " \xE2\x94\x80\xE2\x94\x80 ", 37, 49);

      std::string enter_sym = "\xE2\x86\xB5";
      r_edge -= 1;
      render.draw_text(r_edge, bottom_y, enter_sym, (proc_selected != -1) ? 91 : 90, 49);

      r_edge -= 5;
      render.draw_text(r_edge, bottom_y, " Info", (proc_selected != -1) ? 37 : 90, 49);

      r_edge -= 4;
      render.draw_text(r_edge, bottom_y, " \xE2\x94\x80\xE2\x94\x80 ", 37, 49);

      std::string down_sym = "\xE2\x86\x93";
      r_edge -= 1;
      render.draw_text(r_edge, bottom_y, down_sym, 91, 49);

      r_edge -= 7;
      render.draw_text(r_edge, bottom_y, " select", 37, 49);

      std::string up_sym = "\xE2\x86\x91";
      r_edge -= 1;
      render.draw_text(r_edge, bottom_y, up_sym, 91, 49);

      r_edge -= 1;
      render.draw_text(r_edge, bottom_y, " ", 37, 49);
    }
    else
    {
      // Services
      int target_len = right_w - 3;
      int name_len = std::min(20, std::max(10, target_len / 3));
      int status_len = 10;
      int desc_len = std::max(4, target_len - name_len - status_len - 7);

      std::string header = fmt::format(" {:<{}.{}}   {:<{}.{}}   {:<{}.{}}", "Service Name", name_len, name_len, "Status", status_len,
                                       status_len, "Description", desc_len, desc_len);
      if ((int)header.length() < target_len)
        header.append(target_len - header.length(), ' ');
      if (avail_h > 3)
        render.draw_text(list_x + 2, 2, header, 37, 49, true, true, target_len);

      for (int i = 0; i < list_h && i + svc_scroll < (int)filtered_svcs.size(); i++)
      {
        int idx = i + svc_scroll;
        const auto s = filtered_svcs[idx];
        std::string formatted = fmt::format(" {:<{}.{}}   {:<{}.{}}   {:<{}.{}}", s->name, name_len, name_len, s->active_state, status_len,
                                            status_len, s->description, desc_len, desc_len);
        if ((int)formatted.length() < target_len)
          formatted.append(target_len - formatted.length(), ' ');

        if (idx == svc_selected)
        {
          render.draw_text(list_x + 2, list_start_y + i, formatted, 30, 47, false, false, target_len);
        }
        else
        {
          render.draw_text(list_x + 2, list_start_y + i, formatted, 39, 49, false, false, target_len);
        }
      }

      // Footer Right-Aligned (Services)
      int bottom_y = h - 1;
      for (int k = 0; k < right_w - 2; k++)
      {
        render.draw_text(left_w + 1 + k, bottom_y, "\xE2\x94\x80", 37, 49);
      }

      int r_edge = left_w + right_w - 2;

      std::string counts_str;
      if (svc_selected == -1)
      {
        counts_str = fmt::format(" -/{} ", (int)filtered_svcs.size());
      }
      else
      {
        counts_str = fmt::format(" {}/{} ", svc_selected + 1, (int)filtered_svcs.size());
      }
      r_edge -= counts_str.length();
      render.draw_text(r_edge, bottom_y, counts_str, 37, 49);

      r_edge -= 4;
      render.draw_text(r_edge, bottom_y, " \xE2\x94\x80\xE2\x94\x80 ", 37, 49);

      std::string enter_sym = "\xE2\x86\xB5";
      r_edge -= 1;
      render.draw_text(r_edge, bottom_y, enter_sym, (svc_selected != -1) ? 91 : 90, 49);

      r_edge -= 5;
      render.draw_text(r_edge, bottom_y, " Info", (svc_selected != -1) ? 37 : 90, 49);

      r_edge -= 4;
      render.draw_text(r_edge, bottom_y, " \xE2\x94\x80\xE2\x94\x80 ", 37, 49);

      std::string down_sym = "\xE2\x86\x93";
      r_edge -= 1;
      render.draw_text(r_edge, bottom_y, down_sym, 91, 49);

      r_edge -= 7;
      render.draw_text(r_edge, bottom_y, " select", 37, 49);

      std::string up_sym = "\xE2\x86\x91";
      r_edge -= 1;
      render.draw_text(r_edge, bottom_y, up_sym, 91, 49);

      r_edge -= 1;
      render.draw_text(r_edge, bottom_y, " ", 37, 49);
    }
  }

  if (show_context_menu)
  {
    int cx = w / 2 - 10;
    int cy = h / 2 - 3;
    render.draw_window(cx, cy, 20, 6, "Action", 91);
    std::string act1 = "End Task";
    std::string act2 = "Open Location";
    if (context_menu_selected == 0)
    {
      render.draw_text(cx + 2, cy + 2, "> " + act1, 30, 47, true);
      render.draw_text(cx + 4, cy + 3, act2, 37, 49);
    }
    else
    {
      render.draw_text(cx + 4, cy + 2, act1, 37, 49);
      render.draw_text(cx + 2, cy + 3, "> " + act2, 30, 47, true);
    }
  }

  render.render();
}
