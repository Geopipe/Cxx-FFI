//
//  refl_base.hpp
//  Cxx-FFI
//
//  Created by Thomas Dickerson on 10/18/19.
//
//


#pragma once

#include <boost/mpl/transform.hpp>
#include <boost/mpl/vector.hpp>

#include <memory>

namespace CxxFFI {
	
	template<typename ...Bases> using DefineBases = boost::mpl::vector<Bases...>;
	
	template<typename T> struct ReflBases {
		using type = typename T::ReflBases;
	};
	
	template<template<typename Tp> class PType, typename T> struct CoVariantBases {
		using type = typename boost::mpl::transform<typename ReflBases<T>::type, PType<boost::mpl::_1>>::type;
	};
	
	template<typename T> struct ReflBases<std::shared_ptr<T>> {
		using type = typename CoVariantBases<std::shared_ptr, T>::type;
	};
	
	namespace detail {
		template<typename Derived, typename Base> struct Upcaster {
			static Base* apply(Derived* derived){
				return derived;
			}
		};
		
		template<typename Derived, typename Base>
		struct Upcaster<std::shared_ptr<Derived>, std::shared_ptr<Base>> {
			static std::shared_ptr<Base>* apply(std::shared_ptr<Derived>* derived) {
				// We should really add an extra branch in here to check if we can use a static_pointer_cast instead
				// but this is simpler for now, and deals with multiple inheritance
				return new std::shared_ptr<Base>(std::dynamic_pointer_cast<Base>(*derived));
			}
		};
	}
	
	template<typename Derived, typename Base> Base* upcast(Derived* derived){
		return detail::Upcaster<Derived, Base>::apply(derived);
	}
}
