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

def main(prog_path, libclang_path, api_header, pch_dst, *libclang_args):
	Config.set_library_file(libclang_path)
	index = Index.create(excludeDecls=True)
	# We should really want to use a compilation database here, except that it's only supported by makefiles...
	tu = index.parse(api_header,
					 args=libclang_args,
					 options = TranslationUnit.PARSE_INCOMPLETE | TranslationUnit.PARSE_SKIP_FUNCTION_BODIES)
	
	errors = diagnose_errors(list_diagnostics(tu))
	if errors:
		raise errors
	else:
		tu.save(pch_dst) # We'll need this in a minute, and there's no way to just get it in RAM

		

if __name__ == '__main__':
	from sys import argv
	print argv
	main(*argv)