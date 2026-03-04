solver : main.cpp
	g++ -std=c++17 -o solver main.cpp -ljsoncpp

clean :
	rm -f solver