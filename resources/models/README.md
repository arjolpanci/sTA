# Imported CC0 models

- Player: Quaternius **Casual Character**, Ultimate Modular Men Pack (24 clips).
- Pedestrians: Quaternius Animated Men and Animated Women packs (four each, 11 clips each).
- Vehicles: 18 Kenney Car Kit models, including service/racing vehicles and tractor-police.
- Trees: 12 Poly Haven scanned trees (jacaranda, three island trees, a small
  broadleaf, two quiver trees, plus five second imports of those sources with a
  different canopy), imported by `tools/import_polyhaven_trees.py`.

These assets are dedicated to the public domain under CC0 1.0:
https://creativecommons.org/publicdomain/zero/1.0/
They can be copied, modified and redistributed, including commercially, without attribution.
Credit is retained voluntarily. See `manifest.json` for original pack pages, download locations
and SHA-256 hashes. Quaternius GLB exports were downloaded from the artist's Poly Pizza listings.
Kenney GLBs and their palette texture were extracted from the official pack ZIPs.
Poly Haven trees are downloaded from the URLs in the manifest and rebuilt locally;
`source_sha256` records the download, `sha256` the committed result.
The model files retain all original animations. No runtime network access is required.

Kenney's original license notices are retained alongside the models. The Quaternius pack pages
linked in the manifest state CC0 and free personal/commercial use.

## Tree import

Poly Haven's trees are film-density scans - a single pine is 17 million
triangles - and their glTF exports lose the leaf cutout, which only the .blend
file wires up. `tools/import_polyhaven_trees.py` fixes both, and needs `curl`,
`numpy`, `Pillow` and `gltfpack` (`npx gltfpack` works):

    python3 tools/import_polyhaven_trees.py

It merges the separate alpha map into the base colour as RGBA and switches the
material to glTF MASK, drops the ARM textures nothing here samples, resizes the
maps to 512, simplifies the wood with meshoptimizer, and rebuilds the canopy as
leaf cards. That last step is the one that matters: a scanned canopy is half a
million individually modelled leaves, and a general-purpose simplifier thins it
by deleting whole leaves until the tree is a bare skeleton. Instead the leaf
positions and normals are sampled from the scan and replaced with a couple of
thousand textured quads, so the canopy's shape and density still come from the
scan while the triangle count comes from the budget in the script's table.

Two Poly Haven families were tried and rejected, and the table says so: the
conifers, whose "twig" texture turns out to be bark rather than needles, and
the searsias, which ship no cutout map at all. Both leave a canopy that can be
neither decimated nor rebuilt. Downloads (about 1 GB) land in a scratch
directory and are not committed.

## Runtime conversion

The loader normalizes models to one unit tall, grounds their feet/wheels, and converts
material colors to the existing renderer's display color space. Kenney car palette UVs
are sampled into vertex colors. Actors apply their physical scale at render time.
Tree placement preserves each source trunk origin and scales the tree to its
saved height. Imported materials keep their own textures now: geometry is
grouped per glTF material and drawn with that material's base colour map,
normal map and alpha cutout. Small palette atlases (Kenney's kits) are still
folded into vertex colours at load time, because their UVs address single
texels that bilinear filtering would bleed across.

`include/cgltf/cgltf.h` is cgltf v1.15, obtained from
https://github.com/jkuhlmann/cgltf/blob/v1.15/cgltf.h . Its MIT license is embedded in the header.
