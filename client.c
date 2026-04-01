#include <stdio.h>
#include <string.h>

#include <sys/types.h> // socket man pages say we need this
#include <sys/socket.h>
#include <arpa/inet.h> // for sockaddr_in
#include <unistd.h> // for close

#include <assert.h>
#include <errno.h>

#define MAX_RECV_BYTES 1000

int main(void) {
  int sock = socket(PF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    perror("socket");
    return 1;
  }

  struct sockaddr_in client = {
    .sin_family = AF_INET,
    .sin_port = htons(0) // any port
  };
  inet_aton("127.0.0.1", &client.sin_addr);
  if (bind(sock, (struct sockaddr*)&client, sizeof(struct sockaddr_in)) < 0) {
    perror("bind");
    return 1;
  }

  struct sockaddr_in server = {
    .sin_family = AF_INET,
    .sin_port = htons(3002)
  };
  inet_aton("127.0.0.1", &server.sin_addr);
  if (connect(sock, (struct sockaddr*)&server, sizeof(struct sockaddr_in))) {
    perror("connect");
    return 1;
  }

  char* message =
    "POST /client HTTP/1.1\r\n"
    "Host: 127.0.0.1:3002\r\n"
    "Transfer-Encoding: chunked\r\n"
    "\r\n"
    "7\r\n"
    "supdawg\r\n"
    "a\r\n"
    "in da haus\r\n"
    "0\r\n"
    "\r\n";

  unsigned long bytes_sent = 0;
  while (bytes_sent < strlen(message)) {
    int sent_count = send(sock, message + bytes_sent, strlen(message) - bytes_sent, 0);
    if (sent_count < 0) {
      perror("send");
      return 1;
    }
    fprintf(stderr, "Sent %d bytes\n", sent_count);
    bytes_sent += sent_count;
  }

  // Pad by one to make sure the response is always null-terminated
  char response[MAX_RECV_BYTES + 1] = {0};
  int bytes_received = 0;

  // 0-byte last chunk for transfer-encoding: chunked
  char* last_chunk = "0\r\n\r\n";

  // strcmp works because we ensure the response buffer is always null terminated
  while (strcmp(response + bytes_received - strlen(last_chunk), last_chunk) != 0) {
    assert(bytes_received < MAX_RECV_BYTES && "Too many bytes received!");

    int recv_count = recv(sock, response + bytes_received, MAX_RECV_BYTES - bytes_received, 0);
    if (recv_count < 0) {
      perror("recv");
      return 1;
    }
    if (recv_count == 0) {
      fprintf(stderr, "Server closed connection\n");
      break;
    }

    fprintf(stderr, "Got %d bytes\n", recv_count);
    bytes_received += recv_count;
  }

  fprintf(stderr, "===\n");
  printf("%s", response);
  fprintf(stderr, "===\n");

  if (close(sock) < 0) {
    perror("close");
    return 1;
  }

  fprintf(stderr, "Closed.\n");

  return 0;
}
