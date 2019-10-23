#include <iostream>

extern "C" {
	extern const char * castsTable();
}

int main(int argc, const char *argv[]) {
	std::cout << castsTable() << std::endl;
};
