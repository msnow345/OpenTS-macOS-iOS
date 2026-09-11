# Building OpenTS

> [!IMPORTANT]
> OpenTS supports Visual Studio 2022 `Win32` and `x64` builds, each in Debug
> and Release. All four were verified from a fresh CMake configuration. A
> successful build does not verify runtime behavior.

## Supported target

| Component | Requirement |
| --- | --- |
| Host | Windows |
| Target platforms | 32-bit (`Win32`) and 64-bit (`x64`) |
| Processor | SSE2, so a Pentium 4 or Athlon 64 onward; the `x64` build needs a 64-bit processor and Windows |
| Generator and compiler | Visual Studio 2022 MSVC 19.30 or newer |
| Windows SDK | A Visual Studio-installed Windows SDK |
| CMake | 3.23 or newer |
| C++ language level | C++20 |
| Configurations | Debug and Release, on both platforms |

Other generators, compilers, architectures, and configurations are currently
unsupported.

Install Visual Studio 2022 with the **Desktop development with C++** workload,
a Windows SDK, and CMake 3.23 or newer. Git for Windows is needed to clone the
repository and initialize its dependencies, but not to compile a complete
source tree.

### Save and network compatibility between the platforms

A save records pointer identities at a fixed width, but the members and raw
structures around them travel at the build's own widths, so a `Win32` build and
an `x64` build do not read each other's saves. Their network packets differ for
the same reason.

Nothing detects this. The packed version stamp that saves and network packets
carry records the version, not the pointer width, so a build of either platform
accepts the other's save and admits it to a network game, and the result is a
failed load or a desync rather than a refusal. Until the stamp distinguishes
them, keep a saved game with the platform that wrote it, and play a network
game with peers running the same platform.

## Dependencies

The renderer uses [bgfx](https://github.com/bkaradzic/bgfx), vendored through
`thirdparty/bgfx.cmake` at a tested tag. That submodule contains bgfx, bx, and
bimg as nested submodules, so initialize it recursively:

```powershell
git submodule update --init --recursive
```

The audio layer uses [miniaudio](https://github.com/mackron/miniaudio),
vendored through `thirdparty/miniaudio` at a tested tag and compiled as one
translation unit from `thirdparty/miniaudio-impl.c`.

The UI shell uses [RmlUi](https://github.com/mikke89/RmlUi), vendored through
`thirdparty/RmlUi` at a tested tag and built static with the FreeType font
engine. Its samples carry their own window and renderer backends, which the
shell replaces, so none of them are built.

RmlUi's font engine uses [FreeType](https://freetype.org/), vendored through
`thirdparty/freetype` at a tested tag with bzip2, PNG, HarfBuzz, and Brotli
disabled. FreeType's bundled zlib supplies compressed font stream support.

Developer tooling uses [Dear ImGui](https://github.com/ocornut/imgui), vendored
through `thirdparty/imgui` at a tested tag. Only the core sources are compiled;
the bundled platform and renderer backends are not, because the shell feeds
ImGui through the engine's own message hook and draws it on bgfx.

`bimg_decode`, which bgfx already carries, decodes the PNG and TGA images UI
documents reference and is built and linked with everything else.

For a fresh clone, use `git clone --recurse-submodules`. Configuration stops
with instructions if a submodule is missing. Update a pinned tag in a
separate change.

Compression uses [LZO](https://www.oberhumer.com/opensource/lzo/) 2.10,
vendored under `thirdparty/lzo` and built by `thirdparty/CMakeLists.txt`.
Upstream publishes releases as a tarball rather than through a repository, so
this copy is checked in instead of pinned as a submodule. It holds only the
LZO1X-1 sources the engine calls; take a later release by extracting it over
the files already there, in a separate change.

## Configure and build

Run these commands from the repository root in PowerShell:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A Win32
cmake --build build --config Debug
cmake --build build --config Release
```

`-A` selects the platform, and a build directory holds one of them. Configure
`x64` beside the 32-bit build rather than over it:

```powershell
cmake -S . -B build/x64 -G "Visual Studio 17 2022" -A x64
cmake --build build/x64 --config Debug
cmake --build build/x64 --config Release
```

CMake normally finds Visual Studio through the Visual Studio Installer. For an
unregistered installation, set `CMAKE_GENERATOR_INSTANCE` to its directory and
product version.

The solution contains only Debug and Release. Each writes its runtime files to
`<build directory>/bin/<configuration>/` and copies nothing anywhere else. The
test harnesses build into `<build directory>/test-bin/<configuration>/`, so
`bin/` holds only what the game runs. Compiler and linker intermediates stay in
the selected build directory.

| Configuration | Runtime files |
| --- | --- |
| Debug | `GameD.exe`, `GameD.pdb`, `GameD.map`, `Language.dll` |
| Release | `Game.exe`, `Game.pdb`, `Game.map`, `Language.dll` |

Run a build from its output directory, naming the game data with `-DATADIR=`:

```powershell
build\bin\Debug\GameD.exe -DATADIR=Run
```

`OPENTS_GAME_DIR` names that data directory for the generated Visual Studio
debugger settings and defaults to `Run/`. The data directory is only read from.
Saved games, logs, and crash reports go to the user directory, which defaults to
the executable's own directory, so a build writes beside itself unless
`-USERDIR=` says otherwise.

## Experimental clang-cl cross-build

An unsupported Linux cross-build is available for compiler-portability work. It
uses native `clang-cl`, LLD, and LLVM library and resource tools with the
MSVC headers and libraries. It does not expand the supported build matrix or
establish runtime behavior.

The reconstructed codebase may still contain undefined behavior that the
supported MSVC build happens not to expose. A successful clang-cl build may
therefore run incorrectly or fail at runtime; validate any result separately.

Provide a directory containing a Visual Studio layout and Windows SDK. The
cross-build uses the layout's default MSVC toolset and newest complete SDK.
Configure a single-configuration Ninja build:

```bash
cmake -S . -B build/clang-cl -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/clang-cl-msvc.cmake \
  -DOPENTS_MSVC_ROOT=/path/to/msvc
cmake --build build/clang-cl
```

The toolchain requires `clang-cl`, `lld-link`, `llvm-lib`, `llvm-mt`, and
`llvm-rc` on `PATH`. It exports `compile_commands.json`; one configuration in
`.vscode/c_cpp_properties.clang.example.json` reads that file for IntelliSense.

## Experimental native build

An unsupported native build for the host platform is available for portability
work. It does not expand the supported build matrix or establish runtime
behavior.

```bash
cmake -S . -B build/native -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DOPENTS_EXPERIMENTAL_NATIVE=ON
cmake --build build/native
```

Run it the same way as a supported build, from `build/native/bin/`:

```bash
build/native/bin/Game -DATADIR=Run -USERDIR=build/native/user
```

This build is the one exception to the data directory being read only: the
shipped UI documents resolve under it, so the build writes `ui/` there.

Windows supplies the window, the message loop and the cursor itself. Every
other target gets them from `platform/win32compat`, which keeps the Win32
surface the engine is written against and supplies it from
[SDL](https://github.com/libsdl-org/SDL), pinned as `thirdparty/SDL` and built
with its audio, render and camera subsystems off. Audio stays on miniaudio.
That library is added to the build only when the target is not Windows, so a
Windows configure neither builds nor links it.

## Experimental iOS cross-build

An unsupported iOS build cross-compiles from macOS for porting work. It does
not expand the supported build matrix or establish runtime behavior.

It needs Xcode with the iOS SDK, Ninja, and CMake 3.23 or newer. `CMakePresets.json`
carries the three configurations:

```bash
cmake --preset ios-device
cmake --build --preset ios-device
```

| Preset | Result |
| --- | --- |
| `ios-device` | Ninja, `iphoneos` sysroot, `build/ios-device/bin/OpenTS.app` |
| `ios-simulator` | Ninja, `iphonesimulator` sysroot |
| `ios-device-xcode` | Xcode generator, for tooling that needs an `.xcodeproj` |

Each pins arm64 and `CMAKE_OSX_DEPLOYMENT_TARGET`, and every dependency is
compiled from `thirdparty/` by the same invocation, so one preset variable sets
the minimum OS version of the executable and of every static library it links.
The presets set `OPENTS_EXPERIMENTAL_NATIVE=ON`, since iOS reaches the game
through the same non-Windows path.

A display the player can neither resize nor move the game off names the frame
size itself, so the game is laid out in the smallest frame its art supports,
grown along whichever axis the display has room for. An iPad Pro 11-inch gives
640x440. Everything on screen, the sidebar cameos and the menu entries included,
is that many frame pixels across, so a smaller frame is what makes a target
large enough to hit with a finger. `ScreenWidth` and `ScreenHeight` in the
settings file still win where they are set.

An iOS application is its bundle, so the executable target carries the bundle
metadata and `cmake/ios/Info.plist.in` rather than handing a binary to a
separate Xcode shell project. The build stages `ui/` and `Language.dat` inside
the bundle, where the game finds them beside its executable.
`cmake --install <build> --prefix <dir> --component OpenTS` stages the bundle
alone for signing. `OPENTS_IOS_BUNDLE_ID` and `OPENTS_IOS_BUNDLE_VERSION` name
the bundle identifier and `CFBundleVersion`.

Two targets differ from the desktop native build. miniaudio's device layer
reaches AVAudioSession, so its translation unit compiles as Objective-C. The
`Language` library is not built at all: it carries no code off Windows, an
application bundle holds no loose libraries beside its executable, and the
strings come from `Language.dat` either way. Under the Xcode generator two of
bgfx's Metal sources are named as Objective-C++ as well, because bgfx asks for
that language through a compiler flag it sets only for the other generators.

### Signing, installing and running on a device

`scripts/build/ios/package-ios.sh` configures the Xcode preset, runs
`xcodebuild` once to mint or refresh the provisioning profile, stages the bundle
with `cmake --install`, adds the app icon, checks the staged bundle is complete,
re-signs it, and installs it. `--help` lists the flags; `--clean`,
`--resign-only`, `--install`, `--launch`, `--push-data` and `--touch-log` are the
ones that matter. Signing needs a development certificate: a distribution one
signs for the App Store and produces a bundle a device will not install
directly.

The team id, certificate, bundle identifier and device come from the environment
or from `scripts/build/ios/ios-signing.env`, which is not committed.
`scripts/build/ios/ios-signing.env.example` is the template and records the two
traps that cost the most: the team id is not the identifier inside the
certificate's parentheses, and an identity shortened to `Apple Development`
matches several certificates on a machine that has belonged to more than one
team.

The game reads its data from, and writes its own files into,
`Documents/OpenTS` inside the application's container. The Files app shows it,
so an installation can be dropped in and a saved game copied out without a
cable, and `--push-data <dir>` copies one there over the network. The whole
installation goes across, minus the Windows executables, the editor and the
documentation: the movie archives are most of the two gigabytes and the game
refuses to start without one. The engine's own log is written to
`Documents/OpenTS/Debug`, because the application bundle it would otherwise sit
beside cannot be written to.

A display that draws no pointer of its own loses what the pointer's shape said. Nothing else
on screen reports the modes a player arms from the sidebar: sell, repair, power, a waypoint
path, a super weapon. So the game draws that mode's own cursor, animated, in the bottom left
corner of the frame, held clear of whatever the host keeps for its own gestures, and draws
nothing at all while no mode is armed. The host decides whether it appears: a display with a
visible pointer gets no icon. `OPENTS_MODE_ICON` overrides that for working on it, `1` to
draw the icon and `0` to leave it off.

Fingers reach the engine as the pointer every other host writes. One finger taps
and drags, a held finger is the right button, and two fingers scroll the tactical
view. The recognizer writes down every finger, every decision and every message
it posts when a directory named `touchlog` exists beside the game: `touchlog`
inside the application's `Documents` folder on iOS, which the Files app shows and
a log can be copied out of, and `touchlog` in the working directory elsewhere,
where `OPENTS_TOUCH_LOG_DIR` names a different one. `-TOUCHLOG` on the command
line, or `OPENTS_TOUCH_LOG` in the environment, creates the directory rather than
waiting for one. Each run writes its own file and every line is flushed, so a run
that ends without a crash report still leaves its last gesture on disk.

## Build from Visual Studio Code

With the recommended extensions installed, the repository provides:

- CMake Tools settings;
- a configure task, a configuration picker, and hidden per-configuration tasks
  used by the launch configurations;
- launch and attach configurations;
- Test Explorer integration.

Standard VS Code shortcuts such as `Ctrl+Shift+B`, `F5`, and `Ctrl+F5` work as
usual.

## Build identity

The top-level `CMakeLists.txt` declares the project version in
`project(OpenTS VERSION ...)`. Since `project()` accepts only numbers, any
SemVer prerelease label goes in `OPENTS_VERSION_PRERELEASE`. Both values must
match the development entry in the manual's release registry;
`python manual/tools/manage.py check` verifies this.

Each build writes two generated headers from that version and the repository
state:

| Header | Contents |
| --- | --- |
| `opents_version.h` | The version components, the version string, a prerelease flag, and the packed version number |
| `opents_build.h` | The commit, branch, commit date, whether tracked files were modified, and the version as it is displayed |

The packed version stores the major, minor, and patch components in one byte
each. Saves and network peers reject a different number. Builds within one
release cycle, including prereleases, share it, but their saves, replays, and
network sessions may still be incompatible. The stamp does not record the
target platform; see
[Save and network compatibility between the platforms](#save-and-network-compatibility-between-the-platforms).

The version resources in `Game.exe` and `Language.dll`, the title screen,
version dialog, crash report, and debug log banner all read these headers. A
normal build shows the version and commit, such as `0.1.0 (ab12cd3)`, plus a
marker when tracked files are modified. The commit identifies the build for
diagnostics; it is not a save or network compatibility stamp. An official
build configured with `-DOPENTS_OFFICIAL_BUILD=ON` shows only its declared
version.

`opents_version.h` changes only with the version, so an ordinary commit does not
rebuild code that reads only that header. `opents_build.h` is checked on every
build, so a new commit appears without reconfiguring; an unchanged header is
not rewritten.

A tag or pull-request build uses a detached checkout with no branch. Its stamp
uses a ref that points to the commit, preferring a tag, so a pull-request CI
build names the pull request instead of `HEAD`.

Git is optional at build time once the complete source tree is present. Without
Git or repository metadata, the build records the commit as `unknown` and shows
the version without one.

## Continuous integration

The `Engine` workflow runs for ready pull requests and pushes to `main` when
their changed paths match its engine and build filters. Draft pull requests do
not build until marked ready; the workflow then builds their current commit.

`Engine nightly` runs daily. A scheduled run cancels itself when the newest
commit is at least 25 hours old; manually started runs always build. This keeps
the latest successful scheduled run attached to downloadable artifacts.

Both use the reusable `Engine build` workflow. It runs one job per platform and
configuration, four by default, each on its own Windows runner with Visual
Studio 2022. A job configures and builds its platform with the commands above,
runs CTest, and uploads the executable, language library, symbol file, and
license notices. Artifact names contain the platform, configuration, and short
commit, as in `opents-x64-Release-ab12cd3`. Linker maps are omitted because the
symbol files are sufficient. A failure on either platform fails the workflow.
After a successful pull-request build, `Engine build comment` maintains one
pull-request comment with direct nightly.link downloads.

Publishing a GitHub release runs `Engine release`. It builds the release commit
for both platforms with `-DOPENTS_OFFICIAL_BUILD=ON`, and packages each one's
`Game.exe`, `Language.dll`, `Game.pdb`, and the project and third-party license
notices in a zip named after the release tag and the platform, such as
`OpenTS-v0.2.0-x64.zip`. It attaches both to the release, and appends notes
generated from the manual's change records by
`python manual/tools/manage.py release-notes`. See
[Maintaining](../manual/MAINTAINING.md) for the full release procedure.

CI collects the uploaded artifacts from `build/bin/<configuration>/`.

## Verification boundary

The supported matrix was verified on September 11, 2026 with CMake 4.3.3,
Visual Studio 2022 Community 17.14.37614.0, MSVC 19.44.35228, and Windows SDK
10.0.26100. Fresh Win32 and x64 builds completed successfully in both
configurations, and CTest passed all 40 tests in each of the four. The builds
retain inherited MSVC warnings; warnings are not treated as errors, but
contributions should not add new warnings.

This verifies only that the supported toolchain compiles, links, passes the
tests, and produces the listed files. Runtime behavior requires separate play
testing, and the x64 build has none of that history: only the Win32 build has
been played.

The repository contains no maps, movies, audio, or other original game assets.
Keep legally obtained runtime data local and outside version control. The
repository safety rules are in [CONTRIBUTING.md](../CONTRIBUTING.md).
