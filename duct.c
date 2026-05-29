#include <arpa/inet.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

enum { BUFSZ = 65536 };

int main(int argc, char **argv) {
  int s, fd;
  ssize_t n;
  unsigned char buf[BUFSZ];
  struct sockaddr_in addr = {.sin_family = AF_INET};

  if (argc < 2 || argc > 4) {
    dprintf(2, "usage: duct port            (receive to stdout)\n"
               "       duct host port       (send from stdin)\n"
               "       duct host port file  (send from file)\n");
    return 1;
  }

  s = socket(AF_INET, SOCK_STREAM, 0);

  if (argc == 2) {
    /* receive mode: listen on port, write to stdout */
    addr.sin_port = htons(atoi(argv[1]));
    addr.sin_addr.s_addr = INADDR_ANY;
    bind(s, (struct sockaddr *)&addr, sizeof(addr));
    listen(s, 1);
    s = accept(s, NULL, NULL);
    while ((n = read(s, buf, BUFSZ)) > 0)
      write(STDOUT_FILENO, buf, n);
  } else {
    /* send mode: connect to host:port, read from stdin or file */
    addr.sin_port = htons(atoi(argv[2]));
    inet_pton(AF_INET, argv[1], &addr.sin_addr);
    connect(s, (struct sockaddr *)&addr, sizeof(addr));
    if (argc == 4)
      fd = open(argv[3], O_RDONLY);
    else
      fd = STDIN_FILENO;
    while ((n = read(fd, buf, BUFSZ)) > 0)
      write(s, buf, n);
    if (argc == 4) close(fd);
  }

  return 0;
}
