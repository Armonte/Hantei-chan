If you have a problem open an issue.

# Hantei-chan #
Hantei6 GUI editor for Melty Blood.
I don't really care too much about modding melty so I won't be updating this anymore.
Feel free to fork it. By the way, this program only for windows. 

Stuff that needs to be done:
* Play animations
* Copy pasting lots of stuff 
* Redo/Undo stack
* Display .pat graphics
* Actual support for Under Night In Birth.
* A sprite browser
* Fix the held spinner controls
* More hotkeys, like Ctrl+S
* Other QoL features

-----------------------
## Hotkeys and stuff ##

Left click and drag in the display to scroll the view.
Drag right click to draw a box.
You can switch frames/patterns with the arrow keys.
You can switch the currently selected type of box with Z and X.
Don't try to press these hotkeys while you're drawing a box. You'll just make a mess.

There's also a slight bug when you click a control when the window is unfocused. Yeah, idk.

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
