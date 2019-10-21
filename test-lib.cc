#include "test-lib.hpp"

template<class Base, class Derived> Base* upcast(Derived* d){
	return d;
}


namespace CxxFFI {
	template A* upcast<D, A>(D*);
	template B* upcast<D, B>(D*);
	template C* upcast<D, C>(D*);
	template A* upcast<B, A>(B*);
	template A* upcast<C, A>(C*);
}

boost::filesystem::path testLoc() {
	return boost::dll::this_line_location();
}
