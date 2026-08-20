#include "ExamplePackage.hpp"

#include <Core/Log.hpp>
#include <Packages/PackageApi.hpp>

namespace {
    bool s_Loaded = false;
}

extern "C" INDEX_PACKAGE_API int IndexPackage_OnLoad()
{
    s_Loaded = true;
    IDX_CORE_INFO_TAG("NativePackage", "Loaded the IndexSDK native package example.");
    return 0;
}

extern "C" INDEX_PACKAGE_API void IndexPackage_OnUnload()
{
    s_Loaded = false;
}

extern "C" INDEX_EXAMPLE_PACKAGE_API int IndexExamplePackage_IsLoaded()
{
    return s_Loaded ? 1 : 0;
}