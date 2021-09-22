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

/******************************************************
 * Tools to generate a description of an API's class
 * hierarchy so that languages with C FFIs can emulate
 * C++'s inheritance rules and implicit casting for
 * derived types.
 ******************************************************/
namespace CxxFFI {
	/******************************************************
	 * A metafunction to reflect via SFINAE if a struct 
	 * or class has a member-typed named `ReflBases`
	 * We depend on `ReflBases` enumerating reflected bases
	 * to generate our FFI specification.
	 ******************************************************/
	BOOST_TTI_HAS_TYPE(ReflBases);
	
	/// A helper type for client code to expose any base classes that should be in the public API
	template<typename ...Bases> using DefineBases = boost::mpl::vector<Bases...>;
	
	/**************************************************
	 * Internal implementation details
	 **************************************************/
	namespace detail {
		/// Default implementation of `CxxFFI::ReflBases`.
		template<typename T, bool b = has_type_ReflBases<T>::value> struct MaybeBases {
			using type = typename T::ReflBases;
		};
		
		/// Specialization of `MaybeBases` for classes which do not reflect any base classes.
		template<typename T> struct MaybeBases<T, false> {
			using type = DefineBases<>;
		};
	}
	
	/**************************************************
	 * Metafunction returning the reflected base classes 
	 * for `T`. All structs and classes cooperating with 
	 * #CXXFFI_EXPOSE should include boilerplate of the 
	 * form:
	 * <pre class="markdeep">
	 * ```c++
	 * struct X: Y, Z {
	 *   using ReflBases = DefineBases<Y, Z>
	 *   // Whatever else you wanted to type
	 * };
	 * ```
	 * </pre>
	 * Default implementation is via `detail::MaybeBases`.
	 * Client code may provide specializations to
	 * implement more complex typing rules.
	 **************************************************/
	template<typename T> struct ReflBases {
		using type = typename detail::MaybeBases<T>::type;
	};
	
	/**************************************************
	 * Metafunction implementing covariant typing rules
	 * for templated classes.
	 * For some `T<X>`, if `T<>` is covariant in `X`,
	 * then `T<A> : T<B>` _iff_ `A : B`
	 * (contravariance would be `T<A> : T<B>` _iff_ `B : A`).
	 * 
	 * C++ doesn't have any native notion of covariance
	 * and contravariance, except sort of for function types
	 * (`R(A)` is covariant in `R` and contravariant in `A`),
	 * because it defines inheritance in terms of "subobjects"
	 * and not "subtypes". However, many useful template classes
	 * emulate covariance. For example `std::shared_ptr`
	 * and `std::unique_ptr` both cooperate with 
	 * `std::dynamic_pointer_cast` and `std::static_pointer_cast`
	 * to emulate the casting rules you would expect for
	 * bare pointers.
	 **************************************************/
	template<template<typename Tp> class PType, typename T> struct CoVariantBases {
		using type = typename boost::mpl::transform<typename ReflBases<T>::type, PType<boost::mpl::_1>>::type;
	};
	
	/// Specialiation of `ReflBases` for `std::shared_ptr` using `CoVariantBases`.
	template<typename T> struct ReflBases<std::shared_ptr<T>> {
		using type = typename CoVariantBases<std::shared_ptr, T>::type;
	};
	
	namespace detail {
		/// Default implementation of a cast from `Derived` to `Base`, assuming `Derived : Base`.
		template<typename Derived, typename Base> struct Upcaster {
			static Base* apply(Derived* derived){
				return derived;
			}
		};
		
		/// Specialization of `Upcaster` for emulated covariance in `std::shared_ptr` using `std::dynamic_pointer_cast`
		template<typename Derived, typename Base>
		struct Upcaster<std::shared_ptr<Derived>, std::shared_ptr<Base>> {
			static std::shared_ptr<Base>* apply(std::shared_ptr<Derived>* derived) {
				// We should really add an extra branch in here to check if we can use a static_pointer_cast instead
				// but this is simpler for now, and deals with multiple inheritance
				return new std::shared_ptr<Base>(std::dynamic_pointer_cast<Base>(*derived));
			}
		};
	}
	
	/// Must be instantiated for every pair of related types you want exposed in your FFI. See #CXXFFI_EXPOSE.
	template<typename Derived, typename Base> Base* upcast(Derived* derived){
		return detail::Upcaster<Derived, Base>::apply(derived);
	}
}
