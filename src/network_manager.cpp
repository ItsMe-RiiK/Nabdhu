#include "network_manager.h"

#include <arpa/inet.h>
#include <chrono>
#include <fstream>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <sstream>

NetworkManager::NetworkManager() : prev_time(0.0), first_run(true) {}

NetworkManager::~NetworkManager() {}

std::vector<NetworkInterfaceInfo> NetworkManager::get_network_info()
{
  std::vector<NetworkInterfaceInfo> current_info;
  std::ifstream file("/proc/net/dev");
  if (!file.is_open())
    return current_info;

  auto now = std::chrono::steady_clock::now();
  double current_time = std::chrono::duration<double>(now.time_since_epoch()).count();
  double time_diff = current_time - prev_time;

  std::string line;
  std::getline(file, line);
  std::getline(file, line);

  struct ifaddrs *ifaddr, *ifa;
  if (getifaddrs(&ifaddr) == -1)
  {
    ifaddr = nullptr;
  }

  while (std::getline(file, line))
  {
    size_t colon_pos = line.find(':');
    if (colon_pos != std::string::npos)
    {
      std::string name = line.substr(0, colon_pos);
      name.erase(0, name.find_first_not_of(" \t"));
      name.erase(name.find_last_not_of(" \t") + 1);

      if (name == "lo")
        continue;

      std::string stats = line.substr(colon_pos + 1);
      std::stringstream ss(stats);
      unsigned long long rx_bytes, tx_bytes, dummy;

      ss >> rx_bytes >> dummy >> dummy >> dummy >> dummy >> dummy >> dummy >> dummy >> tx_bytes;

      NetworkInterfaceInfo info;
      info.name = name;
      info.rx_bytes = rx_bytes;
      info.tx_bytes = tx_bytes;
      info.rx_speed_kbps = 0.0;
      info.tx_speed_kbps = 0.0;
      info.ip_address = "";

      if (ifaddr != nullptr)
      {
        for (ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next)
        {
          if (ifa->ifa_addr == nullptr)
            continue;
          if (ifa->ifa_addr->sa_family == AF_INET && std::string(ifa->ifa_name) == name)
          {
            char ip_buf[INET_ADDRSTRLEN];
            struct sockaddr_in *sa = (struct sockaddr_in *)ifa->ifa_addr;
            inet_ntop(AF_INET, &(sa->sin_addr), ip_buf, INET_ADDRSTRLEN);
            info.ip_address = ip_buf;
            break;
          }
        }
      }

      if (!first_run && time_diff > 0)
      {
        for (const auto &p : prev_info)
        {
          if (p.name == name)
          {
            info.rx_speed_kbps = ((rx_bytes - p.rx_bytes) / 1024.0) / time_diff;
            info.tx_speed_kbps = ((tx_bytes - p.tx_bytes) / 1024.0) / time_diff;
            break;
          }
        }
      }
      current_info.push_back(info);
    }
  }

  if (ifaddr != nullptr)
  {
    freeifaddrs(ifaddr);
  }

  prev_info = current_info;
  prev_time = current_time;
  first_run = false;

  return current_info;
}
