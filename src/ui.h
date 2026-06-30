#pragma once

#include "process_manager.h"
#include "service_manager.h"
#include <gtk/gtk.h>
#include <string>
#include <vector>

class UIManager {
public:
  UIManager();
  ~UIManager();

  void run(int argc, char *argv[]);

private:
  ProcessManager process_manager;
  ServiceManager service_manager;

  GtkWidget *window;
  GtkWidget *notebook;

  // Process Tab
  GtkWidget *process_treeview;
  GtkTreeStore *process_store;
  GtkTreeModel *process_filter;
  GtkWidget *process_context_menu;

  GtkWidget *search_entry;
  std::string search_query;

  GtkTreeViewColumn *col_cpu_ptr = nullptr;
  GtkTreeViewColumn *col_mem_ptr = nullptr;

  // Performance Tab
  GtkWidget *perf_cpu_drawing_area;
  GtkWidget *perf_mem_drawing_area;

  // Performance Tab Labels
  GtkWidget *cpu_usage_label;
  GtkWidget *cpu_details_label;
  GtkWidget *mem_usage_label;
  GtkWidget *mem_details_label;

  MemHardwareInfo mem_hw_info;

  std::vector<double> cpu_history;
  std::vector<double> mem_history;
  const int max_history = 60;

  int current_perf_tab; // 0 = CPU, 1 = Memory

  // Service Tab
  GtkWidget *service_treeview;
  GtkListStore *service_store;
  GtkTreeModel *service_filter;

  void setup_ui();
  void setup_process_tab(GtkWidget *parent);
  void setup_performance_tab(GtkWidget *parent);
  void setup_service_tab(GtkWidget *parent);

  static gboolean on_window_key_press(GtkWidget *widget, GdkEventKey *event,
                                      gpointer user_data);

  void refresh_processes();
  void refresh_performance();
  void refresh_services();

  std::string get_color_for_usage(double usage);

  // Callbacks
  static gboolean on_timer_tick(gpointer user_data);

  // Graph drawing callbacks
  static gboolean on_draw_cpu_graph(GtkWidget *widget, cairo_t *cr,
                                    gpointer user_data);
  static gboolean on_draw_mem_graph(GtkWidget *widget, cairo_t *cr,
                                    gpointer user_data);

  // Process Tab Callbacks
  static void on_kill_process_clicked(GtkWidget *widget, gpointer user_data);
  static gboolean on_process_button_press(GtkWidget *widget,
                                          GdkEventButton *event,
                                          gpointer user_data);

  static void on_search_changed(GtkSearchEntry *entry, gpointer data);
  static gboolean on_search_filter_visible(GtkTreeModel *model,
                                           GtkTreeIter *iter, gpointer data);
  static gboolean on_service_search_filter_visible(GtkTreeModel *model,
                                                   GtkTreeIter *iter,
                                                   gpointer data);
  static void on_context_menu_end_task(GtkMenuItem *item, gpointer user_data);
  static void on_context_menu_open_file_location(GtkMenuItem *item,
                                                 gpointer user_data);

  // Service Tab Callbacks
  static void on_start_service_clicked(GtkWidget *widget, gpointer user_data);
  static void on_stop_service_clicked(GtkWidget *widget, gpointer user_data);
  static void on_restart_service_clicked(GtkWidget *widget, gpointer user_data);

  // Perf Tab Sidebar Callbacks
  static void on_perf_sidebar_row_activated(GtkListBox *box, GtkListBoxRow *row,
                                            gpointer user_data);

  // GtkStack for graphs
  GtkWidget *perf_stack;
};
