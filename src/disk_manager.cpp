#include "disk_manager.h"

#include <fstream>
#include <sstream>
#include <sys/statvfs.h>

std::vector<DiskInfo> DiskManager::get_disk_info()
{
  std::vector<DiskInfo> disks;
  std::ifstream file("/proc/mounts");
  if (!file.is_open())
    return disks;

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
  return disks;
}
