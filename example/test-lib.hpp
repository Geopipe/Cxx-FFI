#pragma once

#include <cxx-ffi/refl_base.hpp>

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
