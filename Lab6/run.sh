Token=Q6g5P3f8B3dJ4tgp
g++ server.cpp -static -o bin/server.exe
g++ client.cpp -static -o bin/client.exe
TOKEN=$Token python3 submit.py bin/server.exe bin/client.exe 