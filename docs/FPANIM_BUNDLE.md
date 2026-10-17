# fpan-im: install from Hantei-chan, and the optional bundle

**Game > fpan-im (UNI2 / MBTL): install, status** shows, for the UNI2 and MBTL game folders (the same folder settings
as the Script panel's hot reload, `SqGameDirUni2=` / `SqGameDirMbtl=`):

- **fpan-im status**: not installed / installed (core and shim versions) / partly installed. The versions come from
  the DLLs' version resource (`GetFileVersionInfo`; the DLL is never loaded). A winmm.dll counts as ours when its
  ProductName is `fpan-im`, or, for builds older than the resource, when it contains the `fpan-im` marker string.
- **Game**: connected (the `fpan-im-uni2` / `fpan-im-mbtl` pipe answers `ping`), running but not connected, or not
  connected. The ping runs on a worker every 3 s.
- **Install / Update / Uninstall** copy or remove `<game>\winmm.dll` and `<game>\fpan-im\fpan-im.dll` only.
  - Refused while `uni2.exe` / `MBTL.exe` runs from that folder.
  - A winmm.dll that is not fpan-im is moved to `winmm.chain.dll`, which fpan-im's shim chainloads. If
    `winmm.chain.dll` already exists, it is kept, and the foreign winmm.dll is saved as `winmm.dll.bak-<time>`. The
    window says which. The install writes `fpan-im\install.txt` (`backup=winmm.chain.dll`).
  - Uninstall removes our winmm.dll, `fpan-im\fpan-im.dll` (and `fpan-im\*.pdb`) and `install.txt`. It moves our
    `winmm.chain.dll` back to `winmm.dll`. It keeps `fpan-im\mods`, `fpan-im.ini` and the logs.
  - UNI2-IM / MBTL-IM (`dinput8.dll` and their folders) are never touched. Each plan is checked against the list of
    fpan-im file names before it runs (`fpaninst::PlanTouchesOnlyOurFiles`, tests/fpanim_install_test.cpp).

## The bundle (`res/fpan-im/`)

The copy that gets installed comes from `<Hantei-chan exe>\res\fpan-im\`:

```
res/fpan-im/winmm.dll
res/fpan-im/fpan-im/fpan-im.dll
res/fpan-im/VERSION        (text, shown next to the versions)
```

fpan-im is a private repo, so the bundle is **optional**. The binaries are git-ignored, and the build and the CI pass
without them. Without a bundle, the window says **"fpan-im not bundled"**. **Choose fpan-im folder…** then points it
at any folder that holds `winmm.dll` plus `fpan-im.dll`, either side by side (fpan-im's `build/bin`) or in the
`fpan-im\` layout above. The choice is saved as `FpanimBundleDir=` in the ini.

Fill it locally:

```
cd /mnt/c/dev/fbmod && ./build.sh                    # fpan-im's own build
cd <hantei-chan worktree> && tools/fpanim/bundle.sh  # copies build/bin -> res/fpan-im, stripped, writes VERSION
./build.sh                                            # copies res/fpan-im next to the exe (build/res/fpan-im)
```

`FPANIM_BIN=/other/bin tools/fpanim/bundle.sh` takes the DLLs from another folder, and `tools/fpanim/bundle.sh --clean`
removes the bundle. CMake copies `res/fpan-im` next to the exe only when `res/fpan-im/winmm.dll` exists at configure
time; `build.sh` re-configures on every run. CI release packaging copies the whole `res/` folder. Its optional
"fpan-im bundle" step downloads a zip of this layout when the `FPANIM_BUNDLE_URL` secret is set, and is skipped
otherwise.
