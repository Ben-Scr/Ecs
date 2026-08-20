#pragma once

#if defined(_WIN32)
#if defined(INDEX_EXAMPLE_PACKAGE_BUILD)
#define INDEX_EXAMPLE_PACKAGE_API __declspec(dllexport)
#else
#define INDEX_EXAMPLE_PACKAGE_API __declspec(dllimport)
#endif
#else
#define INDEX_EXAMPLE_PACKAGE_API __attribute__((visibility("default")))
#endif

extern "C" INDEX_EXAMPLE_PACKAGE_API int IndexExamplePackage_IsLoaded();
using IndexExamplePackage_IsLoadedFn = int (*)();