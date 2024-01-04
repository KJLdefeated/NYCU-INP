import socket
import time
from collections import deque

# Server information
SERVER_ADDRESS = 'inp.zoolab.org'
SERVER_PORT = 10302

# Create a TCP socket
client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

# Connect to the server
try:
    client_socket.connect((SERVER_ADDRESS, SERVER_PORT))
    print(f"Connected to {SERVER_ADDRESS}:{SERVER_PORT}")
except ConnectionRefusedError:
    print("Connection refused. Server may be down or unreachable.")
    exit()
map_width, map_height = -1, -1
maze = []

def get_neighbors(maze, current, visited):
    directions = [(0, 1), (0, -1), (1, 0), (-1, 0)]
    neighbors = []
    for dx, dy in directions:
        x, y = current[0] + dx, current[1] + dy
        if 0 <= x < map_height and 0 <= y < map_width and maze[x*map_width + y] != '#' and (x,y) not in visited:
            neighbors.append((x, y))
    return neighbors

# Breadth-First Search
def bfs(maze, start, end):
    queue = deque([(start, [])])
    visited = set()
    while queue:
        (x, y), path = queue.popleft()
        #print(x,y)
        if maze[x*map_width + y] == 'E':
            return path
        if (x, y) not in visited:
            visited.add((x, y))
            for neighbor in get_neighbors(maze, (x, y), visited):
                queue.append((neighbor, path + [(x, y)]))
    return None

# Receive data from the server
while True:
    data = client_socket.recv(1024).decode('utf-8')
    print(data)
    if not data:
        break
    if data.find('Note') >= 0:
        if SERVER_PORT >= 10303:
            data1 = data.split('Note2')[-1]
            dimensions = data1.split(' = ')[-1].split('\n')[0].split('x')
            part_width, part_height = map(int, dimensions)
            print(part_width, part_height)
            
            data = data.split('Note')[1]
            dimensions = data.split(' = ')[-1].split('\n')[0].split('x')
            map_width, map_height = map(int, dimensions)
            print(map_width, map_height)

        data1 = data.split('Note')[1]
        dimensions = data1.split(' = ')[-1].split('\n')[0].split('x')
        map_width, map_height = map(int, dimensions)
        #print(map_width, map_height)
        data = data.split('Note')[-1]
    
    if map_width != -1 and data.find('#') >= 0:
        cur = min(data.find('#'), data.find('.'))
        while len(maze) != map_height * map_width and cur < len(data):
            if data[cur] != '\n': maze.append(data[cur])
            cur += 1

    if len(maze) == map_height * map_width:
        start = [(row_idx, col_idx) for row_idx, row in enumerate(maze) for col_idx, char in enumerate(row) if char == '*'][0][0]
        end = [(row_idx, col_idx) for row_idx, row in enumerate(maze) for col_idx, char in enumerate(row) if char == 'E'][0][0]
        start = (int(start//map_width), start%map_width)
        end = (int(end//map_width), end%map_width)
        print(start, end)
        path = bfs(maze, start, end)
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
        ans+='\n'
        print(ans)
        client_socket.sendall(ans.encode('utf-8'))
        data = client_socket.recv(1024).decode('utf-8')
        print(data)
       

        


# Close the socket
client_socket.close()