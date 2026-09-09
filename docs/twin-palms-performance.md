# Twin Palms validation

Measured September 9, 2026 with the Release build, Mesa Intel Arc (MTL), 1600×900.
Run `./sTA --benchmark` from `out/build/linux-release` to repeat. VSync is disabled;
GPU completion is included. Each view warms for 100 frames, then each paused/active
phase discards 20 frames and measures 120. These are stationary camera samples,
not a guarantee of the frame rate throughout a drive. No other game test was running
during this measurement.

| Active view | Median frame | 95th percentile | Actors | Terrain detail tiles | Scenery batches | Detail mesh GPU memory |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Downtown | 10.45 ms | 16.46 ms | 43 | 42 | 76 | 6.68 MiB |
| Ocean Drive | 4.53 ms | 12.91 ms | 10 | 40 | 85 | 5.29 MiB |
| Sierra | 10.50 ms | 19.29 ms | 1 | 45 | 5 | 4.49 MiB |

The memory column covers resident detailed terrain and building/paint vertex
buffers. It excludes the shared model/texture catalog, coarse horizon, ocean mesh,
map textures, CPU metadata and driver allocations. It is not total process/GPU memory.

Validation covers:

- Five CTest suites: physics, every road in both directions, terrain/LOD geometry,
  imported models/CPU-reference skinning, and six ten-minute inter-district traffic
  simulations. Hiking paths use a pedestrian footprint in the road traversal test.
- Debug and Release OpenGL smoke tours through all 17 landmarks, including 180°
  camera turns and long-distance teleports. Each frame checks the 96 terrain tile,
  96 scenery batch and 64 ambient actor limits. An isolated ocean teleport verifies
  that scenery meshes and ambient actors are actually evicted.
- Spawn/courier metadata, mission lifecycle, swimming, vehicle recovery, debug
  spawning, and shader/model rendering in the smoke tests.
- SHA-256 consistency between the saved map assets and their manifest.

Collision is always available from the saved heightfield and spatial index. Visual
streaming can produce a brief detail transition after a teleport; the coarse terrain
remains visible while the nearby meshes arrive. Ambient actors restart their saved
routes when their area is revisited. Hospital and police buildings are exterior
landmarks; this feature does not add interior or emergency-service gameplay.
