#pragma once

#include <string>
#include <vector>

struct ServiceInfo
{
  std::string name;
  std::string load_state;
  std::string active_state;
  std::string sub_state;
  std::string description;
};

class ServiceManager
{
public:
  ServiceManager();
  ~ServiceManager();

  std::vector<ServiceInfo> get_services();

  bool start_service(const std::string &name);
  bool stop_service(const std::string &name);
  bool restart_service(const std::string &name);
};
