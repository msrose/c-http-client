#include <stdio.h>
#include <string.h> // for memset

#include <sys/types.h> // socket man pages say we need this
#include <sys/socket.h>
#include <arpa/inet.h> // for sockaddr_in
#include <unistd.h> // for close

#include <assert.h>
#include <errno.h>

#define MAX_RECV_BYTES 1000

int main(void) {
  int sock = socket(PF_INET, SOCK_STREAM, 0);
  assert(sock >= 0 && "Failed to create socket");

  struct sockaddr_in client = {
    .sin_family = AF_INET,
    .sin_port = htons(0) // any port
  };
  inet_aton("127.0.0.1", &client.sin_addr);
  int bind_result = bind(sock, (struct sockaddr*)&client, sizeof(struct sockaddr_in));
  assert(bind_result >= 0 && "Failed to bind");

  struct sockaddr_in server = {
    .sin_family = AF_INET,
    .sin_port = htons(3002)
  };
  inet_aton("127.0.0.1", &server.sin_addr);

  int connect_result = connect(sock, (struct sockaddr*)&server, sizeof(struct sockaddr_in));
  assert(connect_result >= 0 && "Failed to connect");

  char* message = "POST /client HTTP/1.1\r\nHost: 127.0.0.1:3002\r\nTransfer-Encoding: chunked\r\n\r\n7\r\nsupdawg\r\n0\r\n\r\n";

  unsigned long bytes_sent = 0;
  while (bytes_sent < strlen(message)) {
    int sent_result = send(sock, message + bytes_sent, strlen(message) - bytes_sent, 0);
    printf("Sent %d bytes\n", sent_result);
    assert(sent_result >= 0 && "Failed to send");
    bytes_sent += sent_result;
  }

  // Pad by one to make sure the response is always null-terminated
  char response[MAX_RECV_BYTES + 1] = {0};
  int bytes_received = 0;

  // 0-byte last chunk for transfer-encoding: chunked
  char* last_chunk = "0\r\n\r\n";
  // Hack for early exit when getting a 400 from Node.js http server
  char* connection_close = "Connection: close\r\n\r\n";

  // strcmp works because we ensure the response buffer is always null terminated
  while (
    strcmp(response + bytes_received - strlen(last_chunk), last_chunk) != 0 &&
    strcmp(response + bytes_received - strlen(connection_close), connection_close) != 0
  ) {
    assert(bytes_received < MAX_RECV_BYTES && "Too many bytes received!");

    int recv_result = recv(sock, response + bytes_received, MAX_RECV_BYTES - bytes_received, 0);
    assert(recv_result >= 0 && "Failed to recv");

    printf("Got %d bytes, full res is\n===\n%s===\n", recv_result, response);
    bytes_received += recv_result;
  }

  int close_result = close(sock);
  assert(close_result >= 0 && "Failed to close");

  printf("Closed.\n");

  return 0;
}
