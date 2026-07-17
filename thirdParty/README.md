# Third-Party Runtime

`thirdParty/bin` contains the Windows x64 runtime DLLs required by the bundled Assimp import library. CMake copies these files next to `openglStudy.exe` after every build.

The binaries were obtained from the local vcpkg `x64-windows` installation:

- Assimp
- jhasse/poly2tri
- minizip
- zlib
- kuba--/zip
- pugixml

Their license texts are preserved in `thirdParty/licenses`. These binaries and libraries remain third-party components and are not covered by any project-level license.
