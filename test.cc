#include <refl_base.hpp>
#include <iostream>

struct A {
	using ReflBases = CxxFFI::DefineBases<>;
};

struct B : virtual A {
	using ReflBases = CxxFFI::DefineBases<A>;
};

struct C : virtual A {
	using ReflBases = CxxFFI::DefineBases<A>;
};

struct D : B, C {
	using ReflBases = CxxFFI::DefineBases<B, C>;
};

int main(int argc, const char *argv[]) {
	using init_set = boost::mpl::set<>;
	using setA = boost::mpl::insert<init_set, A>::type;
	using setAB = boost::mpl::insert<setA, B>::type;
	
	using initSetAB = boost::mpl::set<A,B>;
	std::cout << CxxFFI::detail::readableName<init_set>() << " -> " << CxxFFI::detail::readableName<setA>() << std::endl;
	std::cout << CxxFFI::detail::readableName<setA>() << " -> " << CxxFFI::detail::readableName<setAB>() << std::endl;
	std::cout << CxxFFI::detail::readableName<setAB>() << " =? " << CxxFFI::detail::readableName<initSetAB>() << std::endl;
	std::cout << CxxFFI::CastsTable<A, B, C, D>::apply() << std::endl;
};
