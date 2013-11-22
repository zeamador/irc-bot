#define _XOPEN_SOURCE 600

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>

#define SERVER   "irc.mzima.net"
#define PORT     "6667"
#define NICK     "quaaludes"
#define USER     "quaaludes"
#define REALNAME "quaaludes"
#define CHANNEL  "#swag"

#define BUF_SIZE 4096
char in_buf[BUF_SIZE];
char out_buf[BUF_SIZE];
int sfd;

/* write a formatted string to stdout and socket */
void raw(char *fmt, ...)
{
  va_list ap;
  va_start(ap, fmt);
  vsnprintf(out_buf, BUF_SIZE, fmt, ap);
  va_end(ap);
  printf("<< %s", out_buf);
  size_t len = strlen(out_buf);
  ssize_t ret = write(sfd, out_buf, len);
  assert(ret == len);
}

void handle_line(char *line)
{
  printf(">> %s", line);
  int i, len, start, wordcount;
  char *user, *command, *where, *message, *sep, *target;
  if (strncmp(line, "PING", 4) == 0) {
    // change to PONG and send back
    line[1] = 'O';
    raw(line);
    return;
  }
  if (line[0] == ':') {
    // line could be PRIVMSG or NOTICE, parse it
    len = strlen(line);
    wordcount = 0;
    user = command = where = message = NULL;
    for (i = 1; i < len; i++) {
      if (line[i] == ' ') {
        // end of a word
        line[i] = '\0';
        wordcount++;
        switch(wordcount) {
          case 1: user = line + 1; break;
          case 2: command = line + start; break;
          case 3: where = line + start; break;
        }
        if (i == len - 1)
          continue;
        start = i + 1;
      } else if (line[i] == ':' && wordcount == 3) {
        if (i < len - 1)
          message = line + i + 1;
        break;
      }
    }
    if (wordcount < 2)
      return;
    if (!strncmp(command, "001", 3)) {
      // received welcome message, join channel
      raw("JOIN %s\r\n", CHANNEL);
    } else if (!strncmp(command, "PRIVMSG", 7) || !strncmp(command, "NOTICE", 6)) {
      if (where == NULL || message == NULL)
        return;
      if ((sep = strchr(user, '!')) != NULL)
        // cut user!host to user
        user[sep - user] = '\0';
      if (where[0] == '#' || where[0] == '&' || where[0] == '+' || where[0] == '!')
        // <where> is a channel
        target = where;
      else target = user;

      printf("[from: %s] [reply-with: %s] [where: %s] [reply-to: %s] %s\n", user, command, where, target, message);
      handle_message(user, command, where, target, message);
    }     
  }
}

int main (int argc, char **argv)
{
  struct addrinfo hints;
  struct addrinfo *result, *rp;
  int ret;
  ssize_t nread;
  char *saveptr;

  // Connect to server
  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  ret = getaddrinfo(SERVER, PORT, &hints, &result);
  if (ret != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(ret));
    exit(EXIT_FAILURE);
  }

  for (rp = result; rp != NULL; rp = rp->ai_next) {
    sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
    if (sfd == -1)
      continue;

    if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1)
      break; // Success

    close(sfd);
  }
  if (rp == NULL) {
    // No success
    fprintf(stderr, "Couldn't connect to server %s on port %s\n", SERVER, PORT);
    exit(EXIT_FAILURE);
  }

  freeaddrinfo(result);  // No longer needed
  printf("Connected to server %s on port %s\n", SERVER, PORT);
  
  // Send initial connection messages
  raw("NICK %s\r\n", NICK);
  raw("USER %s 0 0 :%s\r\n", USER, REALNAME);

  // Read lines from socket in a loop and handle them
  while ((nread = read(sfd, in_buf, BUF_SIZE))) {
    // split buffer on newlines
    char *line = strtok_r(in_buf, "\n", &saveptr);
    while (line) {
      handle_line(line);
      line = strtok_r(NULL, "\n", &saveptr);
    }
  }
}
