<p align="center">
  <img src="https://raw.githubusercontent.com/OpenTS-Developers/.github/main/assets/opents-logo.png" alt="OpenTS" width="512">
</p>

<p align="center">
  <a href="https://github.com/OpenTS-Developers/OpenTS/releases"><img src="https://img.shields.io/github/downloads/OpenTS-Developers/OpenTS/total?label=downloads" alt="Downloads"></a>
  <a href="https://github.com/OpenTS-Developers/OpenTS/actions/workflows/engine.yml"><img src="https://github.com/OpenTS-Developers/OpenTS/actions/workflows/engine.yml/badge.svg" alt="Engine build"></a>
  <a href="https://opents-developers.github.io/OpenTS/"><img src="https://github.com/OpenTS-Developers/OpenTS/actions/workflows/manual-pages.yml/badge.svg" alt="Manual"></a>
  <a href="https://www.patreon.com/c/ZivDero"><img src="https://img.shields.io/badge/Patreon-ZivDero-F96854?logo=patreon&logoColor=white" alt="Patreon"></a>
</p>

# OpenTS

OpenTS is a community-led, open-source reconstruction of *Command & Conquer:
Tiberian Sun*. Instead of patching or extending the retail executable, it
rebuilds the engine as a standalone program.

This fork carries a native Apple Silicon macOS build of that engine. See
[Running on macOS](#running-on-macos) for the data script, the build and how to
start it. Everything below describes OpenTS itself and applies here too.

OpenTS gives equal weight to two goals: maintaining a playable engine and
providing a capable platform for modding and engine development. Work on one
goal should not come at the expense of the other.

OpenTS is:

- an independent, community-led source reconstruction targeting Tiberian Sun
  2.03 Firestorm;
- a playable engine based on Electronic Arts' GPL-released source for related
  Command & Conquer games and Tiberian Sun-specific reverse engineering; and
- the active base for maintenance, documentation, modernization, bug fixes,
  and new modding capabilities.

OpenTS is not:

- a remaster or remake;
- an official Electronic Arts source release; or
- a distribution of the original game assets.

OpenTS is an independent community project and is not affiliated with or
endorsed by Electronic Arts.

## Community

- Discord: <https://opents.net/discord>
- Bug reports and proposals:
  [GitHub issues](https://github.com/OpenTS-Developers/OpenTS/issues)

## Downloads

- **Releases** are the recommended builds. Each zip on the
  [releases page](https://github.com/OpenTS-Developers/OpenTS/releases)
  contains `Game.exe`, `Language.dll`, and `Game.pdb`.
- **Nightly builds** are development snapshots from the
  [Engine nightly](https://github.com/OpenTS-Developers/OpenTS/actions/workflows/engine-nightly.yml)
  workflow. Download the latest one without a GitHub account through
  [nightly.link](https://nightly.link/OpenTS-Developers/OpenTS/workflows/engine-nightly/main).
  Nightlies contain the latest merged changes without release validation and
  expire after 90 days.

## Installing

1. Install Tiberian Sun from Command & Conquer The Ultimate Collection on
   Steam or the EA App.
2. Extract the release zip into the Tiberian Sun game directory.
3. Run `Game.exe`.

OpenTS supplies the engine, not the game data: the installation above
provides the original assets. There is no installer, and no extra runtime
library or launch argument is required. Windows is supported; Wine may work,
but there is no supported native Linux build. The engine asks Windows for the
UTF-8 code page, which needs Windows 10 version 1903 or newer. Older Windows
keeps its own code page, so game text still shows, but a path or file name
holding a character that code page lacks may fail.

## Running on macOS

This fork builds and runs the engine natively on Apple Silicon. The game data
still comes from a copy of Tiberian Sun you own.

Install the tools:

```bash
brew install cmake ninja
brew install --cask steamcmd
```

Fetch the game data from your own Steam account:

```bash
./scripts/get-assets.sh <your_steam_username>
```

The script downloads app 2229880 into `Run/`, skips the Windows executables the
engine replaces, and checks that the archives startup needs actually arrived.
Steam Guard prompts for a code on first login. **Quit the Steam desktop client
first**: steamcmd shares its data directory, and a running client holds a lock
that makes steamcmd hang after "Verifying installation..." with no error. Set
`OPENTS_GAME_DIR` to put the data somewhere other than `Run/`.

Build the engine:

```bash
git submodule update --init --recursive
cmake -S . -B build/native -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DOPENTS_EXPERIMENTAL_NATIVE=ON
cmake --build build/native --target OpenTS
```

Build the `OpenTS` target rather than everything: the C++ test harnesses pass
MSVC-only flags and link `kernel32`, so they do not configure here.

Run it:

```bash
build/native/bin/Game -DATADIR=Run -USERDIR=build/native/user
```

`-USERDIR` is where saves, `SUN.INI` and logs are written, so pointing it at a
scratch directory leaves an existing install untouched. Full screen is a
setting in the display options screen.

Windows supplies the window, message loop and cursor itself; every other
platform gets them from `platform/win32compat`, which serves the Win32 surface
the engine is written against out of [SDL](https://github.com/libsdl-org/SDL).
[Building OpenTS](docs/BUILDING.md) covers the build in full.

## Documentation

The [OpenTS manual](https://opents-developers.github.io/OpenTS/) documents
setup, runtime behavior, INI configuration, mapping, and source-level
internals.

## State and plans

Release 0.1.0 runs the full Tiberian Sun 2.03 Firestorm game. The GDI, Nod,
and Firestorm campaigns, skirmish, and save/load have received full
play-through testing. LAN multiplayer has had more limited testing. No
user-visible regression from the original game is currently known. The
renderer uses
[bgfx](https://github.com/bkaradzic/bgfx) and supports modern resolutions
through 4K, including ultrawide.

The first of the project's three development milestones is the current focus:

1. CnCNet and CnCNet client support, including porting the parts of
   [ts-patches](https://github.com/CnCNet/ts-patches) this requires.
2. Feature parity with
   [Vinifera](https://github.com/Vinifera-Developers/Vinifera) and the rest
   of ts-patches.
3. Extending Tiberian Sun with new features, striving toward feature parity
   with Red Alert 2 and Yuri's Revenge, and growing engine capabilities that
   match or exceed the popular Yuri's Revenge engine extensions.

Alongside these goals, the engine is modernized incrementally toward an
entity-component architecture, and new development is shaped so that
migration stays possible. [Project direction](docs/DIRECTION.md) explains the
reasoning.

## Building

OpenTS builds as a 32-bit Windows target with Visual Studio 2022 and CMake.
[Building OpenTS](docs/BUILDING.md) documents the exact requirements,
commands, and outputs.

## Contributing

Bug reports, proposals, documentation improvements, and focused pull requests
are welcome. Review capacity is limited, so discuss non-trivial work with the
maintainers before implementing it. [CONTRIBUTING.md](CONTRIBUTING.md)
explains the current priorities and review policy; source conventions are in
[Style](docs/STYLE.md).

## Origins

OpenTS continues the community reconstruction preserved in the
[TibSun archive](https://github.com/OpenTS-Developers/TibSun), built from
Electronic Arts' published source for related Command & Conquer games and
completed through reverse engineering against the original executable.
[History](docs/HISTORY.md) records the reconstruction's lineage and methods.

## License and acknowledgements

OpenTS is licensed under the GNU General Public License, version 3 or later.
Material derived from Electronic Arts source remains subject to the additional
GPL Section 7 terms in [LICENSE.md](LICENSE.md).
[Third-party notices](THIRD_PARTY_NOTICES.md) identify bundled dependencies
and their licenses.

[ACKNOWLEDGEMENTS.md](ACKNOWLEDGEMENTS.md) thanks the people, projects, and
communities whose work made OpenTS possible.
