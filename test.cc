#include "test-lib.hpp"
#include <refl_base.hpp>

#include <boost/core/demangle.hpp>
#include <boost/dll/library_info.hpp>

#include <iostream>

int main(int argc, const char *argv[]) {
	using CastsTable = CxxFFI::CastsTable<testLoc, A, B, C, D, std::shared_ptr<A>, std::shared_ptr<B>, std::shared_ptr<C>, std::shared_ptr<D>>;
	
	std::cout << CastsTable::apply() << std::endl;
	
};
