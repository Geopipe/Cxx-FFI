#!/usr/bin/env python
"""
Copyright (c) 2017, Geopipe, Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
* Redistributions of source code must retain the above copyright
notice, this list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright
notice, this list of conditions and the following disclaimer in the
documentation and/or other materials provided with the distribution.
* Neither the name of Geopipe, Inc. nor the
names of its contributors may be used to endorse or promote products
derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
"""

import clang.cindex
from clang.cindex import CursorKind, Index, Config, TranslationUnit, TypeKind

from cxx_util import diagnose_errors, list_diagnostics


def topo_sort(super_dag, root):
	visited = {}
	output = []
	def visit(n):
		visit_tag = visited.get(n, None)
		if visit_tag == False:
			raise ValueError("Cyclical Type Dependency")
		elif visit_tag == None:
			visited[n] = False
			for child in super_dag[n][1]:
				visit(child.spelling)
			visited[n] = True
			output.insert(0, n)
		elif visit_tag:
			pass
	visit(root)
	return output

def decompose_type(root_type, results):
	root_type = root_type.get_canonical()
	root_name = root_type.spelling
	root_kind = root_type.kind
	children = []
	if root_kind == TypeKind.FUNCTIONPROTO:
		children = [root_type.get_result()] + list(root_type.argument_types())
	elif root_kind == TypeKind.POINTER:
		children = [root_type.get_pointee()]
	elif root_type.is_pod() or (root_kind == TypeKind.VOID):
		pass
	elif root_kind == TypeKind.RECORD:
		results[root_name] = root_type
	else:
		raise ValueError("Don't know what to do with %s, %s" % (root_type.kind.spelling, root_name))
	for child in children:
		decompose_type(child,results)
		

def expect_success(diagnostics):
	errors = diagnose_errors(diagnostics)
	if errors: return errors
	else: return True
		
def solve_template_base_config(index, pch_dst):
	def refl_base(idx):	return "REFL_BASE%d" % idx
	def refl_ans(idx): return "REFL_ANS%d" % idx
	
	def expect_missing_base_or_success(the_type,idx,indent):
		def f(diagnostics):
			actual_spelling = [d.spelling for d in diagnostics]
			permitted_spelling = ["no type named '%s' in '%s'" % (refl_base(idx), the_type.spelling)]
			if actual_spelling == permitted_spelling: return []
			else: return expect_success(diagnostics)
		return f
	
	
	def complete_ith_src(the_type, idx, indent):
		return "%stypedef typename %s::%s %s;" % (" "*indent, the_type.spelling, refl_base(idx), refl_ans(idx));
	
	def extract_underlying_types_from_src(src, diag_expect, indent):
		tu2 = index.parse("answers.hpp",
						  args=["-std=c++11","-include-pch",pch_dst],
						  unsaved_files=[("answers.hpp",src)],
						  options = TranslationUnit.PARSE_INCOMPLETE | TranslationUnit.PARSE_SKIP_FUNCTION_BODIES)
		expected_result = diag_expect(list_diagnostics(tu2))
		if isinstance(expected_result, BaseException):
			raise expected_result
		elif expected_result:
			def printA(c, i, r):
				if c.kind == CursorKind.TYPEDEF_DECL:
					ans_here = c.underlying_typedef_type.get_canonical()
					return r + [ans_here]
				else:
					for d in c.get_children():
						r = printA(d, i+1, r)
					return r
			return printA(tu2.cursor, indent, [])
		else:
			return expected_result
	
	def solve_template_base(the_type, the_template, known_base_typedefs, indent=0):
		# This seems like a bug in libclang, but a type declared as a template instantiation
		# doesn't have a template ref/type ref sequence,
		# so we have a couple options:
		# - (a) try to parse the name and look it up as we would otherwise
		# - (b) build up the list by trying to compile one typedef at a time.
		# Since (a) seems fairly brittle, we're going to stick with (b) for now.
		if the_template:
			src_template = "\n".join(complete_ith_src(the_type, idx, indent+2) for idx in range(len(known_base_typedefs[the_template])))
			print (" "*indent),"Resolving %s from %s" % (the_type.spelling, the_template)
			return extract_underlying_types_from_src(src_template, expect_success, indent)
		else:
			print (" "*indent),"Resolving %s online, because no template cursor was available" % (the_type.spelling,)
			out = []
			idx = 0
			while True:
				next_base = extract_underlying_types_from_src(
					complete_ith_src(the_type, idx, indent+2),
					expect_missing_base_or_success(the_type, idx, indent), indent)
				if next_base:
					out += next_base
					idx += 1
				else:
					return out
				
	return solve_template_base

class FFIFilter(object):
	def __init__(self, namespace_pred, func_pred, solve_template_base):
		self.namespace_pred = namespace_pred
		self.func_pred = func_pred
		self.in_ns = []
		self.known_base_typedefs = {}
		self.foreign_functions = {}
		self.known_types = {}
		self.exposed_types = {}
		self.solve_template_base = solve_template_base
		
	# Technically we aren't using this as a trampoline atm,
	# just an indirection for shared recursive patterns
	def visit_trampoline(self, cursor, visitor, indent):
		if visitor:
			for child in cursor.get_children():
				visitor(child, indent + 1)
	
	def find_functions(self, cursor, indent = 0):
		if (cursor.kind == CursorKind.FUNCTION_DECL) and self.func_pred(cursor):
			print (" " * indent), cursor.displayname#, cursor.kind
			if (cursor.displayname in self.foreign_functions) and (cursor.canonical != self.foreign_functions[cursor.displayname]):
				print (" " * (indent +1)), "canonicity fail:", cursor.canonical, self.foreign_functions[cursor.displayname]
			self.foreign_functions[cursor.displayname] = cursor.canonical
			next_visitor = None
		else:
			if(cursor.kind == CursorKind.FUNCTION_DECL):
				print("WARNING: " + cursor.displayname + " did not satisfy the function predicate in find_functions. Change the name of the function or the function predicate to fix this.")
			next_visitor = self.find_functions
		self.visit_trampoline(cursor, next_visitor, indent)
			
			
	def find_extern_C(self, cursor, indent = 0):
		if cursor.kind in (CursorKind.LINKAGE_SPEC, CursorKind.UNEXPOSED_DECL):
			next_visitor = self.find_functions
		else:
			next_visitor = self.find_extern_C
		self.visit_trampoline(cursor, next_visitor, indent)
	
	def find_exposed(self):
		for func in self.foreign_functions.values():
			func_type = func.type.get_canonical()
			decompose_type(func_type,self.exposed_types)
			
	def filter_namespaces(self, cursor, indent = 0):
		ns_stack = []
		def ns_visitor(cursor, indent):
			next_visitor = ns_visitor
			if (cursor.kind == CursorKind.NAMESPACE):
				ns_stack.append(cursor.displayname)
				if self.namespace_pred(ns_stack):
					print (" "*indent), "Accepting namespace cursor:", cursor.displayname
					self.in_ns.append(cursor)
					next_visitor = None
				need_pop = True
			else:
				need_pop = False
			self.visit_trampoline(cursor, next_visitor, indent)
			if need_pop:
				ns_stack.pop()
		ns_visitor(cursor, indent)
		
	# We assume that the types here are in canonical form
	def recurse_to_base(self, the_type, inspect_cursor, indent = 0):
		type_name = the_type.spelling
		if type_name not in self.known_types:
			print (" " * indent), type_name, "This type has not previously been registered, which means it's from a template"		
			cursor_children = list(inspect_cursor.get_children())
			if len(cursor_children):
				the_template = cursor_children[0].spelling
			else:
				the_template = None
			solved = self.solve_template_base(the_type, the_template, self.known_base_typedefs, indent+1)
			self.known_types[type_name] = (the_type, solved)
			print (" " * indent), " - found:", [t.spelling for t in solved]
			for t in solved:
				self.recurse_to_base(t, t.get_declaration(), indent + 1)
	
	def find_bases_config(self, derived_by):
		derived_name = derived_by.spelling
		def find_bases(cursor, indent = 0):
			next_visitor = None
			if (derived_by.kind != CursorKind.CLASS_TEMPLATE) and cursor.kind == CursorKind.CXX_BASE_SPECIFIER:
				print (" " * indent), "Has base:", cursor.displayname
				print (" " * indent), " defined by", cursor.type.get_canonical().spelling, " at ",
				print cursor.type.get_canonical().get_declaration().kind
				derived_type  = derived_by.type.get_canonical()
				base_type = cursor.type.get_canonical()
				self.known_types[derived_type.spelling][1].append(base_type)
				self.recurse_to_base(base_type, cursor, indent + 1)
			elif (derived_by.kind == CursorKind.CLASS_TEMPLATE) and (cursor.kind == CursorKind.TYPEDEF_DECL):
				print (" " * indent), "Has typedef:", cursor.displayname
				if cursor.displayname[:9] == "REFL_BASE":
					self.known_base_typedefs[derived_name].append(cursor.displayname)
			else:
				next_visitor = find_bases
			self.visit_trampoline(cursor, next_visitor, indent)
		return find_bases		
		
	def build_type_hierarchy(self, cursor, indent = 0):
		if cursor.kind in (CursorKind.STRUCT_DECL, CursorKind.CLASS_DECL):
			print (" " * indent), cursor.displayname
			decl_type  = cursor.type.get_canonical()
			self.known_types[decl_type.spelling] = (decl_type,[])		
			next_visitor = self.find_bases_config(cursor)
		elif cursor.kind == CursorKind.CLASS_TEMPLATE:
			print (" " * indent), cursor.displayname, "spelled", cursor.spelling
			self.known_base_typedefs[cursor.spelling] = []
			next_visitor = self.find_bases_config(cursor)
		else:
			next_visitor = self.build_type_hierarchy
		self.visit_trampoline(cursor, next_visitor, indent)
		
	def finish_hierarchy(self, indent = 0):
		for (type_name, cx_type) in sorted(self.exposed_types.iteritems()):
			self.recurse_to_base(cx_type, cx_type.get_declaration())
			
	def calc_exposed_bases(self, indent = 0):
		# topo_sort returns each node as the head of its list of bases, so we'll chop it off
		return {type_name:
			[base for base in topo_sort(self.known_types, type_name)[1:]
			 if base in self.exposed_types]
			for type_name in sorted(self.exposed_types.iterkeys())}
	
	def emit_table_for_TU(self, tu_cursor):
		self.filter_namespaces(tu_cursor)
		# First pass: identify all of our types
		# and their inheritance hierarchy
		# And all of the FFI functions
		for ns_cursor in self.in_ns:
			self.build_type_hierarchy(ns_cursor)
			self.find_extern_C(ns_cursor)
		
		# Second pass: generate a table of the
		# types that are exposed
		self.find_exposed()
		self.finish_hierarchy()
		return self.calc_exposed_bases()
		
class CodeGen(object):
	default_template = """/**************************************
*
* This file was automatically generated by:
*   %s
*
* Do not edit it by hand, as changes will be
* overwritten by the next build process
*
**************************************/
#include "%s"

%s
%s
%s
%s
"""
	default_pre_hook = lambda: ("", 0)
	default_post_hook = lambda indent: ""
	default_define_upcast_template = lambda indent: "%stemplate<class From, class To> To* upcast(From *target){ return target; }" % (" " * indent,)
	default_emit_cast = lambda derived, base, indent: ("%stemplate %s *upcast(%s *);" % ((" " * indent), base.spelling, derived.spelling))
	
	def __init__(self, prog_path,
				 pre_hook = default_pre_hook, template = default_template, post_hook = default_post_hook,
				 define_upcast_template = default_define_upcast_template, emit_cast = default_emit_cast):
		self.prog_path = prog_path
		self.pre_hook = pre_hook
		self.template = template
		self.post_hook = post_hook
		self.define_upcast_template = define_upcast_template
		self.emit_cast = emit_cast
		
	def __call__(self, api_header, exposed_types, exposed_bases):
		from os.path import abspath
		(prologue, indent) = self.pre_hook()
		upcast_def = self.define_upcast_template(indent)
		upcasts = "\n".join(self.emit_cast(exposed_types[exposed], exposed_types[base], indent)
							for (exposed, bases) in sorted(exposed_bases.iteritems())
							for base in bases)
		epilogue = self.post_hook(indent)
		return (self.template % (abspath(self.prog_path),
					 api_header,
					 prologue,
					 upcast_def,
					 upcasts,
					 epilogue))

def main(prog_path, libclang_path, api_header, pch_dst, api_casts_dst, namespace_filter, function_filter, namespace_dst, *libclang_args):
	accept_from = set(namespace_filter.split(" "))
	valid_function_prefixes = set(function_filter.split(" "))
	
	Config.set_library_file(libclang_path)
	index = Index.create(excludeDecls=True)
	# We should really want to use a compilation database here, except that it's only supported by makefiles...
	tu = TranslationUnit.from_ast_file(pch_dst, index)

	filt = FFIFilter(lambda s: s[0] in accept_from,
		lambda x: any([x.displayname.startswith(prefix) for prefix in valid_function_prefixes]),
		solve_template_base_config(index, pch_dst))
	
	code_gen = CodeGen(prog_path,
						pre_hook = lambda: ("namespace %s {" % (namespace_dst,), 4),
						post_hook = lambda indent: "}")
	
	with open(api_casts_dst, 'w') as out_handle:
		from os.path import abspath
		out_handle.write(code_gen(api_header, filt.exposed_types, filt.emit_table_for_TU(tu.cursor)))

		

if __name__ == '__main__':
	from sys import argv
	print argv
	main(*argv)
	