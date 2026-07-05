#include "ui.h"
#include <algorithm>
#include <functional>
#include <limits.h>
#include <set>
#include <string>
#include <unistd.h>

enum
{
  COL_PID = 0,
  COL_NAME,
  COL_USER,
  COL_STATUS,
  COL_CPU,
  COL_MEM,
  COL_BG_COLOR,
  COL_FG_COLOR,
  NUM_PROC_COLS
};

enum
{
  COL_SVC_NAME = 0,
  COL_SVC_LOAD,
  COL_SVC_ACTIVE,
  COL_SVC_SUB,
  COL_SVC_DESC,
  NUM_SVC_COLS
};

UIManager::UIManager() : window(nullptr), notebook(nullptr), current_perf_tab(0)
{
  for (int i = 0; i < max_history; ++i)
  {
    cpu_history.push_back(0.0);
    mem_history.push_back(0.0);
  }
}

UIManager::~UIManager() {}

void UIManager::run(int argc, char *argv[])
{
  gtk_init(&argc, &argv);

  window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

  // Force dark theme
  g_object_set(gtk_settings_get_default(), "gtk-application-prefer-dark-theme", TRUE, NULL);

  // Enable transparency for rounded corners
  GdkScreen *screen = gtk_widget_get_screen(window);
  GdkVisual *visual = gdk_screen_get_rgba_visual(screen);
  if (visual != NULL && gdk_screen_is_composited(screen))
  {
    gtk_widget_set_visual(window, visual);
  }

  // Set icon for window natively via GResource
  GError *error = NULL;
  GdkPixbuf *icon_pixbuf = gdk_pixbuf_new_from_resource("/org/taskmanager/IconApp.png", &error);
  if (icon_pixbuf)
  {
    gtk_window_set_icon(GTK_WINDOW(window), icon_pixbuf);
    g_object_unref(icon_pixbuf);
  }
  else
  {
    g_printerr("Error loading window icon: %s\n", error->message);
    g_error_free(error);
  }

  // Modern Header Bar (gives rounded top corners natively in GTK3)
  GtkWidget *header_bar = gtk_header_bar_new();
  gtk_header_bar_set_show_close_button(GTK_HEADER_BAR(header_bar), TRUE);
  gtk_header_bar_set_title(GTK_HEADER_BAR(header_bar), "TaskManager");

  // Add icon to header bar
  error = NULL;
  GdkPixbuf *header_pixbuf = gdk_pixbuf_new_from_resource_at_scale("/org/taskmanager/IconApp.png", 24, 24, TRUE, &error);
  if (header_pixbuf)
  {
    GtkWidget *icon = gtk_image_new_from_pixbuf(header_pixbuf);
    gtk_header_bar_pack_start(GTK_HEADER_BAR(header_bar), icon);
    g_object_unref(header_pixbuf);
  }
  else
  {
    g_printerr("Error loading header icon: %s\n", error->message);
    g_error_free(error);
  }

  search_entry = gtk_search_entry_new();
  gtk_widget_set_tooltip_text(search_entry, "Search processes");
  gtk_header_bar_pack_end(GTK_HEADER_BAR(header_bar), search_entry);
  g_signal_connect(search_entry, "search-changed", G_CALLBACK(on_search_changed), this);

  gtk_window_set_titlebar(GTK_WINDOW(window), header_bar);

  gtk_window_set_default_size(GTK_WINDOW(window), 900, 700);
  gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
  g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
  g_signal_connect(window, "key-press-event", G_CALLBACK(on_window_key_press), this);

  // Apply CSS for rounded corners on UI elements
  GtkCssProvider *provider = gtk_css_provider_new();
  gtk_css_provider_load_from_data(provider,
                                  "window, window.csd > decoration { border-radius: 12px; }"
                                  "headerbar { border-radius: 12px 12px 0 0; }"
                                  "notebook { border-radius: 8px; margin: 6px; }"
                                  "scrolledwindow { border-radius: 8px; }"
                                  "button { border-radius: 6px; }",
                                  -1, NULL);
  gtk_style_context_add_provider_for_screen(gdk_screen_get_default(), GTK_STYLE_PROVIDER(provider),
                                            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref(provider);

  setup_ui();

  refresh_processes();
  refresh_services();

  g_timeout_add(2000, on_timer_tick, this);

  gtk_widget_show_all(window);
  gtk_main();
}

void UIManager::setup_ui()
{
  notebook = gtk_notebook_new();
  gtk_container_add(GTK_CONTAINER(window), notebook);

  GtkWidget *proc_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
  setup_process_tab(proc_vbox);
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook), proc_vbox, gtk_label_new("Processes"));

  GtkWidget *perf_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
  setup_performance_tab(perf_vbox);
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook), perf_vbox, gtk_label_new("Performance"));

  GtkWidget *svc_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
  setup_service_tab(svc_vbox);
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook), svc_vbox, gtk_label_new("Services"));
}

void UIManager::setup_process_tab(GtkWidget *parent)
{
  GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
  gtk_widget_set_vexpand(scrolled_window, TRUE);
  gtk_box_pack_start(GTK_BOX(parent), scrolled_window, TRUE, TRUE, 0);

  process_store = gtk_tree_store_new(NUM_PROC_COLS, G_TYPE_INT, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
                                     G_TYPE_STRING, G_TYPE_STRING);
  process_filter = gtk_tree_model_filter_new(GTK_TREE_MODEL(process_store), NULL);
  gtk_tree_model_filter_set_visible_func(GTK_TREE_MODEL_FILTER(process_filter), on_search_filter_visible, this, NULL);

  process_treeview = gtk_tree_view_new_with_model(process_filter);
  g_object_unref(process_filter); // treeview holds reference
  g_object_unref(process_store);

  gtk_container_add(GTK_CONTAINER(scrolled_window), process_treeview);

  const char *headers[] = {"PID", "Apps Name", "User", "Status", "CPU (%)", "Memory (%)", "BG", "FG"};
  // Skip PID and BG in headers
  int col_indices[] = {COL_NAME, COL_USER, COL_STATUS, COL_CPU, COL_MEM};

  for (int i : col_indices)
  {
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    g_object_set(renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);

    GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes(headers[i], renderer, "text", i, "cell-background", COL_BG_COLOR,
                                                                         "foreground", COL_FG_COLOR, NULL);
    gtk_tree_view_column_set_sort_column_id(column, i);
    gtk_tree_view_column_set_resizable(column, TRUE);

    if (i == COL_NAME)
    {
      gtk_tree_view_column_set_expand(column, TRUE);
      gtk_tree_view_column_set_min_width(column, 250);
    }
    else
    {
      gtk_tree_view_column_set_min_width(column, 80);
    }

    if (i == COL_CPU)
      col_cpu_ptr = column;
    if (i == COL_MEM)
      col_mem_ptr = column;

    gtk_tree_view_append_column(GTK_TREE_VIEW(process_treeview), column);
  }

  // Disable native GTK popup search so it doesn't conflict with app search bar
  gtk_tree_view_set_enable_search(GTK_TREE_VIEW(process_treeview), FALSE);

  process_context_menu = gtk_menu_new();
  GtkWidget *menu_item_end = gtk_menu_item_new_with_label("End task");
  GtkWidget *menu_item_open = gtk_menu_item_new_with_label("Open file location");
  gtk_menu_shell_append(GTK_MENU_SHELL(process_context_menu), menu_item_end);
  gtk_menu_shell_append(GTK_MENU_SHELL(process_context_menu), menu_item_open);
  gtk_widget_show_all(process_context_menu);

  g_signal_connect(menu_item_end, "activate", G_CALLBACK(on_context_menu_end_task), this);
  g_signal_connect(menu_item_open, "activate", G_CALLBACK(on_context_menu_open_file_location), this);

  g_signal_connect(process_treeview, "button-press-event", G_CALLBACK(on_process_button_press), this);
}

void UIManager::setup_performance_tab(GtkWidget *parent)
{
  GtkWidget *paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
  gtk_box_pack_start(GTK_BOX(parent), paned, TRUE, TRUE, 0);

  GtkWidget *sidebar = gtk_list_box_new();
  gtk_widget_set_size_request(sidebar, 200, -1);
  gtk_paned_pack1(GTK_PANED(paned), sidebar, FALSE, FALSE);

  GtkWidget *row_cpu = gtk_list_box_row_new();
  GtkWidget *lbl_cpu = gtk_label_new("CPU");
  gtk_widget_set_margin_top(lbl_cpu, 15);
  gtk_widget_set_margin_bottom(lbl_cpu, 15);
  gtk_container_add(GTK_CONTAINER(row_cpu), lbl_cpu);
  gtk_list_box_insert(GTK_LIST_BOX(sidebar), row_cpu, 0);

  GtkWidget *row_mem = gtk_list_box_row_new();
  GtkWidget *lbl_mem = gtk_label_new("Memory");
  gtk_widget_set_margin_top(lbl_mem, 15);
  gtk_widget_set_margin_bottom(lbl_mem, 15);
  gtk_container_add(GTK_CONTAINER(row_mem), lbl_mem);
  gtk_list_box_insert(GTK_LIST_BOX(sidebar), row_mem, 1);

  g_signal_connect(sidebar, "row-activated", G_CALLBACK(on_perf_sidebar_row_activated), this);

  perf_stack = gtk_stack_new();
  gtk_paned_pack2(GTK_PANED(paned), perf_stack, TRUE, FALSE);

  // Fetch static hardware info once
  mem_hw_info = process_manager.get_memory_hardware_info();

  // CPU Page
  GtkWidget *cpu_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
  gtk_container_set_border_width(GTK_CONTAINER(cpu_box), 15);

  cpu_usage_label = gtk_label_new("CPU\n%0.0");
  PangoAttrList *attr_list = pango_attr_list_new();
  PangoAttribute *attr = pango_attr_scale_new(2.0);
  pango_attr_list_insert(attr_list, attr);
  gtk_label_set_attributes(GTK_LABEL(cpu_usage_label), attr_list);
  pango_attr_list_unref(attr_list);
  gtk_label_set_xalign(GTK_LABEL(cpu_usage_label), 0.0);
  gtk_box_pack_start(GTK_BOX(cpu_box), cpu_usage_label, FALSE, FALSE, 0);

  perf_cpu_drawing_area = gtk_drawing_area_new();
  gtk_widget_set_vexpand(perf_cpu_drawing_area, TRUE);
  gtk_box_pack_start(GTK_BOX(cpu_box), perf_cpu_drawing_area, TRUE, TRUE, 0);
  g_signal_connect(G_OBJECT(perf_cpu_drawing_area), "draw", G_CALLBACK(on_draw_cpu_graph), this);

  cpu_details_label = gtk_label_new("Sockets: -\nThreads: -\nUptime: 00:00:00");
  gtk_label_set_xalign(GTK_LABEL(cpu_details_label), 0.0);
  gtk_box_pack_start(GTK_BOX(cpu_box), cpu_details_label, FALSE, FALSE, 10);

  gtk_stack_add_named(GTK_STACK(perf_stack), cpu_box, "cpu");

  // Mem Page
  GtkWidget *mem_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
  gtk_container_set_border_width(GTK_CONTAINER(mem_box), 15);

  mem_usage_label = gtk_label_new("Memory\n0.0 GB / 0.0 GB (0%)");
  attr_list = pango_attr_list_new();
  attr = pango_attr_scale_new(2.0);
  pango_attr_list_insert(attr_list, attr);
  gtk_label_set_attributes(GTK_LABEL(mem_usage_label), attr_list);
  pango_attr_list_unref(attr_list);
  gtk_label_set_xalign(GTK_LABEL(mem_usage_label), 0.0);
  gtk_box_pack_start(GTK_BOX(mem_box), mem_usage_label, FALSE, FALSE, 0);

  perf_mem_drawing_area = gtk_drawing_area_new();
  gtk_widget_set_vexpand(perf_mem_drawing_area, TRUE);
  gtk_box_pack_start(GTK_BOX(mem_box), perf_mem_drawing_area, TRUE, TRUE, 0);
  g_signal_connect(G_OBJECT(perf_mem_drawing_area), "draw", G_CALLBACK(on_draw_mem_graph), this);

  mem_details_label = gtk_label_new("Speed: -\nSlots used: -\nForm factor: -\nHardware reserved: -");
  gtk_label_set_xalign(GTK_LABEL(mem_details_label), 0.0);
  gtk_box_pack_start(GTK_BOX(mem_box), mem_details_label, FALSE, FALSE, 10);

  gtk_stack_add_named(GTK_STACK(perf_stack), mem_box, "mem");
}

void UIManager::setup_service_tab(GtkWidget *parent)
{
  GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
  gtk_widget_set_vexpand(scrolled_window, TRUE);
  gtk_box_pack_start(GTK_BOX(parent), scrolled_window, TRUE, TRUE, 0);

  service_store = gtk_list_store_new(NUM_SVC_COLS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);
  service_filter = gtk_tree_model_filter_new(GTK_TREE_MODEL(service_store), NULL);
  gtk_tree_model_filter_set_visible_func(GTK_TREE_MODEL_FILTER(service_filter), on_service_search_filter_visible, this, NULL);

  service_treeview = gtk_tree_view_new_with_model(service_filter);
  gtk_tree_view_set_enable_search(GTK_TREE_VIEW(service_treeview), FALSE);
  g_object_unref(service_filter);
  g_object_unref(service_store);

  gtk_container_add(GTK_CONTAINER(scrolled_window), service_treeview);

  const char *headers[] = {"Service Name", "Load", "Active", "Sub", "Description"};
  for (int i = 0; i < NUM_SVC_COLS; ++i)
  {
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    g_object_set(renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL);

    GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes(headers[i], renderer, "text", i, NULL);
    gtk_tree_view_column_set_sort_column_id(column, i);
    gtk_tree_view_column_set_resizable(column, TRUE);

    if (i == COL_SVC_DESC)
    {
      gtk_tree_view_column_set_expand(column, TRUE);
    }
    else if (i == COL_SVC_NAME)
    {
      gtk_tree_view_column_set_min_width(column, 200);
    }
    else
    {
      gtk_tree_view_column_set_min_width(column, 80);
    }

    gtk_tree_view_append_column(GTK_TREE_VIEW(service_treeview), column);
  }

  GtkWidget *button_box = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
  gtk_box_pack_start(GTK_BOX(parent), button_box, FALSE, FALSE, 0);

  GtkWidget *btn_start = gtk_button_new_with_label("Start Service");
  GtkWidget *btn_stop = gtk_button_new_with_label("Stop Service");
  GtkWidget *btn_restart = gtk_button_new_with_label("Restart Service");

  g_signal_connect(btn_start, "clicked", G_CALLBACK(on_start_service_clicked), this);
  g_signal_connect(btn_stop, "clicked", G_CALLBACK(on_stop_service_clicked), this);
  g_signal_connect(btn_restart, "clicked", G_CALLBACK(on_restart_service_clicked), this);

  gtk_container_add(GTK_CONTAINER(button_box), btn_start);
  gtk_container_add(GTK_CONTAINER(button_box), btn_stop);
  gtk_container_add(GTK_CONTAINER(button_box), btn_restart);
}

std::string UIManager::get_color_for_usage(double usage)
{
  std::string color = "";
  if (usage >= 75.0)
    color = "#ff5608e0"; // Semi red
  else if (usage >= 50.0)
    color = "#ff9900ff"; // Orange
  else if (usage >= 25.0)
    color = "#c8ff00ff"; // Green-yellow
  else if (usage > 0.1)
    color = "#2aa514e0"; // Green
  else
    return ""; // Natural theme (Null)

  // Automatically remove alpha channel if format is #RRGGBBAA
  if (color.length() == 9 && color[0] == '#')
  {
    return color.substr(0, 7);
  }
  return color;
}

void UIManager::refresh_processes()
{
  auto processes = process_manager.get_processes();
  std::map<int, ProcessInfo> new_procs;
  std::map<int, std::vector<int>> children_map;

  for (const auto &p : processes)
  {
    new_procs[p.pid] = p;
    children_map[p.ppid].push_back(p.pid);
  }

  auto get_expected_parent = [&](int pid, int ppid, const std::string &name, bool is_app) -> int
  {
    if (new_procs.find(ppid) == new_procs.end())
      return 0;

    std::string p_name = new_procs[ppid].name;
    std::string p_base = p_name.substr(0, p_name.find(' '));
    std::string my_base = name.substr(0, name.find(' '));

    // 1. Parent Categories (Core or DE)
    bool parent_is_init = (ppid <= 2 || p_base.find("systemd") != std::string::npos || p_base.find("kthreadd") != std::string::npos ||
                           p_base.find("lightdm") != std::string::npos);

    bool parent_is_de = (p_base.find("cinnamon") != std::string::npos || p_base.find("gnome-shell") != std::string::npos ||
                         p_base.find("plasma") != std::string::npos);

    // 2. FILTER DAEMON (Core)
    bool is_system_helper =
        (my_base.find("csd-") == 0 || my_base.find("xdg-") == 0 || my_base.find("evolution") == 0 || my_base.find("dbus") == 0 ||
         my_base.find("gvfs") == 0 || my_base.find("polkit") == 0 || my_base.find("dconf") == 0 || my_base.find("at-spi") == 0 ||
         name.find("daemon") != std::string::npos || name.find("portal") != std::string::npos);

    // 3. FILTER APP
    bool is_known_app = (my_base == "msedge" || my_base == "chrome" || my_base == "firefox" || my_base == "code" || my_base == "brave" ||
                         my_base == "sober" || my_base == "taskmanager");

    // 4. SMART GROUPING
    int best_parent_pid = 0;
    for (const auto &pair : new_procs)
    {
      if (pair.first == pid)
        continue;
      std::string cand_name = pair.second.name;
      std::string cand_base = cand_name.substr(0, cand_name.find(' '));
      if (cand_base.length() >= 3 && name.find(cand_base) == 0)
      {
        if (best_parent_pid == 0 || pair.first < best_parent_pid)
        {
          best_parent_pid = pair.first;
        }
      }
    }
    if (best_parent_pid != 0 && best_parent_pid < pid)
    {
      return best_parent_pid;
    }

    // 5. IF PARENT IS NOT INIT, FOLLOW ORIGINAL PARENT
    if (!parent_is_init && !parent_is_de)
      return ppid;

    // 6. RULES FOR FREEING TO SURFACE
    if (is_system_helper)
      return ppid;
    if (is_app || is_known_app)
      return 0;

    return ppid;
  };

  std::map<int, std::vector<int>> actual_children;
  std::map<int, int> actual_parent;
  for (const auto &p : processes)
  {
    actual_parent[p.pid] = get_expected_parent(p.pid, p.ppid, p.name, p.is_app);
    if (actual_parent[p.pid] != 0 && actual_parent[p.pid] != p.pid)
    {
      actual_children[actual_parent[p.pid]].push_back(p.pid);
    }
  }

  std::map<int, double> accumulated_cpu;
  std::map<int, double> accumulated_mem;
  std::set<int> calc_visiting;

  std::function<void(int)> calc_accum = [&](int pid)
  {
    if (calc_visiting.find(pid) != calc_visiting.end())
      return; // prevent cycle
    calc_visiting.insert(pid);

    double cpu = new_procs[pid].cpu_usage;
    double mem = new_procs[pid].memory_kb;

    for (int child_pid : actual_children[pid])
    {
      if (calc_visiting.find(child_pid) == calc_visiting.end())
      {
        calc_accum(child_pid);
        cpu += accumulated_cpu[child_pid];
        mem += accumulated_mem[child_pid];
      }
    }
    accumulated_cpu[pid] = cpu;
    accumulated_mem[pid] = mem;
    calc_visiting.erase(pid);
  };

  for (const auto &pair : new_procs)
  {
    int pid = pair.first;
    if (actual_parent[pid] == 0 || actual_parent[pid] == pid)
    {
      calc_accum(pid);
    }
  }

  std::set<int> remaining_pids;
  for (const auto &p : processes)
    remaining_pids.insert(p.pid);

  std::map<int, GtkTreeIter> current_iters;

  std::function<void(GtkTreeIter *, int)> traverse_and_update = [&](GtkTreeIter *parent_iter, int expected_parent_pid)
  {
    GtkTreeIter iter;
    gboolean valid;
    if (parent_iter == nullptr)
    {
      valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(process_store), &iter);
    }
    else
    {
      valid = gtk_tree_model_iter_children(GTK_TREE_MODEL(process_store), &iter, parent_iter);
    }

    while (valid)
    {
      int pid;
      gtk_tree_model_get(GTK_TREE_MODEL(process_store), &iter, COL_PID, &pid, -1);

      auto it = new_procs.find(pid);
      bool keep = false;

      if (it != new_procs.end())
      {
        int expected_ppid = actual_parent[pid];

        if (expected_ppid == expected_parent_pid)
        {
          keep = true;
        }
      }

      if (keep)
      {
        const auto &p = it->second;
        double disp_cpu = accumulated_cpu[pid];
        double disp_mem = accumulated_mem[pid];

        char cpu_buf[16];
        snprintf(cpu_buf, sizeof(cpu_buf), "%.1f%%", disp_cpu);
        std::string mem_str = std::to_string((unsigned long long)disp_mem / 1024) + " MB";
        std::string bg_color = get_color_for_usage(disp_cpu);
        std::string display_name = p.name + " (" + std::to_string(p.pid) + ")";

        gtk_tree_store_set(process_store, &iter, COL_NAME, display_name.c_str(), COL_USER, p.user.c_str(), COL_STATUS, p.state.c_str(),
                           COL_CPU, cpu_buf, COL_MEM, mem_str.c_str(), COL_BG_COLOR, bg_color.empty() ? NULL : bg_color.c_str(),
                           COL_FG_COLOR, bg_color.empty() ? NULL : "#000000", -1);

        current_iters[pid] = iter;
        remaining_pids.erase(pid);

        GtkTreeIter current_node_iter = iter;
        traverse_and_update(&current_node_iter, pid);

        valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(process_store), &iter);
      }
      else
      {
        valid = gtk_tree_store_remove(process_store, &iter);
      }
    }
  };

  traverse_and_update(nullptr, 0);

  std::set<int> visiting;
  std::function<void(int)> insert_node = [&](int pid)
  {
    if (current_iters.find(pid) != current_iters.end())
      return;
    if (remaining_pids.find(pid) == remaining_pids.end())
      return;
    if (visiting.find(pid) != visiting.end())
      return; // Cycle detected, prevent stack overflow!

    visiting.insert(pid);

    const auto &p = new_procs[pid];

    int expected_ppid = actual_parent[pid];

    GtkTreeIter parent_iter;

    bool has_parent = false;

    // Prevent self-parenting and resolve potential cycles safely
    if (expected_ppid != 0 && expected_ppid != pid)
    {
      insert_node(expected_ppid);
      if (current_iters.find(expected_ppid) != current_iters.end())
      {
        parent_iter = current_iters[expected_ppid];
        has_parent = true;
      }
    }

    GtkTreeIter new_iter;
    gtk_tree_store_append(process_store, &new_iter, has_parent ? &parent_iter : nullptr);

    double disp_cpu = accumulated_cpu[pid];
    double disp_mem = accumulated_mem[pid];

    char cpu_buf[16];
    snprintf(cpu_buf, sizeof(cpu_buf), "%.1f%%", disp_cpu);
    std::string mem_str = std::to_string((unsigned long long)disp_mem / 1024) + " MB";
    std::string bg_color = get_color_for_usage(disp_cpu);
    std::string display_name = p.name + " (" + std::to_string(p.pid) + ")";

    gtk_tree_store_set(process_store, &new_iter, COL_PID, p.pid, COL_NAME, display_name.c_str(), COL_USER, p.user.c_str(), COL_STATUS,
                       p.state.c_str(), COL_CPU, cpu_buf, COL_MEM, mem_str.c_str(), COL_BG_COLOR,
                       bg_color.empty() ? NULL : bg_color.c_str(), COL_FG_COLOR, bg_color.empty() ? NULL : "#000000", -1);

    current_iters[pid] = new_iter;
    remaining_pids.erase(pid);
    visiting.erase(pid);
  };

  std::vector<int> to_insert(remaining_pids.begin(), remaining_pids.end());
  for (int pid : to_insert)
  {
    insert_node(pid);
  }

  refresh_performance();
}

void UIManager::refresh_performance()
{
  double cpu_usage = process_manager.get_global_cpu_usage();
  GlobalMemData mem = process_manager.get_global_memory();

  cpu_history.erase(cpu_history.begin());
  cpu_history.push_back(cpu_usage);

  double total_gb = mem.total_kb / 1024.0 / 1024.0;
  // double free_gb = mem.free_kb / 1024.0 / 1024.0;
  double avail_gb = mem.available_kb / 1024.0 / 1024.0;
  double cached_gb = mem.cached_kb / 1024.0 / 1024.0;
  double swap_tot_gb = mem.swap_total_kb / 1024.0 / 1024.0;
  double swap_used_gb = (mem.swap_total_kb - mem.swap_free_kb) / 1024.0 / 1024.0;
  double used_gb = total_gb - avail_gb;
  double mem_percent = (used_gb / total_gb) * 100.0;

  char mem_text[128];
  snprintf(mem_text, sizeof(mem_text), "Memory\n%.1f GB / %.1f GB (%.0f%%)", used_gb, total_gb, mem_percent);
  gtk_label_set_text(GTK_LABEL(mem_usage_label), mem_text);

  char mem_details_text[512];
  snprintf(mem_details_text, sizeof(mem_details_text),
           "In Use: %.1f GB\nAvailable: %.1f GB\nCached: %.1f GB\nSwap: %.1f / %.1f "
           "GB\n\nSpeed: %s\nSlots used: %d of %d\nForm factor: %s\nType: %s%s",
           used_gb, avail_gb, cached_gb, swap_used_gb, swap_tot_gb, mem_hw_info.speed.c_str(), mem_hw_info.slots_used,
           mem_hw_info.slots_total, mem_hw_info.form_factor.c_str(), mem_hw_info.type.c_str(),
           mem_hw_info.slots_total == 0 ? " (Run as root for hardware info)" : "");
  gtk_label_set_text(GTK_LABEL(mem_details_label), mem_details_text);

  mem_history.erase(mem_history.begin());
  mem_history.push_back(mem_percent);

  // Update labels
  char label_buf[128];
  snprintf(label_buf, sizeof(label_buf), "CPU\n%.1f%%", cpu_usage);
  gtk_label_set_text(GTK_LABEL(cpu_usage_label), label_buf);

  long uptime_s = process_manager.get_system_uptime();
  long up_h = uptime_s / 3600;
  long up_m = (uptime_s % 3600) / 60;
  long up_s = uptime_s % 60;

  snprintf(label_buf, sizeof(label_buf), "Sockets: 1\nVirtual Processors: %d\nUptime: %02ld:%02ld:%02ld",
           process_manager.get_cpu_threads_count(), up_h, up_m, up_s);
  gtk_label_set_text(GTK_LABEL(cpu_details_label), label_buf);

  // Update headers in process tab
  if (col_cpu_ptr)
  {
    char hdr[64];
    snprintf(hdr, sizeof(hdr), "CPU (%.1f%%)", cpu_usage);
    gtk_tree_view_column_set_title(col_cpu_ptr, hdr);
  }
  if (col_mem_ptr)
  {
    char hdr[64];
    snprintf(hdr, sizeof(hdr), "Memory (%.0f%%)", mem_percent);
    gtk_tree_view_column_set_title(col_mem_ptr, hdr);
  }

  gtk_widget_queue_draw(perf_cpu_drawing_area);
  gtk_widget_queue_draw(perf_mem_drawing_area);
}

void UIManager::refresh_services()
{
  auto services = service_manager.get_services();
  gtk_list_store_clear(service_store);
  for (const auto &s : services)
  {
    GtkTreeIter iter;
    gtk_list_store_append(service_store, &iter);
    gtk_list_store_set(service_store, &iter, COL_SVC_NAME, s.name.c_str(), COL_SVC_LOAD, s.load_state.c_str(), COL_SVC_ACTIVE,
                       s.active_state.c_str(), COL_SVC_SUB, s.sub_state.c_str(), COL_SVC_DESC, s.description.c_str(), -1);
  }
}

gboolean UIManager::on_timer_tick(gpointer user_data)
{
  UIManager *ui = static_cast<UIManager *>(user_data);
  ui->refresh_processes();
  return G_SOURCE_CONTINUE;
}

gboolean UIManager::on_draw_cpu_graph(GtkWidget *widget, cairo_t *cr, gpointer user_data)
{
  UIManager *ui = static_cast<UIManager *>(user_data);
  guint width = gtk_widget_get_allocated_width(widget);
  guint height = gtk_widget_get_allocated_height(widget);

  cairo_set_source_rgb(cr, 0.7, 0.95, 0.7);
  cairo_paint(cr);

  cairo_set_source_rgb(cr, 0.5, 0.85, 0.5);
  cairo_set_line_width(cr, 1.0);
  for (int i = 1; i < 10; ++i)
  {
    double x = width * i / 10.0;
    cairo_move_to(cr, x, 0);
    cairo_line_to(cr, x, height);
  }
  for (int i = 1; i < 10; ++i)
  {
    double y = height * i / 10.0;
    cairo_move_to(cr, 0, y);
    cairo_line_to(cr, width, y);
  }
  cairo_stroke(cr);

  cairo_set_source_rgb(cr, 0.1, 0.4, 0.8);
  cairo_set_line_width(cr, 2.0);
  double step_x = (double)width / (ui->max_history - 1);
  cairo_move_to(cr, 0, height - (ui->cpu_history[0] / 100.0) * height);
  for (int i = 1; i < ui->max_history; ++i)
  {
    cairo_line_to(cr, i * step_x, height - (ui->cpu_history[i] / 100.0) * height);
  }
  cairo_stroke_preserve(cr);
  cairo_line_to(cr, width, height);
  cairo_line_to(cr, 0, height);
  cairo_set_source_rgba(cr, 0.1, 0.4, 0.8, 0.2);
  cairo_fill(cr);

  cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
  cairo_rectangle(cr, 0, 0, width, height);
  cairo_stroke(cr);

  // Draw Axis Labels
  cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
  cairo_set_font_size(cr, 11.0);
  cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);

  for (int i = 1; i <= 4; ++i)
  {
    double y = height - (height * i / 4.0);
    if (i == 4)
      y += 12;
    else
      y -= 2;
    cairo_move_to(cr, 5, y);
    cairo_show_text(cr, (std::to_string(i * 25) + "%").c_str());
  }
  cairo_move_to(cr, 5, height - 5);
  cairo_show_text(cr, "60s");
  cairo_move_to(cr, width - 20, height - 5);
  cairo_show_text(cr, "0s");

  return FALSE;
}

gboolean UIManager::on_draw_mem_graph(GtkWidget *widget, cairo_t *cr, gpointer user_data)
{
  UIManager *ui = static_cast<UIManager *>(user_data);
  guint width = gtk_widget_get_allocated_width(widget);
  guint height = gtk_widget_get_allocated_height(widget);

  cairo_set_source_rgb(cr, 0.7, 0.95, 0.7);
  cairo_paint(cr);

  cairo_set_source_rgb(cr, 0.5, 0.85, 0.5);
  cairo_set_line_width(cr, 1.0);
  for (int i = 1; i < 10; ++i)
  {
    double x = width * i / 10.0;
    cairo_move_to(cr, x, 0);
    cairo_line_to(cr, x, height);
  }
  for (int i = 1; i < 10; ++i)
  {
    double y = height * i / 10.0;
    cairo_move_to(cr, 0, y);
    cairo_line_to(cr, width, y);
  }
  cairo_stroke(cr);

  cairo_set_source_rgb(cr, 0.5, 0.1, 0.8);
  cairo_set_line_width(cr, 2.0);
  double step_x = (double)width / (ui->max_history - 1);
  cairo_move_to(cr, 0, height - (ui->mem_history[0] / 100.0) * height);
  for (int i = 1; i < ui->max_history; ++i)
  {
    cairo_line_to(cr, i * step_x, height - (ui->mem_history[i] / 100.0) * height);
  }
  cairo_stroke_preserve(cr);
  cairo_line_to(cr, width, height);
  cairo_line_to(cr, 0, height);
  cairo_set_source_rgba(cr, 0.5, 0.1, 0.8, 0.2);
  cairo_fill(cr);

  cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
  cairo_rectangle(cr, 0, 0, width, height);
  cairo_stroke(cr);

  // Draw Axis Labels
  cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
  cairo_set_font_size(cr, 11.0);
  cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);

  GlobalMemData mem_data = ui->process_manager.get_global_memory();
  double total_gb = mem_data.total_kb / 1024.0 / 1024.0;

  for (int i = 1; i <= 4; ++i)
  {
    double y = height - (height * i / 4.0);
    if (i == 4)
      y += 12;
    else
      y -= 2;
    cairo_move_to(cr, 5, y);

    double val_gb = total_gb * i / 4.0;
    char buf[32];
    snprintf(buf, sizeof(buf), "%.1f GB", val_gb);
    cairo_show_text(cr, buf);
  }
  cairo_move_to(cr, 5, height - 5);
  cairo_show_text(cr, "60s");
  cairo_move_to(cr, width - 20, height - 5);
  cairo_show_text(cr, "0s");

  return FALSE;
}

void UIManager::on_kill_process_clicked(GtkWidget *widget, gpointer user_data)
{
  (void)widget;
  UIManager *ui = static_cast<UIManager *>(user_data);
  GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(ui->process_treeview));
  GtkTreeModel *model;
  GtkTreeIter iter;
  if (gtk_tree_selection_get_selected(selection, &model, &iter))
  {
    int pid;
    gtk_tree_model_get(model, &iter, COL_PID, &pid, -1);
    if (pid > 0 && ui->process_manager.kill_process(pid))
    {
      ui->refresh_processes();
    }
  }
}

gboolean UIManager::on_process_button_press(GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
  UIManager *ui = static_cast<UIManager *>(user_data);
  if (event->type == GDK_BUTTON_PRESS && event->button == 3)
  {
    GtkTreePath *path;
    if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(widget), (gint)event->x, (gint)event->y, &path, NULL, NULL, NULL))
    {
      gtk_tree_selection_select_path(gtk_tree_view_get_selection(GTK_TREE_VIEW(widget)), path);
      gtk_menu_popup_at_pointer(GTK_MENU(ui->process_context_menu), (GdkEvent *)event);
      gtk_tree_path_free(path);
      return TRUE;
    }
  }
  return FALSE;
}

void UIManager::on_search_changed(GtkSearchEntry *entry, gpointer data)
{
  UIManager *ui = static_cast<UIManager *>(data);
  const char *text = gtk_entry_get_text(GTK_ENTRY(entry));
  ui->search_query = text ? text : "";

  // Convert query to lower case
  std::transform(ui->search_query.begin(), ui->search_query.end(), ui->search_query.begin(), ::tolower);

  if (ui->process_filter)
    gtk_tree_model_filter_refilter(GTK_TREE_MODEL_FILTER(ui->process_filter));
  if (ui->service_filter)
    gtk_tree_model_filter_refilter(GTK_TREE_MODEL_FILTER(ui->service_filter));
}

gboolean UIManager::on_window_key_press(GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{
  (void)widget;
  UIManager *ui = static_cast<UIManager *>(user_data);

  if (ui->search_entry)
  {
    return gtk_search_entry_handle_event(GTK_SEARCH_ENTRY(ui->search_entry), (GdkEvent *)event);
  }
  return FALSE;
}

gboolean UIManager::on_search_filter_visible(GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
  UIManager *ui = static_cast<UIManager *>(data);
  if (ui->search_query.empty())
    return TRUE;

  // Helper lambda to check if a single iter matches the query
  auto node_matches = [&](GtkTreeIter *node_iter) -> bool
  {
    char *name;
    gtk_tree_model_get(model, node_iter, COL_NAME, &name, -1);
    if (!name)
      return false;
    std::string name_str(name);
    g_free(name);
    std::transform(name_str.begin(), name_str.end(), name_str.begin(), ::tolower);
    return name_str.find(ui->search_query) != std::string::npos;
  };

  // 1. Check self and ancestors
  GtkTreeIter current = *iter;
  while (true)
  {
    if (node_matches(&current))
      return TRUE;

    GtkTreeIter parent;
    if (!gtk_tree_model_iter_parent(model, &parent, &current))
    {
      break;
    }
    current = parent;
  }

  // 2. Check children recursively
  std::function<bool(GtkTreeIter *)> check_children = [&](GtkTreeIter *parent_iter) -> bool
  {
    GtkTreeIter child;
    if (gtk_tree_model_iter_children(model, &child, parent_iter))
    {
      do
      {
        if (node_matches(&child))
          return true;
        if (check_children(&child))
          return true;
      } while (gtk_tree_model_iter_next(model, &child));
    }
    return false;
  };

  if (check_children(iter))
    return TRUE;

  return FALSE;
}

gboolean UIManager::on_service_search_filter_visible(GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
  UIManager *ui = static_cast<UIManager *>(data);
  if (ui->search_query.empty())
    return TRUE;

  char *name;
  gtk_tree_model_get(model, iter, COL_SVC_NAME, &name, -1);
  if (!name)
    return FALSE;

  std::string name_str(name);
  g_free(name);

  std::transform(name_str.begin(), name_str.end(), name_str.begin(), ::tolower);

  return name_str.find(ui->search_query) != std::string::npos;
}

void UIManager::on_context_menu_end_task(GtkMenuItem *item, gpointer user_data)
{
  (void)item;
  UIManager *ui = static_cast<UIManager *>(user_data);
  GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(ui->process_treeview));
  GtkTreeModel *model;
  GtkTreeIter iter;
  if (gtk_tree_selection_get_selected(selection, &model, &iter))
  {
    int pid;
    gtk_tree_model_get(model, &iter, COL_PID, &pid, -1);
    if (pid > 0 && ui->process_manager.kill_process(pid))
    {
      ui->refresh_processes();
    }
  }
}

void UIManager::on_context_menu_open_file_location(GtkMenuItem *item, gpointer user_data)
{
  (void)item;
  UIManager *ui = static_cast<UIManager *>(user_data);
  GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(ui->process_treeview));
  GtkTreeModel *model;
  GtkTreeIter iter;
  if (gtk_tree_selection_get_selected(selection, &model, &iter))
  {
    int pid;
    gtk_tree_model_get(model, &iter, COL_PID, &pid, -1);
    if (pid > 0)
    {
      char path[1024];
      std::string link = "/proc/" + std::to_string(pid) + "/exe";
      ssize_t len = readlink(link.c_str(), path, sizeof(path) - 1);
      if (len != -1)
      {
        path[len] = '\0';
        std::string full_path(path);
        size_t pos = full_path.find_last_of('/');
        if (pos != std::string::npos)
        {
          std::string dir = full_path.substr(0, pos);
          std::string cmd = "xdg-open '" + dir + "' &";
          system(cmd.c_str());
        }
      }
      else
      {
        std::string cmd = "xdg-open /proc/" + std::to_string(pid) + " &";
        system(cmd.c_str());
      }
    }
  }
}

void UIManager::on_perf_sidebar_row_activated(GtkListBox *box, GtkListBoxRow *row, gpointer user_data)
{
  (void)box;
  UIManager *ui = static_cast<UIManager *>(user_data);
  int index = gtk_list_box_row_get_index(row);
  if (index == 0)
    gtk_stack_set_visible_child_name(GTK_STACK(ui->perf_stack), "cpu");
  else if (index == 1)
    gtk_stack_set_visible_child_name(GTK_STACK(ui->perf_stack), "mem");
}

void UIManager::on_start_service_clicked(GtkWidget *widget, gpointer user_data)
{
  (void)widget;
  UIManager *ui = static_cast<UIManager *>(user_data);
  GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(ui->service_treeview));
  GtkTreeModel *model;
  GtkTreeIter iter;
  if (gtk_tree_selection_get_selected(selection, &model, &iter))
  {
    gchar *name;
    gtk_tree_model_get(model, &iter, COL_SVC_NAME, &name, -1);
    ui->service_manager.start_service(name);
    g_free(name);
    ui->refresh_services();
  }
}

void UIManager::on_stop_service_clicked(GtkWidget *widget, gpointer user_data)
{
  (void)widget;
  UIManager *ui = static_cast<UIManager *>(user_data);
  GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(ui->service_treeview));
  GtkTreeModel *model;
  GtkTreeIter iter;
  if (gtk_tree_selection_get_selected(selection, &model, &iter))
  {
    gchar *name;
    gtk_tree_model_get(model, &iter, COL_SVC_NAME, &name, -1);
    ui->service_manager.stop_service(name);
    g_free(name);
    ui->refresh_services();
  }
}

void UIManager::on_restart_service_clicked(GtkWidget *widget, gpointer user_data)
{
  (void)widget;
  UIManager *ui = static_cast<UIManager *>(user_data);
  GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(ui->service_treeview));
  GtkTreeModel *model;
  GtkTreeIter iter;
  if (gtk_tree_selection_get_selected(selection, &model, &iter))
  {
    gchar *name;
    gtk_tree_model_get(model, &iter, COL_SVC_NAME, &name, -1);
    ui->service_manager.restart_service(name);
    g_free(name);
    ui->refresh_services();
  }
}
