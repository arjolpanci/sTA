#!/usr/bin/env python3
"""Repair the Kenney Nature Kit 1.0 export's scene roots, preserving mesh data.

UniGLTF lists a child as scene root although tmpParent still owns that child.
Strict glTF readers reject this. Point scenes at the identity parent instead.
"""
import json
from pathlib import Path
import struct
import hashlib

root = Path(__file__).resolve().parents[1] / 'resources/models'
manifest = json.loads((root / 'manifest.json').read_text())
for entry in manifest:
    if not entry['file'].startswith('trees/') or not entry['file'].endswith('.glb'):
        continue
    path = root / entry['file']
    data = path.read_bytes()
    size = struct.unpack_from('<I', data, 12)[0]
    doc = json.loads(data[20:20 + size])
    parents = {child: i for i, node in enumerate(doc['nodes']) for child in node.get('children', [])}
    changed = False
    for scene in doc['scenes']:
        roots = []
        for node in scene['nodes']:
            while node in parents:
                node = parents[node]
                changed = True
            if node not in roots:
                roots.append(node)
        scene['nodes'] = roots
    if not changed:
        continue
    encoded = json.dumps(doc, separators=(',', ':')).encode()
    encoded += b' ' * (-len(encoded) % 4)
    tail = data[20 + size:]
    path.write_bytes(struct.pack('<4sIII4s', b'glTF', 2, 20 + len(encoded) + len(tail), len(encoded), b'JSON') + encoded + tail)
    entry['original_sha256'] = entry['sha256']
    entry['sha256'] = hashlib.sha256(path.read_bytes()).hexdigest()
    entry['modification'] = 'Scene root corrected to existing identity parent; mesh and materials unchanged.'
(root / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
