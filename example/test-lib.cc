#include "test-lib.hpp"

#include <casts_table.hpp>
#include <boost/dll/runtime_symbol_info.hpp>

boost::filesystem::path testLoc() {
	return boost::dll::this_line_location();
}

namespace CxxFFI {
	template<> struct APIFilter<A> {
		using type = boost::mpl::bool_<true>;
	};
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

CXXFFI_EXPOSE(castsTable, testLoc, (aRefFromDRef)(sharedBFromSharedDAnd)(sharedCFromSharedDStar));
