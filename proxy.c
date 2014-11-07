// gcc proxy.c -o proxy -lsocket -lnsl -lpthread

#define MAX_HOST_LENGTH 256
#define MAX_REQUEST_LENGTH 300
#define HTTP_PORT 80
#define REC_BUFFER_SIZE 1024

#include <arpa/inet.h>
#include <assert.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

// Handles a client
void *handle_client(void *param);

// Struct used to pass data into threads
typedef struct {
    int sock;
    char ip4[INET_ADDRSTRLEN];
    int port;
} client_sock_info;

// Used to format proxy.log
const char *days[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};

// Header needed from proxy requests
const char proxy_header[] = "GET http://";
const char invalid_request[] = "Status 400\n\n";

// Mutex lock used for logging
pthread_mutex_t lock;

extern int errno;
int main(int argc, char **argv) {
    int server_port, s, new_s, len;
    struct sockaddr_in server;

    // Stores a client's thread
    pthread_t client_thread;

    // Validate inputs
    if(argc == 2) {
        // Grab server port
        server_port = atoi(argv[1]);
    } else {
        fprintf(stderr, "Usage :proxy server_port\n");
        exit(1);
    }

    // Building data structures for sockets
    memset(&server, 0, sizeof (server));

    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(server_port);

    // Create the server's socket
    if ((s = socket (AF_INET, SOCK_STREAM, 0)) < 0) {
        perror ("Error creating socket");
        exit (1);
    }

    // Attempt to bind the socket
    if (bind(s, (struct sockaddr *) &server, sizeof(server)) < 0) {
        perror("Error binding to port");
        exit (1);
    }

    // Attempt to listen on the given port
    if (listen (s, 5) < 0) {
        perror("listen() failed with error\n");
        exit(1);
    } else {
        printf("Listening on port %d...\n", ntohs(server.sin_port));
    }

    // Prepare space for client
    struct sockaddr_in client;
    memset(&client, 0, sizeof(client));

    // Prepare the mutex lock
    if (pthread_mutex_init(&lock, NULL) != 0) {
        printf("Mutex init failed\n");
        exit(1);
    }

    // Handle connections
    while(1) {
        // Accept a connection
        len = sizeof(client);
        if ((new_s = accept(s, (struct sockaddr *) &client, &len)) < 0) {
            printf("errno = %d, new_s = %d\n", errno, new_s);
            perror("Accept failed");
            exit(1);
        } else {
            // Prepare data to pass to a thread
            client_sock_info *client_sock = malloc(sizeof *client_sock);
            assert(client_sock != NULL);
            client_sock->sock = new_s;
            inet_ntop(AF_INET, &(client.sin_addr), client_sock->ip4, INET_ADDRSTRLEN);
            client_sock->port = ntohs(client.sin_port);

            // Create a thread to handle this client
            if(pthread_create(&client_thread, NULL, handle_client, (void *)client_sock) != 0) {
                printf("Thread Failed to start for %s\n", client_sock->ip4);
                free(client_sock);
                exit(1);
            }
        }
    }

    // Cleanup
    close(s);
    pthread_mutex_destroy(&lock);
    //fclose(global_fp);

    return 1;
}

void *handle_client(void *param) {
    int len, s, sock, client_port;
    struct hostent *hp;
    char rec_buffer[REC_BUFFER_SIZE], ip4[INET_ADDRSTRLEN];
    char client_request[MAX_REQUEST_LENGTH+1], host[MAX_HOST_LENGTH+1];
    int totalSent = 0;

    // Get reference to our client data
    client_sock_info *client_sock = param;

    // Copy useful stuff out
    sock = client_sock->sock;
    client_port = client_sock->port;
    memcpy(&ip4, client_sock->ip4, sizeof(ip4));

    // Free memory
    free(client_sock);

    // Recieve the host name (I am assuming ONLY the hostname is sent!)
    len = recv(sock, &client_request, MAX_HOST_LENGTH, 0);
    if(len <= 0) {
        printf("An error occured\n");
        close(sock);
        pthread_exit(NULL);
        return NULL;
    }

    // Make sure there is a null in place
    client_request[len] = '\0';

    int pos = 0;
    int header_size = strlen(proxy_header);
    for(int i=0; i<len; i++) {
        // Check for the header
        if(i < header_size) {
            // Does the header char not match?
            if(client_request[i] != proxy_header[i]) {
                // Send client an error message
                send(sock, invalid_request, sizeof(invalid_request), 0);

                // Cleanup socket and thread
                close(sock);
                pthread_exit(NULL);
                return NULL;
            }
        } else {
            // Check for the ending slash
            if(client_request[i] == '/') {
                // End of request
                break;
            }
            // Copy host
            host[pos++] = client_request[i];
        }
    }

    // Close the string
    host[pos] = '\0';

    // Find Host name
    hp = gethostbyname(host);
    if (!hp) {
        // Send client an error message
        send(sock, invalid_request, sizeof(invalid_request), 0);

        // Cleanup socket and thread
        close(sock);
        pthread_exit(NULL);
        return NULL;
    }

    // Prepare connection info
    struct sockaddr_in sin;
    bzero((char *)&sin, sizeof(sin));
    sin.sin_family = AF_INET;
    bcopy(hp->h_addr, (char *)&sin.sin_addr, hp->h_length);
    sin.sin_port = htons(HTTP_PORT);

    // Attempt to create a socket
    if ((s = socket(AF_INET, SOCK_STREAM, 0 )) < 0) {
        // Send client an error message
        send(sock, invalid_request, sizeof(invalid_request), 0);

        // Cleanup socket and thread
        close(sock);
        pthread_exit(NULL);
        return NULL;
    }

    // Attempt to connect to the server
    if(connect(s, (struct sockaddr *)&sin, sizeof(sin))  < 0) {
        // Send client an error message
        send(sock, invalid_request, sizeof(invalid_request), 0);

        // Cleanup socket and thread
        close(s);
        close(sock);
        pthread_exit(NULL);
        return NULL;
    }

    // Create the request
    char request[MAX_REQUEST_LENGTH];
    sprintf(request, "GET / HTTP/1.0\nHost: %s\n\n", host);

    // Send the request
    send(s, request, strlen(request), 0);

    // Recieve the data from the server
    while((len = read(s, rec_buffer,sizeof(rec_buffer))) > 0) {
        // Send back to our client
        send(sock, rec_buffer, len, 0);

        // Add to the total sent
        totalSent += len;
    }

    // Close connection to client
    close(sock);

    // Close the connection to the server
    close(s);

    // Get current time
    time_t t = time(NULL);
    struct tm *now = localtime(&t);

    // Write out log
    pthread_mutex_lock(&lock);
    FILE *fp = fopen("proxy.log", "a");
    fprintf(fp, "%.3s %.3s %.2d %.2d:%.2d:%.2d %.4d,%s,%d,%d,%s\n",
        days[now->tm_wday], months[now->tm_mon], now->tm_mday, now->tm_hour, now->tm_min, now->tm_sec, (now->tm_year+1900),
        ip4, client_port, totalSent, host);
    fclose(fp);
    pthread_mutex_unlock(&lock);

    // Stop the thread
    pthread_exit(NULL);
    return NULL;
}
