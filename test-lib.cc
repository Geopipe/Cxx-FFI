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

	template std::shared_ptr<A>* upcast<std::shared_ptr<D>, std::shared_ptr<A>>(std::shared_ptr<D>*);
	template std::shared_ptr<B>* upcast<std::shared_ptr<D>, std::shared_ptr<B>>(std::shared_ptr<D>*);
	template std::shared_ptr<C>* upcast<std::shared_ptr<D>, std::shared_ptr<C>>(std::shared_ptr<D>*);
	template std::shared_ptr<A>* upcast<std::shared_ptr<B>, std::shared_ptr<A>>(std::shared_ptr<B>*);
	template std::shared_ptr<A>* upcast<std::shared_ptr<C>, std::shared_ptr<A>>(std::shared_ptr<C>*);
}

boost::filesystem::path testLoc() {
	return boost::dll::this_line_location();
}

