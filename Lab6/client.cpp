#include "header.hpp"

bool send_packet(int sock, struct sockaddr_in* server_addr, struct Packet* packet) {
    size_t sent_size = sendto(sock, packet, sizeof(struct Packet), 0, (struct sockaddr*)server_addr, sizeof(struct sockaddr_in));
    if (sent_size == -1) {
        perror("[Client] Error sending packet");
        return false;
    } else if (sent_size != sizeof(struct Packet)) {
        printf("[Client] Warning: Not all data was sent. Only %ld out of %ld bytes were sent.\n", sent_size, sizeof(struct Packet));
        return false;
    }
    return true;
}

struct Ack getAck(int sock, struct sockaddr_in* server_addr) {
    struct Ack ack_packet;
    ack_packet.file_number = NO_ACK;
    ssize_t recv_size;

    // Wait for an ACK
    socklen_t server_addr_len = sizeof(struct sockaddr_in);
    recv_size = recvfrom(sock, &ack_packet, sizeof(struct Ack), 0, (struct sockaddr*)server_addr, &server_addr_len);
    return ack_packet;
}

void send_file(int sock, struct sockaddr_in sin, Ack ack_packet, int file_number) {
    // Initialize packet
    struct Packet packet;
    // Construct the filename
    char filename[20];
    sprintf(filename, "/files/%06d", file_number);
    FILE* file = fopen(filename, "rb");

    if (file == NULL) {
        perror("[Client] Error opening file");
        exit(EXIT_FAILURE);
    }

    // Determine file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    packet.total_packets = (file_size / PACKET_SIZE) + 1;
    packet.file_number = file_number;
    for (int i = 0; i < packet.total_packets; i++) {
        // The server has stored the packet, no need to send again
        if (ack_packet.stored_packet[i] == true) {
            continue;
        }
        if (fseek(file, i * PACKET_SIZE, SEEK_SET) != 0) {
            perror("[Client] fseek");
            exit(EXIT_FAILURE);
        }
        size_t read_size = fread(packet.data, 1, PACKET_SIZE, file);
        packet.length = read_size;
        packet.packet_number = i;
        packet.checksum = calculateCRC(&packet, offsetof(struct Packet, checksum), 0xFFFF);
        packet.checksum = calculateCRC((uint8_t*)&packet.data, sizeof(packet.data), packet.checksum);
        // dump checksum and packet_number
        // printf("[Client] Checksum: %d, Packet number: %d\n", packet.checksum, packet.packet_number);
        send_packet(sock, &sin, &packet);
    }
    fclose(file);
}

void init_send(int sock, struct sockaddr_in* server_addr) {
    struct Ack ack_packet;
    ack_packet.stored_packet[MAX_PACKETS] = {false};

    // Send the all 1000 files in the beginning
    for (int i = 0; i < CLI_INIT_SEND; i++) {
        ack_packet.file_number = i;
        send_file(sock, *server_addr, ack_packet, i);
        // usleep(100);
    }

    return;
}

int main(int argc, char* argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <path-to-read-files> <total-number-of-files> <port> <server-ip-address>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char* read_files_path = argv[1];
    int total_files = atoi(argv[2]);

    int sock;
    struct sockaddr_in sin;

    setvbuf(stdin, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(strtol(argv[argc - 2], NULL, 0));

    if (inet_pton(AF_INET, argv[argc - 1], &sin.sin_addr) != 1)
        return -fprintf(stderr, "** cannot convert IPv4 address for %s\n", argv[1]);

    if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
        err_quit("socket");

    // Set a timeout for receiving an ACK
    struct timeval timeout;
    timeout.tv_sec = 0;             // second
    timeout.tv_usec = CLI_TIMEOUT;  // usecond
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) perror("setsockopt");

    init_send(sock, &sin);

    while (true) {
        Ack ack_packet = getAck(sock, &sin);
        if (ack_packet.file_number == NO_ACK) {
            printf("[Client] No Ack\n");
            continue;
        }
        if (ack_packet.file_number == FINISH) {
            printf("[Client] Finish\n");
            break;
        }
        send_file(sock, sin, ack_packet, ack_packet.file_number);
    }

    close(sock);

    return 0;
}