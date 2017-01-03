client: client.cpp
	g++ client.cpp -o client.out -lpthread -lssl -lcrypto
	./client.out localhost 8888

server: server.cpp
	g++ server.cpp -o server.out -lpthread -lssl -lcrypto
	./server.out 8888