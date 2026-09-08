# Imported CC0 models

- Player: Quaternius **Casual Character**, Ultimate Modular Men Pack (24 clips).
- Pedestrians: Quaternius Animated Men and Animated Women packs (four each, 11 clips each).
- Vehicles: 18 Kenney Car Kit models, including service/racing vehicles and tractor-police.
- Trees: 12 Kenney Nature Kit variants: six broadleaf, four conifers, two palms.

These assets are dedicated to the public domain under CC0 1.0:
https://creativecommons.org/publicdomain/zero/1.0/
They can be copied, modified and redistributed, including commercially, without attribution.
Credit is retained voluntarily. See `manifest.json` for original pack pages, download locations
and SHA-256 hashes. Quaternius GLB exports were downloaded from the artist's Poly Pizza listings.
Kenney GLBs and their palette texture were extracted from the official pack ZIPs.
The trees have a scene-root repair for an old UniGLTF export error; see
`tools/repair_tree_gltf.py` and the original/repaired hashes in the manifest.
The model files retain all original animations. No runtime network access is required.

Kenney's original license notices are retained alongside the models. The Quaternius pack pages
linked in the manifest state CC0 and free personal/commercial use.

## Runtime conversion

The loader normalizes models to one unit tall, grounds their feet/wheels, and converts
material colors to the existing renderer's display color space. Kenney car palette UVs
are sampled into vertex colors. Actors apply their physical scale at render time.
Tree placement preserves each source trunk origin (including bent palms) and scales
the tree to its saved height. Original mesh/animation files are retained for future changes.

`include/cgltf/cgltf.h` is cgltf v1.15, obtained from
https://github.com/jkuhlmann/cgltf/blob/v1.15/cgltf.h . Its MIT license is embedded in the header.
