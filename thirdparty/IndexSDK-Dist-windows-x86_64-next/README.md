# Index Native SDK

For a Visual Studio 2022 C++ project:

1. Open View > Other Windows > Property Manager.
2. Add the existing property sheet `IndexSDK.props` to `Dist | x64`.
3. Include `<Index.hpp>` and place `<EntryPoint.hpp>` in exactly one `.cpp`.

The property sheet supplies the single include root and loads `IndexSDK.targets`
after the project settings. The targets file applies compiler defines, the import
library, the Windows subsystem, system libraries, and post-build deployment of
runtime DLLs and `IndexAssets`.

The active scene receives a runtime `Camera2D` before its systems start when the
2D renderer is enabled and the scene did not create one during `OnLoad`. Call
`WithoutDefaultCamera2D()` on a scene definition to opt out.

`examples/MinimalApp/MinimalApp.vcxproj` is a buildable reference project.
It references `examples/NativePackage/Pkg.Example.Native.vcxproj`, stages the
DLL in the PackageHost layout, and verifies during `--smoke-test` that the
package loaded before startup-scene deserialization.

For your own engine-linked package DLL, import `IndexPackageSDK.props`, name
the output `Pkg.<Name>.Native.dll`, and keep it beside the generated
`.dll.indexabi` file. Native packages use Index's C++ ABI and therefore must be
rebuilt whenever the SDK changes. The sidecar makes a stale binary fail before
`LoadLibrary` instead of crashing inside package initialization. Package public
headers and the generated import `.lib` can also be consumed normally by C++;
including a header alone does not run `IndexPackage_OnLoad`.