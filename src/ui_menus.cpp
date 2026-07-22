#include "ui.h"

#include <cstdlib>

void UIManager::perform_action()
{
  show_context_menu = false;
  std::vector<std::string> ctx =
    (tab_selected == 0) ? std::vector<std::string>{"End Task", "Open Location", "Cancel"}
                        : std::vector<std::string>{"End Service", "Open Location", "Cancel"};
  if (context_menu_selected < 0 || context_menu_selected >= (int) ctx.size())
    return;
  std::string action = ctx[context_menu_selected];
  if (action == "Cancel")
    return;

  if (tab_selected == 0 && proc_selected >= 0 && proc_selected < (int) filtered_procs.size()) {
    int pid = filtered_procs[proc_selected]->pid;
    if (action == "End Task")
      process_manager.kill_process(pid);
    else if (action == "Open Location") {
      std::string cmd =
        "xdg-open $(dirname $(readlink -f /proc/" + std::to_string(pid) + "/exe)) 2>/dev/null &";
      system(cmd.c_str());
    }
  }
  else if (tab_selected == 1 && svc_selected >= 0 && svc_selected < (int) filtered_svcs.size()) {
    std::string sname = filtered_svcs[svc_selected]->name;
    if (action == "End Service")
      service_manager.stop_service(sname);
    else if (action == "Open Location") {
      std::string cmd = "xdg-open $(dirname $(systemctl show -p FragmentPath " + sname
                      + " | cut -d= -f2)) 2>/dev/null &";
      system(cmd.c_str());
    }
  }
}