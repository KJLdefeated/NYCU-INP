#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/signal.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define N 30

int board[35][35], ori_board[35][35];
int visited[30]={};

int isSafe(int row, int col) {
    int i, j;

    // Check this row on the left side
    for (i = 0; i < N; i++)
        if (board[row][i])
            return 0;

    for (i = 0; i < N; i++)
        if (board[i][col])
            return 0;

    // Check upper diagonal on left side
    for (i = row, j = col; i >= 0 && j >= 0; i--, j--)
        if (board[i][j])
            return 0;
    
    for (i = row, j = col; i < N && j < N; i++, j++)
        if (board[i][j])
            return 0;

    // Check lower diagonal on left side
    for (i = row, j = col; j >= 0 && i < N; i++, j--)
        if (board[i][j])
            return 0;

    for (i = row, j = col; i >= 0 && j < N; i--, j++)
        if (board[i][j])
            return 0;

    return 1;
}

// Recursive function to solve N-Queen problem
int solveNQUtil(int col, int cnt) {
    // All queens are placed
    if (col >= N)
        return 1;

    // Consider this column and try placing this queen in all rows one by one
    for (int i = 0; i < N; i++) {
        // Check if the queen can be placed on board[i][col]
        if(board[i][col]){
            if (solveNQUtil(col + 1, cnt) || cnt == N)
                return 1;
        }
        if (isSafe(i, col)) {
            board[i][col] = 1; // Place the queen
            cnt++;
            if (solveNQUtil(col + 1, cnt) || cnt == N)
                return 1;
            // If placing queen in board[i][col] doesn't lead to a solution, remove queen
            cnt--;
            board[i][col] = 0;
        }
    }
    return 0; // Queen cannot be placed in any row in this column
}

// Solve the N-Queen problem
void solveNQ() {
    if (solveNQUtil(0, 3) == 0) {
        fprintf(stderr, "Solution does not exist");
        return;
    }

    // Print the solution
    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++)
            fprintf(stderr, "%d ", board[i][j]);
        fprintf(stderr,"\n");
    }
}


int main(){
    // connect to unix domain socket
    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0) {
        printf("Error creating socket\n");
        return -1;
    }

    struct sockaddr_un addr;
    addr.sun_family = AF_UNIX;

    strcpy(addr.sun_path, "/queen.sock");
    if (connect(fd, (struct sockaddr*) &addr, sizeof(addr)) < 0) {
        printf("Error connecting to server\n");
        return -1;
    }

    char buf[1024];
    send(fd, "S", 1, 0);
    recv(fd, buf, 1024, 0);
    char* input = strstr(buf, "OK: ");
    input += strlen("OK: ");
    
    int cnt=0;
    for(int i=0;i<N;i++){
        for(int j=0;j<N;j++){
            board[i][j] = (input[cnt++]=='Q'?1:0);
            ori_board[i][j] = board[i][j];
            fprintf(stderr, "%d ", board[i][j]);
        }
        fprintf(stderr, "\n");
    }

    fprintf(stderr, "Solving...\n");
    solveNQ();

    for (int y = 0; y < N; y++) {
        for (int x = 0; x < N; x++) {
            if (board[y][x]==0) {
                continue;
            }
            if(ori_board[y][x]){
                printf("%d, %d\n", y,x);
                continue;
            }

            char msg[1024];
            sprintf(msg, "M %d %d\n", y, x);
            send(fd, msg, sizeof(msg), 0);
            
            // sleep for 100 ms
            recv(fd, buf, 1024, 0);
            usleep(50000);
        }
    }

    // check for the answer
    usleep(50000);
    send(fd, "C\n", 2, 0);
    recv(fd, buf, 1024, 0);

    return 0;
}