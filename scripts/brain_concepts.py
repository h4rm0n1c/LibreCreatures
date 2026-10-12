#!/usr/bin/env python3
"""Decode what a recorded C1 brain has learned (recordings from C1_BRAIN_RECORD).

    brain_concepts.py perception FILE       what each perception neuron stands for
    brain_concepts.py concepts FILE [N]     the N busiest concept neurons: their
                                            inputs, and what was attended and
                                            done while each one fired
    brain_concepts.py rules FILE [N]        per action, the N strongest learned
                                            concept -> decision links
    brain_concepts.py timeline FILE         one line per change of action: tick,
                                            place, drives, what was attended

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


def item_name(lobe, index):
    if lobe == DRIVE:
        return DRIVES[index] if index < len(DRIVES) else f"drive {index}"
    if lobe in (VERB, DECISION):
        return ACTIONS[index] if index < len(ACTIONS) else f"action {index}"
    if lobe in (SOURCE, NOUN, ATTENTION):
        return category_name(index)
    if lobe == GENERAL:
        return f"sense {index}"
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


if __name__ == "__main__":
    signal.signal(signal.SIGPIPE, signal.SIG_DFL)
    commands = {"perception": cmd_perception, "concepts": cmd_concepts,
                "rules": cmd_rules, "timeline": cmd_timeline}
    if len(sys.argv) < 3 or sys.argv[1] not in commands:
        print(__doc__)
        sys.exit(2)
    extra = [int(a) for a in sys.argv[3:]]
    commands[sys.argv[1]](sys.argv[2], *extra)
