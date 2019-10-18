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
	std::cout << CxxFFI::CastsTable<A, B, C, D, std::shared_ptr<D>>::apply() << std::endl;
};
