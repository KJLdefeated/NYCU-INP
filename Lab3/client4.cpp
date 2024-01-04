#include <iostream>
#include <queue>
#include <algorithm>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <set>
#include <vector>
#include <queue>
#include <chrono>
#include <thread>
using namespace std;

int  dx[4] = {0, 0, 1, -1};
int  dy[4] = {1, -1, 0, 0};
char dd[4] = {'D','A','S','W'};
char window[7][11], big_map[300][300];
bool visited[300][300], find_ans=false;
char buffer[1024];
int pathlen=0;

void plot_map(int cur_x, int cur_y){
    for(int i=0;i<7;i++)for(int j=0;j<11;j++)if(window[i][j]=='*') window[i][j]='.';
    for(int i=-3;i<4;i++){
        for(int j=-5;j<6;j++){
            big_map[cur_x+i][cur_y+j] = window[3+i][5+j];
            //cout << big_map[cur_x+i][cur_y+j];
        }
        //cout << endl;
    }
}

bool send_receive(int sockfd, string move){
    cout << move << endl;
    string a=move; a+='\n';
    send(sockfd, a.c_str(), strlen(a.c_str()), 0);
    usleep(350000);
    int bytesRead = -1;
    bytesRead = recv(sockfd, buffer, sizeof(buffer), 0);
    buffer[bytesRead] = '\0'; // Null-terminate the received data
    //cout << buffer << '\n';
    string temp = "";
    int linenum = 0;
    for (int i = 0; buffer[i] != '\0'; i++){
        if(buffer[i]!='\n')
            temp+=buffer[i];
        else{
            if(temp.length()==18){
                for(int j=0;j<11;j++){
                    window[linenum][j] = temp[j+7];
                    //cout << window[linenum][j];
                }
                //cout << endl;
                linenum++;
            }
            temp="";
        }
    }
    return false;
}

string path = "";
void run_maze(int sockfd, int cur_x, int cur_y, int from){
    visited[cur_x][cur_y] = true;
    for(int i=0;i<4;i++){
        int fx = cur_x + dx[i], fy = cur_y + dy[i];
        if(big_map[fx][fy] == ' '){
            string sent = path;
            send_receive(sockfd, path);
            plot_map(cur_x, cur_y);
            path = "";
        }
        if(big_map[fx][fy]!='#' && !visited[fx][fy]){
            if(big_map[fx][fy] == 'E'){
                string ans=path; ans += dd[i]; ans += '\n';
                cout << ans;
                send(sockfd, ans.c_str(), strlen(ans.c_str()), 0);
                usleep(35000);
                char buf[32768];
                recv(sockfd, buf, sizeof(buffer), 0);
                cout << buffer;
                find_ans = true;
            }
            path += dd[i];
            if(find_ans)return;
            run_maze(sockfd, fx, fy, i);
            if(find_ans)return;
        }
    }
    //Backtracking
    //cout << cur_x << ' ' << cur_y << endl;
    if(from == 0) path+=dd[1];
    else if(from == 1) path+=dd[0];
    else if(from == 2) path+=dd[3];
    else if(from == 3) path+=dd[2];
}

int main(){
    int sockfd, connfd;
    struct sockaddr_in servaddr, cli;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr("140.113.213.213");
    servaddr.sin_port = htons(10304);
    int c = connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr));
    cout << c << endl;
    char buf[1024];
    memset(buf,'\0',sizeof(buf));
    int bytesRead = recv(sockfd, buf, sizeof(buf), 0);
    buf[bytesRead] = '\0'; // Null-terminate the received data

    memset(buffer,'\0',sizeof(buffer));
    bytesRead = recv(sockfd, buffer, sizeof(buffer), 0);
    buffer[bytesRead] = '\0'; // Null-terminate the received data
    for (int i=0;i<7;i++)for(int j=0;j<11;j++) window[i][j] = ' ';
    for (int i=0;i<300;i++)for(int j=0;j<300;j++) big_map[i][j] = ' ';
    for (int i=0;i<300;i++)for(int j=0;j<300;j++) visited[i][j] = false;
    string temp="";
    int num = 0;
    for (int i = 0; buffer[i] != '\0'; i++){
        if(buffer[i]!='\n'){
            temp+=buffer[i];
        }
        else{
            num++;
            // cout<<"Str "<<num<<" : "<<temp<<'\n';
            if(num > 7 && num < 15){
                for(int j=7;j<temp.size();j++){
                    window[num-8][j-7] = temp[j];
                    //cout << temp[j];
                }
                //cout << endl;
            }
            temp="";
        }
    }

    int cur_x=150, cur_y=150;
    plot_map(cur_x, cur_y);
    run_maze(sockfd, cur_x, cur_y, 5);
    for(int i=0;i<300;i++){
        for(int j=0;j<300;j++){
            cout << big_map[i][j];
        }
        cout << endl;
    }
    cout << find_ans << endl;

    close(sockfd);
}