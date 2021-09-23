#pragma once
/************************************************************************************
 * @file casts_table.hpp 
 * Deep Magic, implements #CXXFFI_EXPOSE machinery.
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
#include <boost/mpl/push_front.hpp>
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

/******************************************************
 * Tools to generate a description of an API's class
 * hierarchy so that languages with C FFIs can emulate
 * C++'s inheritance rules and implicit casting for
 * derived types.
 ******************************************************/
namespace CxxFFI {
	
	/**************************************************
	 * Internal implementation details
	 **************************************************/
	namespace detail {
		using namespace boost::mpl;
		
		/// Helper metafunction to concatenate two `boost::mpl::vector`s
		template<typename Left, typename Right> struct Concatenate {
			using type = typename copy< Right, back_inserter<Left> >::type;
		};
		
		template<typename Here, typename WIP, typename Done>
		class VisitBases;
		
		/// `boost::mpl`'s convention for metafunctions requires a wrapper struct, see `VisitLooperInner::apply`.
		struct VisitLoopInner {
			/// binary metafunction to recurse on `VisitBases` and combine results.
			template<typename OldState, typename Child> class apply {
				using Here = typename at<OldState, int_<0>>::type; ///< The class being inspected
				using OldWIP = typename at<OldState, int_<1>>::type; ///< The `WIP` set before this step.
				using OldDone = typename at<OldState, int_<2>>::type; ///< The `Done` set before this step.
				using MaybeTail = typename at<OldState, int_<3>>::type; ///< The suffix of the output sequence known before this step.
				
				using VisitBasesThunk = VisitBases<Child, OldWIP, OldDone>; ///< Recursively invoke `VisitBases` on one of `Here`'s base types (amusingly named `Child`).
				using FinalWIP = typename VisitBasesThunk::FinalWIP; ///< The `WIP` set after this step.
				using FinalDone = typename VisitBasesThunk::FinalDone; ///< The `Done` set after this step.
				using MaybeFront = typename VisitBasesThunk::type; ///< Any new prefix for the output after this step.
				
				using JoinedList = typename Concatenate<MaybeFront, MaybeTail>::type; ///< Join the known output prefix and suffix.
			public:
				using type = vector<Here, FinalWIP, FinalDone, JoinedList>; ///< Rebundle loop state for `boost::mpl::accumulate`.
				
			};
		};
		
		/****************************************************************************
		 * Helper metafunction that actually implements one step of the DFS.
		 * @tparam Here The class currently being visited.
		 * @tparam WIP The "work-in-progress" classes with a temporary mark.
		 * @tparam Done The classes already contained in the output sequence.
		 * @pre `Here` is not contained in either `WIP` or `Done`.
		 ****************************************************************************/
		template<typename Here, typename WIP, typename Done> class VisitLoop {
			using InitWIP = WIP;
			using NextWIP = typename insert<InitWIP, Here>::type; ///< Add temporary mark on `Here`
			using ReflBases = typename ReflBases<Here>::type; ///< Retrieve `Here`s bases to iterate over.
			/******************************************************************
			 * Bundle the current state for use with `boost::mpl::accumulate`.
			 * Elements are `Here`, `VisitLoop::NextWIP`, `Done`, and an
			 * empty output sequence. 
			 ******************************************************************/
			using InitState = vector<Here, NextWIP, Done, vector<>>;
			
			using AccumulatedState = typename accumulate<ReflBases, InitState, VisitLoopInner>::type; ///< Execute `VisitLoopInner` over `ReflBases`
			using AccumulatedWIP = typename at<AccumulatedState, int_<1>>::type; ///< The `WIP` set, post loop over `ReflBases`.
			using AccumulatedDone = typename at<AccumulatedState, int_<2>>::type;///< The `Done` set, post loop over `ReflBases`.
			using AccumulatedTail = typename at<AccumulatedState, int_<3>>::type;///< The suffix of the output which is fully known, post loop over `ReflBases`.
		public:
			using FinalWIP = typename erase_key<AccumulatedWIP, typename key_type<AccumulatedWIP, Here>::type>::type; ///< Remove temporary mark on `Here`.
			using FinalDone = typename insert<AccumulatedDone, Here>::type; ///< Set permanent mark on `Here`.
			using type = typename push_front<AccumulatedTail, Here>::type; ///< Prepend `Here` to the output.
		};
		
		/// Helper metafunction to detect if work needs to be done at this step, default is for no work at this step.
		template<typename Here, typename WIP, typename Done, bool done = contains<Done, Here>::type::value> class VisitBasesJointIf {
		public:
			using FinalWIP = WIP; ///< Nothing has changed
			using FinalDone = Done; ///< Nothing has changed
			using type = vector<>; ///< No updates to the output.
		};

		/// Specialization of `VisitBasesJointIf` for when `Here` is not contained in `Done`
		template<typename Here, typename WIP, typename Done> class VisitBasesJointIf<Here, WIP, Done, false> {
			using VisitLoopThunk = VisitLoop<Here, WIP, Done>; ///< Invoke helper metafunction to perform loop over reflected base classes.
		public:
			using FinalWIP = typename VisitLoopThunk::FinalWIP; ///< The updated `WIP` set after this step
			using FinalDone = typename VisitLoopThunk::FinalDone; ///< The updated `Done` set after this step
			using type = typename VisitLoopThunk::type; ///< The suffix of the output which is fully known after this step.
		};

		/**********************************************************************
		 * The primary recursive building block for `ToposortBases`.
		 * @tparam Here The class currently being visited.
		 * @tparam WIP The "work-in-progress" classes with a temporary mark.
		 * If we encounter one of these, a `static_assert` will fail.
		 * @tparam Done The classes already contained in the output sequence.
		 * If we encounter one of these, don't process `Here` further.
		 **********************************************************************/
		template<typename Here, typename WIP, typename Done>
		class VisitBases {
			static_assert(!contains<WIP, Here>::value, "Cycle while toposorting base classes. Your inheritance is broken");
			using IfThunk = VisitBasesJointIf<Here, WIP, Done>; ///< Invoke helper metafunction to determine if we have work at this step.
		public:
			using FinalWIP = typename IfThunk::FinalWIP; ///< The updated `WIP` set after this step.
			using FinalDone = typename IfThunk::FinalDone; ///< The updated `Done` set after this step.
			using type = typename IfThunk::type; ///< The suffix of the output which is fully known after this step.
		};
		
	}
	
	/// `boost::mpl`'s convention for metafunctions requires a wrapper struct, see `ToposortBases::apply`.
	struct ToposortBases {
		/// Metafunction to obtain a topological sort over `T`'s base classes via `VisitBases`.
		template<typename T> class apply {
			using WIP = boost::mpl::set<>; ///< Initialize an empty `WIP` set.
			using Done= boost::mpl::set<>; ///< Initialize an empty `Done` set.
		public:
			using type =typename detail::VisitBases<T, WIP, Done>::type; ///< Perform the sort.
		};
	};
	
	/// `std::ostream` support for the various supporting datastructures used in converting `CastsTableEntries` to JSON.
	template<typename T> auto operator<<(std::ostream& os, const T& t) -> decltype(t(os), os)
	{
		t(os);
		return os;
	}
	
	namespace detail {
		/// Obtain a human readable name for `T` by demangling the name of its typeid.
		template<typename T> std::string readableName() {
			return boost::core::demangle(typeid(T).name());
		}
	}
	
	/***********************************************************************
	 * Some types have more verbose names in the ABI than they do in the API
	 * Specializations of this function allow client code to control demangling
	 * more precisely, and apply some "syntactic resguaring" to match the JSON
	 * name of the type with the API name of the type.
	 * 
	 * Default operation is an identity transform.
	 ***********************************************************************/
	template<typename T> struct NameRewriter {
		static std::string apply(std::string name) {
			return name;
		}
	};
	
	/***********************************************************************
	 * A helper functor which client code can use to apply `NameRewriter` to
	 * type names when they appear as template parameters for a unary template.
	 ***********************************************************************/
	template<template<typename Tp> class PType, typename T> struct SimpleTemplateNameRewriter {
		/// Reusable pre-compiled regular expression to match the (demangled) name of `T` within the (demangled) name of `PType<T>`.
		static const re2::RE2& simpleTemplateNameRegExp() {
			static re2::RE2 ans("^([^<]+<\\s*)(" + re2::RE2::QuoteMeta(detail::readableName<T>()) + ")\\s*(>)$");
			return ans;
		}
		
		/// Apply `NameWriter<T>` to the (demangled) name of `T` as it appears within the (demangled) name of `Ptype<T>`.
		static std::string apply(std::string name) {
			std::string innerReplace = NameRewriter<T>::apply(detail::readableName<T>());
			std::string ret;
			if( re2::RE2::Extract(name, simpleTemplateNameRegExp(), "\\1" + innerReplace + "\\3", &ret)  ) {
				return ret;
			} else {
#ifdef DEBUG
				
				std::cerr << "Couldn't parse template, but SimpleTemplateNameRewriter is opt-in" << std::endl;
				abort();
#else
				return name;
#endif
			}
		}
	};
	
	namespace detail {
		/// Reusable pre-compiled regular expression to match `std::__1` within a demangled type name.
		static inline const re2::RE2& stdABIFlatteningNameRegExp() {
			static re2::RE2 ans("^(" + re2::RE2::QuoteMeta("std::__1") + ")(.+)");
			return ans;
		}
	}
	
	/// A functor to replace `std::__1` with `std` in the (demangled) name of `T`.
	template<typename T> struct StdABIFlatteningNameRewriter {
		/// Perform the replacement.
		static std::string apply(std::string name) {
			std::string ret;
			if( re2::RE2::Extract(name, detail::stdABIFlatteningNameRegExp(), "std\\2", &ret ) ) {
				return ret;
			} else {
#ifdef DEBUG
				std::cerr << "Couldn't parse template, but StdABIFlatteningNameRewriter is opt-in" << std::endl;
				abort();
#else
				return name;
#endif
			}
		}
	};
	
	/// `NameRewriter` specialization for `std::shared_ptr<T>`. Applies both `StdABIFlatteningNameRewriter` and `SimpleTemplateNameRewriter`.
	template<typename T> struct NameRewriter<std::shared_ptr<T>> {
		using FullT = std::shared_ptr<T>;
		static std::string apply(std::string name) {
			return StdABIFlatteningNameRewriter<std::shared_ptr<T>>::apply(SimpleTemplateNameRewriter<std::shared_ptr, T>::apply(name));
		}
	};
	
	namespace detail {
		using namespace boost::mpl;
		
		/// Return an empty string if the `boost::mpl` metaiterators `Start` and `End` are equivalent, or `sep` otherwise.
		template<typename Start, typename End> std::string maybeSeparator(std::string sep = ", ") {
			return std::is_same<Start, End>::value ? "" : sep;
		}
		
		/****************************************************************
		 * Recursive functor to emit the value associated with JSON
		 * key-value pair represented by a `CastsTableEntry`. 
		 * @tparam Derived The class whose base classes we are traversing.
		 * @tparam Start The base-class metaiterator we are starting from.
		 * @tparam End The past-the-end base-class metaiterator.
		 ****************************************************************/
		template<typename Derived, typename Start, typename End> struct CastsTableSubEntries {
			using Here = typename deref<Start>::type; ///< Dereference `Start` to base class to be inspected at this step.
			using Next = typename next<Start>::type; ///< Advance `Start` to obtain metaiterator for next recursive step.
			using CastFunc = Here*(*)(Derived*); ///< A pointer to a function casting from `Derived` to `Here` must have this form.
			
			std::string &derivedName; ///< The demangled name of `Derived`.
			std::map<std::string, std::string> &knownCasts; ///< Map from demangled base class names to the symbols implementing upcasts from `Derived` to that base class.
			/****************************************************************
			 * Emit the JSON for a single base class and its associated upcast, 
			 * then recursively invoke `CastsTableSubEntries::operator()` for
			 * the next base class, if any. Together these form a JSON array,
			 * which is the value associated with the key for `Derived`.
			 ****************************************************************/
			std::ostream& operator()(std::ostream& o) const {
				/************************************************
				 * The following two lines are cursed. 
				 * A better language lawyer than I may be able to 
				 * determine if we can replace `__attribute__((used))`
				 * with a `volatile` upfront and at least be
				 * more portably and standards-compliantly cursed
				 ************************************************/
				static constexpr const CastFunc instantiateMe __attribute__((used)) = &upcast<Derived, Here>;
				static_assert(instantiateMe != nullptr, "The compiler is optimizing badly");
				std::string baseName = readableName<Here>();
				std::string castSymbol = knownCasts[baseName];
				if (castSymbol.length()) {
					o << "\n\t\t" << std::quoted(NameRewriter<Here>::apply(baseName)) << " : " << std::quoted(castSymbol) << maybeSeparator<Next, End>();
				}
#ifdef DEBUG
				else {
					std::cerr << "Warning: couldn't find upcast from " << derivedName << " to " << baseName << std::endl;
				}
#endif
				return o << CastsTableSubEntries<Derived, Next, End>{derivedName, knownCasts};
			}
		};

		/// past-the-end specialization of `CastsTableSubEntries` (i.e. the recursive base case).
		template<typename Derived, typename End> struct CastsTableSubEntries<Derived, End, End> {
			std::string &derivedName;
			std::map<std::string, std::string> &knownCasts;
			/// Do nothing
			std::ostream& operator()(std::ostream& o) const {
				return o;
			}
		};
		
		/****************************************************************
		 * Functor to emit the JSON key-value pair for
		 * inheritance hierarchy in `TopoSorted`.
		 * @tparam TopoSorted A `boost::mpl::vector` containing a single
		 * inheritance hierarchy, sorted from most-derived to least-derived.
		 ****************************************************************/
		template<typename TopoSorted> struct CastsTableEntry {
			using Derived = typename at<TopoSorted, int_<0>>::type; ///< The most derived class will be the key for our key-value pair.
			using Bases = typename pop_front<TopoSorted>::type; ///< All known base classes `Derived`, as discovered via `ToposortBases`.
			using Begin = typename begin<Bases>::type; ///< Metaiterator to the beginning of `Bases`.
			using End = typename end<Bases>::type; ///< Metaiterator past-the-end of `Bases`.
			std::map<std::string, std::map<std::string, std::string> > &knownCasts; ///< See `CastsTableEntries::knownCasts`.
			/// Actually emit the key-value pair for this `CastsTableEntry`.
			std::ostream& operator()(std::ostream& o) const {
				std::string derivedName = readableName<Derived>();
				return o << "\n\t" << "" << std::quoted(NameRewriter<Derived>::apply(derivedName)) << " : {" << CastsTableSubEntries<Derived, Begin, End>{derivedName, knownCasts[derivedName]} << "}" ;
			}
		};
		
		/****************************************************************
		 * A recursive functor to emit the JSON key-value pair for
		 * inheritance hierarchy of all discovered classes in the API.
		 * @tparam Start Metaiterator defining start of current recursive step
		 * @tparam End Metaiterator past-the-end of current recursive step.
		 ****************************************************************/
		template<typename Start, typename End> struct CastsTableEntries {
			using Here = typename deref<Start>::type; ///< The most-derived class in the inheritance hierarchy to emit at this step.
			using Next = typename next<Start>::type; ///< Metaiterator defining start of next recursive step.
			
			/***************************************************************
			 * A map from all discovered API classes to a map from their base
			 * classes to the (demangled) symbol names for the upcasts between
			 * a discovered class and the given base.
			 * 
			 * In other words, `knownCasts["B"]["A"]` would return the (demangled)
			 * name of the symbol for `CxxFFI::upcast<B,A>`.
			 ***************************************************************/
			std::map<std::string, std::map<std::string, std::string> > &knownCasts;

			/// Invoke `CastsTableEntry<Here>`, then proceed with recursion.
			std::ostream& operator()(std::ostream& o) const {
				return o << CastsTableEntry<Here>{knownCasts} << maybeSeparator<Next, End>() << CastsTableEntries<Next, End>{knownCasts};
			}
		};
		
		/// Past-the-end specialization of `CastsTableEntries` (i.e. the recursive base case).
		template<typename End> struct CastsTableEntries<End, End> {
			std::map<std::string, std::map<std::string, std::string> > &knownCasts; ///< See `CastsTableEntries::knownCasts`.
			/// Do nothing;
			std::ostream& operator()(std::ostream& o) const {
				return o;
			}
		};
		
		/// `boost::mpl`'s convention for metafunctions requires a wrapper struct, see `Vect2Set::apply`.
		struct Vec2Set {
			/// Convert a `boost::mpl::vector` to a `boost::mpl::set`.
			template<typename Vec> struct apply {
				using type = typename fold<Vec, set0<>, insert<_1,_2>>::type;
			};
		};
		
		/// `boost::mpl`'s convention for metafunctions requires a wrapper struct, see `SetUnion::apply`.
		struct SetUnion {
			/// Take the union of two `boost::mpl::set`s.
			template<typename left, typename right> struct apply {
				using type = typename copy<right, inserter<left, insert<_1, _2>>>::type;
			};
		};
		
		/****************************************************************
		 * A recursive functor to generate a regular expression matching
		 * the demangled names of all discovered types in the API.
		 * @tparam Start Metaiterator defining start of current recursive step
		 * @tparam End Metaiterator past-the-end of current recursive step.
		 ****************************************************************/
		template<typename Start, typename End> struct EscapedTypeNames {
			using Here = typename deref<Start>::type; ///< The class to be added to the regex this step.
			using Next = typename next<Start>::type; ///< The metaiterator defining start of next recursive step.
			/// Quote the (demangled) name of `Here`, ensuring any regex special characters are escaped, then proceed to next recursive step.
			std::ostream& operator()(std::ostream& o) const {
				return o << "(?:" << re2::RE2::QuoteMeta(readableName<Here>()) << ")" << maybeSeparator<Next,End>("|") << EscapedTypeNames<Next, End>();
			}
		};
		
		/// Past-the-end specialization of `EscapedTypeNames` (aka the recursive base-case).
		template<typename End> struct EscapedTypeNames<End, End> {
			/// Do nothing
			std::ostream& operator()(std::ostream& o) const {
				return o;
			}
		};
		
		/// A functor generating a regular expression matching all discovered API types.
		template<typename KnownTypes> struct MatchKnownTypes {
			using Begin = typename begin<KnownTypes>::type; ///< Metaiterator to the beginning of `KnownTypes`.
			using End = typename end<KnownTypes>::type; ///< Metaiterator past-the-end of `KnownTypes`.

			/// Invoke `EscapedTypeNames` and slam the whole alternation of names together into a capture group
			static std::string apply() {
				std::ostringstream o;
				o << "(" << EscapedTypeNames<Begin, End>() << ")";
				return o.str();
			};
		};
		
		/// Retrieve the symbol table from the `__text` or `.text` section of a shared library.
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
		
		/// `boost::mpl`'s convention for metafunctions requires a wrapper struct, see `FilterUnused::apply`.
		template<typename SeedTypes> struct FilterUnused {
			/// Remove any types which appear in `Sorted` but don't appear in `SeedTypes`
			template<typename Sorted> struct apply {
				using type = typename copy_if<Sorted, contains<SeedTypes, _1> >::type;
			};
		};
	}
	
	/*************************************************************************************
	 * Functor to build a JSON casts table for some set of API-exposed types.
	 * @tparam libraryLocation The pointed-to function should return a `boost::filesystem::path`
	 * to a shared library whose symbol table will be examined. See `#CXXFFI_EXPOSE`'s `LOC`
	 * parameter for more information.
	 * @tparam SeedTypes The set of types exposed through the API.
	 *************************************************************************************/
	template<boost::filesystem::path(*libraryLocation)(), typename SeedTypes> class CastsTable {
		/// Transform `SeedTypes` via `ToposortBases` to construct a vector of types. `SeedTypes` may be associative
		using Hierarchy = typename boost::mpl::transform<SeedTypes, ToposortBases, boost::mpl::back_inserter<boost::mpl::vector0<>>>::type;
		/// Remove as irrelevant any types which appear in the inheritance hierachies but not the actual exposed API functions.
		using HierarchyFiltered = typename boost::mpl::transform<Hierarchy, detail::FilterUnused<SeedTypes>>::type;
		/// Uniquify the discovered types.
		using KnownTypes = typename boost::mpl::fold<typename boost::mpl::transform<HierarchyFiltered,detail::Vec2Set>::type, boost::mpl::set0<>, detail::SetUnion>::type;
		/// Used to create regular expression for matching the (demangled) name of any element in `KnownTypes`.
		using MatchKnownTypes = detail::MatchKnownTypes<KnownTypes>;
		
		/// Memoize result of `CastsTable::MatchKnownTypes`.
		static std::string& matchKnownTypes() {
			static std::string ans = MatchKnownTypes::apply();
			return ans;
		}
		
		/// Create a two-level map from derived classes to base classes to upcast symbols. See `CastsTableEntries::knownCasts`.
		static std::map<std::string, std::map<std::string, std::string> > genKnownCasts() {
			std::string& knownTypes = matchKnownTypes();
			// Create a regular expressin matching the (demangled) symbol name for `CxxFFI::upcast` for all known types.
			std::string matchUpcastSrc = knownTypes + re2::RE2::QuoteMeta("*") + "\\s+" + re2::RE2::QuoteMeta("CxxFFI::upcast<") + knownTypes + re2::RE2::QuoteMeta(",") + "\\s*" + knownTypes + "\\s*" + re2::RE2::QuoteMeta(">(") + knownTypes + re2::RE2::QuoteMeta("*)");
#ifdef DEBUG
			std::cout << "Parsing symbols via " << matchUpcastSrc << std::endl;
#endif
			re2::RE2 matchUpcast(matchUpcastSrc);
			
			// Traverse the symbol table for library in question and filter out the upcasts.
			// These are guaranteed to have been instantiated by the fused runtime/compile-time
			// loop present in `CastsTableSubEntries::operator()`. The (unused) `instantiateMe`
			// field forces the instantiation of every type exposed in the API so that it will
			// exist when execute this loop. wibbly-wobbly/timey-wimey
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
		
		/// Memoize result of `CastsTable::genKnownCasts`.
		static std::map<std::string, std::map<std::string, std::string> >& knownCasts() {
			static std::map<std::string, std::map<std::string, std::string> > ans = genKnownCasts();
			return ans;
		}
		
		/// Build up the JSON blob for the casts table via invoking `CastsTableEntries` on each entry of `CastsTable::HierarchyFiltered`.
		static std::string genCastsTable() {
			std::ostringstream o;
			using Begin = typename boost::mpl::begin<HierarchyFiltered>::type;
			using End = typename boost::mpl::end<HierarchyFiltered>::type;
			using CastsTableEntries = detail::CastsTableEntries<Begin, End>;
			CastsTableEntries entries{knownCasts()};
			o << "{" << entries << "}";
			return o.str();
		}
		
		/// Memoize result of `CastsTable::genCastsTable()`
		static const std::string& castsTable() {
			static std::string ans = genCastsTable();
			return ans;
			
		};
		
	public:
		/// Obtain the casts table JSON blob as a plain C string.
		static const char * apply() {
			return castsTable().c_str();
		}
		
		/// Obtain the known types regex as a plain C string.
		static const char * knownTypes() {
			return matchKnownTypes().c_str();
		}
	};
	
	namespace detail {
		/// `boost::mpl`'s convention for metafunctions requires a wrapper struct, see `BareType::apply`.
		class BareType {
			/// Recursive helper metafunction for `BareType`.
			template<typename T, bool = std::is_const<T>::value || std::is_volatile<T>::value || std::is_reference<T>::value || std::is_pointer<T>::value || std::is_array<T>::value> struct BranchBare {
				/// Remove from `T` one layer of cv-qualification, pointers, references, and array-extents and then recurse back into `BranchBare`.
				using type = typename BranchBare<typename std::remove_all_extents<typename std::remove_pointer<typename std::remove_reference<typename std::remove_cv<T>::type>::type>::type>::type>::type;
			};
			
			/// Base-case specialization when `T` has no cv-qualification, pointers, references, or array-extents.
			template<typename T> struct BranchBare<T, false> {
				using type = T; ///< Tautologically bare `T`.
			};
		public:
			/// Obtain a "bare" `T`, stripped of all cv-qualification, pointers, references, and array extents.
			template<typename T> struct apply{
				using type = typename BranchBare<T>::type; ///< The resulting "bare" `T`.
			};
		};
	}
	
	template<typename T> struct APIFilter; // Forward declaration to allow use in following MPL meta-func
	
	namespace detail {
		/// Convert `APIFilter` to the `boost::mpi` convention of metafunctions requiring a wrapper struct.
		struct APIFilterApplier {
			template<typename T> struct apply {
				using type = typename APIFilter<T>::type;
			};
		};
	};
	
	/******************************************************************
	 * A metafunction to filter types which should not appear in the 
	 * generated casts table. The default implementation accepts
	 * a class if any of its base classes are accepted.
	 * 
	 * Client code can define customizations via specialization.
	 * @tparam T The type to accepted or rejected for inclusion in the casts table.
	 * @pre Should only be used on bare types, so client code must enforce that, 
	 * `via ExtractFuncTypes`.
	 * @pre If relying on default implementation, client code must still provide 
	 * specializations for at least the set of base-classes.
	 ******************************************************************/
	template<typename T> struct APIFilter {
	private:
		using Bases = typename CxxFFI::ReflBases<T>::type; ///< Access `T`'s reflected base types.
		using BasesPass = typename boost::mpl::transform<Bases,detail::APIFilterApplier>::type; ///< Recursively apply `APIFilter` to each type in `Bases`.
	public:
		/// boost::mpl:bool_<false> if rejected or boost::mpl::bool_<true> if accepted.
		using type = typename boost::mpl::fold<BasesPass, boost::mpl::bool_<false>, boost::mpl::or_<boost::mpl::_1, boost::mpl::_2>>::type;
	};
	
	/// Specialization of `APIFilter` passing `std::shared_ptr<T>` if `T` passes.
	template<typename T> struct APIFilter<std::shared_ptr<T>> {
		using type = typename APIFilter<T>::type;
	};
	
	/// `boost::mpl`'s convention for metafunctions requires a wrapper struct, see `ExtractFuncTypes::apply<R(Args...)>`.
	struct ExtractFuncTypes {
		/// Don't apply this metafunction to non-function types, see specialization (`ExtractFuncTypes::apply<R(Args...)>`) for detailed notes..
		template<typename T> class apply {
			static_assert(std::is_function<T>::value, "Can't use ExtractFuncTypes on non-function-type");
		};
		
		/******************************************************************
		 * A metafunction to extract (as a `boost::mpl::vector`) the types 
		 * appearing in a function signature, stripped of cv-qualification, 
		 * references, pointers, and array-extents.
		 ******************************************************************/
		template<typename R, typename ...Args> class apply<R(Args...)> {
			using RawTypes = boost::mpl::vector<R, Args...>; ///< The types appearing in the signature
		public:
			/// The result vector
			using type = typename boost::mpl::transform<RawTypes, detail::BareType>::type;
		};
	};
	
	/// `boost::mpl`'s convention for metafunctions requires a wrapper struct, see `DiscoverAPITypes::apply`.
	struct DiscoverAPITypes {
		/******************************************************************
		 * A metafunction to extract all of the unique types (up to 
		 * equivalence under `detail::BareType`) appearing in the signatures
		 * of some set of function types.
		 * @tparam Funcs The `boost::mpl::vector` of function types to examine
		 ******************************************************************/
		template<typename Funcs> class apply {
			using FuncTypes = typename boost::mpl::transform<Funcs, ExtractFuncTypes>::type; ///< Extract the bare types from each function type, result is a vector of vectors
			using UniqueFuncTypes = typename boost::mpl::transform<FuncTypes, detail::Vec2Set>::type; ///< Convert the inner vectors to sets
			using SeedTypes = typename boost::mpl::fold<UniqueFuncTypes, boost::mpl::set0<>, detail::SetUnion>::type; ///< Union the inner sets into a single set
		public:
			/// Return the elements of `SeedTypes` which pass the `APIFilter`.
			using type = typename boost::mpl::copy_if<SeedTypes, detail::APIFilterApplier, boost::mpl::inserter<boost::mpl::set0<>, boost::mpl::insert<boost::mpl::_1, boost::mpl::_2>>>::type;
		};
	};
	
	namespace detail {
		using namespace boost::mpl;
		/// Metafunction constructing an empty `boost::mpl::vector` (recursive base case)
		template<typename ...Args> struct VariadicVectorConstructor {
			using type = vector0<>;
		};
		
		/// Recursive metafunction to construct the `boost::mpl::vector` consisting of `Head` followed by `Tail...`.
		template<typename Head, typename ...Tail> struct VariadicVectorConstructor<Head, Tail...> {
			/// Construct the `Tail...` and then prepend `Head`.
			using type = typename push_front<typename VariadicVectorConstructor<Tail...>::type, Head>::type;
		};
	}
	
	/// For some goofy reason, `boost::mpl::vector`s have to be constructed by adding elements one at a time, so here is a convenience wrapper as a variadic template.
	template<typename ...Args> using Vector = typename detail::VariadicVectorConstructor<Args...>::type;
}

/**************************************************************
 * @def _CXXFFI_DECLTYPE_PASTER(R, _, ELEM) Helper macro for
 * transforming boost preprocessor sequences, converting
 * `ELEM` to `(decltype(ELEM))`
 **************************************************************/
#define _CXXFFI_DECLTYPE_PASTER(R, _, ELEM) (decltype(ELEM))
/**************************************************************
 * @def CXXFFI_EXPOSE(NAME, LOC, XS)
 * Builds a DAG of some class hierarchy, instantiates `CxxFFI::upcast` 
 * for each pair of related functions in the DAG, and creates
 * a function returning a JSON blob mapping subtype names to
 * to supertype names and the symbol name (for use with `dlsym`)
 * of the associated `CxxFFI::upcast` instantiation for that
 * pair of classes.
 * 
 * @param NAME The name of the generated function returning
 * the JSON description of the class hierarchy.
 * @param LOC A constant expression (used as a template parameter)
 * of type `boost::filesystem::path(*)()`. The
 * pointed-to function should return a `boost::filesystem::path`
 * to a shared library whose symbol table will be examined.
 * Should be the same library containing the class hierarchy being
 * exported. A canonical implementation to obtain this information
 * is:
 * <pre class="markdeep">
 * ```c++
 * static boost::filesystem::path dllLoc() {
 * 	 return boost::dll::this_line_location();
 * }
 * ```
 * </pre>
 * @param XS A 
 * boost preprocessor sequence](https://www.boost.org/doc/libs/1_73_0/libs/preprocessor/doc/data/sequences.html)
 * enumerating the API functions from whose signatures the set of relevant
 * API classes should be extracted.
 **************************************************************/
#define CXXFFI_EXPOSE(NAME, LOC, XS) \
extern "C" { \
	const char* NAME(){\
		using APIFuncs = CxxFFI::Vector< BOOST_PP_SEQ_ENUM(BOOST_PP_SEQ_FOR_EACH(_CXXFFI_DECLTYPE_PASTER, _, XS)) >;\
		using APITypes = typename CxxFFI::DiscoverAPITypes::apply<APIFuncs>::type;\
		using CastsTable = CxxFFI::CastsTable<LOC, APITypes>;\
		return CastsTable::apply();\
	}\
}
