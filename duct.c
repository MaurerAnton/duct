/*
 * duct — move raw bytes over TCP
 * Copyright (C) 2026  Anton Maurer
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

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
  struct sockaddr_in addr;
  if (argc < 2 || argc > 4) {
    fprintf(stderr, "usage: duct port            (receive to stdout)\n"
                    "       duct host port       (send from stdin)\n"
                    "       duct host port file  (send from file)\n");
    return 1;
  }
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
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
