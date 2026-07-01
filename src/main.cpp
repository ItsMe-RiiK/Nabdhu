#include "ui.h"
#include <fcntl.h>
#include <unistd.h>

int enforce_single_instance() {
  int fd = open("/tmp/my_task_manager.lock", O_RDWR | O_CREAT, 0666);
  if (fd < 0) {
    return -1;
  }

  struct flock fl;
  fl.l_type = F_WRLCK;
  fl.l_whence = SEEK_SET;
  fl.l_start = 0;
  fl.l_len = 0;

  if (fcntl(fd, F_SETLK, &fl) < 0) {
    close(fd);
    return -1;
  }

  return 0;
}

int main(int argc, char *argv[]) {
  if (enforce_single_instance() < 0) {
    return 1;
  }

  UIManager app;
  app.run(argc, argv);
  return 0;
}
