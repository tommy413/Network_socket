client: client.cpp
	g++ client.cpp -o client.out -lpthread
	./client.out 140.112.107.39 8889

server: server.cpp
	g++ server.cpp -o server.out -lpthread
	./server.out 8888