#include "service_manager.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <sstream>

ServiceManager::ServiceManager() {}

ServiceManager::~ServiceManager() {}

#include <chrono>

static std::vector<ServiceInfo> cached_services;
static auto last_service_time = std::chrono::steady_clock::time_point::min();

std::vector<ServiceInfo> ServiceManager::get_services()
{
  auto now = std::chrono::steady_clock::now();
  if (last_service_time != std::chrono::steady_clock::time_point::min() &&
      std::chrono::duration_cast<std::chrono::seconds>(now - last_service_time).count() < 10) {
      return cached_services;
  }
  
  std::vector<ServiceInfo> services;

  std::string cmd = "systemctl list-units --type=service --all --no-pager "
                    "--no-legend --plain";

  std::array<char, 256> buffer;
  std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
  if (!pipe)
  {
    return services;
  }

  std::string result;
  while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr)
  {
    std::string line = buffer.data();
    if (line.empty())
      continue;

    std::stringstream ss(line);
    ServiceInfo info;

    ss >> info.name >> info.load_state >> info.active_state >> info.sub_state;

    std::string desc;
    std::getline(ss, desc);
    if (!desc.empty())
    {
      size_t start = desc.find_first_not_of(" \t");
      if (start != std::string::npos)
      {
        info.description = desc.substr(start);
      }
    }

    if (!info.name.empty() && info.name.find(".service") != std::string::npos)
    {
      services.push_back(info);
    }
  }

  cached_services = services;
  last_service_time = now;
  return services;
}

bool ServiceManager::start_service(const std::string &name)
{
  std::string cmd = "pkexec systemctl start " + name;
  return system(cmd.c_str()) == 0;
}

bool ServiceManager::stop_service(const std::string &name)
{
  std::string cmd = "pkexec systemctl stop " + name;
  return system(cmd.c_str()) == 0;
}

bool ServiceManager::restart_service(const std::string &name)
{
  std::string cmd = "pkexec systemctl restart " + name;
  return system(cmd.c_str()) == 0;
}
