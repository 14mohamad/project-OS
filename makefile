# Compiler
CXX = g++

# Compiler flags
CXXFLAGS = -std=c++17 -pthread -fprofile-arcs -ftest-coverage

# Source files
CLIENT_SRC = combined_client.cpp
SERVER_SRC = lf_server.cpp
SERVERPIPE_SRC = pipe_server.cpp

# Output binaries
CLIENT_BIN = combinedclient
SERVER_BIN = lf_server
SERVERPIPE_BIN = pipe_server

# Default target
all: $(CLIENT_BIN) $(SERVER_BIN) $(SERVERPIPE_BIN)

# Rule to build client
$(CLIENT_BIN): $(CLIENT_SRC)
	$(CXX) $(CXXFLAGS) -o $@ $<

# Rule to build server
$(SERVER_BIN): $(SERVER_SRC)
	$(CXX) $(CXXFLAGS) -o $@ $<

# Rule to build serverpipe
$(SERVERPIPE_BIN): $(SERVERPIPE_SRC)
	$(CXX) $(CXXFLAGS) -o $@ $<

# Clean up the build artifacts and coverage files
clean:
	find . -type f \( -name "*.gcov" -o -name "*.gcda" -o -name "*.gcno" \) -delete
	rm -f $(CLIENT_BIN) $(SERVER_BIN) $(SERVERPIPE_BIN)

.PHONY: all clean
