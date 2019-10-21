#pragma once

#include <refl_base.hpp>
#include <boost/dll/runtime_symbol_info.hpp>

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

template<class Base, class Derived> Base* upcast(Derived* d);

boost::dll::fs::path testLoc();
