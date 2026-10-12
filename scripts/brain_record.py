#!/usr/bin/env python3
"""Read brain recordings made with C1_BRAIN_RECORD (src/c1/brain/brain_recorder.hpp).

    brain_record.py summary FILE...     frames, ticks, size, per-lobe activity
    brain_record.py frames FILE         one line per frame: tick, place, action,
                                        attention, neurons and dendrites changed

As a module, read_recording(path) yields Frame objects that carry the full
brain state at each recorded tick (neurons and dendrites rebuilt from the
keyframes and deltas).
"""
import struct
import sys
import zlib
from dataclasses import dataclass, field

LOBE_NAMES = ["perception", "drive", "source", "verb", "noun",
              "general sense", "decision", "attention", "concept"]


@dataclass
class Lobe:
    index: int
    neuron_count: int
    grid_width: int
    grid_height: int
    rule_targets: tuple
    dendrite_counts: list  # per neuron: (rule 0 count, rule 1 count)
    perception_copy: int | None = None  # 0 none, 1 copied, 2 mutually exclusive; None in v3

    @property
    def name(self):
        return LOBE_NAMES[self.index] if self.index < len(LOBE_NAMES) else f"lobe {self.index}"


@dataclass
class Layout:
    moniker: int
    lobes: list
    version: int = 4
    neuron_keys: list = field(default_factory=list)    # (lobe, neuron)
    dendrite_keys: list = field(default_factory=list)  # (lobe, neuron, rule, dendrite)

    def __post_init__(self):
        self.neuron_keys, self.dendrite_keys = [], []
        for lobe in self.lobes:
            for neuron, (count0, count1) in enumerate(lobe.dendrite_counts):
                self.neuron_keys.append((lobe.index, neuron))
                for rule, count in ((0, count0), (1, count1)):
                    for dendrite in range(count):
                        self.dendrite_keys.append((lobe.index, neuron, rule, dendrite))


@dataclass
class Frame:
    tick: int
    x: int
    y: int
    action: int
    attention_slot: int
    attention_classifier: int
    keyframe: bool
    neurons: dict      # (lobe, neuron) -> (activation, firing strength)
    dendrites: dict    # (lobe, neuron, rule, dendrite) -> (target, current, target weight, baseline, state)
    changed_neurons: int
    changed_dendrites: int


class Reader:
    def __init__(self, data):
        self.data = data
        self.pos = 0

    def take(self, fmt):
        values = struct.unpack_from("<" + fmt, self.data, self.pos)
        self.pos += struct.calcsize("<" + fmt)
        return values if len(values) > 1 else values[0]

    def varint(self):
        value = shift = 0
        while True:
            byte = self.data[self.pos]
            self.pos += 1
            value |= (byte & 0x7f) << shift
            if byte < 0x80:
                return value
            shift += 7

    def done(self):
        return self.pos >= len(self.data)


def read_layout(r):
    magic = r.data[:4]
    if magic != b"C1BR":
        raise ValueError("not a C1 brain recording")
    r.pos = 4
    version = r.take("I")
    if version not in (3, 4):
        raise ValueError(f"unknown recording version {version}")
    moniker = r.take("I")
    lobes = []
    for index in range(r.take("I")):
        count = r.take("I")
        width, height, rule0, rule1 = r.take("4B")
        copy = r.take("B") if version >= 4 else None
        counts = [r.take("2B") for _ in range(count)]
        lobes.append(Lobe(index, count, width, height, (rule0, rule1), counts, copy))
    return Layout(moniker, lobes, version=version)


def read_chunks(r):
    """Yield each chunk's records as a Reader.  A file the game is still
    writing can end part-way through a chunk; that tail is left out."""
    while len(r.data) - r.pos >= 8:
        raw_size, compressed_size = r.take("2I")
        if len(r.data) - r.pos < compressed_size:
            return
        raw = zlib.decompress(r.data[r.pos:r.pos + compressed_size])
        r.pos += compressed_size
        if len(raw) != raw_size:
            raise ValueError("chunk size does not match its header")
        yield Reader(raw)


def read_recording(path):
    """Yield (layout, Frame) for every frame, with the full state rebuilt."""
    with open(path, "rb") as f:
        file_reader = Reader(f.read())
    layout = read_layout(file_reader)
    neurons, dendrites = {}, {}
    context = None
    for r in read_chunks(file_reader):
        while not r.done():
            kind = r.take("c")
            if kind == b"C":
                context = r.take("IiiBbI")
            elif kind == b"K":
                r.take("I")
                for key in layout.neuron_keys:
                    neurons[key] = r.take("2B")
                for key in layout.dendrite_keys:
                    dendrites[key] = r.take("H4B")
                yield layout, Frame(*context, True, neurons, dendrites,
                                    len(layout.neuron_keys), len(layout.dendrite_keys))
            elif kind == b"D":
                r.take("I")
                changed_n = r.varint()
                number = 0
                for _ in range(changed_n):
                    number += r.varint()
                    mask = r.take("B")
                    key = layout.neuron_keys[number]
                    act, fire = neurons[key]
                    if mask & 1: act = r.take("B")
                    if mask & 2: fire = r.take("B")
                    neurons[key] = (act, fire)
                    number += 1
                changed_d = r.varint()
                number = 0
                for _ in range(changed_d):
                    number += r.varint()
                    mask = r.take("B")
                    key = layout.dendrite_keys[number]
                    target, cur, tgt, base, state = dendrites[key]
                    if mask & 1: target = r.take("H")
                    if mask & 2: cur = r.take("B")
                    if mask & 4: tgt = r.take("B")
                    if mask & 8: base = r.take("B")
                    if mask & 16: state = r.take("B")
                    dendrites[key] = (target, cur, tgt, base, state)
                    number += 1
                yield layout, Frame(*context, False, neurons, dendrites, changed_n, changed_d)
            else:
                raise ValueError(f"bad record type {kind!r}")


def summary(paths):
    import os
    for path in paths:
        frames = keyframes = 0
        first = last = None
        firing_ticks = {}
        layout = None
        for layout, frame in read_recording(path):
            frames += 1
            keyframes += frame.keyframe
            first = frame.tick if first is None else first
            last = frame.tick
            for (lobe, _), (_, fire) in frame.neurons.items():
                if fire:
                    firing_ticks[lobe] = firing_ticks.get(lobe, 0) + 1
        size = os.path.getsize(path)
        print(f"{path}: moniker {layout.moniker:08x}, {frames} frames "
              f"({keyframes} keyframes), ticks {first}-{last}, {size:,} bytes "
              f"({size / max(frames, 1):.0f} per frame)")
        for lobe in layout.lobes:
            dendrite_total = sum(a + b for a, b in lobe.dendrite_counts)
            mean_firing = firing_ticks.get(lobe.index, 0) / max(frames, 1)
            print(f"  {lobe.name:13s} {lobe.neuron_count:4d} neurons "
                  f"{dendrite_total:5d} dendrites  rules -> {lobe.rule_targets}  "
                  f"mean firing neurons {mean_firing:.1f}")


def frames(path):
    for _, frame in read_recording(path):
        clas = frame.attention_classifier
        print(f"{frame.tick}\t{frame.x},{frame.y}\taction={frame.action}\t"
              f"slot={frame.attention_slot}\tit={clas >> 24} {(clas >> 16) & 255} {(clas >> 8) & 255}\t"
              f"{'K' if frame.keyframe else 'D'} n={frame.changed_neurons} d={frame.changed_dendrites}")


if __name__ == "__main__":
    if len(sys.argv) < 3 or sys.argv[1] not in ("summary", "frames"):
        print(__doc__)
        sys.exit(2)
    if sys.argv[1] == "summary":
        summary(sys.argv[2:])
    else:
        frames(sys.argv[2])
