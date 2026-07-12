#pragma once
#include <string>
#include <vector>

struct DiskInfo
{
  std::string mount_point;
  unsigned long long total_bytes;
  unsigned long long free_bytes;
  unsigned long long used_bytes;
  double used_percent;
};

class DiskManager
{
public:
  std::vector<DiskInfo> get_disk_info();
};
