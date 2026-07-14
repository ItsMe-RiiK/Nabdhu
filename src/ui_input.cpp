#include "input.h"
#include "terminal.h"
#include "ui.h"

#include <algorithm>
#include <chrono>

void UIManager::handle_input()
{
  input::Event ev = input::read_event();
  if (ev.key == input::KeyCode::Unknown)
    return;

  if (ev.key == input::KeyCode::Resize)
  {
    return; // Will be redrawn naturally
  }

  static input::KeyCode last_key = input::KeyCode::Unknown;
  static char last_ch = 0;
  static auto last_key_time = std::chrono::steady_clock::now();
  auto now = std::chrono::steady_clock::now();

  bool is_held =
      (ev.key == last_key && ev.ch == last_ch && std::chrono::duration_cast<std::chrono::milliseconds>(now - last_key_time).count() < 600);

  last_key = ev.key;
  last_ch = ev.ch;
  last_key_time = now;

  if (is_held)
  {
    bool allow_hold =
        (ev.key == input::KeyCode::Up || ev.key == input::KeyCode::Down || ev.key == input::KeyCode::Right ||
         ev.key == input::KeyCode::Left || ev.key == input::KeyCode::MouseScrollUp || ev.key == input::KeyCode::MouseScrollDown ||
         (in_search_mode && ev.key == input::KeyCode::Backspace) || (in_search_mode && ev.key == input::KeyCode::Char));

    if (!allow_hold)
    {
      return;
    }
  }

  if (show_main_menu)
  {
    if (ev.key == input::KeyCode::Up)
      main_menu_selected--;
    if (ev.key == input::KeyCode::Down)
      main_menu_selected++;
    if (ev.key == input::KeyCode::Enter)
    {
      if (main_menu_selected == 0) // Options
        show_main_menu = false;
      else if (main_menu_selected == 1) // Help
        show_main_menu = false;
      else if (main_menu_selected == 2) // Quit
        running = false;
    }
    if (ev.key == input::KeyCode::Escape || (ev.key == input::KeyCode::Char && ev.ch == 'm'))
    {
      show_main_menu = false;
    }

    if (main_menu_selected < 0)
      main_menu_selected = 0;
    if (main_menu_selected > 2)
      main_menu_selected = 2;

    return;
  }

  if (show_context_menu)
  {
    if (ev.key == input::KeyCode::Up)
      context_menu_selected--;
    if (ev.key == input::KeyCode::Down)
      context_menu_selected++;
    if (ev.key == input::KeyCode::Enter)
      perform_action();
    int max_c = 3;
    if (context_menu_selected < 0)
      context_menu_selected = 0;
    if (context_menu_selected >= max_c)
      context_menu_selected = max_c - 1;
    return;
  }

  if (ev.key == input::KeyCode::Tab)
  {
    if (!in_search_mode)
    {
      tab_selected = (tab_selected == 0) ? 1 : 0;
      search_query.clear(); // Clear search query when switching tabs
      if (tab_selected == 1 && raw_svcs.empty())
      {
        raw_svcs = service_manager.get_services();
      }
      apply_filter(); // Always apply filter to clear search results and refresh
    }
    return;
  }

  int h = terminal::get_height();
  int list_start_y = 4;
  int list_h = std::max(1, h - list_start_y);

  if (ev.key == input::KeyCode::Up)
  {
    if (tab_selected == 0)
    {
      if (proc_selected == -1)
      {
        proc_scroll = std::max(0, proc_scroll - 1);
      }
      else if (proc_selected < proc_scroll || proc_selected >= proc_scroll + list_h)
      {
        proc_selected = proc_scroll;
      }
      else
      {
        if (proc_selected > 0)
          proc_selected--;
        else
          proc_selected = -1;
      }
      if (proc_selected >= 0 && proc_selected < proc_scroll)
        proc_scroll = proc_selected;
    }
    else
    {
      if (svc_selected == -1)
      {
        svc_scroll = std::max(0, svc_scroll - 1);
      }
      else if (svc_selected < svc_scroll || svc_selected >= svc_scroll + list_h)
      {
        svc_selected = svc_scroll;
      }
      else
      {
        if (svc_selected > 0)
          svc_selected--;
        else
          svc_selected = -1;
      }
      if (svc_selected >= 0 && svc_selected < svc_scroll)
        svc_scroll = svc_selected;
    }
  }
  else if (ev.key == input::KeyCode::Down)
  {
    if (tab_selected == 0)
    {
      if (proc_selected == -1)
      {
        proc_selected = proc_scroll;
      }
      else if (proc_selected < proc_scroll || proc_selected >= proc_scroll + list_h)
      {
        proc_selected = proc_scroll;
      }
      else
      {
        proc_selected++;
      }
      if (proc_selected >= proc_scroll + list_h)
        proc_scroll = proc_selected - list_h + 1;
    }
    else
    {
      if (svc_selected == -1)
      {
        svc_selected = svc_scroll;
      }
      else if (svc_selected < svc_scroll || svc_selected >= svc_scroll + list_h)
      {
        svc_selected = svc_scroll;
      }
      else
      {
        svc_selected++;
      }
      if (svc_selected >= svc_scroll + list_h)
        svc_scroll = svc_selected - list_h + 1;
    }
  }
  else if (ev.key == input::KeyCode::MouseScrollUp)
  {
    if (ev.mouse_x > 40)
    {
      if (tab_selected == 0)
        proc_scroll -= 3;
      else
        svc_scroll -= 3;
    }
  }
  else if (ev.key == input::KeyCode::MouseScrollDown)
  {
    if (ev.mouse_x > 40)
    {
      if (tab_selected == 0)
        proc_scroll += 3;
      else
        svc_scroll += 3;
    }
  }
  else if (ev.key == input::KeyCode::Enter || (ev.key == input::KeyCode::MouseClickRight))
  {
    if (tab_selected == 0 && proc_selected == -1)
    {
      if (!filtered_procs.empty())
        proc_selected = proc_scroll;
    }
    else if (tab_selected == 1 && svc_selected == -1)
    {
      if (!filtered_svcs.empty())
        svc_selected = svc_scroll;
    }
    else
    {
      show_context_menu = true;
      context_menu_selected = 0;
    }
  }
  else if (ev.key == input::KeyCode::Backspace)
  {
    if (in_search_mode && !search_query.empty())
    {
      search_query.pop_back();
      apply_filter();
    }
  }
  else if (ev.key == input::KeyCode::Enter)
  {
    if (in_search_mode)
    {
      in_search_mode = false;
    }
    else
    {
      if (tab_selected == 0 && proc_selected == -1)
      {
        if (!filtered_procs.empty())
          proc_selected = proc_scroll;
      }
      else if (tab_selected == 1 && svc_selected == -1)
      {
        if (!filtered_svcs.empty())
          svc_selected = svc_scroll;
      }
      else
      {
        show_context_menu = true;
        context_menu_selected = 0;
      }
    }
  }
  else if (ev.key == input::KeyCode::Escape)
  {
    if (in_search_mode)
    {
      in_search_mode = false;
    }
    else if (show_context_menu)
    {
      show_context_menu = false;
    }
    // Esc does not hide highlight and does not open main menu
    return;
  }
  else if (ev.key == input::KeyCode::Char)
  {
    if (in_search_mode)
    {
      search_query += ev.ch;
      apply_filter();
    }
    else
    {
      if (ev.ch == '1')
        show_cpu = !show_cpu;
      else if (ev.ch == '2')
        show_mem = !show_mem;
      else if (ev.ch == '3')
        show_net = !show_net;
      else if (ev.ch == '4')
        show_proc = !show_proc;
      else if (ev.ch == 'd' || ev.ch == 'D')
        show_disk = !show_disk;
      else if (ev.ch == 'f' || ev.ch == 'F')
        in_search_mode = true;
      else if (ev.ch == 'm' || ev.ch == 'M')
      {
        show_main_menu = true;
        main_menu_selected = 0;
      }
      else if (ev.ch == 's' || ev.ch == 'S')
      {
        if (current_sort == SortBy::CPU)
          current_sort = SortBy::Name;
        else if (current_sort == SortBy::Name)
          current_sort = SortBy::PID;
        else
          current_sort = SortBy::CPU;

        apply_filter();
      }
      else if (ev.ch == '+' || ev.ch == '=')
      {
        if (refresh_rate_ms >= 86400000)
        {
          refresh_warn_frames = 15;
        }
        else
        {
          refresh_rate_ms += 100;
          if (refresh_rate_ms > 86400000)
            refresh_rate_ms = 86400000;
        }
      }
      else if (ev.ch == '-' || ev.ch == '_')
      {
        if (refresh_rate_ms <= 100)
        {
          refresh_warn_frames = 15;
        }
        else
        {
          refresh_rate_ms -= 100;
          if (refresh_rate_ms < 100)
            refresh_rate_ms = 100;
        }
      }
    }
  }

  if (proc_selected < -1)
    proc_selected = -1;
  if (proc_selected >= (int)filtered_procs.size())
    proc_selected = std::max(-1, (int)filtered_procs.size() - 1);

  if (svc_selected < -1)
    svc_selected = -1;
  if (svc_selected >= (int)filtered_svcs.size())
    svc_selected = std::max(-1, (int)filtered_svcs.size() - 1);

  if (proc_scroll < 0)
    proc_scroll = 0;
  if (proc_scroll > std::max(0, (int)filtered_procs.size() - list_h))
    proc_scroll = std::max(0, (int)filtered_procs.size() - list_h);

  if (svc_scroll < 0)
    svc_scroll = 0;
  if (svc_scroll > std::max(0, (int)filtered_svcs.size() - list_h))
    svc_scroll = std::max(0, (int)filtered_svcs.size() - list_h);
}
