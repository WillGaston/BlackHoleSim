CXX = g++
CXXFLAGS = -std=c++17 -Wall -O2 -Iglad/include
LDFLAGS = -lglfw -ldl -lGL -lm

TARGET = blackhole_gl
SRC = blackhole.cpp glad/src/glad.c

all: $(TARGET)

$(TARGET):
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)

clean:
	rm -f $(TARGET)
