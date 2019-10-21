#include "test-lib.hpp"

template<class Base, class Derived> Base* upcast(Derived* d){
	return d;
}


template A* upcast<A, D>(D*);
template B* upcast<B, D>(D*);
template C* upcast<C, D>(D*);
template A* upcast<A, B>(B*);
template A* upcast<A, C>(C*);

boost::dll::fs::path testLoc() {
	return boost::dll::this_line_location();
}
