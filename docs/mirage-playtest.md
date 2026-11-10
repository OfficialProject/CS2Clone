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
