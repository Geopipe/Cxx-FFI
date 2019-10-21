#pragma once

#include <refl_base.hpp>
#include <iostream>

#include <boost/dll/runtime_symbol_info.hpp>
#include <boost/dll/shared_library.hpp>
#include <boost/dll/library_info.hpp>
#include <boost/dll/smart_library.hpp>
#include <boost/dll/import_mangled.hpp>

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
