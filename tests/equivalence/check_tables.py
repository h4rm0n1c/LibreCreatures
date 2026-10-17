#!/usr/bin/env python3
"""Compares LC's tables copied from Creatures.exe with the bytes in the exe.

    tests/equivalence/check_tables.py <CE 10.3 Creatures.exe>

Each table is read from the LC source (its initializer: numbers and char
literals, or enum and string names mapped to values) and compared with the
exe at the address given, at the width given.  The exe is only read.
Exit status 1 when a table differs.
"""
import os
import re
import struct
import sys

SRC = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..')

# (source file, name of the table in it, exe address, element width)
NUMERIC = [
    ('src/c1/objects/simple_object.cpp', 'kSimpleObjectBehaviorSelectors', 0x4579a4, 1),
    ('src/c1/world/map.hpp', 'kInitialWorldRooms', 0x457060, 4),
    ('src/c1/world/map.hpp', 'kAmbientEnvironmentRecords', 0x457028, 1),
    ('src/c1/world/map.hpp', 'kAmbientLightProfiles', 0x4542f0, 1),
    ('src/c1/creatures/skeleton.cpp', 'kPoseChainLengths', 0x45ac50, 4),
    ('src/c1/creatures/skeleton.cpp', 'kPoseChainOffsets', 0x45ac30, 4),
    ('src/c1/creatures/skeleton.cpp', 'kPoseChainRenderPlaneOffsets', 0x45ac70, 1),
    ('src/c1/creatures/creature.cpp', 'kActionPhraseNeedsTarget', 0x454304, 1),
    ('src/c1/creatures/creature.cpp', 'kActionPhraseWordIndices', 0x454314, 1),
    ('src/c1/creatures/update.hpp', 'kDriveLowerThresholds', 0x4547b0, 4),
    ('src/c1/creatures/update.hpp', 'kDriveUpperThresholds', 0x454770, 4),
    ('src/c1/creatures/body.hpp', 'kBodyPartSpriteFrameCounts', 0x45ab2c, 1),
    ('src/c1/creatures/learned_words.cpp', 'kLearnedWordPhonemeSubstitutions', 0x4663c0, 1),
    ('src/c1/common/static_data.cpp', 'g_biochemistry_tick_selector_masks', 0x454500, 4),
    ('src/c1/common/static_data.cpp', 'g_biochemistry_tick_selector_q16_multipliers', 0x454580, 4),
    ('src/c1/common/static_data.cpp', 'g_goal_direction_drive_scale_factors', 0x4542b0, 4),
    ('src/c1/common/static_data.cpp', 'g_motion_pose_digit_by_vertical_bin_and_distance_bin', 0x454690, 1),
    ('src/c1/common/static_data.cpp', 'g_interaction_pose_pairs_ascii_0_to_5', 0x45abc8, 1),
    ('src/c1/common/static_data.cpp', 'g_goal_direction_action_candidate_scores', 0x45424c, 1),
    ('src/c1/sound/sound.cpp', 'kEncodedBarney', 0x45af20, 1),
    ('src/c1/sound/sound.cpp', 'kEncodedBibble', 0x45af30, 1),
    ('src/c1/sound/sound.cpp', 'kEncodedNilnil', 0x45af40, 1),
]
# Enum tables: (file, table, address, width, {enum name: value}).
REQUIREMENT = {'not_selectable': 0, 'always_eligible': 1, 'requires_no_target': 2, 'requires_target': 3}
PARTS = ['head', 'body', 'left_thigh', 'left_shin', 'left_foot', 'right_thigh', 'right_shin',
         'right_foot', 'left_humerus', 'left_radius', 'right_humerus', 'right_radius', 'tail_root',
         'tail_tip']
ENUMS = [
    ('src/c1/creatures/creature.cpp', 'kActionTargetRequirements', 0x454450, 1, REQUIREMENT),
    ('src/c1/creatures/body.hpp', 'kAppearancePartGroupMembers', 0x45ab40, 4,
     dict({'nullopt': -1}, **{p: i for i, p in enumerate(PARTS)})),
]
# Pointer tables of strings: (file, table, address).
STRINGS = [
    ('src/c1/creatures/creature.cpp', 'kDefaultVocabularyGroup1', 0x454330),
    ('src/c1/creatures/creature.cpp', 'kDefaultVocabularyGroup2', 0x4543d0),
    ('src/c1/creatures/creature.cpp', 'kDefaultVocabularyGroup3', 0x454410),
]


class Exe:
    def __init__(self, path):
        d = open(path, 'rb').read()
        pe = struct.unpack_from('<I', d, 0x3c)[0]
        count = struct.unpack_from('<H', d, pe + 6)[0]
        optional = struct.unpack_from('<H', d, pe + 20)[0]
        self.sections = []
        for i in range(count):
            _, size, rva, raw_size, raw = struct.unpack_from('<8sIIII', d, pe + 24 + optional + i * 40)
            self.sections.append((0x400000 + rva, d[raw:raw + raw_size]))

    def read(self, address, length):
        for base, raw in self.sections:
            if base <= address < base + len(raw):
                return raw[address - base:address - base + length]
        raise ValueError(f'{address:#x} is not in the image')

    def ints(self, address, count, width):
        b = self.read(address, count * width)
        return [int.from_bytes(b[i * width:(i + 1) * width], 'little', signed=True) for i in range(count)]

    def string(self, address):
        b = self.read(address, 256)
        return b[:b.index(b'\0')].decode('latin1')


def initializer(path, name):
    text = open(os.path.join(SRC, path), errors='ignore').read()
    m = re.search(r'\b' + re.escape(name) + r'\b\s*(\[[^\]]*\])*\s*(=\s*)?\{', text)
    if m is None:
        raise ValueError(f'{name} is not in {path}')
    start = m.end() - 1
    depth = 0
    for i in range(start, len(text)):
        depth += {'{': 1, '}': -1}.get(text[i], 0)
        if depth == 0:
            return re.sub(r'//[^\n]*', '', text[start:i + 1])


def numbers(body):
    out = []
    for m in re.finditer(r"'(\\?.)'|-?0x[0-9a-fA-F]+|-?\d+", body):
        if m.group(1) is not None:
            out.append(0 if m.group(1) == '\\0' else ord(m.group(1)[-1]))
        else:
            out.append(int(m.group(0), 0))
    return out


def signed(value, width):
    value &= (1 << (8 * width)) - 1
    return value - (1 << (8 * width)) if value >> (8 * width - 1) else value


def main():
    if len(sys.argv) != 2:
        print(__doc__.strip())
        return 2
    exe = Exe(sys.argv[1])
    differ = 0

    def report(name, lc, native):
        nonlocal differ
        if lc == native:
            print(f'same    {name} ({len(lc)} entries)')
            return
        differ += 1
        first = next(i for i, (a, b) in enumerate(zip(lc + [None], native + [None])) if a != b)
        print(f'DIFFER  {name}: entry {first}: lc {lc[first:first + 6]} exe {native[first:first + 6]}')

    for path, name, address, width in NUMERIC:
        lc = [signed(v, width) for v in numbers(initializer(path, name))]
        report(name, lc, exe.ints(address, len(lc), width))
    for path, name, address, width, mapping in ENUMS:
        lc = [mapping[t.split('::')[-1]] for t in re.findall(r'[A-Za-z_]+::[A-Za-z_]+', initializer(path, name))]
        report(name, lc, exe.ints(address, len(lc), width))
    for path, name, address in STRINGS:
        lc = re.findall(r'"([^"]*)"', initializer(path, name))
        pointers = struct.unpack(f'<{len(lc)}I', exe.read(address, 4 * len(lc)))
        report(name, lc, [exe.string(p) for p in pointers])

    # kDefaultStimulusContexts: 36 pointers at 00454460 to 20-byte records;
    # only the descriptor (bytes 8..11) carries data.
    body = initializer('src/c1/creatures/stimulus.hpp', 'kDefaultStimulusContexts')
    lc = [tuple(int(x, 16) for x in m) for m in re.findall(
        r'\{nullptr, nullptr, \{(0x[0-9a-f]+), (0x[0-9a-f]+), (0x[0-9a-f]+), (0x[0-9a-f]+)\}', body)]
    native = []
    for p in struct.unpack('<36I', exe.read(0x454460, 144)):
        record = exe.read(p, 20)
        native.append(tuple(record[8:12]) if not any(record[:8] + record[12:]) else None)
    report('kDefaultStimulusContexts', lc, native)

    # kRandomizedVocabularyBanks: 8 rows of 5 string pointers at 004546d0.
    text = open(os.path.join(SRC, 'src/c1/creatures/learned_words.cpp')).read()
    words = dict(re.findall(r'constexpr char (k\w+)\[\] = "([^"]*)";', text))
    rows = re.findall(r'\{([^{}]*)\}', initializer('src/c1/creatures/learned_words.cpp',
                                                   'kRandomizedVocabularyBanks'))
    lc = [words[k] for row in rows for k in re.findall(r'k\w+', row)]
    pointers = struct.unpack('<40I', exe.read(0x4546d0, 160))
    report('kRandomizedVocabularyBanks', lc, [exe.string(p) for p in pointers])

    print(f'{differ} tables differ')
    return 1 if differ else 0


if __name__ == '__main__':
    sys.exit(main())
