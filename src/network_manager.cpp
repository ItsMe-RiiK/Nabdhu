#include "network_manager.h"

#include <arpa/inet.h>
#include <chrono>
#include <fstream>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <sstream>
#include <algorithm>
#include <unordered_map>

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

  static auto last_ifaddr_time = std::chrono::steady_clock::time_point::min();
  static std::unordered_map<std::string, std::string> cached_ips;
  bool refresh_ips = (std::chrono::duration_cast<std::chrono::seconds>(now - last_ifaddr_time).count() >= 30);

  if (refresh_ips) {
      struct ifaddrs *ifaddr, *ifa;
      if (getifaddrs(&ifaddr) != -1) {
          cached_ips.clear();
          for (ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
              if (ifa->ifa_addr == nullptr) continue;
              if (ifa->ifa_addr->sa_family == AF_INET) {
                  char ip_buf[INET_ADDRSTRLEN];
                  struct sockaddr_in *sa = (struct sockaddr_in *)ifa->ifa_addr;
                  inet_ntop(AF_INET, &(sa->sin_addr), ip_buf, INET_ADDRSTRLEN);
                  cached_ips[ifa->ifa_name] = ip_buf;
              }
          }
          freeifaddrs(ifaddr);
          last_ifaddr_time = now;
      }
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
      
      auto it = cached_ips.find(name);
      if (it != cached_ips.end()) {
          info.ip_address = it->second;
      } else {
          info.ip_address = "";
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

  std::sort(current_info.begin(), current_info.end(), [](const NetworkInterfaceInfo &a, const NetworkInterfaceInfo &b) {
    return a.name < b.name;
  });

  prev_info = current_info;
  prev_time = current_time;
  first_run = false;

  return current_info;
}
