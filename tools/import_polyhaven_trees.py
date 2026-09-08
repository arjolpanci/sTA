#!/usr/bin/env python3
"""Turn Poly Haven's CC0 scanned trees into game-budget GLB models.

The source assets are film-density scans (a single pine is 17 million
triangles) whose glTF exports also drop the leaf alpha: Poly Haven ships the
leaf cutout as a separate texture that only the .blend file wires up, so the
exported material is an opaque quad. Both problems are fixed here, once, at
import time:

  1. the leaf/twig alpha map is merged into the base colour as an RGBA PNG and
     the material is switched to glTF MASK, which is what the renderer's
     alphaCutoff path expects;
  2. the metallic/roughness (ARM) textures are dropped, since the Blinn-Phong
     renderer never reads them;
  3. meshoptimizer (gltfpack) simplifies the wood down to a tree budget;
  4. the canopy is rebuilt as leaf cards. A scanned canopy is half a million
     individually modelled leaves, and no general-purpose simplifier can thin
     that: it deletes whole leaves until the tree is a bare skeleton. So the
     leaf positions and normals are sampled from the scan and replaced with a
     few thousand textured quads - the silhouette still comes from the scan,
     the triangle count comes from the budget.

Downloads land in a scratch directory and are not committed; only the packed
GLBs under resources/models/trees are. Re-running is idempotent and the
manifest records the source URL and the hash of both the download and result.

Usage:  python3 tools/import_polyhaven_trees.py [--work DIR] [--only NAME ...]
"""

import argparse
import hashlib
import json
import math
import os
import random
import shutil
import struct
import subprocess
import sys
import zlib
from pathlib import Path

import numpy as np
from PIL import Image

API = "https://api.polyhaven.com"
RESOLUTION = "1k"
ROOT = Path(__file__).resolve().parent.parent
OUT_DIR = ROOT / "resources/models/trees"

# Slot order is load-bearing: the baked island scene stores a tree variant index
# into this list (0-5 broadleaf, 6-9 highland, 10-11 coastal), so entries are
# replaced in place rather than reordered. Poly Haven has no palms, so the two
# coastal slots use quiver trees - the closest silhouette in a CC0 library.
# (Poly Haven id, model name, wood triangle budget, leaf cards, card scale).
# The columns after the wood budget are canopy cards, their size, and whether
# simplification may sacrifice quality to reach the budget - which the
# jacaranda's tangle of thin branches needs and a smooth trunk cannot survive. Trees whose foliage is
# modelled rather than painted on cards - the aloes - ask for no cards at all.
# Five slots are second imports of a source with a different canopy: seven
# distinct scans is all this library can supply here, since the conifers'
# "twig" texture is bark rather than needles and the searsias ship no cutout
# map at all, which leaves both with a canopy that can be neither decimated
# nor rebuilt.
TREES = [
    ("jacaranda_tree", "tree_jacaranda", 3500, 1400, 2.4, True),
    ("island_tree_01", "tree_island_a", 3500, 1200, 2.2, False),
    ("island_tree_02", "tree_island_b", 3500, 1500, 1.8, False),
    ("island_tree_03", "tree_island_c", 3500, 1300, 2.2, False),
    ("tree_small_02", "tree_small", 2500, 1100, 1.6, False),
    ("jacaranda_tree", "tree_jacaranda_airy", 3500, 900, 2.8, True),
    ("island_tree_01", "tree_island_dense", 3500, 1800, 1.6, False),
    ("island_tree_02", "tree_island_wide", 3500, 1000, 2.6, False),
    ("tree_small_02", "tree_small_open", 2500, 700, 2.0, False),
    ("island_tree_03", "tree_island_slim", 3500, 900, 1.8, False),
    ("quiver_tree_01", "tree_quiver_a", 5000, 0, 0, False),
    ("quiver_tree_02", "tree_quiver_b", 4000, 0, 0, False),
]


def fetch(url, destination):
    """Poly Haven's CDN rejects the default urllib agent, so shell out to curl."""
    if destination.exists() and destination.stat().st_size > 0:
        return destination
    destination.parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(["curl", "-sfL", url, "-o", str(destination)], check=True)
    return destination


def fetch_json(url):
    return json.loads(subprocess.run(["curl", "-sfL", url], check=True, capture_output=True, text=True).stdout)


def sha256(path):
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for block in iter(lambda: handle.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def alpha_source(files, material_suffix):
    """Find the cutout map that belongs to a material, e.g. leaves -> leaves_alpha."""
    for key in (f"{material_suffix}_alpha", f"{material_suffix}_mask"):
        entry = files.get(key, {}).get(RESOLUTION)
        if not entry:
            continue
        # Prefer png/jpg over exr - these are read back with Pillow.
        for extension in ("png", "jpg", "jpeg"):
            if extension in entry:
                return entry[extension]["url"]
    return None


# 512 for both maps: the era this is aiming at, and it keeps a dozen trees to a
# few megabytes in the repository. The normal map is what carries the detail.
COLOUR_PIXELS = 512
NORMAL_PIXELS = 512


def downscale(path, limit, has_alpha):
    image = Image.open(path)
    if max(image.size) > limit:
        scale = limit / max(image.size)
        image = image.resize((max(1, int(image.width * scale)), max(1, int(image.height * scale))), Image.LANCZOS)
    if has_alpha:
        image.convert("RGBA").save(path, optimize=True)
    else:
        image.convert("RGB").save(path, quality=88, optimize=True)
    return path


def prune(gltf):
    """Drop images the rewritten materials no longer reference.

    gltfpack keeps every image it is handed, so the ARM maps removed above have
    to actually leave the document or they end up embedded in the GLB.
    """
    used = []
    def keep(reference):
        if reference is None:
            return None
        if reference["index"] not in used:
            used.append(reference["index"])
        reference["index"] = used.index(reference["index"])
        return reference

    for material in gltf.get("materials", []):
        keep(material.get("pbrMetallicRoughness", {}).get("baseColorTexture"))
        keep(material.get("normalTexture"))
        keep(material.get("emissiveTexture"))
    textures = [gltf["textures"][index] for index in used]
    sources = []
    for texture in textures:
        if texture["source"] not in sources:
            sources.append(texture["source"])
        texture["source"] = sources.index(texture["source"])
    gltf["images"] = [gltf["images"][index] for index in sources]
    gltf["textures"] = textures
    return gltf


COMPONENT = {5120: "i1", 5121: "u1", 5122: "i2", 5123: "u2", 5125: "u4", 5126: "f4"}
COMPONENTS = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4}


def read_accessor(gltf, buffers, index):
    accessor = gltf["accessors"][index]
    view = gltf["bufferViews"][accessor["bufferView"]]
    data = buffers[view.get("buffer", 0)]
    start = view.get("byteOffset", 0) + accessor.get("byteOffset", 0)
    width = COMPONENTS[accessor["type"]]
    dtype = np.dtype("<" + COMPONENT[accessor["componentType"]])
    stride = view.get("byteStride") or width * dtype.itemsize
    raw = np.frombuffer(data, dtype=np.uint8, count=accessor["count"] * stride, offset=start)
    columns = raw.reshape(accessor["count"], stride)[:, : width * dtype.itemsize]
    return np.ascontiguousarray(columns).view(dtype).reshape(accessor["count"], width)


def leaf_cards(positions, normals, count, scale, seed):
    """Replace a scanned canopy with `count` textured quads spanning it.

    Sampling the scan's own leaf vertices keeps the canopy's shape, density
    gradient and hollow interior; only the geometry that expresses it changes.
    """
    generator = np.random.default_rng(seed)
    picked = generator.choice(len(positions), size=min(count, len(positions)), replace=False)
    origin, normal = positions[picked], normals[picked]
    lengths = np.linalg.norm(normal, axis=1, keepdims=True)
    normal = np.where(lengths > 1e-6, normal / np.maximum(lengths, 1e-6), np.array([0.0, 1.0, 0.0]))

    extent = positions.max(axis=0) - positions.min(axis=0)
    size = scale * float(np.linalg.norm(extent)) / math.sqrt(max(len(origin), 1))

    # Any vector not parallel to the leaf normal gives a usable card plane; the
    # cards are then spun around their own normal so a canopy of them does not
    # read as a grid of aligned quads.
    reference = np.where(np.abs(normal[:, 1:2]) > 0.9, np.array([1.0, 0.0, 0.0]), np.array([0.0, 1.0, 0.0]))
    right = np.cross(reference, normal)
    right /= np.maximum(np.linalg.norm(right, axis=1, keepdims=True), 1e-6)
    up = np.cross(normal, right)
    angle = generator.uniform(0, 2 * math.pi, len(origin))[:, None]
    spun_right = right * np.cos(angle) + up * np.sin(angle)
    spun_up = up * np.cos(angle) - right * np.sin(angle)
    half = size * 0.5 * generator.uniform(0.75, 1.25, len(origin))[:, None]

    corners = [(-1, -1), (1, -1), (1, 1), (-1, 1)]
    vertices = np.concatenate([origin + half * (x * spun_right + y * spun_up) for x, y in corners], axis=1)
    positions_out = vertices.reshape(-1, 3)
    normals_out = np.repeat(normal, 4, axis=0)
    uv_out = np.tile(np.array([[0, 1], [1, 1], [1, 0], [0, 0]], dtype=np.float32), (len(origin), 1))
    base = np.arange(len(origin))[:, None] * 4
    indices_out = (base + np.array([0, 1, 2, 0, 2, 3])).reshape(-1)
    return (positions_out.astype("<f4"), normals_out.astype("<f4"),
            uv_out.astype("<f4"), indices_out.astype("<u4"))


def read_glb(path):
    data = path.read_bytes()
    if data[:4] != b"glTF":
        raise ValueError(f"{path} is not a binary glTF")
    offset, chunks = 12, {}
    while offset < len(data):
        length, kind = struct.unpack_from("<I4s", data, offset)
        offset += 8
        chunks[kind.decode().strip("\x00 ")] = data[offset:offset + length]
        offset += length
    return json.loads(chunks["JSON"]), bytearray(chunks["BIN"])


def write_glb(path, gltf, binary):
    def pad(block, filler):
        return block + filler * (-len(block) % 4)
    json_chunk = pad(json.dumps(gltf, separators=(",", ":")).encode(), b" ")
    bin_chunk = pad(bytes(binary), b"\0")
    total = 12 + 8 + len(json_chunk) + 8 + len(bin_chunk)
    with open(path, "wb") as handle:
        handle.write(struct.pack("<4sII", b"glTF", 2, total))
        handle.write(struct.pack("<I4s", len(json_chunk), b"JSON"))
        handle.write(json_chunk)
        handle.write(struct.pack("<I4s", len(bin_chunk), b"BIN\0"))
        handle.write(bin_chunk)


def inject_leaf_cards(path, material_name, cards):
    """Swap the packed canopy primitive for the generated cards, in place."""
    gltf, binary = read_glb(path)
    material = next(i for i, entry in enumerate(gltf["materials"]) if entry.get("name") == material_name)
    primitive = next((p for mesh in gltf["meshes"] for p in mesh["primitives"] if p.get("material") == material), None)
    if primitive is None:
        # The stub was small enough that simplification removed it outright;
        # the material survived, so the cards just bring their own primitive.
        primitive = {"material": material}
        mesh = gltf["meshes"][next(node["mesh"] for node in gltf["nodes"] if "mesh" in node)]
        mesh["primitives"].append(primitive)

    positions, normals, uv, indices = cards
    def store(array, kind, component):
        offset = len(binary)
        binary.extend(array.tobytes())
        binary.extend(b"\0" * (-len(binary) % 4))
        gltf["bufferViews"].append({"buffer": 0, "byteOffset": offset, "byteLength": array.nbytes})
        accessor = {"bufferView": len(gltf["bufferViews"]) - 1, "componentType": component,
                    "count": len(array), "type": kind}
        if kind == "VEC3" and component == 5126:
            accessor["min"] = array.min(axis=0).tolist()
            accessor["max"] = array.max(axis=0).tolist()
        gltf["accessors"].append(accessor)
        return len(gltf["accessors"]) - 1

    primitive["attributes"] = {"POSITION": store(positions, "VEC3", 5126),
                               "NORMAL": store(normals, "VEC3", 5126),
                               "TEXCOORD_0": store(uv, "VEC2", 5126)}
    primitive["indices"] = store(indices.reshape(-1, 1), "SCALAR", 5125)
    # Cards are flat quads seen from both sides, and the alpha test needs to run
    # on the shadow pass too - both are material state the engine reads back.
    gltf["materials"][material]["doubleSided"] = True
    gltf["materials"][material]["alphaMode"] = "MASK"
    gltf["materials"][material]["alphaCutoff"] = 0.5
    gltf["buffers"][0]["byteLength"] = len(binary)
    write_glb(path, gltf, binary)
    return len(indices) // 3

def stub(gltf, directory, primitive):
    """Shrink a primitive to one triangle, in a buffer of its own."""
    position = np.array([[0, 0, 0], [0.01, 0, 0], [0, 0.01, 0]], dtype="<f4")
    normal = np.tile(np.array([0, 1, 0], dtype="<f4"), (3, 1))
    uv = np.zeros((3, 2), dtype="<f4")
    indices = np.arange(3, dtype="<u4")

    blob, views = bytearray(), []
    for array in (position, normal, uv, indices):
        views.append({"buffer": len(gltf["buffers"]), "byteOffset": len(blob), "byteLength": array.nbytes})
        blob.extend(array.tobytes())
    (directory / "stub.bin").write_bytes(bytes(blob))
    gltf["buffers"].append({"uri": "stub.bin", "byteLength": len(blob)})
    first = len(gltf["bufferViews"])
    gltf["bufferViews"].extend(views)
    gltf["accessors"].extend([
        {"bufferView": first, "componentType": 5126, "count": 3, "type": "VEC3",
         "min": position.min(axis=0).tolist(), "max": position.max(axis=0).tolist()},
        {"bufferView": first + 1, "componentType": 5126, "count": 3, "type": "VEC3"},
        {"bufferView": first + 2, "componentType": 5126, "count": 3, "type": "VEC2"},
        {"bufferView": first + 3, "componentType": 5125, "count": 3, "type": "SCALAR"},
    ])
    base = len(gltf["accessors"]) - 4
    primitive["attributes"] = {"POSITION": base, "NORMAL": base + 1, "TEXCOORD_0": base + 2}
    primitive["indices"] = base + 3


def drop_ground_plate(gltf, node):
    """Remove the patch of ground a tree was scanned standing on.

    Poly Haven's coastal trees come rooted in a slab of rock or soil. It is
    part of the source mesh but nothing a city street wants under a tree, and
    it is recognisable without naming it per asset: a primitive that sits at
    the base of the model and is wider than it is tall, which no trunk is.
    """
    mesh = gltf["meshes"][gltf["nodes"][node]["mesh"]]
    bounds = [(np.array(gltf["accessors"][p["attributes"]["POSITION"]]["min"]),
               np.array(gltf["accessors"][p["attributes"]["POSITION"]]["max"])) for p in mesh["primitives"]]
    floor = min(low[1] for low, _ in bounds)
    height = max(high[1] for _, high in bounds) - floor

    def is_plate(low, high):
        span = max(high[0] - low[0], high[2] - low[2])
        return low[1] < floor + 0.05 * height and span > 1.4 * (high[1] - low[1])

    kept = [primitive for primitive, bound in zip(mesh["primitives"], bounds) if not is_plate(*bound)]
    dropped = len(mesh["primitives"]) - len(kept)
    if kept:
        mesh["primitives"] = kept
    return dropped


def single_plant(gltf, asset):
    """Reduce a multi-plant scan to one plant, in mesh-local coordinates.

    Some Poly Haven scans (the searsias) are a row of seven separate specimens,
    which would import as a hedge. The largest is kept and the rest dropped.
    The kept node's own transform is stripped as well, so the generated leaf
    cards - which are built in mesh space - line up with the wood.
    """
    def volume(node):
        mesh = gltf["meshes"][node["mesh"]]
        low = np.array([gltf["accessors"][p["attributes"]["POSITION"]]["min"] for p in mesh["primitives"]]).min(axis=0)
        high = np.array([gltf["accessors"][p["attributes"]["POSITION"]]["max"] for p in mesh["primitives"]]).max(axis=0)
        return float(np.prod(high - low))

    candidates = [i for i, node in enumerate(gltf["nodes"]) if "mesh" in node]
    if not candidates:
        raise RuntimeError(f"{asset}: no mesh node to import")
    if any(gltf["nodes"][i].get("children") for i in candidates):
        raise RuntimeError(f"{asset}: nested mesh nodes are not supported")
    kept = max(candidates, key=lambda i: volume(gltf["nodes"][i]))
    for key in ("matrix", "translation", "rotation", "scale"):
        gltf["nodes"][kept].pop(key, None)
    gltf["nodes"] = [gltf["nodes"][kept]]
    gltf["scenes"] = [{"nodes": [0]}]
    gltf["scene"] = 0
    return 0


def prepare(asset, work):
    """Download one tree and rewrite its glTF into something the engine can use."""
    files = fetch_json(f"{API}/files/{asset}")
    entry = files["gltf"][RESOLUTION]["gltf"]
    directory = work / asset
    source = fetch(entry["url"], directory / Path(entry["url"]).name)
    for relative, info in entry["include"].items():
        fetch(info["url"], directory / relative)

    gltf = json.loads(source.read_text())
    images = gltf.get("images", [])
    canopy = None
    cut_out = 0
    for material in gltf.get("materials", []):
        pbr = material.setdefault("pbrMetallicRoughness", {})
        # Nothing in the renderer reads roughness/metalness/AO.
        pbr.pop("metallicRoughnessTexture", None)
        material.pop("occlusionTexture", None)

        name = material.get("name", "")
        suffix = name[len(asset):].lstrip("_") if name.startswith(asset) else name
        url = alpha_source(files, suffix)
        base = pbr.get("baseColorTexture")
        if base is None or url is None:
            # Thick, modelled foliage (aloes, succulents) has no cutout map at
            # all; glTF still marks it BLEND, which would cost a pointless
            # alpha test against a texture with no alpha channel.
            material.pop("alphaMode", None)
            material.pop("alphaCutoff", None)
            continue

        image = images[gltf["textures"][base["index"]]["source"]]
        colour_path = directory / image["uri"]
        alpha_path = fetch(url, directory / "textures" / Path(url).name)
        colour = Image.open(colour_path).convert("RGB")
        alpha = Image.open(alpha_path).convert("L")
        if alpha.size != colour.size:
            alpha = alpha.resize(colour.size, Image.LANCZOS)
        colour.putalpha(alpha)
        merged = colour_path.with_name(colour_path.stem + "_cutout.png")
        colour.save(merged)
        image["uri"] = f"textures/{merged.name}"
        image.pop("mimeType", None)
        # MASK rather than BLEND: cut-out leaves still write depth, so a canopy
        # needs no back-to-front sorting and casts a correct shadow.
        material["alphaMode"] = "MASK"
        material["alphaCutoff"] = 0.5
        cut_out += 1
        canopy = material["name"]

    mesh_node = single_plant(gltf, asset)
    drop_ground_plate(gltf, mesh_node)

    canopy_geometry = None
    if canopy is not None:
        buffers = [(directory / entry["uri"]).read_bytes() for entry in gltf["buffers"]]
        index = next(i for i, entry in enumerate(gltf["materials"]) if entry.get("name") == canopy)
        mesh = gltf["meshes"][gltf["nodes"][mesh_node]["mesh"]]
        primitive = next((p for p in mesh["primitives"] if p.get("material") == index), None)
        if primitive is not None:
            canopy_geometry = (canopy,
                               read_accessor(gltf, buffers, primitive["attributes"]["POSITION"]),
                               read_accessor(gltf, buffers, primitive["attributes"]["NORMAL"]))
            # A canopy is most of the source mesh, and gltfpack's ratio is
            # global: left in place it would spend the whole budget on foliage
            # that gets thrown away, decimating a trunk down to 20 triangles.
            # It is reduced to a stub that only keeps the material alive, and
            # the cards take its place once packing is done.
            stub(gltf, directory, primitive)

    prune(gltf)
    normal_images = {gltf["textures"][material["normalTexture"]["index"]]["source"]
                     for material in gltf.get("materials", []) if "normalTexture" in material}
    for index, image in enumerate(gltf["images"]):
        path = directory / image["uri"]
        downscale(path, NORMAL_PIXELS if index in normal_images else COLOUR_PIXELS, path.suffix == ".png")

    rewritten = directory / "prepared.gltf"
    rewritten.write_text(json.dumps(gltf))
    triangles = sum(gltf["accessors"][p["indices"]]["count"] // 3
                    for mesh in gltf["meshes"] for p in mesh["primitives"])
    return rewritten, triangles, canopy_geometry, entry["url"], sha256(source)


def pack(gltfpack, prepared, target, triangles, output, aggressive):
    ratio = min(1.0, target / max(triangles, 1))
    subprocess.run([
        gltfpack, "-i", str(prepared), "-o", str(output),
        "-si", f"{ratio:.6f}", "-se", "0.15",  # a budget, but not past the point a trunk collapses
        "-sp",                                 # branch tangles are all seams; allow collapsing across them
        *(["-sa"] if aggressive else []),
        "-vpf", "-vtf", "-vnf",                # float attributes: no KHR_mesh_quantization to support
        "-noq",
    ], check=True, capture_output=True, text=True)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--work", default=os.environ.get("STA_ASSET_WORK", "/tmp/sta-assets"))
    parser.add_argument("--only", nargs="*", default=None)
    parser.add_argument("--gltfpack", default=shutil.which("gltfpack") or "npx --yes gltfpack")
    arguments = parser.parse_args()

    gltfpack = arguments.gltfpack
    work = Path(arguments.work)
    work.mkdir(parents=True, exist_ok=True)
    OUT_DIR.mkdir(parents=True, exist_ok=True)

    records = []
    for asset, name, budget, cards, card_scale, aggressive in TREES:
        if arguments.only and name not in arguments.only and asset not in arguments.only:
            continue
        print(f"{asset} -> {name}.glb", flush=True)
        prepared, triangles, canopy, url, source_hash = prepare(asset, work)
        output = OUT_DIR / f"{name}.glb"
        pack(gltfpack, prepared, budget, triangles, output, aggressive)
        leaves = 0
        if cards and canopy:
            material, positions, normals = canopy
            leaves = inject_leaf_cards(output, material,
                                       leaf_cards(positions, normals, cards, card_scale, zlib.crc32(name.encode())))
        print(f"  {triangles:,} triangles -> {budget:,} of wood + {leaves:,} of canopy, "
              f"{output.stat().st_size/1e6:.1f} MB", flush=True)
        records.append({
            "file": f"trees/{name}.glb",
            "source": f"https://polyhaven.com/a/{asset}",
            "download": url,
            "author": "Poly Haven",
            "license": "CC0-1.0",
            "source_sha256": source_hash,
            "sha256": sha256(output),
        })

    manifest = ROOT / "resources/models/manifest.json"
    existing = json.loads(manifest.read_text())
    # Records for assets that have been deleted are stale provenance, so they go.
    existing = [record for record in existing if (ROOT / "resources/models" / record["file"]).exists()]
    by_file = {record["file"]: record for record in existing}
    for record in records:
        by_file[record["file"]] = record
    manifest.write_text(json.dumps(sorted(by_file.values(), key=lambda r: r["file"]), indent=2) + "\n")
    print(f"manifest updated with {len(records)} tree(s)")


if __name__ == "__main__":
    sys.exit(main())
