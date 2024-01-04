#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <langinfo.h>
#include <libgen.h>
#include <locale.h>
#include <netinet/in.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define errquit(m)	{ perror(m); exit(-1); }
#define BUFFER_SIZE 1024
#define MAX_EVENTS 10
#define INITIAL_CAPACITY 32

static int port_http = 80;
static int port_https = 443;
static const char *docroot = "/html";

struct ClientInfo {
    int socket;
    pthread_t thread;
    SSL_CTX* ssl_context;
    SSL* ssl_connection;
};

struct ClientInfo *client_infos;
size_t client_capacity = INITIAL_CAPACITY;
size_t num_clients = 0;


void cleanup_ssl(struct ClientInfo *client_info) {
    SSL_free(client_info->ssl_connection);
    SSL_CTX_free(client_info->ssl_context);
}

int create_server_socket(int port){
    int server_fd;
    struct sockaddr_in sin;

    if ((server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
        errquit("socket");
    }

    int v = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &v, sizeof(v));

    bzero(&sin, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr*)&sin, sizeof(sin)) < 0) {
        fprintf(stderr, "Error binding to port %d\n", port);
        perror("bind");
        exit(EXIT_FAILURE);
    }

    if(port == 443){
        SSL_library_init();
        SSL_load_error_strings();
        OpenSSL_add_all_algorithms();

        SSL_CTX* ssl_ctx = SSL_CTX_new(SSLv23_server_method());
        if (!ssl_ctx) {
            fprintf(stderr, "Error creating SSL context\n");
            ERR_print_errors_fp(stderr);
            exit(EXIT_FAILURE);
        }
        SSL_CTX_use_certificate_file(ssl_ctx, "/cert/server.crt", SSL_FILETYPE_PEM);
        SSL_CTX_use_PrivateKey_file(ssl_ctx, "/cert/server.key", SSL_FILETYPE_PEM);
        SSL_CTX_set_cipher_list(ssl_ctx, "TLSv1.2");
        SSL_CTX_set_options(ssl_ctx, SSL_OP_SINGLE_DH_USE);    
    }
    if (listen(server_fd, 1200) < 0) {
            errquit("listen");
    }
    return server_fd;
}

char* extractFilePath(const char* path) {
    const char* question_mark = strchr(path, '?');
    size_t path_length = question_mark ? (size_t)(question_mark - path) : strlen(path);

    char* file_path = malloc(path_length + 1);
    if (!file_path) return NULL;
    strncpy(file_path, path, path_length);
    file_path[path_length] = '\0';

    return file_path;
}

const char *file_type(const char *file_name){
    const char *dot = strrchr(file_name, '.');
    if (!dot || dot == file_name)
        return "";
    return dot + 1;
}

void urlDecode(const char* url, char* decoded) {
    size_t len = strlen(url);
    size_t decoded_pos = 0;

    for (size_t i = 0; i < len; i++) {
        if (url[i] == '%' && i + 2 < len && isxdigit(url[i + 1]) && isxdigit(url[i + 2])) {
            char hex[3] = {url[i + 1], url[i + 2], '\0'};
            int value = (int)strtol(hex, NULL, 16);
            decoded[decoded_pos++] = (char)value;
            i += 2;
        } else {
            decoded[decoded_pos++] = url[i];
        }
    }
    // Null-terminate the decoded string
    decoded[decoded_pos] = '\0';
}

void HTTP_handle_client(int client_socket) {
	char buffer[BUFFER_SIZE];
    int read_size;
    char response_header[100];

    // Read the incoming request
    read_size = recv(client_socket, buffer, BUFFER_SIZE - 1, 0);
    if (read_size < 0) {
        perror("Error reading from socket");
        close(client_socket);
		return;
    }

	if (strncmp(buffer, "GET ", 4) != 0) {
		const char *not_implemented_response = "HTTP/1.0 501 Not Implemented\r\n\r\n";
        write(client_socket, not_implemented_response, strlen(not_implemented_response));
		close(client_socket);
		return;
    }
    
    
	char tmp[100], filename[100];
    sscanf(buffer, "GET %255s HTTP/1.1", tmp);
	urlDecode(tmp, filename);
	if (strcmp(filename, "/") == 0 || strcmp(filename, "/?AAA=BBB") == 0) {
        strcpy(filename, "/index.html"); // If no file specified, serve index.html
    }
    
    char* file_path = extractFilePath(filename);
	char fullpath[150];
    sprintf(fullpath, "%s%s", docroot, file_path);
   
    char file_end[150];
	strcpy(file_end, file_type(fullpath));
    
	const char *last_dot = strrchr(file_end, '/');
	size_t len_to_keep = strlen(fullpath);
    if (last_dot != NULL) {
		len_to_keep -= strlen(last_dot);
	}
	char full_path[150] = {0};
	strncpy(full_path, fullpath, len_to_keep);

	struct stat path_stat;

    if (stat(full_path, &path_stat) != 0) {
        const char *forbidden_response = "HTTP/1.0 404 Forbidden\r\n\r\n";
        write(client_socket, forbidden_response, strlen(forbidden_response));
        close(client_socket);
		return;
    }
	//printf("%s\n", filename);
	if(S_ISDIR(path_stat.st_mode)){
		if(full_path[strlen(full_path) - 1] != '/'){
			char *redirect_response = "HTTP/1.0 301 Moved Permanently\r\nLocation: ";
			char *redirect_end = "/\r\n\r\n";
			char redirect_location[200];
			sprintf(redirect_location, "%s%s%s", redirect_response, filename, redirect_end);
			write(client_socket, redirect_location, strlen(redirect_location));
			close(client_socket);
			return;
		}
		char index_path[450];
		snprintf(index_path, sizeof(index_path), "%s/index.html", full_path);
		if(access(index_path, F_OK) == -1){
			const char *forbidden_response = "HTTP/1.0 403 Forbidden\r\n\r\n";
			write(client_socket, forbidden_response, strlen(forbidden_response));
			close(client_socket);
			return;
		}
	}
	
    // Open the requested file
	FILE* file = fopen(full_path, "rb");
    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    rewind(file);
    // Get file content and read size
    char* file_content = (char*)malloc(file_size);
    size_t fread_size = fread(file_content, 1, file_size, file);
	const char* mime_type = "application/octet-stream";
	if (strstr(full_path, ".html") || strstr(full_path, ".txt"))
        mime_type = "text/html";
	else if (strstr(full_path, ".jpg"))
        mime_type = "image/jpeg";
    else if (strstr(full_path, ".mp3"))
        mime_type = "audio/mpeg";
    else if (strstr(full_path, ".png"))
        mime_type = "image/png";
	

    // Send the HTTP response header
	sprintf(response_header, "HTTP/1.0 200 OK\r\nContent-Length: %zu\r\nContent-Type: %s; charset=utf-8\r\n\r\n", fread_size, mime_type);
	//printf("%s", response_header);
    write(client_socket, response_header, strlen(response_header));

    // Read and send the contents of the file
	write(client_socket, file_content, fread_size);
    
    // Clean up
    fclose(file);
	free(file_content);
	close(client_socket);
}

void HTTPS_handle_client(struct ClientInfo *client_info){
    char buffer[BUFFER_SIZE];
    char response_header[100];
    int read_size;
    read_size = SSL_read(client_info->ssl_connection, buffer, sizeof(buffer));
    setlocale(LC_ALL, "en_US.UTF-8");
    if(read_size < 0){
        ERR_print_errors_fp(stderr);
        close(client_info->socket);
        return;
    }

    if (strncmp(buffer, "GET ", 4) != 0) {
		const char *not_implemented_response = "HTTP/1.0 501 Not Implemented\r\n\r\n";
        SSL_write(client_info->ssl_connection, not_implemented_response, strlen(not_implemented_response));
        close(client_info->socket);
		return;
    }
    
	char tmp[100], filename[100];
    sscanf(buffer, "GET %255s HTTP/1.1", tmp);
	urlDecode(tmp, filename);
	if (strcmp(filename, "/") == 0 || strcmp(filename, "/?AAA=BBB") == 0) {
        strcpy(filename, "/index.html"); // If no file specified, serve index.html
    }
    
	char* file_path = extractFilePath(filename);
	char full_path[150];
    sprintf(full_path, "%s%s", docroot, file_path);

	struct stat path_stat;

    if (stat(full_path, &path_stat) != 0) {
        const char *forbidden_response = "HTTP/1.0 404 Forbidden\r\n\r\n";
        SSL_write(client_info->ssl_connection, forbidden_response, strlen(forbidden_response));
        close(client_info->socket);
		return;
    }
	//printf("%s\n", filename);
	if(S_ISDIR(path_stat.st_mode)){
		if(full_path[strlen(full_path) - 1] != '/'){
			char *redirect_response = "HTTP/1.0 301 Moved Permanently\r\nLocation: ";
			char *redirect_end = "/\r\n\r\n";
			char redirect_location[200];
			sprintf(redirect_location, "%s%s%s", redirect_response, filename, redirect_end);
			SSL_write(client_info->ssl_connection, redirect_location, strlen(redirect_location));
            close(client_info->socket);
			return;
		}
		char index_path[450];
		snprintf(index_path, sizeof(index_path), "%s/index.html", full_path);
		if(access(index_path, F_OK) == -1){
			const char *forbidden_response = "HTTP/1.0 403 Forbidden\r\n\r\n";
			SSL_write(client_info->ssl_connection, forbidden_response, strlen(forbidden_response));
            close(client_info->socket);
			return;
		}
	}
	
    // Open the requested file
	FILE* file = fopen(full_path, "rb");
    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    rewind(file);
    // Get file content and read size
    char* file_content = (char*)malloc(file_size);
    size_t fread_size = fread(file_content, 1, file_size, file);
	const char* mime_type = "application/octet-stream";
	if (strstr(full_path, ".html") || strstr(full_path, ".txt"))
        mime_type = "text/html";
	else if (strstr(full_path, ".jpg"))
        mime_type = "image/jpeg";
    else if (strstr(full_path, ".mp3"))
        mime_type = "audio/mpeg";
    else if (strstr(full_path, ".png"))
        mime_type = "image/png";
	

    // Send the HTTP response header
	sprintf(response_header, "HTTP/1.0 200 OK\r\nContent-Length: %zu\r\nContent-Type: %s; charset=utf-8\r\n\r\n", fread_size, mime_type);
	//printf("%s", response_header);
    SSL_write(client_info->ssl_connection, response_header, strlen(response_header));

    // Read and send the contents of the file
	SSL_write(client_info->ssl_connection, file_content, fread_size);
    
    // Clean up
    fclose(file);
	free(file_content);
	//cleanup_ssl(client_info);
     if (SSL_shutdown(client_info->ssl_connection) == 0) {
        SSL_shutdown(client_info->ssl_connection);
    }
    SSL_free(client_info->ssl_connection);
    close(client_info->socket); 
}

int main(int argc, char *argv[]) {
    SSL_library_init();
    SSL_load_error_strings();
    OpenSSL_add_all_algorithms();
	client_infos = malloc(sizeof(struct ClientInfo) * client_capacity);

	if(argc > 1) { port_http  = strtol(argv[1], NULL, 0); }
	if(argc > 2) { if((docroot = strdup(argv[2])) == NULL) errquit("strdup"); }
	if(argc > 3) { port_https = strtol(argv[3], NULL, 0); }

	int https_s = create_server_socket(port_https);
    int http_s = create_server_socket(port_http);

    SSL_CTX *https_ssl_context = SSL_CTX_new(SSLv23_server_method());
    if (SSL_CTX_use_certificate_file(https_ssl_context, "/cert/server.crt", SSL_FILETYPE_PEM) <= 0) {
        perror("SSL_CTX_use_certificate_file");
        exit(EXIT_FAILURE);
    }
    if (SSL_CTX_use_PrivateKey_file(https_ssl_context, "/cert/server.key", SSL_FILETYPE_PEM) <= 0) {
        perror("SSL_CTX_use_PrivateKey_file");
        exit(EXIT_FAILURE);
    }

    int epoll_fd = epoll_create1(0);
    if (epoll_fd == -1) {
        perror("epoll_create1");
        exit(EXIT_FAILURE);
    }

	struct epoll_event http_event;
    http_event.events = EPOLLIN;
    http_event.data.fd = http_s;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, http_s, &http_event) == -1) {
        perror("epoll_ctl");
        exit(EXIT_FAILURE);
    }

    struct epoll_event https_event;
    https_event.events = EPOLLIN;
    https_event.data.fd = https_s;
    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, https_s, &https_event) == -1) {
        perror("epoll_ctl");
        exit(EXIT_FAILURE);
    }
	
	while(1){
		struct epoll_event events[MAX_EVENTS];
        int num_events = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (num_events == -1) {
            perror("epoll_wait");
            exit(EXIT_FAILURE);
        }

		for (int i = 0; i < num_events; i++) {
            if (events[i].data.fd == http_s) {
                // Accept new connections and create threads
                int client_fd = accept(http_s, NULL, NULL);
                if (client_fd < 0) {
                    perror("accept");
                    continue;
                }

				if (num_clients >= client_capacity) {
					client_capacity *= 2;
					client_infos = realloc(client_infos, sizeof(struct ClientInfo) * client_capacity);
				}

				struct ClientInfo *client_info = &client_infos[num_clients++];
                client_info->socket = client_fd;
                client_info->ssl_context = NULL;
                client_info->ssl_connection = NULL;
                pthread_create(&client_info->thread, NULL, (void *(*)(void *))HTTP_handle_client, (void *)(intptr_t)client_fd);
                pthread_detach(client_info->thread); 
            }
            if (events[i].data.fd == https_s){
                int client_fd = accept(https_s, NULL, NULL);
                if (client_fd < 0) {
                    perror("accept");
                    continue;
                }

                if (num_clients >= client_capacity) {
					client_capacity *= 2;
					client_infos = realloc(client_infos, sizeof(struct ClientInfo) * client_capacity);
				}

                struct ClientInfo *client_info = &client_infos[num_clients++];
                SSL *ssl_connection = SSL_new(https_ssl_context);
                SSL_set_fd(ssl_connection, client_fd);
                client_info->socket = client_fd;
                client_info->ssl_context = https_ssl_context;
                client_info->ssl_connection = ssl_connection;
                
                if (SSL_accept(ssl_connection) <= 0) {
                    // Handle SSL handshake error
                    ERR_print_errors_fp(stderr);
                    close(client_fd);
                    SSL_free(ssl_connection);
                    continue;
                }

                pthread_create(&client_info->thread, NULL, (void *(*)(void *))HTTPS_handle_client, client_info);
                pthread_detach(client_info->thread);     
            }
        }
	}
	close(epoll_fd);
    close(http_s);
    close(https_s);
	free(client_infos);
	return 0;
}
