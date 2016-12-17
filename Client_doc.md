# Client.cpp 說明文件

## 如何編譯並運行

1. By makefile : 將makefile檔(client_mkfile.GNUmakefile)放到與cpp檔相同路徑下並執行$make

2. By command line : 用terminal在cpp檔所在位置執行：
					 $g++ Client.cpp -o Client.out
					 $Client.out [server ip] [server port]

## 執行程式

用terminal在cpp檔所在位置執行Client.out : ./Client.out [arg1] [arg2]
arg1:目標ip(server) arg2:目標port

##需求環境

- Linux OS
- gcc version 4.8.2

