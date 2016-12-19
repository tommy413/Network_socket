client: client.cpp
	g++ client.cpp -o client.out -lpthread
	./client.out localhost 8888

server: server.cpp
	g++ server.cpp -o server.out -lpthread
	./server.out 8888