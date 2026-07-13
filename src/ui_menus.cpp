#include "ui.h"

void UIManager::perform_action() {
    show_context_menu = false;
    std::vector<std::string> ctx = (tab_selected == 0) ? std::vector<std::string>{"End Task", "Cancel"} : std::vector<std::string>{"End Service", "Cancel"};
    if (context_menu_selected < 0 || context_menu_selected >= (int)ctx.size()) return;
    std::string action = ctx[context_menu_selected];
    if (action == "Cancel") return;

    if (tab_selected == 0 && proc_selected >= 0 && proc_selected < (int)filtered_procs.size()) {
        if (action == "End Task") process_manager.kill_process(filtered_procs[proc_selected]->pid);
    } else if (tab_selected == 1 && svc_selected >= 0 && svc_selected < (int)filtered_svcs.size()) {
        if (action == "End Service") service_manager.stop_service(filtered_svcs[svc_selected]->name);
    }
}
