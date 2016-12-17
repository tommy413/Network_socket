client: client.cpp
	g++ client.cpp -o client.out
	./client.out 140.112.107.39 8889

server: server.cpp
	g++ server.cpp -o server.out
	./server.out