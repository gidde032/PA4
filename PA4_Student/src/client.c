#include "client.h"

void printSyntax() {
  printf("incorrect usage syntax!\n");
  printf("usage: $ ./client server_addr server_port\n");
  printf(
      "       (commands are read from stdin; use < input.txt to redirect)\n");
}

// ============================================================
// You write everything below except main() and the fprintf format
// strings inside each function. Figure out what each parameter
// means from the protocol in instruction.md; fill in the reads,
// writes, and control flow so the fprintfs print the right values.
// ============================================================

int connect_to_server(char *server_addr, int server_port) {
  // creating socket
  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    perror("socket");
    return -1;
  }

  // completing socket address struct
  struct sockaddr_in addr;
  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(server_port);

  // converting address to binary
  if (inet_pton(AF_INET, server_addr, &addr.sin_addr) < 1) {
    perror("inet_pton");
    close(sockfd);
    return -1;
  }

  // connecting to server
  if (connect(sockfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
    perror("connect");
    close(sockfd);
    return -1;
  }

  return sockfd;
}

void get_item_list(int sock_fd, FILE *log_fp) {
  int count = 0;

  // send the request, read the count, then loop reading items.
  write(sock_fd, &(msg_enum){ITEM_LIST}, sizeof(msg_enum));
  read(sock_fd, &count, sizeof(int));

  fprintf(log_fp, "=== Item List (%d items) ===\n", count);

  for (int i = 0; i < count; i++) {
    struct item it;
    read(sock_fd, &it, sizeof(struct item));
    fprintf(log_fp, "  %s | stock: %d | price: $%.2f\n", it.name, it.stock,
            it.price);
  }
  fprintf(log_fp, "\n");
}

void search_item(int sock_fd, char *query, FILE *log_fp) {
  int count = 0;

  write(sock_fd, &(msg_enum){SEARCH_ITEM}, sizeof(msg_enum));
  write(sock_fd, query, MAX_STR);
  read(sock_fd, &count, sizeof(int));

  fprintf(log_fp, "=== Search results for \"%s\" (%d matches) ===\n", query,
          count);

  for (int i = 0; i < count; i++) {
    struct item it;
    read(sock_fd, &it, sizeof(struct item));
    fprintf(log_fp, "  %s | stock: %d | price: $%.2f\n", it.name, it.stock,
            it.price);
  }

  fprintf(log_fp, "\n");
}

// BONUS (+10): like search_item, but cipher the query on the wire.
// Use encrypt_str() from utils.h; the server will call decrypt_str().
void enc_search_item(int sock_fd, char *query, FILE *log_fp) {
  int count = 0;

  write(sock_fd, &(msg_enum){ENC_SEARCH_ITEM}, sizeof(msg_enum));
  char enc_query[MAX_STR];
  strncpy(enc_query, query, MAX_STR);
  encrypt_str(enc_query);
  write(sock_fd, enc_query, MAX_STR);
  read(sock_fd, &count, sizeof(int));

  fprintf(log_fp, "=== Search results for \"%s\" (%d matches) ===\n", query,
          count);

  for (int i = 0; i < count; i++) {
    struct item it;
    read(sock_fd, &it, sizeof(struct item));
    fprintf(log_fp, "  %s | stock: %d | price: $%.2f\n", it.name, it.stock,
            it.price);
  }

  fprintf(log_fp, "\n");
}

void get_stock(int sock_fd, char *item_name, FILE *log_fp) {
  int stock = 0;
  float price = 0;
  char err[MAX_STR] = {0};

  write(sock_fd, &(msg_enum){GET_STOCK}, sizeof(msg_enum));
  write(sock_fd, item_name, MAX_STR);
  read(sock_fd, &stock, sizeof(int));
  read(sock_fd, &price, sizeof(float));

  if (stock >= 0) {
    // on success:
    fprintf(log_fp, "Stock check: %s | stock: %d | price: $%.2f\n\n", item_name,
            stock, price);
  } else {
    // on error:
    read(sock_fd, err, MAX_STR);
    fprintf(log_fp, "Stock check error for %s: %s\n\n", item_name, err);
  }
}

void buy_item(int sock_fd, char *item_name, int amount, FILE *log_fp) {
  int new_stock = 0;
  float total_cost = 0;
  char err[MAX_STR] = {0};

  write(sock_fd, &(msg_enum){BUY_ITEM}, sizeof(msg_enum));
  write(sock_fd, item_name, MAX_STR);
  write(sock_fd, &amount, sizeof(int));

  read(sock_fd, &new_stock, sizeof(int));
  read(sock_fd, &total_cost, sizeof(float));

  if (new_stock >= 0) {
    fprintf(log_fp, "Bought %d x %s for $%.2f (remaining stock: %d)\n\n",
            amount, item_name, total_cost, new_stock);
  } else {
    read(sock_fd, err, MAX_STR);
    fprintf(log_fp, "Buy error for %s: %s\n\n", item_name, err);
  }
}

void sell_item(int sock_fd, char *item_name, int amount, FILE *log_fp) {
  int new_stock = 0;
  char err[MAX_STR] = {0};

  write(sock_fd, &(msg_enum){SELL_ITEM}, sizeof(msg_enum));
  write(sock_fd, item_name, MAX_STR);
  write(sock_fd, &amount, sizeof(int));

  read(sock_fd, &new_stock, sizeof(int));

  if (new_stock >= 0) {
    fprintf(log_fp, "Sold %d x %s (new stock: %d)\n\n", amount, item_name,
            new_stock);
  } else {
    read(sock_fd, err, MAX_STR);
    fprintf(log_fp, "Sell error for %s: %s\n\n", item_name, err);
  }
}

// Read commands from stdin, dispatch to the functions above.
// Recognized commands: LIST | SEARCH <q> | ESEARCH <q> (bonus) |
//                      STOCK <name> | BUY <name> <n> | SELL <name> <n> | QUIT
// When isatty(0) is true, print a "> " prompt before each command.
void process_input(int sock_fd) {
  char line[MAX_STR];

  while (fgets(line, MAX_STR, stdin)) {
    if (isatty(0)) {
      printf("> ");
    }

    char *cmd = strtok(line, " \n");
    if (!cmd) {
      continue;
    }

    if (strcmp(cmd, "LIST") == 0) {
      get_item_list(sock_fd, stdout);
    } else if (strcmp(cmd, "SEARCH") == 0) {
      char *query = strtok(NULL, "\n");
      if (query) {
        search_item(sock_fd, query, stdout);
      }
    } else if (strcmp(cmd, "ESEARCH") == 0) {
      char *query = strtok(NULL, "\n");
      if (query) {
        enc_search_item(sock_fd, query, stdout);
      }
    } else if (strcmp(cmd, "STOCK") == 0) {
      char *item_name = strtok(NULL, "\n");
      if (item_name) {
        get_stock(sock_fd, item_name, stdout);
      }
    } else if (strcmp(cmd, "BUY") == 0) {
      char *item_name = strtok(NULL, " \n");
      char *amount_str = strtok(NULL, " \n");
      if (item_name && amount_str) {
        int amount = atoi(amount_str);
        buy_item(sock_fd, item_name, amount, stdout);
      }
    } else if (strcmp(cmd, "SELL") == 0) {
      char *item_name = strtok(NULL, " \n");
      char *amount_str = strtok(NULL, " \n");
      if (item_name && amount_str) {
        int amount = atoi(amount_str);
        sell_item(sock_fd, item_name, amount, stdout);
      }
    } else if (strcmp(cmd, "QUIT") == 0) {
      break;
    }
  }
}

int main(int argc, char *argv[]) {

  // 1. check argc == 3, call printSyntax() on error
  if (argc != 3) {
    printSyntax();
    return 1;
  }
  // 2. parse server_addr and server_port
  char *server_addr = argv[1];
  char *server_port_str = argv[2];
  int server_port = atoi(server_port_str);

  // 3. sock_fd = connect_to_server(...)
  int sock_fd = connect_to_server(server_addr, server_port);

  // 4. process_input()
  process_input(sock_fd);

  // 5. close()
  close(sock_fd);

  return 0;
}
