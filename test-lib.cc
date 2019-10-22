#include "test-lib.hpp"

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

A& aRefFromDRef(D& d){
	return d;
}

std::shared_ptr<B> sharedBFromSharedDAnd(std::shared_ptr<D> &d) {
	return std::static_pointer_cast<B>(d);
}

std::shared_ptr<C> sharedCFromSharedDStar(std::shared_ptr<D> *d) {
	return std::static_pointer_cast<C>(*d);
}

using APIFuncs = boost::mpl::vector<decltype(aRefFromDRef),decltype(sharedBFromSharedDAnd),decltype(sharedCFromSharedDStar)>;
using APITypes = typename CxxFFI::DiscoverAPITypes::apply<APIFuncs>::type;
using CastsTable = CxxFFI::CastsTable<testLoc, APITypes>;

const char * castsTable() {
	return CastsTable::apply();
}
