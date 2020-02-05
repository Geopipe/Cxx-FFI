#pragma once
/************************************************************************************
 *
 * Author: Thomas Dickerson
 * Copyright: 2019 - 2020, Geopipe, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 ************************************************************************************/

#include <boost/mpl/transform.hpp>
#include <boost/mpl/vector.hpp>

#include <boost/tti/has_type.hpp>

#include <memory>

namespace CxxFFI {
	
	BOOST_TTI_HAS_TYPE(ReflBases);
	
	template<typename ...Bases> using DefineBases = boost::mpl::vector<Bases...>;
	
	namespace detail {
		template<typename T, bool b = has_type_ReflBases<T>::value> struct MaybeBases {
			using type = typename T::ReflBases;
		};
		
		template<typename T> struct MaybeBases<T, false> {
			using type = DefineBases<>;
		};
	}
	
	template<typename T> struct ReflBases {
		using type = typename detail::MaybeBases<T>::type;
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
