"""Self-authored GLB fixtures and end-to-end converter/runtime loader tests.

Also regenerate the committed sample with: python tests/asset_pipeline_test.py --sample
No Blender file is generated or required.
"""
import copy
import json
import math
from pathlib import Path
import struct
import subprocess
import sys


POSITIONS = [(-.5, -.5, .5), (.5, -.5, .5), (.5, .5, .5), (-.5, .5, .5),
             (-.5, -.5, -.5), (.5, -.5, -.5), (.5, .5, -.5), (-.5, .5, -.5)]
COLORS = [(1, .1, .1), (.1, 1, .1), (.1, .3, 1), (1, 1, .1),
          (.1, 1, 1), (1, .1, 1), (1, .6, .1), (.6, .2, 1)]
INDICES = [0, 1, 2, 2, 3, 0, 1, 5, 6, 6, 2, 1, 4, 0, 3, 3, 7, 4,
           3, 2, 6, 6, 7, 3, 4, 5, 1, 1, 0, 4, 5, 4, 7, 7, 6, 5]


def fixture(index_type=5123, colors=False, rgba=False, copies=1):
    # Exercise nonzero bufferView/accessor offsets and interleaved stride.
    binary = bytearray(b'PAD!')
    for position in POSITIONS:
        binary += b'PAD!' + struct.pack('<3f', *position)
    normal_start = len(binary)
    for normal in POSITIONS:  # Deliberately non-unit corner normals, with offset/stride.
        binary += b'PAD!' + struct.pack('<3f', *normal)
    color_start = len(binary)
    if colors:
        for color in COLORS:
            binary += bytes(round(c * 255) for c in color)
            if rgba:
                binary += bytes([128])
    while len(binary) % 4:
        binary += b'\0'
    index_start = len(binary)
    binary += b'PAD!'
    code = {5121: 'B', 5123: 'H', 5125: 'I'}[index_type]
    binary += struct.pack('<' + code * len(INDICES), *INDICES)
    views = [{'buffer': 0, 'byteOffset': 4, 'byteLength': 128, 'byteStride': 16},
             {'buffer': 0, 'byteOffset': index_start, 'byteLength': len(binary) - index_start}]
    accessors = [dict(bufferView=0, byteOffset=4, componentType=5126, count=8, type='VEC3',
                      min=[-.5] * 3, max=[.5] * 3),
                 dict(bufferView=1, byteOffset=4, componentType=index_type, count=36, type='SCALAR')]
    attributes = {'POSITION': 0}
    if colors:
        views.append(dict(buffer=0, byteOffset=color_start, byteLength=8 * (4 if rgba else 3)))
        accessors.append(dict(bufferView=2, componentType=5121, normalized=True,
                              count=8, type='VEC4' if rgba else 'VEC3'))
        attributes['COLOR_0'] = 2
    views.append(dict(buffer=0, byteOffset=normal_start, byteLength=128, byteStride=16))
    attributes['NORMAL'] = len(accessors)
    accessors.append(dict(bufferView=len(views) - 1, byteOffset=4, componentType=5126,
                          count=8, type='VEC3'))
    primitive = dict(attributes=attributes, indices=1, mode=4)
    document = dict(asset={'version': '2.0', 'generator': 'VulkanEngine self-authored fixture'},
                    buffers=[{'byteLength': len(binary)}], bufferViews=views, accessors=accessors,
                    meshes=[{'primitives': [copy.deepcopy(primitive) for _ in range(copies)]}],
                    nodes=[{'mesh': 0}], scenes=[{'nodes': [0]}], scene=0)
    return document, binary


def write_glb(path, document, binary):
    encoded = json.dumps(document, separators=(',', ':')).encode()
    encoded += b' ' * (-len(encoded) % 4)
    binary = binary + b'\0' * (-len(binary) % 4)
    content = struct.pack('<II', len(encoded), 0x4e4f534a) + encoded
    content += struct.pack('<II', len(binary), 0x004e4942) + binary
    path.write_bytes(struct.pack('<III', 0x46546c67, 2, 12 + len(content)) + content)


def main():
    if sys.argv[1:] == ['--sample']:
        path = Path(__file__).resolve().parents[1] / 'assets/models/test.glb'
        path.parent.mkdir(parents=True, exist_ok=True)
        write_glb(path, *fixture(colors=True, rgba=True))
        print('Wrote self-authored sample:', path)
        return
    converter, loader, directory = sys.argv[1:]
    directory = Path(directory)
    directory.mkdir(parents=True, exist_ok=True)
    checks = 0

    def run(arguments, success=True, message=None):
        nonlocal checks
        result = subprocess.run([str(x) for x in arguments], capture_output=True, text=True)
        if (result.returncode == 0) != success:
            raise AssertionError(f'{arguments}: {result.stdout}\n{result.stderr}')
        if message and message not in result.stdout + result.stderr:
            raise AssertionError(f'Missing {message!r}: {result.stdout}\n{result.stderr}')
        checks += 1
        return result

    run([converter], False, 'Usage:')
    run([converter, directory / 'missing.glb', directory / 'missing.mesh'], False, 'Failed to open/parse')
    valid = None
    for index_type in (5121, 5123, 5125):
        for color_mode in ('none', 'rgb', 'rgba'):
            for copies in (1, 2):
                name = f'{index_type}-{color_mode}-{copies}'
                source = directory / (name + '.glb')
                target = source.with_suffix('.mesh')
                write_glb(source, *fixture(index_type, color_mode != 'none', color_mode == 'rgba', copies))
                result = run([converter, source, target], message=f'triangles: {12 * copies}')
                assert 'ignoring attribute NORMAL' not in result.stderr
                raw = target.read_bytes()
                assert struct.unpack_from('<4I', raw) == (0x4853454d, 2, 8 * copies, 36 * copies)
                assert len(raw) == 16 + 36 * 8 * copies + 4 * 36 * copies
                for i in range(8 * copies):
                    vertex = struct.unpack_from('<9f', raw, 16 + 36 * i)
                    expected = POSITIONS[i % 8]
                    assert all(abs(a - b) < 1e-6 for a, b in zip(vertex[:3], expected))
                    expected_normal = [c / math.sqrt(.75) for c in POSITIONS[i % 8]]
                    assert all(abs(a - b) < 1e-6 for a, b in zip(vertex[3:6], expected_normal))
                    expected = [.8] * 3 if color_mode == 'none' else [round(c * 255) / 255 for c in COLORS[i % 8]]
                    assert all(abs(a - b) < 1e-6 for a, b in zip(vertex[6:], expected))
                indices = struct.unpack_from('<' + 'I' * 36 * copies, raw, 16 + 36 * 8 * copies)
                assert list(indices) == [i + 8 * j for j in range(copies) for i in INDICES]
                loaded = run([loader, target], message=f'{8 * copies} {36 * copies}')
                rows = loaded.stdout.splitlines()[1:]
                assert len(rows) == 8 * copies
                for i, row in enumerate(rows):
                    expected = struct.unpack_from('<9f', raw, 16 + 36 * i)
                    assert len(row.split()) == 9
                    assert all(abs(a - b) < 1e-5 for a, b in zip(map(float, row.split()), expected))
                valid = raw

    # Distinct malformed input paths, including metadata and geometry failures.
    for problem in ('topology', 'position', 'indices', 'sparse', 'animation', 'extension', 'range', 'empty'):
        document, binary = fixture()
        primitive = document['meshes'][0]['primitives'][0]
        if problem == 'topology': primitive['mode'] = 1
        if problem == 'position': primitive['attributes'] = {}
        if problem == 'indices': primitive.pop('indices')
        if problem == 'sparse': document['accessors'][0]['componentType'] = 5123
        if problem == 'animation': document['skins'] = [{'joints': [0]}]
        if problem == 'extension': document['extensionsRequired'] = ['KHR_draco_mesh_compression']
        if problem == 'range': struct.pack_into('<H', binary, document['bufferViews'][1]['byteOffset'] + 4, 99)
        if problem == 'empty': document['meshes'] = []; document['nodes'] = []
        source = directory / (problem + '.glb')
        write_glb(source, document, binary)
        run([converter, source, source.with_suffix('.mesh')], False)
    run([converter, source, directory / 'absent' / 'output.mesh'], False)
    good = directory / '5123-none-1.glb'
    run([converter, good, good], False, 'different files')
    run([converter, good, directory / 'absent' / 'output.mesh'], False, 'open output')

    for problem in ('missing', 'type', 'component', 'normalized', 'view', 'sparse',
                    'count', 'zero', 'tiny', 'nan', 'inf'):
        document, binary = fixture()
        attributes = document['meshes'][0]['primitives'][0]['attributes']
        normal = document['accessors'][attributes['NORMAL']]
        message = 'Unsupported NORMAL accessor format'
        if problem == 'missing':
            attributes.pop('NORMAL'); message = 'Mesh has no NORMAL attribute'
        if problem == 'type': normal['type'] = 'VEC2'
        if problem == 'component': normal['componentType'] = 5123
        if problem == 'normalized': normal['normalized'] = True
        if problem == 'view': normal.pop('bufferView')
        if problem == 'sparse':
            normal['sparse'] = dict(count=1, indices=dict(bufferView=1, componentType=5123),
                                    values=dict(bufferView=0))
        if problem == 'count':
            normal['count'] = 7; message = 'POSITION/NORMAL accessor counts must match'
        if problem in ('zero', 'tiny', 'nan', 'inf'):
            offset = document['bufferViews'][normal['bufferView']]['byteOffset'] + 4
            value = {'zero': 0., 'tiny': 1e-12, 'nan': float('nan'), 'inf': float('inf')}[problem]
            struct.pack_into('<3f', binary, offset, value, 0., 0.)
            message = 'NORMAL length is too small' if problem in ('zero', 'tiny') else 'Non-finite NORMAL'
        source = directory / ('normal-' + problem + '.glb')
        write_glb(source, document, binary)
        run([converter, source, source.with_suffix('.mesh')], False, message)

    # Validate hostile disk counts before allocations; test complete file/content checks.
    corruptions = {'short': b'MESH', 'truncated': valid[:-1], 'trailing': valid + b'x'}
    for name, offset, value in [('magic', 0, 0), ('version', 4, 1), ('zero', 8, 0),
                                ('huge', 8, 0xffffffff), ('nontriangles', 12, 5)]:
        blob = bytearray(valid)
        struct.pack_into('<I', blob, offset, value)
        corruptions[name] = blob
    blob = bytearray(valid)
    struct.pack_into('<f', blob, 16, float('nan'))
    corruptions['nan'] = blob
    for component in range(3):
        blob = bytearray(valid)
        struct.pack_into('<f', blob, 16 + 12 + 4 * component, float('inf'))
        corruptions['normal-inf-' + str(component)] = blob
    blob = bytearray(valid)
    struct.pack_into('<I', blob, len(blob) - 4, 999999)
    corruptions['bad-index'] = blob
    for name, blob in corruptions.items():
        path = directory / ('corrupt-' + name + '.mesh')
        path.write_bytes(blob)
        run([loader, path], False, 'Unsupported mesh version: 1, expected 2' if name == 'version' else None)
    print(f'Asset pipeline passed {checks} process checks plus binary layout/content assertions.')


if __name__ == '__main__':
    main()
