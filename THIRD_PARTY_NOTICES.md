# Third-party components

SCAPMS uses Qt 6.8.3 Core, Gui, Widgets and their platform/image plugins,
dynamically linked. Qt is copyright The Qt Company Ltd and other contributors.
The open-source Qt libraries used here are available under LGPL v3, with some
files under compatible permissive terms. This application's source and build
instructions are included; the DLLs are replaceable.

Qt licensing: https://www.qt.io/licensing/open-source-lgpl-obligations
Qt 6.8.3 source: https://download.qt.io/archive/qt/6.8/6.8.3/single/
Qt license texts: https://code.qt.io/cgit/qt/qtbase.git/tree/LICENSES?h=v6.8.3

The Windows package also contains MinGW-w64 / GCC runtime components distributed
with Qt's MinGW 13.1 toolchain. GCC runtime libraries are covered by their
applicable licenses and the GCC Runtime Library Exception.
https://www.gnu.org/licenses/gcc-exception-3.1.html
https://www.mingw-w64.org/

CMake, Ninja and aqtinstall are build tools and are not bundled in the application
distribution. ReportLab is used only to generate the assessment PDF.
