#include "ui.h"

#include <cstdio>
#include <fcntl.h>
#include <unistd.h>

int enforce_single_instance()
{
  std::string lock_path;
  const char* xdg_runtime_dir = getenv("XDG_RUNTIME_DIR");
  if (xdg_runtime_dir != nullptr && xdg_runtime_dir[0] != '\0') {
    lock_path = std::string(xdg_runtime_dir) + "/nabdhu.lock";
  }
  else {
    lock_path = "/tmp/nabdhu_" + std::to_string(getuid()) + ".lock";
  }

  int fd = open(lock_path.c_str(), O_RDWR | O_CREAT | O_NOFOLLOW, 0600);
  if (fd < 0) {
    return -1;
  }

  struct flock fl;
  fl.l_type   = F_WRLCK;
  fl.l_whence = SEEK_SET;
  fl.l_start  = 0;
  fl.l_len    = 0;

  if (fcntl(fd, F_SETLK, &fl) < 0) {
    close(fd);
    return -1;
  }

  return 0;
}

int main(int argc, char* argv[])
{
  if (enforce_single_instance() < 0) {
    return 1;
  }

  UIManager app;
  app.run(argc, argv);
  return 0;
}
