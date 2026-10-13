#!/usr/bin/env python3
"""Decode what a recorded C1 brain has learned (recordings from C1_BRAIN_RECORD).

  Any command takes --genome GEN (e.g. the parents' mum1.gen) and
  --chemicals ALLCHEMICALS.STR (default: two folders above GEN): general
  senses are then named from its stimulus genes, and the decision lobe's
  dendrite types from its rules (stock: type 0 excites, grown by Reward;
  type 1 inhibits, grown by Punishment).

    brain_concepts.py perception FILE       what each perception neuron stands for
    brain_concepts.py concepts FILE [N]     the N busiest concept neurons: their
                                            inputs, and what was attended and
                                            done while each one fired
    brain_concepts.py rules FILE [N]        per action, the N strongest learned
                                            concept -> decision links
    brain_concepts.py timeline FILE         one line per change of action: tick,
                                            place, drives, what was attended
    brain_concepts.py meanings FILE [N]     how often concept neurons change what
                                            they mean, and the meanings held longest
    brain_concepts.py learning FILE [N]     learning by what it was about: action,
                                            dendrite role, and the meaning the
                                            concept had at that moment
    brain_concepts.py beliefs FILE [N]      what the brain holds at the end: each
                                            action's live links summed by the
                                            current meaning of the concept they read

`rules` shows the brain's last state only.  A dendrite that migrates gets a
new source and fresh weights from the genome's range, and concept neurons
migrate too, so only `learning`, which reads the recorded dendrite events
(format 5), separates learning from rewiring.

Names assume the standard C1 lobe order (0 perception, 1 drive, 2 stimulus
source, 3 verb, 4 noun, 5 general sense, 6 decision, 7 attention, 8 concept),
which the stock genome uses.  The perception lobe holds the neurons of the
lobes copied into it, one lobe after another; recordings before format 4 do
not say which lobes those are, so their perception neurons stay numbered.
"""
import collections
import signal
import sys

from brain_record import read_recording

PERCEPTION, DRIVE, SOURCE, VERB, NOUN, GENERAL, DECISION, ATTENTION, CONCEPT = range(9)

# The CAOS manual's Drives table (1-based there).
DRIVES = ["pain", "need for pleasure", "hunger", "coldness", "hotness",
          "tiredness", "sleepiness", "loneliness", "crowdedness", "fear",
          "boredom", "anger", "sex drive"]

# decision.str in the game folder.
ACTIONS = ["quiescent", "activate 1", "activate 2", "deactivate", "approach",
           "retreat", "get", "drop", "say need", "rest", "go left", "go right"]


def category_name(index):
    """A noun, stimulus source or attention neuron: the attention category.
    Simple objects (family 2) use their genus, compound objects (3) genus +
    25, creatures (4) genus + 35 (attention.cpp); 0 is unknown or scenery.
    Words are the noun lobe's (the manual's BlackBoardWords)."""
    simple = ["unknown/scenery", "hand", "call button", "water", "plant", "egg",
              "food", "drink", "vendor", "music", "animal", "fire", "shower/clock",
              "toy", "big toy", "weed"]
    compound = {26: "mover", 27: "lift", 28: "computer", 29: "fun", 30: "bang"}
    creature = {35: "creature", 36: "norn", 37: "grendel", 38: "ettin", 39: "shee"}
    if index < len(simple):
        return simple[index]
    return compound.get(index) or creature.get(index) or f"category {index}"


# General sense neurons the engine sets itself: near the wall being faced,
# riding a vehicle, and facts about an attended creature (creature.cpp,
# UpdatePerception @0040bf10); how near the attended object is (255 - 2 x
# horizontal distance, within 128 px) and whether it is active (its actv
# is not 0) (update.cpp, UpdateAllCreatureBrainInputs @00432ee0).  The rest
# are named by the genome's stimulus genes (set_genome).
ENGINE_SENSES = {3: "near a wall", 4: "in a vehicle", 12: "it is near me",
                 13: "it is active", 15: "it is a creature",
                 16: "it shares a parent with me", 17: "it is my parent",
                 18: "it is my child", 19: "it is the opposite sex"}
GENERAL_SENSES = dict(ENGINE_SENSES)
# Dendrite type roles in the decision lobe, from the genome (set_genome).
DECISION_TYPES = {0: "type 0", 1: "type 1"}
# The game's allchemicals.str, for chemical names (--chemicals, else the one
# two folders above the genome, as in a game install's Genetics folder).
CHEMICALS_FILE = None


def set_genome(path):
    """Name general senses from a genome's stimulus genes, and the decision
    lobe's dendrite types from its state and reinforcement rules."""
    import os
    import genome_brain
    GENERAL_SENSES.clear()
    GENERAL_SENSES.update(ENGINE_SENSES)
    for neuron, label in genome_brain.general_sense_names(path).items():
        if neuron < 32:
            GENERAL_SENSES[neuron] = label
    lobes = genome_brain.lobe_rules(path)
    if len(lobes) > DECISION:
        decision = lobes[DECISION]
        chemicals = {}
        for candidate in [CHEMICALS_FILE,
                          os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(path))),
                                       "allchemicals.str")]:
            if candidate and os.path.exists(candidate):
                chemicals = genome_brain.read_chemical_names(candidate)
                break
        inputs = genome_brain.chemical_inputs(path, chemicals)
        for t, rule in enumerate(decision["types"]):
            sign = ("+" if f"+ type {t}" in decision["state rule"] or
                    decision["state rule"].startswith(f"type {t}") else
                    "-" if f"- type {t}" in decision["state rule"] else "?")
            role = {"+": "excites", "-": "inhibits"}.get(sign, "type")
            chem = rule["reinforcement rule"]
            if chem.startswith("chem ") and (DECISION, int(chem[5])) in inputs:
                chem = inputs[(DECISION, int(chem[5]))]
            DECISION_TYPES[t] = f"{role} (type {t}, grown by {chem})"


def item_name(lobe, index):
    if lobe == DRIVE:
        return DRIVES[index] if index < len(DRIVES) else f"drive {index}"
    if lobe in (VERB, DECISION):
        return ACTIONS[index] if index < len(ACTIONS) else f"action {index}"
    if lobe in (SOURCE, NOUN, ATTENTION):
        return category_name(index)
    if lobe == GENERAL:
        return GENERAL_SENSES.get(index, f"sense {index}")
    return f"neuron {index}"


LOBE_WORDS = {DRIVE: "drive", SOURCE: "stimulus from", VERB: "verb", ATTENTION: "attending",
              NOUN: "heard/seen noun", GENERAL: "general sense"}


def perception_labels(layout):
    """Label for each perception neuron: the copied lobe and item it holds."""
    perception = layout.lobes[PERCEPTION]
    labels = [f"perception {i}" for i in range(perception.neuron_count)]
    if layout.version < 4:
        return labels
    cursor = 0
    for lobe in layout.lobes[1:]:
        if not lobe.perception_copy:
            continue
        for index in range(lobe.neuron_count):
            if cursor >= perception.neuron_count:
                return labels
            word = LOBE_WORDS.get(lobe.index, lobe.name)
            labels[cursor] = f"{word}: {item_name(lobe.index, index)}"
            cursor += 1
    return labels


def target_label(layout, perception, lobe_index, rule, target):
    """Name the neuron a dendrite of a ``lobe_index`` neuron reads."""
    target_lobe = layout.lobes[lobe_index].rule_targets[rule]
    if target == 0xffff:
        return None
    if target_lobe == PERCEPTION:
        return perception[target] if target < len(perception) else f"perception {target}"
    if target_lobe == CONCEPT:
        return f"concept {target}"
    return f"{layout.lobes[target_lobe].name} {item_name(target_lobe, target)}"


def attended(frame):
    if frame.attention_slot < 0:
        return "nothing"
    return category_name(frame.attention_slot)


def action_name(index):
    return ACTIONS[index] if index < len(ACTIONS) else f"action {index}"


def concept_inputs(layout, perception, dendrites, neuron):
    """The perception neurons one concept neuron reads, by rule."""
    lobe = layout.lobes[CONCEPT]
    count0, count1 = lobe.dendrite_counts[neuron]
    out = []
    for rule, count in ((0, count0), (1, count1)):
        for d in range(count):
            target = dendrites[(CONCEPT, neuron, rule, d)][0]
            label = target_label(layout, perception, CONCEPT, rule, target)
            if label:
                out.append(f"{'+' if rule == 0 else '-'}{label}")
    return out


def cmd_perception(path):
    for layout, _ in read_recording(path):
        if layout.version < 4:
            print("format 3 recording: the copied lobes are not recorded, so "
                  "perception neurons cannot be named")
        for index, label in enumerate(perception_labels(layout)):
            print(f"{index:4d}  {label}")
        return


def cmd_concepts(path, top=30):
    fired = collections.Counter()
    seen_with = collections.defaultdict(collections.Counter)
    done_with = collections.defaultdict(collections.Counter)
    layout = last = None
    frames = 0
    for layout, frame in read_recording(path):
        frames += 1
        last = frame
        what = attended(frame)
        act = action_name(frame.action)
        for neuron in range(layout.lobes[CONCEPT].neuron_count):
            if frame.neurons[(CONCEPT, neuron)][1]:
                fired[neuron] += 1
                seen_with[neuron][what] += 1
                done_with[neuron][act] += 1
    if layout is None:
        return
    perception = perception_labels(layout)
    print(f"{frames} frames; concept neurons that fired: {len(fired)} of "
          f"{layout.lobes[CONCEPT].neuron_count}")
    for neuron, count in fired.most_common(top):
        inputs = concept_inputs(layout, perception, last.dendrites, neuron)
        seen = ", ".join(f"{k} {v * 100 // count}%" for k, v in seen_with[neuron].most_common(3))
        done = ", ".join(f"{k} {v * 100 // count}%" for k, v in done_with[neuron].most_common(3))
        print(f"concept {neuron:3d}  fired {count * 100 / frames:5.1f}% of frames  "
              f"inputs [{'; '.join(inputs) or 'none'}]")
        print(f"             attending {seen}  |  doing {done}")


def cmd_rules(path, top=8):
    layout = last = None
    for layout, frame in read_recording(path):
        last = frame
    if layout is None:
        return
    perception = perception_labels(layout)
    decision = layout.lobes[DECISION]
    for neuron in range(decision.neuron_count):
        if neuron >= len(ACTIONS):
            break
        links = []
        count0, count1 = decision.dendrite_counts[neuron]
        for rule, count in ((0, count0), (1, count1)):
            for d in range(count):
                target, current, stw, ltw, strength = last.dendrites[(DECISION, neuron, rule, d)]
                if target == 0xffff or decision.rule_targets[rule] != CONCEPT:
                    continue
                links.append((ltw, current, stw, strength, rule, target))
        links.sort(reverse=True)
        print(f"{action_name(neuron)}:")
        for ltw, current, stw, strength, rule, target in links[:top]:
            inputs = concept_inputs(layout, perception, last.dendrites, target)
            print(f"   type {rule}  LTW {ltw:3d} STW {stw:3d} weight {current:3d} "
                  f"strength {strength:3d}  concept {target} [{'; '.join(inputs) or 'none'}]")


def cmd_timeline(path):
    previous = None
    for layout, frame in read_recording(path):
        # A drive's level is its firing strength (the early phase leaves its
        # activation at the lobe's baseline).
        key = (frame.action, frame.attention_slot)
        if key == previous:
            continue
        previous = key
        drives = sorted(((frame.neurons[(DRIVE, i)][1], DRIVES[i])
                         for i in range(min(len(DRIVES), layout.lobes[DRIVE].neuron_count))),
                        reverse=True)[:3]
        top = ", ".join(f"{name} {value}" for value, name in drives if value)
        print(f"{frame.tick}\t{frame.x},{frame.y}\t{action_name(frame.action)} "
              f"-> {attended(frame)}\t[{top}]")


def concept_meaning(layout, perception, dendrites, neuron):
    """What a concept neuron reads now: its attached inputs (strength > 0),
    or "loose" when none are attached."""
    lobe = layout.lobes[CONCEPT]
    count0, count1 = lobe.dendrite_counts[neuron]
    inputs = []
    for rule, count in ((0, count0), (1, count1)):
        for d in range(count):
            target, _, _, _, strength = dendrites[(CONCEPT, neuron, rule, d)]
            if strength and target != 0xffff:
                inputs.append(target_label(layout, perception, CONCEPT, rule, target))
    return tuple(sorted(inputs)) or ("loose",)


OPENING = {"migrate", "attach", "rescue"}
CLOSING = {"went loose", "migrate failed"}


class BrainReplay:
    """Replays a format 5+ recording's events and tracks, for every concept
    neuron, what it means over time: the set of its inputs attached at
    full strength (strength > 0).  A meaning is a sorted tuple of input
    labels, or ("loose",).  Decision events are handed to `on_decision`
    with the meaning their target concept had at that moment."""

    def __init__(self, path):
        self.path = path
        self.layout = None
        self.perception = None
        self.concept_state = {}   # concept dendrite key -> (source, current, STW, LTW, strength)
        self.decision_state = {}  # decision dendrite key -> same
        self.meaning = {}         # concept neuron -> current meaning
        self.epochs = collections.defaultdict(list)  # neuron -> [start, end, meaning]
        self.first_tick = self.last_tick = None

    def _meaning_of(self, neuron):
        lobe = self.layout.lobes[CONCEPT]
        count0, count1 = lobe.dendrite_counts[neuron]
        inputs = []
        for rule, count in ((0, count0), (1, count1)):
            for d in range(count):
                source, _, _, _, strength = self.concept_state[(CONCEPT, neuron, rule, d)]
                if strength and source != 0xffff:
                    inputs.append(target_label(self.layout, self.perception, CONCEPT, rule, source))
        return tuple(sorted(inputs)) or ("loose",)

    def _refresh(self, neuron, tick):
        meaning = self._meaning_of(neuron)
        if meaning != self.meaning.get(neuron):
            history = self.epochs[neuron]
            if history:
                history[-1][1] = tick
            history.append([tick, None, meaning])
            self.meaning[neuron] = meaning

    def meaning_of_target(self, decision_key):
        source = self.decision_state[decision_key][0]
        if source == 0xffff:
            return ("none",)
        return self.meaning.get(source, ("?",))

    def run(self, on_decision=None, on_frame=None):
        for layout, frame in read_recording(self.path):
            if self.layout is None:
                self.layout = layout
                if layout.version < 5:
                    raise SystemExit(f"format {layout.version} recording: it has no events; "
                                     "meanings over time need format 5 or later")
                self.perception = perception_labels(layout)
                self.first_tick = frame.tick
                for key, values in frame.dendrites.items():
                    if key[0] == CONCEPT:
                        self.concept_state[key] = values
                    elif key[0] == DECISION:
                        self.decision_state[key] = values
                for neuron in range(layout.lobes[CONCEPT].neuron_count):
                    self._refresh(neuron, frame.tick)
                if on_frame:
                    on_frame(self, frame)
                continue
            for event in frame.events:
                if event.key[0] == CONCEPT:
                    self.concept_state[event.key] = event.after
                    self._refresh(event.key[1], frame.tick)
                elif event.key[0] == DECISION:
                    meaning = (self.meaning_of_target(event.key)
                               if event.kind not in OPENING else None)
                    self.decision_state[event.key] = event.after
                    if event.kind in OPENING:
                        meaning = self.meaning_of_target(event.key)
                    if on_decision:
                        on_decision(self, frame, event, meaning)
            self.last_tick = frame.tick
            if on_frame:
                on_frame(self, frame)
        for history in self.epochs.values():
            if history and history[-1][1] is None:
                history[-1][1] = self.last_tick


def describe_meaning(meaning):
    return "; ".join(meaning)


def cmd_meanings(path, top=15):
    """How often concept neurons change meaning, and which meanings last."""
    replay = BrainReplay(path)
    replay.run()
    span = max(replay.last_tick - replay.first_tick, 1)
    epochs = [e for history in replay.epochs.values() for e in history]
    real = [e for e in epochs if e[2] != ("loose",)]
    changes = sum(len(h) - 1 for h in replay.epochs.values())
    # A meaning replaced within the same tick (an attach and a weaken in one
    # update) was never in force; it is counted but has no length.
    lasting = sorted(e[1] - e[0] for e in real if e[1] > e[0])
    print(f"ticks {replay.first_tick}-{replay.last_tick}; {len(replay.epochs)} concept neurons, "
          f"{changes} meaning changes; {len(lasting)} attached meanings held for a tick or more "
          f"({len(real) - len(lasting)} replaced within their tick), median "
          f"{lasting[len(lasting) // 2] if lasting else 0} ticks")
    held = collections.Counter()
    holders = collections.defaultdict(set)
    for neuron, history in replay.epochs.items():
        for start, end, meaning in history:
            if meaning != ("loose",):
                held[meaning] += end - start
                holders[meaning].add(neuron)
    loose_now = sum(1 for m in replay.meaning.values() if m == ("loose",))
    print(f"concept neurons loose at the end: {loose_now}")
    print(f"meanings held longest (neuron-ticks, % of the run summed over neurons):")
    for meaning, ticks in held.most_common(top):
        print(f"   {ticks * 100 / span:6.0f}%  on {len(holders[meaning]):3d} neurons  "
              f"[{describe_meaning(meaning)}]")


def cmd_learning(path, top=12):
    """Learning grouped by what it was about: (action, dendrite role,
    meaning of the concept at the moment), from the recorded events."""
    totals = collections.defaultdict(lambda: collections.Counter())

    def on_decision(replay, frame, event, meaning):
        _, neuron, rule, _ = event.key
        bucket = totals[(neuron, rule, meaning)]
        if event.kind == "ltw step":
            bucket["dream LTW" if event.dreaming else "awake LTW"] += event.after[3] - event.before[3]
        elif event.kind == "dream LTW raise":
            bucket["dream LTW"] += event.after[3] - event.before[3]
        elif event.kind == "reinforce":
            bucket["dream reinforce" if event.dreaming else "reinforce"] += 1
        elif event.kind in OPENING:
            bucket["links made"] += 1
        elif event.kind in CLOSING:
            bucket["links lost"] += 1
            bucket["LTW lost"] += event.before[3]

    replay = BrainReplay(path)
    replay.run(on_decision=on_decision)
    print(f"ticks {replay.first_tick}-{replay.last_tick}")

    def line(key, c):
        neuron, rule, meaning = key
        return (f"{action_name(neuron):11s} {DECISION_TYPES[rule]:38s} awake LTW {c['awake LTW']:+5d}  "
                f"dream LTW {c['dream LTW']:+5d}  reinforced {c['reinforce']:5d}  links made "
                f"{c['links made']:4d} lost {c['links lost']:4d} (LTW lost {c['LTW lost']})  "
                f"[{describe_meaning(meaning)}]")

    awake = sorted(totals.items(), key=lambda kv: kv[1]["awake LTW"], reverse=True)
    print(f"\nlearned while awake, by what it was about:")
    for key, c in awake[:top]:
        if c["awake LTW"] > 0:
            print("   " + line(key, c))
    dream = sorted(totals.items(), key=lambda kv: kv[1]["dream LTW"], reverse=True)
    print(f"\nlearned while dreaming (instincts):")
    for key, c in dream[:top]:
        if c["dream LTW"] > 0:
            print("   " + line(key, c))
    lost = sum(c["LTW lost"] for c in totals.values())
    gained = sum(c["awake LTW"] + c["dream LTW"] for c in totals.values())
    print(f"\nLTW gained {gained}, LTW lost when links went loose or failed to migrate {lost}")


def cmd_beliefs(path, top=20):
    """What the brain holds at the end: for each action, the LTW of its live
    links summed by the current meaning of the concept each reads, with
    inhibiting links counted against."""
    replay = BrainReplay(path)
    replay.run()
    held = collections.defaultdict(lambda: [0, 0, 0])  # (action, meaning) -> [excite, inhibit, links]
    for key, (source, _, _, ltw, strength) in replay.decision_state.items():
        if not strength or source == 0xffff or not ltw:
            continue
        _, neuron, rule, _ = key
        meaning = replay.meaning.get(source, ("?",))
        if meaning == ("loose",):
            continue
        entry = held[(neuron, meaning)]
        entry[rule] += ltw
        entry[2] += 1
    print(f"at tick {replay.last_tick}: {len(held)} meaning -> action beliefs "
          f"(type 0 = {DECISION_TYPES[0]}, type 1 = {DECISION_TYPES[1]})")
    ranked = sorted(held.items(), key=lambda kv: abs(kv[1][0] - kv[1][1]), reverse=True)
    for (neuron, meaning), (excite, inhibit, links) in ranked[:top]:
        net = excite - inhibit
        print(f"   {'DO  ' if net > 0 else 'DONT'} {action_name(neuron):11s} net {net:+5d} "
              f"(+{excite} -{inhibit}, {links} links)  when [{describe_meaning(meaning)}]")


if __name__ == "__main__":
    signal.signal(signal.SIGPIPE, signal.SIG_DFL)
    commands = {"perception": cmd_perception, "concepts": cmd_concepts,
                "rules": cmd_rules, "timeline": cmd_timeline,
                "learning": cmd_learning, "meanings": cmd_meanings,
                "beliefs": cmd_beliefs}
    if len(sys.argv) < 3 or sys.argv[1] not in commands:
        print(__doc__)
        sys.exit(2)
    args = sys.argv[3:]
    if "--chemicals" in args:
        at = args.index("--chemicals")
        CHEMICALS_FILE = args[at + 1]
        del args[at:at + 2]
    if "--genome" in args:
        at = args.index("--genome")
        set_genome(args[at + 1])
        del args[at:at + 2]
    commands[sys.argv[1]](sys.argv[2], *[int(a) for a in args])
