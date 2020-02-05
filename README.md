# Cxx-FFI
A utility library to support using C++ libraries through a C FFI, available under LGPL.
Please compile the examples using CMake to see how it works.
Depends on RE2 and Boost.

The implementation depends heavily on template metaprogramming, and runs some nontrivial algorithms at compile time, including a topological sort.
While the code attempts to be efficient, if compile times are a concern, you may wish to limit its use to a standalone dynamic library which links in all the other libraries you wish to expose in your FFI.

The `ReflBases`, `APIFilter`, and `NameRewriter` templates all provide entry points for customization (and integration with libraries whose source code and inheritance hierarchies are outside your control).
By default, hooks are provided for `std::shared_ptr`.

A legacy version, based on libclang's Python bindings is present in the `legacy/python` subdirectory.
The Python version is provided under a more permissive license (see doc comments at the top of each .py), but has substantial limitations.

Pull-requests with either enhancements or bug-fixes are welcome.
