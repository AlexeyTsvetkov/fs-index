CXX := g++
CXXFLAGS := -Wall -std=c++11 -O2

.PHONY := clean dirs
HEADERS := $(wildcard src/*.hpp)

all: dirs updatedb locate

dirs:
	mkdir -p obj/

updatedb: obj/updatedb.o
	$(CXX) $(CXXFLAGS) $< -o $@ -lboost_system -lboost_filesystem

locate: obj/locate.o
	$(CXX) $(CXXFLAGS) $< -o $@ -lboost_system -lboost_filesystem

obj/%.o: src/%.cpp $(HEADERS)
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -rf obj
	rm -f locate
	rm -f updatedb