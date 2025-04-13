#include "raylib.h"
#include <arpa/inet.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

typedef struct {
    Vector2 position;
    int radius;
    Color color;
    float lifetime;
} Circle;

typedef struct {
    float x;
    float y;
} ClickPacket;

#define MAX_CIRCLES 3
#define MAX_MISSES 3

#define MIN_LIFETIME 2
#define MAX_LIFETIME 5

#define MIN_RADIUS 20
#define MAX_RADIUS 75

Circle circles[MAX_CIRCLES];
int circle_count = 0;
int misses = 0;
int score = 0;

bool run_as_server = false;
char peer_ip[256] = {0};
int server_port = 8080;
int peer_port = 0;

int sockfd;
struct sockaddr_in server_addr, peer_addr;
socklen_t addr_len = sizeof(struct sockaddr_in);

void print_usage() {
    printf("Usage: clicky [options]\n");
    printf("Options:\n");
    printf("  -s, --server      Run as server\n");
    printf("  -p, --peer        Run as peer (remote controller)\n");
}

void make_socket_nonblocking(int socket) {
    int flags = fcntl(socket, F_GETFL, 0);
    fcntl(socket, F_SETFL, flags | O_NONBLOCK);
}

Circle spawnCircle(int x, int y) {
    Circle c;
    c.position = (Vector2){x, y};
    c.radius = GetRandomValue(MIN_RADIUS, MAX_RADIUS);
    c.color = (Color){GetRandomValue(0, 255), GetRandomValue(0, 255),
                      GetRandomValue(0, 255), 255};
    c.lifetime = GetRandomValue(MIN_LIFETIME, MAX_LIFETIME);
    return c;
}

void game() {
    float delta = GetFrameTime();

    if (run_as_server) {
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            Vector2 clickPos = GetMousePosition();
            for (int i = 0; i < circle_count; i++) {
                if (CheckCollisionCircles(clickPos, 8, circles[i].position,
                                          circles[i].radius)) {
                    circles[i] = circles[--circle_count];
                    score++;
                    break;
                }
            }
        }

        ClickPacket packet;
        ssize_t received =
            recvfrom(sockfd, &packet, sizeof(packet), 0, NULL, NULL);
        if (received == sizeof(ClickPacket)) {
            if (circle_count < MAX_CIRCLES) {
                circles[circle_count++] =
                    spawnCircle((int)packet.x, (int)packet.y);
            }
        }
    } else {
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            Vector2 clickPos = GetMousePosition();
            ClickPacket packet = {clickPos.x, clickPos.y};
            sendto(sockfd, &packet, sizeof(packet), 0,
                   (struct sockaddr *)&peer_addr, addr_len);
        }
    }

    BeginDrawing();
    ClearBackground(BLACK);

    if (run_as_server) {
        DrawText("Clicky - Server", 20, 20, 18, WHITE);
        DrawText(TextFormat("Score: %d", score), 20, 50, 18, WHITE);
        DrawText(TextFormat("Misses: %d", misses), 20, 80, 18, WHITE);
    } else {
        DrawText("Clicky - Peer (Remote Control)", 20, 20, 18, WHITE);
    }

    DrawCircleV(GetMousePosition(), 8, RED);

    if (run_as_server) {
        for (int i = 0; i < circle_count; i++) {
            circles[i].lifetime -= delta;
            if (circles[i].lifetime <= 0) {
                circles[i] = circles[--circle_count];
                misses++;
            } else {
                DrawCircleV(circles[i].position, circles[i].radius,
                            circles[i].color);
            }
        }
    }

    EndDrawing();
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        print_usage();
        return 1;
    }

    if (strcmp(argv[1], "--server") == 0 | strcmp(argv[1], "-s") == 0) {
        run_as_server = true;
        printf("Running as server...\n");
        printf("Enter server port (8080): ");
        scanf("%d", &server_port);
        if (server_port <= 0 || server_port > 65535) {
            fprintf(stderr, "Invalid port number\n");
            return 1;
        }
        printf("Listening on port %d...\n", server_port);
    } else if (strcmp(argv[1], "--peer") == 0 | strcmp(argv[1], "-p") == 0) {
        run_as_server = false;
        printf("Running as peer (remote control)...\n");
        printf("Enter server IP address: ");
        scanf("%s", peer_ip);
        printf("Enter server port: ");
        scanf("%d", &peer_port);
        printf("Connecting to server at %s:%d...\n", peer_ip, peer_port);
    } else {
        print_usage();
        return 1;
    }

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return 1;
    }

    if (run_as_server) {
        server_addr.sin_family = AF_INET;
        server_addr.sin_port = htons(server_port);
        server_addr.sin_addr.s_addr = INADDR_ANY;
        if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) <
            0) {
            perror("bind");
            close(sockfd);
            return 1;
        }
        make_socket_nonblocking(sockfd);
    } else {
        peer_addr.sin_family = AF_INET;
        peer_addr.sin_port = htons(peer_port);
        if (inet_pton(AF_INET, peer_ip, &peer_addr.sin_addr) <= 0) {
            fprintf(stderr, "Invalid server IP address\n");
            close(sockfd);
            return 1;
        }
    }

    InitWindow(1280, 800, "Clicky");
    SetTargetFPS(144);

    while (!WindowShouldClose()) {
        if (misses >= MAX_MISSES) {
            BeginDrawing();
            ClearBackground(BLACK);
            DrawText("Game Over", 20, 20, 40, RED);
            DrawText(TextFormat("Final Score: %d", score), 20, 80, 18, WHITE);
            EndDrawing();
        } else {
            game();
        }
    }

    CloseWindow();
    close(sockfd);
    return 0;
}
