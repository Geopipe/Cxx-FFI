# Cxx-FFI
A utility based on libclang to support using C++ libraries through a C FFI

This script parses a C++ header containing an `extern "C"` interface,
to generate a C++ file containing a minimal set of upcasts for coverage
of the C++ types which are exposed through the `extern "C"` API.

It can also be imported as a module if your needs require a more intricate configuration,
in which case you should study `main` to understand the primary entry-points to useful code.

It depends on the `clang.cindex` module, which can be most easily obtained with `pip install sealang` (see [PyPI](https://pypi.python.org/pypi/sealang) for details).
It is known to work with both libclang 3.5 and libclang 3.6. YMMV with other library versions.

Pull-requests with either enhancements or bug-fixes are welcome.
