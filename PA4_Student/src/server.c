#include "server.h"

void printSyntax(){
    printf("incorrect usage syntax!\n");
    printf("usage: $ ./server server_addr server_port num_workers\n");
}

// ============================================================
// load_inventory: read items.csv into the global inventory[] array
// CSV format:
//     name,stock,price
//     laptop,50,999.99
//     ...
// Skip the header line. Set num_items to the number of items loaded.
// ============================================================
void load_inventory(char *filename)
{
    FILE *f = fopen(filename, "r");
    if (!f) {
        perror("fopen");
        exit(1);
    }

    char line[128];
    fgets(line, sizeof(line), f); // skipping header

    // read each line into inventory, don't exceed max num of items
    while (fgets(line, sizeof(line), f) && num_items < MAX_ITEMS) {
        struct item *item = &inventory[num_items];
        sscanf(line, "%63[^,],%d,%f", item->name, &item->stock, &item->price);
        num_items++;
    }

    fclose(f);
}

// ============================================================
// handle_list_items: send the full inventory to the client.
// Protocol:
//     write ITEM_LIST (msg_enum)
//     write num_items (int)
//     write each struct item (num_items of them)
// Remember to lock the inventory_lock mutex while reading inventory.
// ============================================================
void handle_list_items(int client_fd)
{
    msg_enum rsp = ITEM_LIST;
    write(client_fd, &rsp, sizeof(msg_enum));

    pthread_mutex_lock(&inventory_lock);
    write(client_fd, &num_items, sizeof(int));
    int i = 0;
    while (i < num_items)
    {
        write(client_fd, &inventory[i], sizeof(struct item));
        i++;
    }
    pthread_mutex_unlock(&inventory_lock);
}

// ============================================================
// handle_search: read a query string from the client, then return
// every item whose name contains the query as a substring.
// Protocol (recv): char query[MAX_STR]
// Protocol (send): SEARCH_RESULTS (msg_enum), count (int),
//                  then count x struct item
// ============================================================
void handle_search(int client_fd)
{
    struct item results[MAX_RESULTS];
    int count = 0;

    // reading query from client
    char query[MAX_STR];
    read(client_fd, query, MAX_STR);

    pthread_mutex_lock(&inventory_lock);
    int i = 0;
    while (i < num_items) // searching inventory for items in query
    {
        if (strstr(inventory[i].name, query)  && count < MAX_RESULTS) { // checking if query is a substring of item
            results[count] = inventory[i];
            count++;
        }
        i++;
    }
    pthread_mutex_unlock(&inventory_lock);

    // writing response
    msg_enum rsp = SEARCH_RESULTS;
    write(client_fd, &rsp, sizeof(msg_enum));

    write(client_fd, &count, sizeof(int));

    i = 0;
    while (i < count)
    {
        write(client_fd, &results[i], sizeof(struct item));
        i++;
    }
}

// ============================================================
// BONUS (optional, +10 pts):
// handle_enc_search: like handle_search, but the query was ciphered on
// the wire using the Caesar shift from utils.h. Call decrypt_str() on
// it before running the normal substring search. Reply format is the
// standard SEARCH_RESULTS message (unciphered).
// ============================================================
void handle_enc_search(int client_fd)
{
    // TODO (bonus): read char query[MAX_STR]; decrypt_str(query);
    //               then run the same search loop as handle_search,
    //               and write SEARCH_RESULTS + count + items back.
}

// ============================================================
// handle_get_stock: read an item name and respond with its stock/price.
// Protocol (recv): char name[MAX_STR]
// Protocol (send): STOCK_INFO (msg_enum), stock (int), price (float)
//             or:  ERROR_MSG (msg_enum), char err[MAX_STR]
// ============================================================
void handle_get_stock(int client_fd)
{
    char name[MAX_STR]; // reading item name
    read(client_fd, name, MAX_STR);

    pthread_mutex_lock(&inventory_lock);
    for (int i = 0; i < num_items; i++) { // loop through inventory checking for item
        struct item *item = &inventory[i];
        if (strcmp(item->name, name) == 0) { // write stock info if found
            msg_enum rsp = STOCK_INFO;
            write(client_fd, &rsp, sizeof(msg_enum));
            write(client_fd, &item->stock, sizeof(int));
            write(client_fd, &item->price, sizeof(float));
            pthread_mutex_unlock(&inventory_lock);
            return;
        }
    }
    pthread_mutex_unlock(&inventory_lock);

    msg_enum rsp = ERROR_MSG; // not found error
    char err[MAX_STR];
    strncpy(err, "item not found", MAX_STR);

    write(client_fd, &rsp, sizeof(msg_enum));
    write(client_fd, err, MAX_STR);
}

// ============================================================
// handle_buy_item: read item name and amount, decrement stock.
// Protocol (recv): char name[MAX_STR], int amount
// Protocol (send): BUY_OK (msg_enum), new_stock (int), total_cost (float)
//             or:  ERROR_MSG (msg_enum), char err[MAX_STR]
// Send an error if the item doesn't exist OR if stock < amount.
// ============================================================
void handle_buy_item(int client_fd)
{
    char name[MAX_STR]; // reading item name and amount
    int amount;
    read(client_fd, name, MAX_STR);
    read(client_fd, &amount, sizeof(int));

    pthread_mutex_lock(&inventory_lock);
    for (int i = 0; i < num_items; i++) { // loop through inventory looking for item
        struct item *item = &inventory[i];
        if (strcmp(item->name, name) == 0) {
            if (item->stock < amount) {
                pthread_mutex_unlock(&inventory_lock);
                msg_enum rsp = ERROR_MSG;
                char err[MAX_STR];
                strncpy(err, "not enough stock", MAX_STR);

                write(client_fd, &rsp, sizeof(msg_enum));
                write(client_fd, err, MAX_STR);
                return;
            }
            item->stock -= amount;
            float total_cost = amount * item->price;
            msg_enum rsp = BUY_OK;
            write(client_fd, &rsp, sizeof(msg_enum));
            write(client_fd, &item->stock, sizeof(int));
            write(client_fd, &total_cost, sizeof(float));
            pthread_mutex_unlock(&inventory_lock);
            return;
        }
    }
    pthread_mutex_unlock(&inventory_lock);

    msg_enum rsp = ERROR_MSG; // not found error
    char err[MAX_STR];
    strncpy(err, "item not found", MAX_STR);

    write(client_fd, &rsp, sizeof(msg_enum));
    write(client_fd, err, MAX_STR);
}

// ============================================================
// handle_sell_item: read item name and amount, increment stock.
// Protocol (recv): char name[MAX_STR], int amount
// Protocol (send): SELL_OK (msg_enum), new_stock (int)
//             or:  ERROR_MSG (msg_enum), char err[MAX_STR]
// ============================================================
void handle_sell_item(int client_fd)
{
    char name[MAX_STR]; // reading item name and amount
    int amount;
    read(client_fd, name, MAX_STR);
    read(client_fd, &amount, sizeof(int));

    pthread_mutex_lock(&inventory_lock);
    for (int i = 0; i < num_items; i++) { // loop through inventory checking for item
        struct item *item = &inventory[i];
        if (strcmp(item->name, name) == 0) { // write stock info if found
            item->stock += amount;
            msg_enum rsp = SELL_OK;
            write(client_fd, &rsp, sizeof(msg_enum));
            write(client_fd, &item->stock, sizeof(int));
            pthread_mutex_unlock(&inventory_lock);
            return;
        }
    }
    pthread_mutex_unlock(&inventory_lock);

    msg_enum rsp = ERROR_MSG; // not found error
    char err[MAX_STR];
    strncpy(err, "item not found", MAX_STR);

    write(client_fd, &rsp, sizeof(msg_enum));
    write(client_fd, err, MAX_STR);
}

// ============================================================
// save_inventory: write the current inventory[] to output/inventory.csv
// Format should match items.csv (header: "name,stock,price").
// ============================================================
void save_inventory()
{
    FILE *f = fopen("output/inventory.csv", "w");
    if (!f) {
        perror("fopen");
        exit(1);
    }

    // write header
    fprintf(f, "name,stock,price\n");

    // write each item's information
    for (int i = 0; i < num_items; i++) {
        struct item *item = &inventory[i];
        fprintf(f, "%s,%d,%.2f\n", item->name, item->stock, item->price);
    }
    
    fclose(f);
}

// ============================================================
// handle_client: the worker thread function for one client connection.
// Loop reading a msg_enum from the socket and dispatching to the right
// handler. Break out of the loop and close the socket when the client
// disconnects (read returns <= 0).
// ============================================================
void *handle_client(void *arg)
{
    // extracting client_fd from arg
    int client_fd = *((int *)arg);
    free(arg);

    // looping while client is requesting
    while (1) {
        msg_enum msg;
        if (read(client_fd, &msg, sizeof(msg_enum)) < 1) { // break if client disconnects
            break;
        }
        
        switch (msg) { // handle request by type
            case LIST_ITEMS:
                handle_list_items(client_fd);
                break;
            case SEARCH_ITEM:
                handle_search(client_fd);
                break;
            case GET_STOCK:
                handle_get_stock(client_fd);
                break;
            case BUY_ITEM:
                handle_buy_item(client_fd);
                break;
            case SELL_ITEM:
                handle_sell_item(client_fd);
                break;
            default:
                break;
        }
    }

    close(client_fd);
    return NULL;
}

// ============================================================
// sigterm_handler: on SIGTERM, save inventory and exit(0).
// ============================================================
void sigterm_handler(int sig)
{
    save_inventory();
    exit(0);
}

int main(int argc, char *argv[])
{
    if (argc != 4) {
        printSyntax();
        return 1;
    }

    char *server_addr = argv[1];
    int server_port = atoi(argv[2]);

    bookeepingCode(); // set up output/

    load_inventory("items.csv");

    signal(SIGTERM, sigterm_handler); // use specified handler

    // creating socket
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return 1;
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
        return 1;
    }

    // binding socket and listening
    bind(sockfd, (struct sockaddr *)&addr, sizeof(addr));
    listen(sockfd, 10);

    while (1) { // accept loop
        int client_fd = accept(sockfd, NULL, NULL); // accept client
        int *fd = malloc(sizeof(int));
        *fd = client_fd;

        pthread_t client; // create thread using client handling function
        pthread_create(&client, NULL, handle_client, fd);
        pthread_detach(client);
    }

    return 0;
}
