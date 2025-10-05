# Hantei-chan

![Build Status](https://github.com/Armonte/Hantei-chan/actions/workflows/build.yml/badge.svg)

A modern frame data editor for French Bread fighting games (Melty Blood, Under Night In-Birth) with advanced visualization and editing tools.

![screenshot](https://user-images.githubusercontent.com/39018575/119175726-311f4580-ba38-11eb-83dd-2d7d57c17f02.png)

## Features

- **Frame Data Editing** - Edit hitboxes, hurtboxes, and collision data
- **Pattern Management** - Organize and edit character move patterns
- **Effect System** - Visual preview and editing of effect spawns and relationships
- **Spawned Pattern Visualization** - Recursive tree view showing all spawned patterns with their timing and offsets
- **Undo/Redo** - Full undo/redo stack for safe editing
- **Timeline View** - Visual timeline for effect spawns and pattern timing
- **Multi-Tab Interface** - Work on multiple patterns simultaneously
- **Live Parameter Updates** - See changes in real-time with ImGui drag controls

## Download

### Latest Release
Get the latest stable release from the [Releases page](https://github.com/Armonte/Hantei-chan/releases)

### Development Builds
Development builds are available from successful [GitHub Actions runs](https://github.com/Armonte/Hantei-chan/actions) (requires GitHub login)

## Quick Start

1. Download the latest release
2. Extract the ZIP file
3. Run `gonptechan.exe`
4. Open a `.ha6` character file

## Controls

- **Left click + drag** - Scroll the view
- **Right click + drag** - Draw hitboxes
- **Arrow keys** - Navigate frames/patterns
- **Z/X** - Switch selected box type
- **Ctrl+Z / Ctrl+Y** - Undo/Redo
- **Ctrl+S** - Save

## Documentation

- [Issues](https://github.com/Armonte/Hantei-chan/issues) - Report bugs and request features
- [Effect Type 3 Status](Hantei_Docs/Effect_Type3_Implementation_Status.md) - Preset effect implementation
- [Multi-View Architecture](Hantei_Docs/multi_view_architecture.md) - Technical documentation

## Building from Source

### Requirements
- CMake 3.27+
- MinGW-w64 (Windows) or cross-compile toolchain (Linux)

### Windows
```bash
git clone --recursive https://github.com/Armonte/Hantei-chan.git
cd Hantei-chan
cmake -B build -G "MinGW Makefiles"
cmake --build build
```

### Linux (MinGW cross-compile)
```bash
git clone --recursive https://github.com/Armonte/Hantei-chan.git
cd Hantei-chan
cmake -B build -DCMAKE_TOOLCHAIN_FILE=mingw-toolchain.cmake
cmake --build build
```

See [.github/README.md](.github/README.md) for CI/CD details.

## Contributing

Contributions are welcome! Please:

1. Check the [Issues tab](https://github.com/Armonte/Hantei-chan/issues) for current bugs and feature requests
2. Fork the repository
3. Create a feature branch
4. Submit a pull request

## Credits & Acknowledgments

This project stands on the shoulders of many talented developers and reverse engineers in the French Bread modding community.

### Original Creator
- **omanko (Meifuku)** - Original Hantei-chan creator

### Current Maintainers
- **gonpgonp** - Active maintainer ([repo](https://github.com/gonpgonp/Hantei-chan))
- **Armonte** - This fork, unifying the best features from all forks

### Fork Contributors
- **sosfiro** - PAT editor, tab system, view options, auto backup, render improvements
- **Eiton** - Shortcuts viewer, UI improvements ([repo](https://github.com/Eiton/Hantei-chan))
- **faith (Fatih120)** - UNI/UNIB support, extensive documentation, character repository ([repo](https://github.com/Fatih120/undernightinbirth))
- **raijinili** - Fork contributions ([repo](https://github.com/Raijinili/Hantei-chan))
- **dante-chiesa** - Fork improvements ([repo](https://github.com/dante-chiesa/Hantei-chan))

### Community Tools & Research
- **mauve** - Reversing legend: IAMP and many hacking tools/casters
- **madscientist** - CCCaster for MBAACC, rollback netcode
- **rhekar** - Current CCCaster maintainer
- **u4ick** - FPAN tools, PAT tool, bgmake, cglib, and many other essential tools
- **pc_volt** - MeltyLib Ghidra decompilation and database ([MeltyLib](https://github.com/PCvolt/MeltyLib))
- **dantarion** - Community contributions

### Additional Contributors
- See [GitHub Contributors](https://github.com/Armonte/Hantei-chan/graphs/contributors) for code contributors

### Project Vision
This fork aims to unify the best features from all community forks into a comprehensive tool that benefits the entire French Bread modding ecosystem. We're building on years of incredible work by the community.

## License

This project maintains the same license as the original Hantei-chan.

## Support

- **Issues** - Report bugs or request features on the [Issues page](https://github.com/Armonte/Hantei-chan/issues)
- **Discussions** - Ask questions in [GitHub Discussions](https://github.com/Armonte/Hantei-chan/discussions)
