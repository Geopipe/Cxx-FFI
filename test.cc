#include "test-lib.hpp"
#include <refl_base.hpp>

#include <boost/core/demangle.hpp>
#include <boost/dll/library_info.hpp>

#include <iostream>

int main(int argc, const char *argv[]) {
	std::cout << CxxFFI::CastsTable<A, B, C, D, std::shared_ptr<D>>::apply() << std::endl;
	
	auto here = testLoc();
	auto inf = boost::dll::library_info(here);
	
	std::vector<std::string> exports = inf.symbols("__text");
	if(!exports.size()) {
		exports = inf.symbols(".text");
	}
	for(size_t i = 0; i < exports.size(); i++){
		std::cout << exports[i] << " -> " << boost::core::demangle(exports[i].c_str()) << std::endl;
	}
	
	
	
};
