# Harbor Island (baked asset, version 1)

This directory is the playable world, not a cache. Commit its files with map edits.
The game loads them as read-only assets and never calls the authoring tool.
CMake copies them into the build's resource directory; it does not regenerate them.

To deliberately rebuild:

```sh
python3 tools/build_island.py
cmake --build out/build/linux-debug
ctest --test-dir out/build/linux-debug --output-on-failure
```

The standard-library-only tool uses seeded, five-octave value noise, radial coastline
shaping, ridged mountain detail, a tidal channel, and road/terrace earthworks. Roads,
buildings, trees, bridges and patrol routes are baked alongside the heightfield.
Edit the authored district/road definitions in the script to change the layout.
Use `--output /tmp/island-preview` to experiment without replacing the playable map.

Files:

- `island.bin`: `STAISL1\n`, then little-endian uint32 resolution, float32 sample
  spacing, sea level and seed; then resolution-squared float32 elevations followed
  by resolution-squared float32 road-mask samples. Rows run from negative Z to
  positive Z; columns from negative X to positive X. The grid is centered at 0,0.
- `island.scene`: versioned text records. `B` is a solid box, `D` a decorative box,
  `R` a ramp, `V` a vehicle with patrol points, `P` a pedestrian with patrol points,
  and `L` an authored road segment for validation and map overlays. Heights are
  absolute metres. Bridge decks remain separate geometry above the seabed.
- `island-overview.png`: saved north-up terrain/road preview used by the minimap.
- `island.json`: dimensions, seed, counts and SHA-256 hashes of those three files.

No imported data, textures or third-party asset licenses are needed for this map.
The heightmap triangles and physical support queries share the same cell diagonal;
collision therefore matches the rendered surface rather than a different bilinear
height approximation. The water shader uses filtered height samples for its visual
shoreline/depth effect, and is not a fluid simulation.
