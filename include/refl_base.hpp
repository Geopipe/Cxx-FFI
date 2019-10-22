//
//  refl_base.hpp
//  Cxx-FFI
//
//  Created by Thomas Dickerson on 10/18/19.
//
//


#pragma once

#include <boost/core/demangle.hpp>

#include <boost/mpl/accumulate.hpp>
#include <boost/mpl/at.hpp>
#include <boost/mpl/back_inserter.hpp>
#include <boost/mpl/contains.hpp>
#include <boost/mpl/copy.hpp>
#include <boost/mpl/erase_key.hpp>
#include <boost/mpl/key_type.hpp>
#include <boost/mpl/pop_front.hpp>
#include <boost/mpl/set.hpp>
#include <boost/mpl/transform.hpp>
#include <boost/mpl/vector.hpp>

#include <re2/re2.h>

#include <map>
#include <memory>
#include <sstream>
#include <type_traits>

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
				return new std::shared_ptr<Base>(std::dynamic_pointer_cast<Base>(derived));
			}
		};
	}
	
	template<typename Derived, typename Base> Base* upcast(Derived* derived){
		return detail::Upcaster<Derived, Base>::apply(derived);
	}
	
	
	namespace detail {
		using namespace boost::mpl;
		
		template<typename Left, typename Right> struct Concatenate {
			using type = typename copy< Right, back_inserter<Left> >::type;
		};
		
		template<typename Here, typename WIP, typename Done>
		class VisitBases;
		
		struct VisitLoopInner {
			template<typename OldState, typename Child> class apply {
				using Here = typename at<OldState, int_<0>>::type;
				using OldWIP = typename at<OldState, int_<1>>::type;
				using OldDone = typename at<OldState, int_<2>>::type;
				using MaybeTail = typename at<OldState, int_<3>>::type;
				
				using VisitBasesThunk = VisitBases<Child, OldWIP, OldDone>;
				using FinalWIP = typename VisitBasesThunk::FinalWIP;
				using FinalDone = typename VisitBasesThunk::FinalDone;
				using MaybeFront = typename VisitBasesThunk::type;
				
				using JoinedList = typename Concatenate<MaybeFront, MaybeTail>::type;
			public:
				using type = vector<Here, FinalWIP, FinalDone, JoinedList> ;
				
			};
		};
		
		template<typename Here, typename WIP, typename Done> class VisitLoop {
			using InitWIP = WIP;
			using NextWIP = typename insert<InitWIP, Here>::type;
			using ReflBases = typename ReflBases<Here>::type;
			using InitState = vector<Here, NextWIP, Done, vector<>>;
			
			using AccumulatedState = typename accumulate<ReflBases, InitState, VisitLoopInner>::type;
			using AccumulatedWIP = typename at<AccumulatedState, int_<1>>::type;
			using AccumulatedDone = typename at<AccumulatedState, int_<2>>::type;
			using AccumulatedTail = typename at<AccumulatedState, int_<3>>::type;
		public:
			using FinalWIP = typename erase_key<AccumulatedWIP, typename key_type<AccumulatedWIP, Here>::type>::type;
			using FinalDone = typename insert<AccumulatedDone, Here>::type;
			using type = typename push_front<AccumulatedTail, Here>::type;
		};
		
		template<typename Here, typename WIP, typename Done>
		class VisitBases {
			static_assert(!contains<WIP, Here>::value, "Cycle while toposorting base classes. Your inheritance is broken");
			template<bool done> class JointIf {
			public:
				using FinalWIP = WIP;
				using FinalDone = Done;
				using type = vector<>;
			};
			
			template<> class JointIf<false> {
				using VisitLoopThunk = VisitLoop<Here, WIP, Done>;
			public:
				using FinalWIP = typename VisitLoopThunk::FinalWIP;
				using FinalDone = typename VisitLoopThunk::FinalDone;
				using type = typename VisitLoopThunk::type;
			};
			
			using DoneHere = typename contains<Done, Here>::type;
			using IfThunk = JointIf<DoneHere::value>;
		public:
			using FinalWIP = typename IfThunk::FinalWIP;
			using FinalDone = typename IfThunk::FinalDone;
			using type = typename IfThunk::type;
		};
		
	}
	
	 struct ToposortBases {
		 template<typename T> class apply {
			 using WIP = boost::mpl::set<>;
			 using Done= boost::mpl::set<>;
		 public:
			 using type =typename detail::VisitBases<T, WIP, Done>::type;
		 };
	};
	
	template<typename T> auto operator<<(std::ostream& os, const T& t) -> decltype(t(os), os)
	{
		t(os);
		return os;
	}
	
	namespace detail {
		using namespace boost::mpl;
		
		template<typename Start, typename End> std::string maybeComma() {
			return std::is_same<Start, End>::value ? "" : ", ";
		}
		
		template<typename T> std::string readableName() {
			return boost::core::demangle(typeid(T).name());
		}
		
		template<typename Start, typename End> struct CastsTableSubEntries {
			using Here = typename deref<Start>::type;
			using Next = typename next<Start>::type;
			std::ostream& operator()(std::ostream& o) const {
				return o << readableName<Here>() << maybeComma<Next, End>() << CastsTableSubEntries<Next, End>();
			}
		};
		template<typename End> struct CastsTableSubEntries<End, End> {
			std::ostream& operator()(std::ostream& o) const {
				return o;
			}
		};
		
		template<typename T> struct CastsTableEntry {
			using TopoSorted = typename pop_front<typename ToposortBases::apply<T>::type>::type;
			using Begin = typename begin<TopoSorted>::type;
			using End = typename end<TopoSorted>::type;
			std::ostream& operator()(std::ostream& o) const {
				return o << "[" << readableName<T>() << ", [" << CastsTableSubEntries<Begin, End>() << "]]" ;
			}
		};
		
		template<typename Start, typename End> struct CastsTableEntries {
			using Here = typename deref<Start>::type;
			using Next = typename next<Start>::type;
			std::ostream& operator()(std::ostream& o) const {
				return o << CastsTableEntry<Here>() << maybeComma<Next, End>() << CastsTableEntries<Next, End>();
			}
		};
		
		template<typename End> struct CastsTableEntries<End, End> {
			std::ostream& operator()(std::ostream& o) const {
				return o;
			}
		};
	}
	
	template<typename ...T> class CastsTable {
		using StartTypes = boost::mpl::vector<T...>;
		using Hierarchy = boost::mpl::transform<typename boost::mpl::begin<StartTypes>::type,
												typename boost::mpl::end<StartTypes>::type,
												ToposortBases>;
		static std::map<std::string, std::string> genKnownCasts() {
			
		}
		
		static const std::map<std::string, std::string>& knownCasts() {
			static std::map<std::string, std::string> ans = genKnownCasts();
			return ans;
		}
		
		static std::string genCastsTable() {
			std::ostringstream o;
			o << "[" << detail::CastsTableEntries<typename boost::mpl::begin<StartTypes>::type,
			typename boost::mpl::end<StartTypes>::type>() << "]";
			return o.str();
		}
		
		static const std::string& castsTable() {
			static std::string ans = genCastsTable();
			return ans;
			
		};
		
	public:
		static const char * apply() {
			return castsTable().c_str();
		}
	};
}

