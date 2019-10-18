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
	
	template<typename T> struct ToposortBases {
		using WIP = boost::mpl::set<>;
		using Done= boost::mpl::set<>;
		using type =typename detail::VisitBases<T, WIP, Done>::type;
	};
	
	template<typename T> auto operator<<(std::ostream& os, const T& t) -> decltype(t(os), os)
	{
		t(os);
		return os;
	}
	
	namespace detail {
		using namespace boost::mpl;
		
		template<typename ...R> std::string maybeComma() {
			return (sizeof...(R) == 0) ? "" : ", ";
		}
		
		template<typename Start, typename End> std::string itMaybeComma() {
			return std::is_same<Start, End>::value ? "" : ", ";
		}
		
		template<typename T> std::string readableName() {
			return boost::core::demangle(typeid(T).name());
		}
		
		template<typename T, typename Start, typename End> struct CastsTableSubEntries {
			using Here = typename deref<Start>::type;
			using Next = typename next<Start>::type;
			std::ostream& operator()(std::ostream& o) const {
				return o << readableName<Here>() << itMaybeComma<Next, End>() << CastsTableSubEntries<T, Next, End>();
			}
		};
		template<typename T, typename End> struct CastsTableSubEntries<T, End, End> {
			std::ostream& operator()(std::ostream& o) const {
				return o;
			}
		};
		
		template<typename T> struct CastsTableEntry {
			using TopoSorted = typename pop_front<typename ToposortBases<T>::type>::type;
			std::ostream& operator()(std::ostream& o) const {
				return o << "[" << readableName<T>() << ", [" << CastsTableSubEntries<T, typename begin<TopoSorted>::type, typename end<TopoSorted>::type>() << "]]" ;
			}
		};
		
		template<typename ...T> struct CastsTableEntries {
			std::ostream& operator()(std::ostream& o) const {
				return o;
			}
		};
		
		template<typename H, typename ...R> struct CastsTableEntries<H, R...> {
			std::ostream& operator()(std::ostream& o) const {
				return o << CastsTableEntry<H>() << maybeComma<R...>() << CastsTableEntries<R...>();
			}
		};
	}
	
	template<typename ...T> class CastsTable {
		static std::string genCastsTable() {
			std::ostringstream o;
			o << "[" << detail::CastsTableEntries<T...>() << "]";
			return o.str();
		}
		
		static const std::string castsTable;
	public:
		static const char * apply() {
			return castsTable.c_str();
		}
	};
	
	template<typename ...T> const std::string CastsTable<T...>::castsTable = CastsTable<T...>::genCastsTable();
}

