#!/usr/bin/env python3
"""Where a recorded creature's chemicals came from (format 9 recordings).

    chemical_provenance.py CHEMICAL FILE [N]   at each of the N largest rises
                                               of CHEMICAL (a number or name,
                                               e.g. Punishment), what it was
                                               made from, by root origin
    chemical_provenance.py roots FILE          over the whole recording, each
                                               brain chemical input's origins

The recording holds every change to every chemical as flows: emitters,
reactions, decay, stimuli, CAOS `chem`, bacteria, instincts.  This replays
them and keeps, for each chemical, how much of what is there now came from
each root origin: an emitter (named by the locus that drives it, such as a
drive or "always on"), a stimulus (what it was and what gave it), a script,
a bacterium or an instinct.  A reaction passes its reactants' origins on to
its products in proportion to what it used; decay and use take from every
origin in proportion.  A reaction with no reactants is itself a root.

The mixing is proportional, an interpretation; the flows themselves are
exact (brain_record.py check proves they explain every change).

As a module: Provenance(frame stream) and mix(chemical).
"""
import collections
import os
import sys

from brain_record import read_recording

STANDARD_LOBES = ["perception", "drive", "stimulus source", "verb", "noun",
                  "general sense", "decision", "attention", "concept"]
DRIVES = ["pain", "need for pleasure", "hunger", "coldness", "hotness", "tiredness",
          "sleepiness", "loneliness", "crowdedness", "fear", "boredom", "anger",
          "sex drive", "drive 13", "drive 14", "drive 15"]
STIMULI = ["disappointment", "pointer pats me", "creature pats me", "pointer slaps me",
           "creature slaps me", "it is approaching", "it is retreating",
           "I bump into wall", "object comes into view", "unrecognised word",
           "heard user speak", "heard creature speak", "I am quiescent",
           "I've activated 1", "I've activated 2", "I've deactivated",
           "I am approaching", "I have retreated", "I have got", "I have dropped",
           "I've stated need", "I am resting", "I am sleeping", "I am travelling",
           "laying", "involuntary action", "travelling (old)", "I have retreated (2)"] + [
           f"involuntary action {i}" for i in range(8)]
# Emitter loci (Creature::resolve_genome_locus; brain lobes: 0 active
# fraction, 1/2 loose dendrites).
CREATURE_EMITTERS = {
    0: lambda l: "pose transitions" if l == 0 else None,
    1: lambda l: f"floating locus {l}",
    2: lambda l: {0: "fertility", 1: "pregnancy"}.get(l),
    3: lambda l: "dead" if l == 0 else None,
    4: lambda l: {0: "always on", 1: "asleep", 2: "air coldness", 3: "air hotness",
                  4: "ambient light", 5: "crowdedness"}.get(l),
    5: lambda l: f"drive {DRIVES[l]}" if l < 16 else None,
}

CHEMICAL_NAMES = {}


def chemical_name(number):
    return CHEMICAL_NAMES.get(number) or f"chemical {number}"


def emitter_name(emitter):
    domain = emitter["domain"] if emitter["domain"] < 2 else emitter["domain"] & 1
    if domain == 0:
        lobe = emitter["tissue"]
        lobe = STANDARD_LOBES[lobe] if lobe < len(STANDARD_LOBES) else f"lobe {lobe}"
        what = ["active fraction", "type 0 loose", "type 1 loose"]
        locus = what[emitter["locus"]] if emitter["locus"] < 3 else f"locus {emitter['locus']}"
        return f"{lobe} lobe {locus}"
    table = CREATURE_EMITTERS.get(emitter["tissue"])
    return (table(emitter["locus"]) if table else None) or \
        f"tissue {emitter['tissue']} locus {emitter['locus']}"


def classifier_text(packed):
    return f"{packed >> 24} {(packed >> 16) & 255} {(packed >> 8) & 255}"


ACTIONS = ["quiescent", "activate 1", "activate 2", "deactivate", "approach",
           "retreat", "get", "drop", "say need", "rest", "go left", "go right"]

# The lab's classifier list (c1-lab, beside this repo), for naming sources.
CLASSIFIER_LIST = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..",
                               "c1-lab", "c1lab", "data", "c1_classifiers.tsv")


def load_classifier_names(path=CLASSIFIER_LIST):
    """packed classifier -> name (first candidate of each species row)."""
    names = {}
    if not os.path.exists(path):
        return names
    for line in open(path, encoding="utf-8"):
        fields = line.rstrip("\n").split("\t")
        if line.startswith("#") or len(fields) < 4 or not all(f.strip() for f in fields[:3]):
            continue
        try:
            family, genus, species = (int(f) for f in fields[:3])
        except ValueError:
            continue
        names.setdefault(family << 24 | genus << 16 | species << 8, fields[3])
    return names


def stimulus_name(stimulus, names=None, action=None):
    """A stimulus as a root origin: what it was, what gave it, what the
    creature was doing, and the chemicals it carried."""
    what = (STIMULI[stimulus.built_in] if stimulus.built_in < len(STIMULI)
            else "stim")
    if stimulus.flags & 1:
        source = "itself"
    else:
        name = (names or {}).get(stimulus.source_classifier)
        source = (f"{name} ({classifier_text(stimulus.source_classifier)})" if name
                  else classifier_text(stimulus.source_classifier))
    doing = ACTIONS[action] if action is not None and action < len(ACTIONS) else "?"
    chemicals = ", ".join(f"{chemical_name(c)} {a}" for c, a in stimulus.chemicals if c and a)
    return f"{what} from {source} while {doing}: {chemicals or 'no chemicals'}"


class Provenance:
    """Per chemical, amount by root origin, kept up to date frame by frame."""

    def __init__(self, classifier_names=None):
        self.mixes = collections.defaultdict(collections.Counter)
        self.chemistry = {}
        self.stimuli = {}   # number -> its root label
        self.classifier_names = (classifier_names if classifier_names is not None
                                 else load_classifier_names())
        self.action = None  # the action under way (the previous frame's)
        self.started = False

    def _root(self, flow):
        if flow.origin == "emitter":
            emitters = self.chemistry.get("emitters", [])
            if flow.number < len(emitters):
                emitter = emitters[flow.number]
            return f"emitter: {emitter_name(emitter)} -> {chemical_name(emitter['chemical'])}"
            return f"emitter {flow.number}"
        if flow.origin == "stimulus":
            return self.stimuli.get(flow.number, "stimulus")
        if flow.origin == "reaction":
            return f"reaction {flow.number}"
        return flow.origin

    def _take(self, chemical, amount):
        """Remove `amount` from a chemical's mix in proportion; returns what
        was removed, by origin."""
        mix = self.mixes[chemical]
        total = sum(mix.values())
        taken = collections.Counter()
        if total <= 0 or amount <= 0:
            return taken
        share = min(1.0, amount / total)
        for origin, value in list(mix.items()):
            part = value * share
            taken[origin] += part
            mix[origin] = value - part
            if mix[origin] <= 1e-9:
                del mix[origin]
        return taken

    def update(self, frame):
        if frame.chemistry:
            self.chemistry = frame.chemistry
        for stimulus in frame.stimuli:
            self.stimuli[stimulus.number] = stimulus_name(
                stimulus, self.classifier_names, self.action)
        self.action = frame.action
        if not self.started:
            # What is there before the recording starts has no known origin.
            for chemical, level in enumerate(frame.chemicals):
                if level:
                    self.mixes[chemical] = collections.Counter({"before the recording": level})
            self.started = bool(frame.chemicals)
            return
        reactions = collections.defaultdict(list)
        for flow in frame.flows:
            if flow.origin == "reaction":
                reactions[flow.number].append(flow)
        # Reactions turn what they use into what they make.
        for number, flows in reactions.items():
            used = collections.Counter()
            for flow in flows:
                if flow.change < 0:
                    used.update(self._take(flow.chemical, -flow.change))
            made = [f for f in flows if f.change > 0]
            total_used = sum(used.values())
            for flow in made:
                if total_used > 0:
                    for origin, value in used.items():
                        self.mixes[flow.chemical][origin] += flow.change * value / total_used
                else:
                    self.mixes[flow.chemical][self._root(flow)] += flow.change
        for flow in frame.flows:
            if flow.origin == "reaction":
                continue
            if flow.change > 0:
                self.mixes[flow.chemical][self._root(flow)] += flow.change
            else:
                self._take(flow.chemical, -flow.change)

    def mix(self, chemical):
        """Shares (0..1) of a chemical's current amount, by root origin."""
        mix = self.mixes.get(chemical, {})
        total = sum(mix.values())
        if total <= 0:
            return {}
        return {origin: value / total for origin, value in
                sorted(mix.items(), key=lambda kv: -kv[1])}


def describe_mix(mix, top=3):
    parts = [f"{share * 100:.0f}% {origin}" for origin, share in list(mix.items())[:top]
             if share >= 0.01]
    return ", ".join(parts) or "no known origin"


def brain_inputs(chemistry):
    """(lobe, slot) -> chemical, from the brain receptors on loci 13..16."""
    found = {}
    for receptor in chemistry.get("receptors", []):
        domain = receptor["domain"] if receptor["domain"] < 2 else receptor["domain"] & 1
        if domain == 0 and 13 <= receptor["locus"] <= 16:
            found[(receptor["tissue"], receptor["locus"] - 13)] = receptor["chemical"]
    return found


def resolve(text):
    if text.isdigit():
        return int(text)
    exact = [n for n, v in CHEMICAL_NAMES.items() if v.lower() == text.lower()]
    found = exact or [n for n, v in CHEMICAL_NAMES.items() if text.lower() in v.lower()]
    if not found:
        raise SystemExit(f"no chemical called {text!r} (give --chemicals ALLCHEMICALS.STR)")
    return found[0]


def cmd_chemical(chemical, path, top=10):
    chemical = resolve(chemical)
    provenance = Provenance()
    rises = []
    previous = None
    for _, frame in read_recording(path):
        if not frame.chemicals:
            raise SystemExit("format 9 recordings only (chemicals and flows)")
        provenance.update(frame)
        level = frame.chemicals[chemical]
        if previous is not None and level > previous:
            rises.append((level - previous, frame.tick, level, provenance.mix(chemical)))
        previous = level
    print(f"{chemical_name(chemical)}: {len(rises)} rises")
    for rise, tick, level, mix in sorted(rises, key=lambda r: -r[0])[:top]:
        print(f"   tick {tick}: +{rise} to {level}: {describe_mix(mix, 4)}")


def cmd_roots(path):
    provenance = Provenance()
    totals = collections.defaultdict(collections.Counter)
    inputs = {}
    for _, frame in read_recording(path):
        if not frame.chemicals:
            raise SystemExit("format 9 recordings only (chemicals and flows)")
        provenance.update(frame)
        if frame.chemistry:
            inputs = brain_inputs(frame.chemistry)
        for (lobe, slot), chemical in inputs.items():
            level = frame.chemicals[chemical]
            for origin, share in provenance.mix(chemical).items():
                totals[(lobe, slot, chemical)][origin] += level * share
    for (lobe, slot, chemical), origins in sorted(totals.items()):
        total = sum(origins.values())
        if total <= 0:
            continue
        name = STANDARD_LOBES[lobe] if lobe < len(STANDARD_LOBES) else f"lobe {lobe}"
        print(f"{name} chem {slot} = {chemical_name(chemical)}:")
        for origin, value in origins.most_common(6):
            print(f"   {value * 100 / total:5.1f}%  {origin}")


def load_chemical_names(path):
    import os
    import struct
    if not path or not os.path.exists(path):
        return
    data = open(path, "rb").read()
    count, position = struct.unpack_from("<H", data, 0)[0], 2
    for number in range(count):
        length = data[position]
        position += 1
        if length == 0xFF:
            length = struct.unpack_from("<H", data, position)[0]
            position += 2
        CHEMICAL_NAMES[number] = data[position:position + length].decode("latin-1")
        position += length


if __name__ == "__main__":
    args = sys.argv[1:]
    if "--chemicals" in args:
        at = args.index("--chemicals")
        load_chemical_names(args[at + 1])
        del args[at:at + 2]
    if len(args) >= 2 and args[0] == "roots":
        cmd_roots(args[1])
    elif len(args) >= 2:
        cmd_chemical(args[0], args[1], *[int(a) for a in args[2:]])
    else:
        print(__doc__)
        sys.exit(2)
