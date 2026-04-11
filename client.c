#include <stdio.h>
#include <string.h>

#include <sys/types.h> // socket man pages say we need this
#include <sys/socket.h>
#include <arpa/inet.h> // for sockaddr_in
#include <unistd.h> // for close

#include <assert.h>
#include <errno.h>

#define debug(...) fprintf(stderr, __VA_ARGS__)

int open_connection(char* address, short port) {
  int fd = socket(PF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    debug("socket failed");
    return -1;
  }
  debug("Created socket, descriptor is %d\n", fd);

  struct sockaddr_in client = {
    .sin_family = AF_INET,
    .sin_port = htons(0) // any port
  };
  inet_aton("127.0.0.1", &client.sin_addr);
  if (bind(fd, (struct sockaddr*)&client, sizeof(client)) < 0) {
    debug("bind failed");
    return -1;
  }
  socklen_t addr_len = sizeof(client);
  if (getsockname(fd, (struct sockaddr*)&client, &addr_len) < 0) {
    debug("getsockname failed");
    return -1;
  }
  debug("Client bound to port %d\n", ntohs(client.sin_port));

  struct sockaddr_in server = {
    .sin_family = AF_INET,
    .sin_port = htons(port)
  };
  inet_aton(address, &server.sin_addr);
  if (connect(fd, (struct sockaddr*)&server, sizeof(server))) {
    debug("connect failed");
    return -1;
  }
  debug("Opened connection to %s:%d\n", address, port);

  return fd;
}

int send_message(int fd, char* message, size_t size) {
  unsigned int bytes_sent = 0;

  while (bytes_sent < size) {
    int sent_count = send(fd, message + bytes_sent, size - bytes_sent, 0);
    if (sent_count < 0) {
      return sent_count;
    }
    debug("Sent %d bytes\n", sent_count);
    bytes_sent += sent_count;
  }

  return bytes_sent;
}

int recv_message(int fd, char* buffer, size_t buffer_size, int (*finished)(char*, size_t)) {
  size_t bytes_received = 0;

  while (!finished(buffer, bytes_received)) {
    assert(bytes_received < buffer_size && "Too many bytes received!");

    int recv_count = recv(fd, buffer + bytes_received, buffer_size - bytes_received, 0);
    if (recv_count < 0) {
      return -1;
    }
    if (recv_count == 0) {
      debug("Server closed connection\n");
      break;
    }

    debug("Got %d bytes\n", recv_count);
    bytes_received += recv_count;
  }

  return bytes_received;
}

#define MAX_RECV_BYTES 1000
#define SERVER_ADDRESS "127.0.0.1"
#define SERVER_PORT 3002
#define HOST_HEADER "Host: 127.0.0.1:3002\r\n"

// Assume transfer-encoding: chunked
int is_response_finished(char* buffer, size_t bytes_received) {
  // 0-byte last chunk for transfer-encoding: chunked
  char* last_chunk = "0\r\n\r\n";
  size_t last_chunk_len = strlen(last_chunk);
  return memcmp(buffer + bytes_received - last_chunk_len, last_chunk, last_chunk_len) == 0;
}

void print_bytes(char* buffer, size_t size) {
  for (size_t i = 0; i < size; i++) {
    printf("%c", buffer[i]);
  }
}

typedef struct {
  char* data;
  size_t size;
} Message;

int main(void) {
  // Don't buffer writes to sdout, so that stderr/stdout ordering is consistent
  setbuf(stdout, NULL);

  Message message1 = {
    .data = "POST /my-post-req HTTP/1.1\r\n"
            HOST_HEADER
            "Transfer-Encoding: chunked\r\n"
            "\r\n"
            "7\r\n"
            "supdawg\r\n"
            "a\r\n"
            "in da haus\r\n"
            "0\r\n"
            "\r\n",
    .size = strlen(message1.data)
  };

  Message message2 = {
    .data = "GET /my-get-req HTTP/1.1\r\n"
            HOST_HEADER
            "\r\n",
    .size = strlen(message2.data)
  };

  Message message3 = {
    .data = "POST /my-message-with-null\r\n"
            HOST_HEADER
            "Content-Length: 11\r\n"
            "\r\n"
            "here we\0are"
            "\r\n",
    .size = strlen(message3.data) + 6 // data contains a null byte
  };

  Message messages[] = {
    message1,
    message2,
    message3,
  };

  for (unsigned long i = 0; i < sizeof(messages) / sizeof(Message); i++) {
    debug("Starting HTTP request\n");
    int fd = open_connection(SERVER_ADDRESS, SERVER_PORT);
    if (fd < 0) {
      perror("open_connection");
      return 1;
    }

    Message message = messages[i];
    int bytes_sent = send_message(fd, message.data, message.size);
    if (bytes_sent < 0) {
      perror("send_message");
      return 1;
    }

    char response[MAX_RECV_BYTES] = {0};

    int bytes_received = recv_message(fd, response, MAX_RECV_BYTES, is_response_finished);
    if (bytes_received < 0) {
      perror("recv_message");
      return 1;
    }

    debug("===\n");
    print_bytes(response, bytes_received);
    debug("\n===\n");

    if (close(fd) < 0) {
      perror("close");
      return 1;
    }

    debug("Closed.\n\n");
  }

  return 0;
}
