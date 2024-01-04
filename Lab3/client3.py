import socket
import time
from collections import deque

# Server information
SERVER_ADDRESS = 'inp.zoolab.org'
SERVER_PORT = 10303

# Create a TCP socket
client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
init_maze = [[' ' for _ in range(500)] for _ in range(500)]

# Connect to the server
try:
    client_socket.connect((SERVER_ADDRESS, SERVER_PORT))
    print(f"Connected to {SERVER_ADDRESS}:{SERVER_PORT}")
except ConnectionRefusedError:
    print("Connection refused. Server may be down or unreachable.")
    exit()
map_width, map_height = -1, -1

def get_neighbors(maze, current, visited):
    directions = [(0, 1), (0, -1), (1, 0), (-1, 0)]
    neighbors = []
    for dx, dy in directions:
        x, y = current[0] + dx, current[1] + dy
        if maze[x][y] != '#' and (x,y) not in visited:
            neighbors.append((x, y))
    return neighbors

# Breadth-First Search
def bfs(maze, start):
    queue = deque([(start, [])])
    visited = set()
    while queue:
        (x, y), path = queue.popleft()
        if maze[x][y] == 'E':
            return path
        if (x, y) not in visited:
            visited.add((x, y))
            for neighbor in get_neighbors(maze, (x, y), visited):
                queue.append((neighbor, path + [(x, y)]))
    return None

def send_receive(msg):
    msg+='\n'
    client_socket.sendall(msg.encode('utf-8'))
    data = client_socket.recv(1024).decode('utf-8')
    if data.find('Enter') == -1:
        client_socket.recv(1024).decode('utf-8')
    seq = data.split('\n')[1:8]
    #print(seq)
    return seq

def update(x, y, view):
    for i in range(-3, 4):
        for j in range(-5, 6):
            init_maze[x+i][y+j] = view[3+i][7+5+j]

start_time = time.time()
# Receive data from the server
data = client_socket.recv(1024).decode('utf-8')
data = client_socket.recv(1024).decode('utf-8')
seq = send_receive('i'*101)
seq = send_receive('j'*101)
cur_x, cur_y = 10, 10
for i in range(50):
    move = 'l'
    if i % 2 == 1:
        move = 'j'
    for j in range(30):
        if move == 'j':
            cur_y -= 11
        else:
            cur_y += 11
        v = send_receive(move*11)
        update(cur_x, cur_y, v)
    cur_x += 7
    v = send_receive('k'*7)
    update(cur_x, cur_y, v)

str_maze = ""
true_maze = [[' ' for _ in range(101)] for _ in range(101)]
num, num1 = 0, 0
for i in range(500):
    for j in range(500):
        if init_maze[i][j] != ' ':
            str_maze += init_maze[i][j]
            true_maze[num1][num] = init_maze[i][j]
            num += 1
            if num % 101 == 0:
                num=0
                num1+=1
                str_maze += '\n'
#print(str_maze)
#print(true_maze)
start = (-1, -1)
for i in range(101):
    for j in range(101):
        if true_maze[i][j] == '*':
            start = (i,j)
end = (-1, -1)
for i in range(101):
    for j in range(101):
        if true_maze[i][j] == 'E':
            end = (i,j)
#print(start)
#print(end)
path = bfs(true_maze, start)
path.append(end)
ans = ""
for i in range(len(path)-1):
    dx, dy = path[i][0]-path[i+1][0], path[i][1]-path[i+1][1]
    if dx == 1:
        ans += 'W'
    elif dx == -1:
        ans += 'S'
    elif dy == 1:
        ans += 'A'
    elif dy == -1:
        ans += 'D'
print(ans)
ans+='\n'
client_socket.sendall(ans.encode('utf-8'))
data = client_socket.recv(1024).decode('utf-8')
print(data)
data = client_socket.recv(8192).decode('utf-8')
print(data)
data = client_socket.recv(8192).decode('utf-8')
print(data)
end_time = time.time()
elapsed_time = end_time - start_time
print("Time taken:", elapsed_time, "seconds")
# Close the socket
client_socket.close()