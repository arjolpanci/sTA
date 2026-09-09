# Twin Palms (heightfield v2, scene v3)

This directory contains the saved playable world. Commit all four assets together.
CMake copies them; neither the build nor the game invokes the authoring tool.

```sh
python3 tools/build_island.py
cmake --build out/build/linux-debug
ctest --test-dir out/build/linux-debug --output-on-failure
```

The deterministic, standard-library Python authoring tool builds an original coastal
city inspired by Vice City's geography. Two islands face each other across a bay.
Ocean Drive, its promenade and two beaches occupy the eastern shore; residential
streets, civic buildings and parks connect to the western island's forested Sierra.
Roads follow authored spline control points. Lots face curved streets, vegetation
respects road clearance, and bridges remain separate decks above the seabed.
Use `--output /tmp/island-preview` for experiments.

Files and units:

- `island.bin`: `STAISL2\n`, little-endian uint32 resolution, float32 spacing,
  sea level and seed, then resolution² float32 heights and coarse road coverage.
  A uint32 fine-mask resolution and resolution² unsigned bytes follow. The current
  terrain is 1025² samples at 6 m spacing (6144 m square); road coverage is 4097²
  samples at 1.5 m spacing. Coverage 145 marks earth paths, 255 asphalt.
- `island.scene`: `STA_SCENE 3`, followed by whitespace-separated records.
  `B`/`D`: solid/decorative box (center XYZ, size XYZ, RGB, facade flag, yaw).
  `T`: tree (feet XYZ, height, yaw, model index).
  `U`: prop (feet XYZ, yaw, height, model index).
  `M`: terrain-following paint (center XZ, size XZ, RGB).
  `R`: walkable ramp (center XZ, width, length, along-X flag, low/high heights, RGB).
  `L`: road (half-width, point count, XYZ points).
  `V`: vehicle (model, yaw, speed, point count, XYZ points).
  `P`: pedestrian (RGB, speed, point count, XYZ points).
  `A`: landmark (kind, XYZ, name with underscores instead of spaces).
  Heights and dimensions are metres; yaw is degrees. Rows run from negative Z
  to positive Z and columns from negative X to positive X, centered on the origin.
- `island-overview.png`: north-up terrain, roads, buildings and bridges, baked for
  the minimap so static map geometry does not need rebuilding each frame.
- `island.json`: world name, dimensions, landmark catalog, counts and SHA-256 hashes.

Vehicle/tree/prop indices follow `src/Game/model_catalog.hpp`. Scene v1/v2 are still
accepted; v3 bakes street props instead of performing expensive global placement at
startup. Model sources and license records remain in `resources/models/`.

## Runtime residency

Full height, road, placement and spatial collision metadata stay in CPU memory.
Collision queries never depend on whether a visual mesh has arrived. Model geometry
and textures are shared catalog assets, loaded once rather than copied per placement.

Terrain starts with small coarse horizon tiles. A single background job builds at
most two 192 m tiles at a time; the render thread uploads completed meshes. Near tiles
use the physical 6 m grid, medium tiles use 24 m sampling, and the horizon uses 48 m.
Skirts close LOD boundaries. At most 96 detailed tiles remain resident. Requests are
prioritized by camera/player distance and visibility, with nearby coverage in all
directions for turns and shadow casters. Obsolete jobs are discarded after teleports.

Buildings and paint retain source record references in 128 m cells. At most two
batches are built/uploaded per frame, with a 96-batch cap. Distance hysteresis retains
recently visible cells without allowing memory to grow as the player explores.
Trees and props use nearby spatial cells and shared GPU instancing; both color and
shadow passes also perform their own frustum tests.

Ambient actors activate near the player, at most one successful spawn per rendered
frame, with limits of 24 vehicles and 40 pedestrians. Distant actors retire after
750 m; their palette buffers are released. The driven car stays alive. Returning to
an area recreates ambient actors from saved routes. Manually spawned debug cars are
session objects outside the ambient cap. Shared actor model assets remain cached.

F1 → Rendering shows detailed tile/batch residency and their GPU mesh bytes. Those
bytes exclude shared models/textures, the coarse horizon, water, and metadata.
`--smoke-test` tours all landmarks and checks caps across teleports; `--benchmark`
measures downtown, Ocean Drive and the Sierra with ambient streaming enabled.
