server_all:
	g++ server.cpp -o server.out -lpthread -lssl -lcrypto
	./server.out 8888
client_all:
	g++ client.cpp -o client.out -lpthread -lssl -lcrypto
	./client.out localhost 8888
compile_all:
	g++ client.cpp -o client.out -lpthread -lssl -lcrypto
	g++ server.cpp -o server.out -lpthread -lssl -lcrypto
client: client.out
	./client.out localhost 8888
compile_client:
	g++ client.cpp -o client.out -lpthread -lssl -lcrypto
server: server.out
	./server.out 8888
compile_server:
	g++ server.cpp -o server.out -lpthread -lssl -lcrypto