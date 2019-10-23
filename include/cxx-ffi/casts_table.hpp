//
//  casts_table.hpp
//  Cxx-FFI
//
//  Created by Thomas Dickerson on 10/18/19.
//
//


#pragma once

#include <boost/core/demangle.hpp>

#include <boost/dll/library_info.hpp>

#include <boost/mpl/accumulate.hpp>
#include <boost/mpl/at.hpp>
#include <boost/mpl/back_inserter.hpp>
#include <boost/mpl/contains.hpp>
#include <boost/mpl/copy.hpp>
#include <boost/mpl/copy_if.hpp>
#include <boost/mpl/erase_key.hpp>
#include <boost/mpl/key_type.hpp>
#include <boost/mpl/pop_front.hpp>
#include <boost/mpl/set.hpp>
#include <boost/mpl/transform.hpp>
#include <boost/mpl/vector.hpp>

#include <boost/preprocessor.hpp>

#include <re2/re2.h>

#include <iomanip>

#ifdef DEBUG
#include <iostream>
#endif

#include <map>
#include <memory>
#include <sstream>
#include <type_traits>

#include <cxx-ffi/refl_base.hpp>



namespace CxxFFI {
	
	
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
		
		template<typename Start, typename End> std::string maybeSeparator(std::string sep = ", ") {
			return std::is_same<Start, End>::value ? "" : sep;
		}
		
		template<typename T> std::string readableName() {
			return boost::core::demangle(typeid(T).name());
		}
		
		template<typename Derived, typename Start, typename End> struct CastsTableSubEntries {
			using Here = typename deref<Start>::type;
			using Next = typename next<Start>::type;
			using CastFunc = Here*(*)(Derived*);
			
			std::string &derivedName;
			std::map<std::string, std::string> &knownCasts;
			std::ostream& operator()(std::ostream& o) const {
				constexpr const CastFunc instantiateMe = &upcast<Derived, Here>;
				static_assert(instantiateMe != nullptr, "The compiler is optimizing badly");
				std::string baseName = readableName<Here>();
				std::string castSymbol = knownCasts[baseName];
				if (castSymbol.length()) {
					o << "[" << std::quoted(baseName) << ", " << std::quoted(castSymbol) << "]" << maybeSeparator<Next, End>();
				}
#ifdef DEBUG
				else {
					std::cerr << "Warning: couldn't find upcast from " << derivedName << " to " << baseName << std::endl;
				}
#endif
				return o << CastsTableSubEntries<Derived, Next, End>{derivedName, knownCasts};
			}
		};
		template<typename Derived, typename End> struct CastsTableSubEntries<Derived, End, End> {
			std::string &derivedName;
			std::map<std::string, std::string> &knownCasts;
			std::ostream& operator()(std::ostream& o) const {
				return o;
			}
		};
		
		template<typename TopoSorted> struct CastsTableEntry {
			using Derived = typename at<TopoSorted, int_<0>>::type;
			using Bases = typename pop_front<TopoSorted>::type;
			using Begin = typename begin<Bases>::type;
			using End = typename end<Bases>::type;
			std::map<std::string, std::map<std::string, std::string> > &knownCasts;
			std::ostream& operator()(std::ostream& o) const {
				std::string derivedName = readableName<Derived>();
				return o << "[" << std::quoted(derivedName) << ", [" << CastsTableSubEntries<Derived, Begin, End>{derivedName, knownCasts[derivedName]} << "]]" ;
			}
		};
		
		template<typename Start, typename End> struct CastsTableEntries {
			using Here = typename deref<Start>::type;
			using Next = typename next<Start>::type;
			
			std::map<std::string, std::map<std::string, std::string> > &knownCasts;
			std::ostream& operator()(std::ostream& o) const {
				return o << CastsTableEntry<Here>{knownCasts} << maybeSeparator<Next, End>() << CastsTableEntries<Next, End>{knownCasts};
			}
		};
		
		template<typename End> struct CastsTableEntries<End, End> {
			std::map<std::string, std::map<std::string, std::string> > &knownCasts;
			std::ostream& operator()(std::ostream& o) const {
				return o;
			}
		};
		
		struct Vec2Set {
			template<typename Vec> struct apply {
				using type = typename fold<Vec, set0<>, insert<_1,_2>>::type;
			};
		};
		
		struct SetUnion {
			template<typename left, typename right> struct apply {
				using type = typename copy<right, inserter<left, insert<_1, _2>>>::type;
			};
		};
		
		template<typename Start, typename End> struct EscapedTypeNames {
			using Here = typename deref<Start>::type;
			using Next = typename next<Start>::type;
			std::ostream& operator()(std::ostream& o) const {
				return o << "(?:" << re2::RE2::QuoteMeta(readableName<Here>()) << ")" << maybeSeparator<Next,End>("|") << EscapedTypeNames<Next, End>();
			}
		};
		
		template<typename End> struct EscapedTypeNames<End, End> {
			std::ostream& operator()(std::ostream& o) const {
				return o;
			}
		};
		
		template<typename KnownTypes> struct MatchKnownTypes {
			using Begin = typename begin<KnownTypes>::type;
			using End = typename end<KnownTypes>::type;
			static std::string apply() {
				std::ostringstream o;
				o << "(" << EscapedTypeNames<Begin, End>() << ")";
				return o.str();
			};
		};
		
		std::vector<std::string> symbolTable(boost::dll::library_info &inf) {
			std::vector<std::string> exports = inf.symbols("__text");
			if(exports.size()) {
#ifdef DEBUG
				std::cout << "Found symbols in __text: probably a Mach-O environment" << std::endl;
#endif
			} else {
				exports = inf.symbols(".text");
#ifdef DEBUG
				if(exports.size()) {
					std::cout << "Found symbols in .text: probably an ELF environment" << std::endl;
				} else {
					std::cerr << "No symbols found. Might need help on this platform" << std::endl;
				}
#endif
			}
			return exports;
		}
		
		template<typename SeedTypes> struct FilterUnused {
			template<typename Sorted> struct apply {
				using type = typename copy_if<Sorted, contains<SeedTypes, _1> >::type;
			};
		};
	}
	
	template<boost::filesystem::path(*libraryLocation)(), typename SeedTypes> class CastsTable {
		using Hierarchy = typename boost::mpl::transform<SeedTypes, ToposortBases, boost::mpl::back_inserter<boost::mpl::vector0<>>>::type; // SeedTypes may be associative
		using HierarchyFiltered = typename boost::mpl::transform<Hierarchy, detail::FilterUnused<SeedTypes>>::type;
		using KnownTypes = typename boost::mpl::fold<typename boost::mpl::transform<HierarchyFiltered,detail::Vec2Set>::type, boost::mpl::set0<>, detail::SetUnion>::type;
		using MatchKnownTypes = detail::MatchKnownTypes<KnownTypes>;
		
		static std::string& matchKnownTypes() {
			static std::string ans = MatchKnownTypes::apply();
			return ans;
		}
		
		static std::map<std::string, std::map<std::string, std::string> > genKnownCasts() {
			std::string& knownTypes = matchKnownTypes();
			std::string matchUpcastSrc = knownTypes + re2::RE2::QuoteMeta("*") + "\\s+" + re2::RE2::QuoteMeta("CxxFFI::upcast<") + knownTypes + re2::RE2::QuoteMeta(",") + "\\s*" + knownTypes + "\\s*" + re2::RE2::QuoteMeta(">(") + knownTypes + re2::RE2::QuoteMeta("*)");
#ifdef DEBUG
			std::cout << "Parsing symbols via " << matchUpcastSrc << std::endl;
#endif
			re2::RE2 matchUpcast(matchUpcastSrc);
			
			std::map<std::string, std::map<std::string, std::string> > knownCasts;
			boost::dll::library_info inf(libraryLocation());
			std::vector<std::string> exports(detail::symbolTable(inf));
			std::string returnType, derivedType, baseType, argType;
			for(std::string symbol : exports) {
				std::string readable(boost::core::demangle(symbol.c_str()));
				if(re2::RE2::FullMatch(readable, matchUpcast, &returnType, &derivedType, &baseType, &argType)) {
					if(returnType == baseType && argType == derivedType) {
#ifdef DEBUG
						std::cout << "knownCasts[" << argType << "][" << returnType << "] = " << symbol << std::endl;
#endif
						knownCasts[argType][returnType] = symbol;
					}
#ifdef DEBUG
					else {
						std::cerr << symbol << " parses as an upcast, but the types don't match: " << readable << std::endl;
					}
#endif
				}
#ifdef DEBUG
				else {
					std::cout << "Skipping unmatched symbol " << symbol << " (aka " << readable << " )" << std::endl;
				}
#endif
			}
			
			return knownCasts;
		}
		
		static std::map<std::string, std::map<std::string, std::string> >& knownCasts() {
			static std::map<std::string, std::map<std::string, std::string> > ans = genKnownCasts();
			return ans;
		}
		
		static std::string genCastsTable() {
			std::ostringstream o;
			using Begin = typename boost::mpl::begin<HierarchyFiltered>::type;
			using End = typename boost::mpl::end<HierarchyFiltered>::type;
			using CastsTableEntries = detail::CastsTableEntries<Begin, End>;
			CastsTableEntries entries{knownCasts()};
			o << "[" << entries << "]";
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
		
		static const char * knownTypes() {
			return matchKnownTypes().c_str();
		}
	};
	
	namespace detail {
		class BareType {
			template<typename T, bool = std::is_const<T>::value || std::is_volatile<T>::value || std::is_reference<T>::value || std::is_pointer<T>::value || std::is_array<T>::value> struct BranchBare {
				using type = typename BranchBare<typename std::remove_all_extents<typename std::remove_pointer<typename std::remove_reference<typename std::remove_cv<T>::type>::type>::type>::type>::type;
			};
			
			template<typename T> struct BranchBare<T, false> {
				using type = T;
			};
		public:
			template<typename T> struct apply{
				using type = typename BranchBare<T>::type;
			};
		};
	}
	
	template<typename T> struct APIFilter; // Forward declaration to allow use in following MPL meta-func
	
	namespace detail {
		struct APIFilterApplier {
			template<typename T> struct apply {
				using type = typename APIFilter<T>::type;
			};
		};
	};
	
	// Should only be used on bare types, so DiscoverAPITypes needs to enforce that, via ExtractFuncTypes.
	template<typename T> struct APIFilter {
	private:
		using Bases = typename CxxFFI::ReflBases<T>::type;
		using BasesPass = typename boost::mpl::transform<Bases,detail::APIFilterApplier>::type;
	public:
		using type = typename boost::mpl::fold<BasesPass, boost::mpl::bool_<false>, boost::mpl::or_<boost::mpl::_1, boost::mpl::_2>>::type;
	};
	
	template<typename T> struct APIFilter<std::shared_ptr<T>> {
		using type = typename APIFilter<T>::type;
	};
	
	struct ExtractFuncTypes {
		template<typename T> class apply {
			static_assert(std::is_function<T>::value, "Can't use ExtractFuncTypes on non-function-type");
		};
		
		template<typename R, typename ...Args> class apply<R(Args...)> {
			using RawTypes = boost::mpl::vector<R, Args...>;;
		public:
			using type = typename boost::mpl::transform<RawTypes, detail::BareType>::type;
		};
	};
	
	struct DiscoverAPITypes {
		template<typename Funcs> class apply {
			using FuncTypes = typename boost::mpl::transform<Funcs, ExtractFuncTypes>::type;
			using UniqueFuncTypes = typename boost::mpl::transform<FuncTypes, detail::Vec2Set>::type;
			using SeedTypes = typename boost::mpl::fold<UniqueFuncTypes, boost::mpl::set0<>, detail::SetUnion>::type;
		public:
			using type = typename boost::mpl::copy_if<SeedTypes, detail::APIFilterApplier, boost::mpl::inserter<boost::mpl::set0<>, boost::mpl::insert<boost::mpl::_1, boost::mpl::_2>>>::type;
		};
	};
}

#define _CXXFFI_DECLTYPE_PASTER(R, _, ELEM) (decltype(ELEM))
#define CXXFFI_EXPOSE(NAME, LOC, XS) \
extern "C" { \
	const char* NAME(){\
		using APIFuncs = boost::mpl::vector< BOOST_PP_SEQ_ENUM(BOOST_PP_SEQ_FOR_EACH(_CXXFFI_DECLTYPE_PASTER, _, XS)) >;\
		using APITypes = typename CxxFFI::DiscoverAPITypes::apply<APIFuncs>::type;\
		using CastsTable = CxxFFI::CastsTable<LOC, APITypes>;\
		return CastsTable::apply();\
	}\
}
