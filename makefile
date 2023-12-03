# 编译器
CXX = g++

# 编译选项
CXXFLAGS = -std=c++11 -Wall -g

# 头文件目录
INCLUDE_DIRS = -I./include \
               -I./include/http\
               -I./include/streams
               #-I.//include/directory

# 源文件目录
SRC_DIR = src

# 目标文件目录
OBJ_DIR = obj

# 链接库目录
#LIB_DIRS = -L/path/to/ssl/library \
           -L/path/to/pthread/library \
           -L/path/to/boost/library \
           -L/path/to/yaml-cpp/library

# 链接库
LIBS = -lboost_system -lboost_filesystem \
		-lyaml-cpp -lpthread -lssl -lcrypto \
                -lz

# 源文件
SRCS = $(wildcard $(SRC_DIR)/*.cc) \
        $(wildcard $(SRC_DIR)/http/*.cc) \
        $(wildcard $(SRC_DIR)/streams/*.cc) \
        $(SRC_DIR)/example/test_iomanager.cc \
        
        #$(SRC_DIR)/example/test_address.cc \
        #$(SRC_DIR)/example/test_scheduler.cc \
        #$(SRC_DIR)/example/test_stream.cc \
        #$(SRC_DIR)/example/test_tcpServer.cc \
        #$(SRC_DIR)/example/test_hook.cc \
        #$(SRC_DIR)/example/test_socket.cc \
        #$(SRC_DIR)/example/test_fiber.cc \
        #$(SRC_DIR)/example/test_myfiber.cc \
        #$(SRC_DIR)/example/test_http_connect.cc \
        #$(SRC_DIR)/example/echo_server.cc \
        $(SRC_DIR)/example/main.cc \
        $(SRC_DIR)/example/test_http_server.cc \
        $(SRC_DIR)/example/test_log.cc \
        


# 目标文件
OBJS = $(patsubst $(SRC_DIR)/%.cc,$(OBJ_DIR)/%.o,$(SRCS))

# 可执行文件
TARGET = your_executable

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(INCLUDE_DIRS) -o $@ $(OBJS) $(LIB_DIRS) $(LIBS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cc
	$(CXX) $(CXXFLAGS) $(INCLUDE_DIRS) -c $< -o $@

.PHONY: clean
clean:
	rm -f $(OBJ_DIR)/*.o $(TARGET) $(OBJ_DIR)/http/*.o \
                $(OBJ_DIR)/example/*.o $(OBJ_DIR)/streams/*.o 
