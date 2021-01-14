#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <wait.h>
#include "service.h"

#define MAX_CONN 1

extern void service_init(int fd);
extern int service_do(int fd);
extern void service_exit(int fd);

void find_new_fdmax(int *fdmax, fd_set *master);

int main(int argc, char *argv[]) {
  struct addrinfo hints;
  struct addrinfo *result, *rp;
  int listener, s;
  int fdmax;
  int newfd;        // newly accept()ed socket descriptor

  fd_set master;
  fd_set read_fds;
  FD_ZERO(&master);
  FD_ZERO(&read_fds);



  char *port = "8080";
  if (argc == 2) {
    port = argv[1];
  }

  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = AI_PASSIVE;
  hints.ai_protocol = 0;
  hints.ai_canonname = NULL;
  hints.ai_addr = NULL;
  hints.ai_next = NULL;

  s = getaddrinfo(NULL, port, &hints, &result);
  if (s != 0) {
    exit(EXIT_FAILURE);
  }

  for (rp = result; rp != NULL; rp = rp->ai_next) {
    listener = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &(int) {1}, sizeof(int)) < 0) {
      exit(EXIT_FAILURE);
    }
    if (listener == -1) {
      continue;
    }
    // bind the socket to a local port
    if (bind(listener, rp->ai_addr, rp->ai_addrlen) == 0) {
      break;                  /* Success */
    }
    close(listener);
  }

  // No address worked
  if (rp == NULL) {
    exit(EXIT_FAILURE);
  }
  if (listen(listener, MAX_CONN) < 0) {
    exit(EXIT_FAILURE);
  }

  /* Wait for incoming connection requests */
  FD_SET(listener, &master);
  fdmax = listener;

  while (1) {
    read_fds = master;
    if (select(fdmax + 1, &read_fds, NULL, NULL, NULL) == -1) {
      perror("select");
      exit(EXIT_FAILURE);
    }

    // run through the existing connections looking for data to read
    for (int i = 0; i <= fdmax; i++) {
      if (FD_ISSET(i, &read_fds)) {
        if (i == listener) {
          // handle new connections
          newfd = accept(listener, rp->ai_addr, &rp->ai_addrlen);

          if (newfd == -1) {
            perror("accept");
            exit(EXIT_FAILURE);
          } else {
            FD_SET(newfd, &master); // add to master set
            if (newfd > fdmax) {    // keep track of the max
              fdmax = newfd;
            }
            service_init(newfd);
          }
        } else {
          // handle data_read from a client
          int data_read = service_do(i);
          if(data_read == 0) {
            service_exit(i);
            close(i);
            FD_CLR(i, &master);
            if(i == fdmax) {
              find_new_fdmax(&fdmax, &master);
            }
          }
        }
      }
    }
  }
  close(listener);
  freeaddrinfo(result);
  return EXIT_SUCCESS;
}

void find_new_fdmax(int *fdmax, fd_set *master) {
  int max = 0;
  for (int j = 0; j < (*fdmax); j++) {
    if (FD_ISSET(j, master) && // get new maximum
        (j > max)) {
      max = j;
    }
  }
  (*fdmax) = max;
}
