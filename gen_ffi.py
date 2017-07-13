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

import pprint

class CxxSyntaxError(SyntaxError):
	pass

def decompose_type(root_type, results):
	root_type = root_type.get_canonical()
	root_name = root_type.spelling
	root_kind = root_type.kind
	children = []
	if root_kind == TypeKind.FUNCTIONPROTO:
		#print "decomposing a function:",root_name
		children = [root_type.get_result()] + list(root_type.argument_types())
	elif root_kind == TypeKind.POINTER:
		#print "decomposing a pointer:",root_name
		children = [root_type.get_pointee()]
	elif root_type.is_pod() or (root_kind == TypeKind.VOID):
		#print "uninteresting terminal:",root_name
		pass
	elif root_kind == TypeKind.RECORD:
		results[root_name] = root_type
	else:
		raise ValueError("Don't know what to do with %s, %s" % (root_type.kind.spelling, root_name))
	for child in children:
		decompose_type(child,results)
		
def solve_template_base_config(index, pch_dst):
	def solve_template_base(the_type, the_template, known_base_typedefs, indent=0):
		src_template = "\n".join("%stypedef typename %s::REFL_BASE%d REFL_ANS%d;" % (" "*(indent+2),the_type.spelling,d,d) for d in range(len(known_base_typedefs[the_template])))
		print (" "*indent),"Resolving %s from %s" % (the_type.spelling, the_template)
		print src_template
		tu2 = index.parse("answers.hpp",
						  args=["-std=c++11","-include-pch",pch_dst],
						  unsaved_files=[("answers.hpp",src_template)],
						  options = TranslationUnit.PARSE_INCOMPLETE | TranslationUnit.PARSE_SKIP_FUNCTION_BODIES)
		diagnostics = list(tu2.diagnostics)
		if len(diagnostics) > 0:
			raise CxxSyntaxError("\n".join(pprint.pformat(d) for d in diagnostics))
		else:
			def printA(c, i, r):
				#print (" "*i), c.displayname
				if c.kind == CursorKind.TYPEDEF_DECL:
					ans_here = c.underlying_typedef_type.get_canonical()
					#print (" " * (i + 1)), ans_here
					return r + [ans_here]
				else:
					for d in c.get_children():
						r = printA(d, i+1, r)
					return r
			return printA(tu2.cursor, indent, [])
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
				#print (" "*indent), cursor.displayname
				need_pop = False
			self.visit_trampoline(cursor, next_visitor, indent)
			if need_pop:
				ns_stack.pop()
		ns_visitor(cursor, indent)
	
	def find_bases_config(self, derived_by):
		derived_name = derived_by.spelling
		def find_bases(cursor, indent = 0):
			next_visitor = None
			if (derived_by.kind != CursorKind.CLASS_TEMPLATE) and cursor.kind == CursorKind.CXX_BASE_SPECIFIER:
				print (" " * indent), "Has base:", cursor.displayname
				print (" " * indent), " defined by", cursor.type.get_canonical().spelling, " at ",
				print cursor.type.get_canonical().get_declaration().kind
				print (" " * indent), " with children", [c.kind for c in cursor.get_children()]
				derived_type  = derived_by.type.get_canonical()
				base_type = cursor.type.get_canonical()
				self.known_types[derived_type.spelling][1].append(base_type)
				if base_type.spelling not in self.known_types:
					print (" " * indent), " This base is not previously registered, which means it's from a template"
					self.known_types[base_type.spelling] = (base_type, self.solve_template_base(base_type, list(cursor.get_children())[0].spelling, self.known_base_typedefs,indent+1))
					print (" " * indent), " - found:", [t.spelling for t in self.known_types[base_type.spelling][1]]
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
			print (" " * indent), " this decl has children", [c.kind for c in cursor.get_children()]
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

def main(prog_path, libclang_path, api_header, pch_dst, api_casts_dst, namespace_filter, *libclang_args):
	# OKAY - so, some pseudo-code:
	# Filter nodes to extern "C"
	# - Iterate over all of the extern "C" functions that match some regex: i.e. make*
	# - Get all of their argument types and return types.
	# - Chase those to the declaration of the canonical version.
	# - n.b. we should really generate this file by configuring it like a header to fill in the location of libclang and the includes for our target.
	accept_from = set(namespace_filter.split(" "))
	
	Config.set_library_file(libclang_path)
	index = Index.create(excludeDecls=True)
	# We should really want to use a compilation database here, except that it's only supported by makefiles...
	tu = index.parse(api_header,
					 args=libclang_args,
					 options = TranslationUnit.PARSE_INCOMPLETE | TranslationUnit.PARSE_SKIP_FUNCTION_BODIES)
	
	diagnostics = list(tu.diagnostics)
	if len(diagnostics) > 0:
		raise CxxSyntaxError("\n".join(pprint.pformat(d) for d in diagnostics))
	else:
		tu.save(pch_dst) # We'll need this in a minute, and there's no way to just get it in RAM
		
		filt = FFIFilter(lambda s: s[0] in accept_from,
						 lambda x: x.displayname[:4] == "make" or x.displayname[:7] == "release",
						 solve_template_base_config(index, pch_dst))
		filt.filter_namespaces(tu.cursor)
		# First pass: identify all of our types
		# and their inheritance hierarchy
		# And all of the FFI functions
		for ns_cursor in filt.in_ns:
			filt.build_type_hierarchy(ns_cursor)
			filt.find_extern_C(ns_cursor)
		print filt.known_base_typedefs
		print sorted(filt.known_types.keys())
		# Second pass: generate a table of the ones that exposed
		#print sorted(filt.foreign_functions.keys())
		filt.find_exposed()
		print sorted(filt.exposed_types)
	with open(api_casts_dst, 'w') as out_handle:
		from os.path import abspath
		out_handle.write("""/**************************************
*
* This file was automatically generated by:
*   %s
*
* Do not edit it by hand, as changes will be
* overwritten by the next build process
*
**************************************/
""" % (abspath(prog_path),))

if __name__ == '__main__':
	from sys import argv
	main(*argv)
	