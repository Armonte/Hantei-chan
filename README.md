# Hantei-chan #
Hantei6 GUI editor for Melty Blood.
I don't really care too much about modding melty so I won't be updating this anymore.
Feel free to fork it. By the way, this program only for windows. 

Stuff that needs to be done:
* Play animations
* Copy pasting stuff
* Redo/Undo stack
* Display .pat graphics
* Better support for Under Night In Birth.
* A sprite browser
* Fix holding the arrow in spinner controls
* Anything else you may need

-----------------------
## How to build ##
Building is extremely simple, and it works with both MSVC and Mingw-w64.
Clone the git, init the submodules and run cmake.
Assuming you're using ninja:

```
git clone --recursive [insert repo link here] hanteichan
cd hanteichan
mkdir build
cmake -S. -Bbuild -GNinja -DCMAKE_BUILD_TYPE=Release
cmake --build build
build\hanteichan.exe
```

And that's it!