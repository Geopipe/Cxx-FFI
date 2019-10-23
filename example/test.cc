#include "test-lib.hpp"
#include <refl_base.hpp>

#include <boost/core/demangle.hpp>
#include <boost/dll/library_info.hpp>

#include <iostream>

extern "C" {
	extern const char * castsTable();
}

int main(int argc, const char *argv[]) {
	std::cout << castsTable() << std::endl;
};
