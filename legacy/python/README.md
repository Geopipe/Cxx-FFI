# Cxx-FFI
A utility based on libclang to support using C++ libraries through a C FFI

This script parses a C++ header containing an `extern "C"` interface,
to generate a C++ file containing a minimal set of upcasts for coverage
of the C++ types which are exposed through the `extern "C"` API.

It can also be imported as a module if your needs require a more intricate configuration,
in which case you should study `main` to understand the primary entry-points to useful code.

It depends on the `clang.cindex` module.
For older libclangs (e.g. 3.5 or 3.6), this can be most easily obtained with `pip install sealang` (see [PyPI](https://pypi.python.org/pypi/sealang) for details).
For newer libclangs (e.g. 6.0), the `python-clang-x.y` package on Ubuntu, and the `py27-clang +xy` package on MacPorts provide the necessary bindings.

Pull-requests with either enhancements or bug-fixes are welcome.
