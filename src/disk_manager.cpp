#include "disk_manager.h"

#include <chrono>
#include <fstream>
#include <sstream>
#include <sys/statvfs.h>

std::vector<DiskInfo> DiskManager::get_disk_info()
{
  static std::vector<DiskInfo> cached_disks;
  static auto last_disk_time = std::chrono::steady_clock::now();
  static bool first_run = true;
  auto now = std::chrono::steady_clock::now();
  
  if (!first_run && std::chrono::duration_cast<std::chrono::seconds>(now - last_disk_time).count() < 15) {
      return cached_disks;
  }
  first_run = false;

  std::vector<DiskInfo> disks;
  std::ifstream file("/proc/mounts");
  if (!file.is_open())
    return cached_disks;

  std::string line;
  while (std::getline(file, line))
  {
    std::stringstream ss(line);
    std::string device, mount_point, fs_type;
    ss >> device >> mount_point >> fs_type;

    // Filter for physical drives
    if (device.rfind("/dev/", 0) == 0 && device.rfind("/dev/loop", 0) != 0)
    {
      struct statvfs stat;
      if (statvfs(mount_point.c_str(), &stat) == 0)
      {
        DiskInfo info;
        info.mount_point = mount_point;
        info.total_bytes = stat.f_blocks * stat.f_frsize;
        info.free_bytes = stat.f_bfree * stat.f_frsize;
        info.used_bytes = info.total_bytes - info.free_bytes;
        if (info.total_bytes > 0)
        {
          info.used_percent = (info.used_bytes / (double)info.total_bytes) * 100.0;
        }
        else
        {
          info.used_percent = 0.0;
        }

        bool duplicate = false;
        for (const auto &d : disks)
        {
          if (d.mount_point == mount_point)
          {
            duplicate = true;
            break;
          }
        }
        if (!duplicate)
        {
          disks.push_back(info);
        }
      }
    }
  }
  
  cached_disks = disks;
  last_disk_time = now;
  return disks;
}
