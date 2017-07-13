# Cxx-FFI
A utility based on libclang to support using C++ libraries through a C FFI

This script parses a C++ header containing an `extern "C"` interface,
to generate a C++ file containing a minimal set of upcasts for coverage
of the C++ types which are exposed through the `extern "C"` API.

It depends on the `clang.cindex` module, which can be most easily obtained with `pip install sealang` (see [PyPI](https://pypi.python.org/pypi/sealang) for details).
