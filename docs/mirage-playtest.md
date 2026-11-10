# Playtest Mirage locally

The Movement Feel Lab automatically opens Mirage when `mirage.map` is beside `feel_lab.exe`. It starts at a T spawn from the map. Reset returns you to that spawn. Without a local or embedded map file, the executable opens the target range. In either arena, clear five targets to finish a timed practice round. Hold Fire to shoot, press E to reload by default, and use Reset to start over.

The imported level uses the installed CS2 world and props with flat shading. The lab uses the compiled world collision and selected solid entity meshes for a swept player hull, walls, floors, slopes and steps. The CS:GO-style movement simulator still runs at 128 Hz. The lab supports the imported ladder surfaces. Move toward a ladder to climb, back away to descend while attached, or press Jump to detach. Moving props and breakables still require implementation. Bomb and buy volumes are excluded from collision.

## Rebuild the map from an installed copy

Download the Windows x64 CLI from [Source 2 Viewer 20.1](https://github.com/ValveResourceFormat/ValveResourceFormat/releases/tag/20.1) and extract its ZIP. Use Python 3.12 with NumPy 2.5.4. From the repository root, set the paths for your installation and build.

```powershell
python -m pip install numpy==2.5.4
python tools/map_import/import_mirage.py "C:/Program Files (x86)/Steam/steamapps/common/Counter-Strike Global Offensive/game/csgo/maps/de_mirage.vpk" --viewer-cli "path/to/Source2Viewer-CLI.exe" --work build/mirage-export --out build/Release/mirage.map
& build/Release/feel_lab.exe --validate-map build/Release/mirage.map
& build/Release/feel_lab.exe
```

The importer writes the map and a JSON report with the input hash, triangle counts, spawn coordinates, and export filters. Use a new export directory and output filename on each run. The map stays local and is loaded separately from the executable. You can also pass its path with `--map`.

The Windows playtest workflow builds and runs asset-independent collision tests. The optional `--validate-map` command loads a local map and checks a spawn jump, landing, and movement from T spawn without opening a window.

Open **Local Lobby** to choose Target Practice, Plant the Bomb as T, or Defuse the Bomb as CT. Settings are available from the main menu, lobby, pause menu and match. Press Esc during a round to resume, restart, or return to the lobby. Completed rounds display a result and offer a rematch.

In Plant mode, hold Use / C4 (F by default) on the ground at A or B for three seconds, then let the 40-second fuse run out. In Defuse mode, start at CT spawn with a live bomb at A. Hold Use / C4 near it for ten seconds before it detonates. Releasing the control interrupts progress. The fuse, arming and no-kit defuse times follow the CS:GO snapshot. The 115-second plant window is a local practice preset. These scenarios run solo.

Choose **Offline Match** in the Mirage lobby for a short local match against two bots. You start as T, switch to CT after three rounds, and play until either side wins four rounds or the score is tied after six. The buy menu opens before each round. Use **B** to reopen it during the buy phase and **1 / 2 / 3** to equip rifle, pistol, or knife. Reload, fire, and Use follow your settings. The scoreboard appears after each round and again at match end.

The bots guard A, shoot when they have a clear line of sight, and move locally to plant or defuse. They use collision but do not navigate the full map. Weapon models are geometric placeholders with a separate 60-degree viewmodel projection. Weapon and economy values are local match tuning.
