Hantei6 GUI editor for Melty Blood. This program only for windows.

-----------------------
## How to build ##
Building is extremely simple, and it works with both MSVC and Mingw-w64.
Clone the git, init the submodules and run cmake.
Assuming you're using ninja:

```
git clone --recursive https://github.com/meifuku/HA6-gui.git
cd HA6-gui
mkdir build
cmake -S. -Bbuild -GNinja -DCMAKE_BUILD_TYPE=Release
cmake --build build
build\ha6gui.exe
```

And that's it!