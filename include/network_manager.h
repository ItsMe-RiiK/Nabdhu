#pragma once
#include <string>
#include <vector>

struct NetworkInterfaceInfo
{
  std::string        name;
  unsigned long long rx_bytes;
  unsigned long long tx_bytes;
  double             rx_speed_kbps;
  double             tx_speed_kbps;
  std::string        ip_address;
};

class NetworkManager
{
public:
  NetworkManager();
  ~NetworkManager();

  std::vector<NetworkInterfaceInfo> get_network_info();

private:
  std::vector<NetworkInterfaceInfo> prev_info;
  double                            prev_time;
  bool                              first_run;
};
