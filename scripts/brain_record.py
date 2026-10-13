#!/usr/bin/env python3
"""Read brain recordings made with C1_BRAIN_RECORD (src/c1/brain/brain_recorder.hpp).

    brain_record.py summary FILE...     frames, ticks, size, per-lobe activity
    brain_record.py frames FILE         one line per frame: tick, place, action,
                                        attention, neurons and dendrites changed
    brain_record.py check FILE...       (format 5+) every change to a dendrite's
                                        source, STW, LTW or strength must be the
                                        result of the recorded events; (format
                                        9) every change to a chemical must be
                                        the sum of its recorded flows; (format
                                        10) every vote sum must be its recorded
                                        contributions, clamped and scaled by the
                                        lobe's input gain; reports any that are
                                        not

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
    genome: bytes = b""     # format 8: the creature's own .gen file
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
    changed_dendrite_keys: list | None = None  # None on a keyframe (all may differ)
    flags: int = 0          # format 5: 1 asleep, 2 dreaming
    drives: tuple = ()      # format 7: the creature's 16 drive levels
    # life stage (format 8) is flags >> 2 & 7: see Frame.life_stage
    lobe_chemicals: dict = field(default_factory=dict)  # format 7: (lobe, 0..3) -> value
    events: list = field(default_factory=list)  # format 5: Event since the last frame
    # Format 9:
    chemicals: tuple = ()   # the 256 chemical concentrations
    flows: list = field(default_factory=list)    # Flow since the last frame
    stimuli: list = field(default_factory=list)  # Stimulus since the last frame
    chemistry: dict = field(default_factory=dict)  # receptors, emitters, reactions
    hand: tuple = ()        # (x, y) of the hand
    attended: tuple = ()    # (handle, x, y) of the attended object; handle 0 none
    # Format 10:
    votes: dict = field(default_factory=dict)  # lobe -> Votes (the last update's sums)
    goal_matrix: tuple = ()  # 40 attention records x 16 drive weights, flat
    words: dict = field(default_factory=dict)  # record -> (heard, said, reinforcement)
    instincts: list = field(default_factory=list)  # Instinct started since the last frame


EVENT_KINDS = {1: "reinforce", 2: "relax", 3: "ltw step", 4: "strengthen",
               5: "weaken", 6: "went loose", 7: "attach", 8: "rescue",
               9: "migrate", 10: "migrate failed", 11: "dream LTW raise",
               12: "reset"}


def life_stage(frame):
    """The creature's life stage at a frame (format 8; 0 before)."""
    return (frame.flags >> 2) & 7


@dataclass
class Event:
    kind: str
    dreaming: bool
    key: tuple       # (lobe, neuron, rule, dendrite)
    before: tuple    # (source, current, STW, LTW, strength)
    after: tuple
    instinct: int | None = None  # format 10: the instinct being dreamed, if dreaming


FLOW_ORIGINS = {0: "unknown", 1: "emitter", 2: "reaction", 3: "decay",
                4: "stimulus", 5: "script", 6: "bacterium", 7: "instinct"}


@dataclass
class Flow:
    origin: str      # FLOW_ORIGINS
    number: int      # emitter or reaction (its place in chemistry), stimulus number
    chemical: int
    change: int


@dataclass
class Stimulus:
    number: int
    built_in: int            # 0..35, or 255 for a scripted stim
    source: int              # the source object's CAOS handle
    source_classifier: int   # packed: family << 24 | genus << 16 | species << 8
    source_x: int
    source_y: int
    flags: int               # 1 self, 2 ignored asleep, 4 halved, 8 fed the sense
    attention_record: int    # 255 none
    sense_neuron: int
    source_activation: int
    sense_activation: int
    chemicals: tuple         # ((chemical, amount) x 4)


@dataclass
class Votes:
    sums: list           # per neuron (type0, type1): the SVRule's inputs
    contributions: dict  # dendrite key -> (STW x source firing) >> 8


@dataclass
class Instinct:
    number: int
    inputs: tuple        # ((lobe, neuron) x 3) it replays
    decision_neuron: int
    chemical: int
    concentration: int


def zigzag(value):
    return value >> 1 if not value & 1 else -((value + 1) >> 1)


def read_chemistry(r):
    """The 'B' record body: receptors, emitters, reactions as loaded."""
    tables = {}
    for name, width, fields in (
            ("receptors", 8, ("domain", "tissue", "locus", "chemical", "threshold",
                              "nominal", "gain", "flags")),
            ("emitters", 8, ("domain", "tissue", "locus", "chemical", "threshold",
                             "period", "amount", "flags"))):
        tables[name] = [dict(zip(fields, r.take(f"{width}B"))) for _ in range(r.take("B"))]
    reactions = []
    for _ in range(r.take("B")):
        values = r.take("9B")
        reactions.append({"reactants": [(values[0], values[1]), (values[2], values[3])],
                          "products": [(values[4], values[5]), (values[6], values[7])],
                          "rate": values[8]})
    tables["reactions"] = reactions
    return tables


def read_values(r, values, total):
    """An 'I'/'X' style run: whole (u8 0) or changed values (u8 1)."""
    if r.take("B") == 0:
        return list(r.take(f"{total}B")) if total > 1 else [r.take("B")]
    values = list(values)
    number = 0
    for _ in range(r.varint()):
        number += r.varint()
        values[number] = r.take("B")
        number += 1
    return values


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
    if version not in (3, 4, 5, 6, 7, 8, 9, 10):
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


def frame_inputs(inputs, layout):
    """(drives, lobe chemicals) from a format 7 'I' state; empty before."""
    if not inputs:
        return (), {}
    chemicals = {(lobe, slot): inputs[16 + 4 * lobe + slot]
                 for lobe in range(len(layout.lobes)) for slot in range(4)}
    return tuple(inputs[:16]), chemicals


def read_recording(path):
    """Yield (layout, Frame) for every frame, with the full state rebuilt."""
    with open(path, "rb") as f:
        file_reader = Reader(f.read())
    layout = read_layout(file_reader)
    neurons, dendrites = {}, {}
    running = {}  # format 6: each dendrite's state as events last left it
    inputs = []   # format 7: drives then each lobe's chem 0..3
    context = None
    flags = 0
    events = []
    chemicals, flows, stimuli, chemistry, world = [], [], [], {}, {}
    goal_matrix, words, instincts = [], {}, []
    current_instinct = None

    def extras():
        return {"chemicals": tuple(chemicals), "flows": flows, "stimuli": stimuli,
                "chemistry": chemistry, "words": dict(words), "instincts": instincts,
                **world}
    for r in read_chunks(file_reader):
        while not r.done():
            kind = r.take("c")
            if kind == b"G":
                length = r.take("I")
                layout.genome = bytes(r.data[r.pos:r.pos + length])
                r.pos += length
            elif kind == b"C":
                context = r.take("IiiBbI")
                flags = r.take("B") if layout.version >= 5 else 0
            elif kind == b"I":
                inputs = read_values(r, inputs, 16 + 4 * len(layout.lobes))
            elif kind == b"B":
                r.take("I")
                chemistry = read_chemistry(r)
            elif kind == b"X":
                chemicals = read_values(r, chemicals, 256)
            elif kind == b"F":
                flows = []
                for _ in range(r.varint()):
                    origin, number, chemical = r.take("B"), r.varint(), r.take("B")
                    change = zigzag(r.varint())
                    flows.append(Flow(FLOW_ORIGINS.get(origin, str(origin)), number,
                                      chemical, change))
            elif kind == b"W":
                hand_x, hand_y, handle, at_x, at_y = r.take("iiIii")
                world = {"hand": (hand_x, hand_y), "attended": (handle, at_x, at_y)}
            elif kind == b"V":
                votes = {}
                for _ in range(r.take("B")):
                    lobe = r.take("B")
                    count = len(layout.lobes[lobe].dendrite_counts)
                    raw = r.take(f"{2 * count}B") if count else ()
                    if count == 1 and not isinstance(raw, tuple):
                        raw = (raw,)
                    sums = [(raw[2 * i], raw[2 * i + 1]) for i in range(count)]
                    contributions, number = {}, 0
                    for _ in range(r.varint()):
                        number += r.varint()
                        contributions[layout.dendrite_keys[number]] = r.take("B")
                        number += 1
                    votes[lobe] = Votes(sums, contributions)
                world["votes"] = votes
            elif kind == b"M":
                if r.take("B") == 0:
                    goal_matrix = [zigzag(r.varint()) for _ in range(r.varint())]
                else:
                    number = 0
                    for _ in range(r.varint()):
                        number += r.varint()
                        goal_matrix[number] = zigzag(r.varint())
                        number += 1
                world["goal_matrix"] = tuple(goal_matrix)
            elif kind == b"L":
                for _ in range(r.take("B")):
                    index = r.take("B")
                    heard = bytes(r.take(f"{r.take('B')}s")).decode("latin-1")
                    said = bytes(r.take(f"{r.take('B')}s")).decode("latin-1")
                    words[index] = (heard, said, r.take("I"))
            elif kind == b"N":
                number = r.varint()
                pairs = r.take("6B")
                decision_neuron, chemical, concentration = r.take("3B")
                instincts.append(Instinct(number, tuple(zip(pairs[0::2], pairs[1::2])),
                                          decision_neuron, chemical, concentration))
                current_instinct = number
            elif kind == b"S":
                number, built_in, source, classifier, sx, sy = r.varint(), *r.take("BIIii")
                flags_s, record, sense, act_s, act_g = r.take("5B")
                pairs = r.take("8B")
                stimuli.append(Stimulus(number, built_in, source, classifier, sx, sy, flags_s,
                                        record, sense, act_s, act_g,
                                        tuple(zip(pairs[0::2], pairs[1::2]))))
            elif kind == b"E" and layout.version == 5:
                event_kind, event_flags = r.take("2B")
                number = r.varint()
                before = r.take("H4B")
                after = r.take("H4B")
                events.append(Event(EVENT_KINDS.get(event_kind, str(event_kind)),
                                    bool(event_flags & 1),
                                    layout.dendrite_keys[number], before, after))
            elif kind == b"E":
                # Format 6: only what the event changed; the state before it
                # is the running state (keyframe, deltas, events so far).
                code = r.take("B")
                key = layout.dendrite_keys[r.varint()]
                event_kind = EVENT_KINDS.get(code & 0x7f, str(code & 0x7f))
                before = running[key]
                source, current, stw, ltw, strength = before
                if event_kind in ("reinforce", "relax"):
                    stw = r.take("B")
                elif event_kind == "ltw step":
                    ltw = r.take("B")
                elif event_kind in ("strengthen", "weaken"):
                    strength = r.take("B")
                elif event_kind in ("attach", "rescue", "migrate", "migrate failed"):
                    source, stw, ltw, strength = r.take("H3B")
                elif event_kind == "went loose":
                    current = stw = ltw = strength = 0
                elif event_kind == "dream LTW raise":
                    ltw = stw
                elif event_kind == "reset":
                    current, stw = 0, ltw
                after = (source, current, stw, ltw, strength)
                running[key] = after
                events.append(Event(event_kind, bool(code & 0x80), key, before, after,
                                    current_instinct if code & 0x80 else None))
            elif kind == b"K":
                r.take("I")
                for key in layout.neuron_keys:
                    neurons[key] = r.take("2B")
                for key in layout.dendrite_keys:
                    dendrites[key] = r.take("H4B")
                running = dict(dendrites)
                yield layout, Frame(*context, True, neurons, dendrites,
                                    len(layout.neuron_keys), len(layout.dendrite_keys),
                                    None, flags, *frame_inputs(inputs, layout), events,
                                    **extras())
                events, flows, stimuli, instincts = [], [], [], []
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
                changed_keys = []
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
                    running[key] = dendrites[key]
                    changed_keys.append(key)
                    number += 1
                yield layout, Frame(*context, False, neurons, dendrites, changed_n, changed_d,
                                    changed_keys, flags, *frame_inputs(inputs, layout), events,
                                    **extras())
                events, flows, stimuli, instincts = [], [], [], []
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


def structural(values):
    """A dendrite's source, STW, LTW and strength: the fields events explain.
    Current weight (susceptibility) changes every tick and is not evented."""
    source, _, stw, ltw, strength = values
    return (source, stw, ltw, strength)


def check(paths):
    """Every structural change in the frames must match the events."""
    failed = False
    for path in paths:
        state = None
        frames = events = continuity = unexplained = 0
        flow_count = unexplained_chemicals = unknown_flows = 0
        vote_count = unexplained_votes = 0
        gains = None
        chemicals = None
        examples = []
        for layout, frame in read_recording(path):
            if layout.version < 5:
                print(f"{path}: format {layout.version} has no events; nothing to check")
                break
            frames += 1
            if frame.votes and layout.genome:
                if gains is None:
                    import genome_brain
                    gains = [lobe["input gain"] for lobe in
                             genome_brain.lobe_rules(layout.genome, ((frame.flags >> 5) & 3) or None)]
                for lobe, votes in frame.votes.items():
                    totals = {}
                    for (_, neuron, rule, _), value in votes.contributions.items():
                        totals[(neuron, rule)] = totals.get((neuron, rule), 0) + value
                    for neuron, pair in enumerate(votes.sums):
                        for rule in (0, 1):
                            expected = (gains[lobe] * min(255, totals.get((neuron, rule), 0))) >> 8
                            vote_count += 1
                            if expected != pair[rule]:
                                unexplained_votes += 1
                                if len(examples) < 5:
                                    examples.append(
                                        f"tick {frame.tick}: lobe {lobe} neuron {neuron} type {rule} "
                                        f"sum {pair[rule]}, contributions give {expected}")
            if frame.chemicals:
                if chemicals is not None:
                    totals = [0] * 256
                    for flow in frame.flows:
                        totals[flow.chemical] += flow.change
                        flow_count += 1
                    for chemical in range(256):
                        if frame.chemicals[chemical] - chemicals[chemical] != totals[chemical]:
                            unexplained_chemicals += 1
                            if len(examples) < 5:
                                examples.append(
                                    f"tick {frame.tick}: chemical {chemical} went "
                                    f"{chemicals[chemical]} -> {frame.chemicals[chemical]}, "
                                    f"flows say {totals[chemical]:+d}")
                    unknown_flows += sum(1 for f in frame.flows if f.origin == "unknown")
                chemicals = frame.chemicals
            if state is None:
                # Events start after the first keyframe.
                state = {k: structural(v) for k, v in frame.dendrites.items()}
                continue
            for event in frame.events:
                events += 1
                if state[event.key] != structural(event.before):
                    continuity += 1
                    if len(examples) < 5:
                        examples.append(f"tick {frame.tick}: {event.kind} on {event.key} "
                                        f"starts from {structural(event.before)}, "
                                        f"last known {state[event.key]}")
                state[event.key] = structural(event.after)
            keys = frame.changed_dendrite_keys
            if keys is None:
                keys = layout.dendrite_keys
            for key in keys:
                now = structural(frame.dendrites[key])
                if now != state[key]:
                    unexplained += 1
                    if len(examples) < 5:
                        examples.append(f"tick {frame.tick}: {key} is {now} in the frame, "
                                        f"events say {state[key]}")
                    state[key] = now
        else:
            ok = (continuity == 0 and unexplained == 0 and unexplained_chemicals == 0
                  and unexplained_votes == 0)
            failed |= not ok
            flows_text = ""
            if chemicals is not None:
                flows_text = (f"; {flow_count} chemical flows, chemical changes the "
                              f"flows do not explain: {unexplained_chemicals}, flows "
                              f"of unknown origin: {unknown_flows}")
            if vote_count:
                flows_text += (f"; {vote_count} vote sums, not matching their "
                               f"contributions: {unexplained_votes}")
            print(f"{path}: {frames} frames, {events} events; "
                  f"events that start from a state the file did not have: {continuity}; "
                  f"frame changes no event explains: {unexplained}{flows_text}"
                  f" -> {'OK' if ok else 'FAILED'}")
            for line in examples:
                print("   " + line)
    return not failed


if __name__ == "__main__":
    if len(sys.argv) < 3 or sys.argv[1] not in ("summary", "frames", "check"):
        print(__doc__)
        sys.exit(2)
    if sys.argv[1] == "summary":
        summary(sys.argv[2:])
    elif sys.argv[1] == "check":
        sys.exit(0 if check(sys.argv[2:]) else 1)
    else:
        frames(sys.argv[2])
