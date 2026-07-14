#include "network_manager.h"

#include <algorithm>
#include <arpa/inet.h>
#include <chrono>
#include <cstring>
#include <fcntl.h>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <unistd.h>
#include <unordered_map>

NetworkManager::NetworkManager() : prev_time(0.0), first_run(true) {}

NetworkManager::~NetworkManager() {}

std::vector<NetworkInterfaceInfo> NetworkManager::get_network_info()
{
  std::vector<NetworkInterfaceInfo> current_info;
  int fd = open("/proc/net/dev", O_RDONLY);
  if (fd == -1)
    return current_info;

  auto now = std::chrono::steady_clock::now();
  double current_time = std::chrono::duration<double>(now.time_since_epoch()).count();
  double time_diff = current_time - prev_time;

  static auto last_ifaddr_time = std::chrono::steady_clock::time_point::min();
  static std::unordered_map<std::string, std::string> cached_ips;
  bool refresh_ips = (std::chrono::duration_cast<std::chrono::seconds>(now - last_ifaddr_time).count() >= 30);

  if (refresh_ips)
  {
    struct ifaddrs *ifaddr, *ifa;
    if (getifaddrs(&ifaddr) != -1)
    {
      cached_ips.clear();
      for (ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next)
      {
        if (ifa->ifa_addr == nullptr)
          continue;
        if (ifa->ifa_addr->sa_family == AF_INET)
        {
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

  char buf[4096];
  ssize_t bytes = read(fd, buf, sizeof(buf) - 1);
  close(fd);

  if (bytes > 0)
  {
    buf[bytes] = '\0';
    char *ptr = buf;

    // skip first two lines
    for (int i = 0; i < 2; i++)
    {
      while (*ptr && *ptr != '\n')
        ptr++;
      if (*ptr == '\n')
        ptr++;
    }

    while (*ptr)
    {
      while (*ptr == ' ' || *ptr == '\t')
        ptr++;
      char *colon = strchr(ptr, ':');
      if (colon)
      {
        *colon = '\0';
        std::string name(ptr);
        ptr = colon + 1;

        if (name != "lo")
        {
          unsigned long long rx_bytes = strtoull(ptr, &ptr, 10);
          for (int i = 0; i < 7; i++)
            strtoull(ptr, &ptr, 10); // skip 7 fields
          unsigned long long tx_bytes = strtoull(ptr, &ptr, 10);

          NetworkInterfaceInfo info;
          info.name = name;
          info.rx_bytes = rx_bytes;
          info.tx_bytes = tx_bytes;
          info.rx_speed_kbps = 0.0;
          info.tx_speed_kbps = 0.0;

          auto it = cached_ips.find(name);
          if (it != cached_ips.end())
            info.ip_address = it->second;
          else
            info.ip_address = "";

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
      while (*ptr && *ptr != '\n')
        ptr++;
      if (*ptr == '\n')
        ptr++;
    }
  }

  std::sort(
      current_info.begin(), current_info.end(), [](const NetworkInterfaceInfo &a, const NetworkInterfaceInfo &b) { return a.name < b.name; }
  );

  prev_info = current_info;
  prev_time = current_time;
  first_run = false;

  return current_info;
}
