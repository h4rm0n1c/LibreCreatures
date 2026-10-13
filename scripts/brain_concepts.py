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
    brain_concepts.py teaching FILE [N]     (format 7) the decision lobe's Reward
                                            and Punishment inputs against hunger,
                                            strongest drive, action and attention
    brain_concepts.py stream FILE [FROM TO EVERY]
                                            the norn's stream of thought: drives,
                                            perception, concepts firing with their
                                            current meanings, action scores, choice,
                                            Reward/Punishment and what it learns, at
                                            each change (or every EVERY ticks)
    brain_concepts.py causes FILE [N]       (format 7) why it was rewarded or
                                            punished and which links that taught;
                                            (format 9) traced, not guessed: each
                                            reinforcement is credited to the root
                                            origins of the Reward or Punishment
                                            that drove it (chemical_provenance.py)
    brain_concepts.py why FILE [FROM TO]    (format 10) why it did what it did: at
                                            each change of action or attention, the
                                            action scores with the concept links
                                            that pushed for and against them, and
                                            what drew its attention
    brain_concepts.py goals FILE            (format 10) its second learner: which
                                            kinds of object it has learned relieve
                                            which drives, and when it learned it
    brain_concepts.py words FILE            (format 10) the words it has learned,
                                            with when and how strongly
    brain_concepts.py instincts FILE        (format 10) each instinct it dreamed:
                                            what it replays, and the links it set
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


GENOME_OVERRIDE = False   # --genome given: do not use the recording's own genome
GENOME_NOTES = []          # what configure() found, for the report header


def configure(source, stage=0, sex=None, layout=None):
    """Name general senses, the decision lobe's dendrite roles and the lobes'
    chemical inputs from a genome (a .gen path or its bytes), as it stands
    at life stage `stage` for a creature of `sex`.  With the recorded
    `layout`, check the genome's lobes against it."""
    import os
    import genome_brain
    GENERAL_SENSES.clear()
    GENERAL_SENSES.update(ENGINE_SENSES)
    for neuron, label in genome_brain.general_sense_names(source, stage, sex).items():
        if neuron < 32:
            GENERAL_SENSES[neuron] = label
    lobes = genome_brain.lobe_rules(source, sex)
    chemicals = {}
    candidates = [CHEMICALS_FILE]
    if isinstance(source, str):
        candidates.append(os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(source))),
                                       "allchemicals.str"))
    for candidate in candidates:
        if candidate and os.path.exists(candidate):
            chemicals = genome_brain.read_chemical_names(candidate)
            break
    inputs = genome_brain.chemical_inputs(source, chemicals, stage, sex)
    if len(lobes) > DECISION:
        decision = lobes[DECISION]
        for t, rule in enumerate(decision["types"]):
            state_rule = decision["state rule"].split()
            sign = "?"
            for i, word in enumerate(state_rule):
                if word == f"type{t}":
                    before = state_rule[i - 1] if i else None
                    sign = "-" if before == "MINUS" else "+" if before in (None, "PLUS") else "?"
            role = {"+": "excites", "-": "inhibits"}.get(sign, "type")
            chem = rule["reinforcement rule"]
            if chem.startswith("chem") and chem[4:].isdigit() and (DECISION, int(chem[4:])) in inputs:
                chem = inputs[(DECISION, int(chem[4:]))]
            DECISION_TYPES[t] = f"{role} (type {t}, grown by {chem})"
    GENOME_NOTES.clear()
    GENOME_NOTES.append(f"genome: {'recording' if isinstance(source, bytes) else source}, "
                        f"life stage {stage}, sex {({1: 'male', 2: 'female'}).get(sex, '?')}")
    if layout is not None:
        if len(lobes) != len(layout.lobes):
            GENOME_NOTES.append(f"WARNING: genome builds {len(lobes)} lobes, recording has "
                                f"{len(layout.lobes)}")
        for index, (gene, recorded) in enumerate(zip(lobes, layout.lobes)):
            if gene["perception copy"] != recorded.perception_copy and recorded.perception_copy is not None:
                GENOME_NOTES.append(f"WARNING: lobe {index} perception copy {gene['perception copy']} "
                                    f"in the genome, {recorded.perception_copy} recorded")
            for t in range(2):
                if gene["types"][t]["source lobe"] % max(len(layout.lobes), 1) != recorded.rule_targets[t]:
                    GENOME_NOTES.append(f"WARNING: lobe {index} type {t} reads lobe "
                                        f"{gene['types'][t]['source lobe']} in the genome, "
                                        f"{recorded.rule_targets[t]} recorded")


def set_genome(path):
    """--genome: decode with this genome instead of the recording's own."""
    global GENOME_OVERRIDE
    GENOME_OVERRIDE = True
    configure(path)


def recording(path):
    """read_recording, configured from the creature's own genome (format 8)
    at its life stage and sex, again whenever the life stage changes."""
    from brain_record import life_stage
    configured = None
    for layout, frame in read_recording(path):
        if not GENOME_OVERRIDE and layout.genome:
            key = (life_stage(frame), (frame.flags >> 5) & 3)
            if key != configured:
                configure(layout.genome, key[0], key[1] or None, layout)
                if configured is None:
                    for note in GENOME_NOTES:
                        print(note)
                configured = key
        yield layout, frame


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
    for layout, _ in recording(path):
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
    for layout, frame in recording(path):
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
    for layout, frame in recording(path):
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
    for layout, frame in recording(path):
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
        for layout, frame in recording(self.path):
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


def cmd_teaching(path, top=8):
    """What taught the decision lobe (format 7): its Reward (chem 0) and
    Punishment (chem 1) inputs each frame, against the hunger drive, the
    chosen action, what was attended and the strongest drive."""
    frames = 0
    totals = collections.Counter()
    by_hunger = collections.defaultdict(lambda: [0, 0, 0])   # bucket -> [frames, reward, punishment]
    by_action = collections.defaultdict(lambda: [0, 0, 0])
    by_attended = collections.defaultdict(lambda: [0, 0, 0])
    by_drive = collections.defaultdict(lambda: [0, 0, 0])
    layout = None
    for layout, frame in recording(path):
        if layout.version < 7:
            raise SystemExit(f"format {layout.version} recording: it has no lobe inputs; "
                             "teaching needs format 7")
        if not frame.drives:
            continue
        frames += 1
        reward = frame.lobe_chemicals[(DECISION, 0)]
        punishment = frame.lobe_chemicals[(DECISION, 1)]
        totals["reward"] += reward
        totals["punishment"] += punishment
        totals["reward frames"] += reward > 0
        totals["punishment frames"] += punishment > 0
        hunger = frame.drives[2]
        top_drive = max(range(13), key=lambda i: frame.drives[i])
        for table, key in ((by_hunger, min(hunger // 50, 4)), (by_action, action_name(frame.action)),
                           (by_attended, attended(frame)),
                           (by_drive, DRIVES[top_drive] if frame.drives[top_drive] else "none")):
            row = table[key]
            row[0] += 1
            row[1] += reward
            row[2] += punishment
    if not frames:
        return
    print(f"{frames} frames; Reward present in {totals['reward frames'] * 100 // frames}% "
          f"(mean {totals['reward'] / frames:.1f}), Punishment present in "
          f"{totals['punishment frames'] * 100 // frames}% (mean {totals['punishment'] / frames:.1f})")

    def show(title, table, keys=None):
        print(f"\n{title}: frames, mean Reward, mean Punishment")
        items = sorted(table.items(), key=lambda kv: -kv[1][0]) if keys is None else \
            [(k, table[k]) for k in keys if k in table]
        for key, (count, reward, punishment) in items[:top if keys is None else len(items)]:
            print(f"   {str(key):22s} {count * 100 / frames:5.1f}%  reward {reward / count:6.1f}  "
                  f"punishment {punishment / count:6.1f}")
    labels = {0: "hunger 0-49", 1: "hunger 50-99", 2: "hunger 100-149", 3: "hunger 150-199",
              4: "hunger 200+"}
    show("by hunger", {labels[k]: v for k, v in by_hunger.items()},
         [labels[k] for k in range(5)])
    show("by strongest drive", by_drive)
    show("by action", by_action)
    show("by what was attended", by_attended)


def frame_drives(frame):
    """The 16 drive levels: recorded (format 7) or the drive lobe's outputs."""
    if frame.drives:
        return frame.drives
    return tuple(frame.neurons.get((DRIVE, i), (0, 0))[1] for i in range(16))


def top_drives(drives, count=3):
    ranked = sorted(((drives[i], DRIVES[i]) for i in range(len(DRIVES))), reverse=True)
    return [f"{name} {value}" for value, name in ranked[:count] if value]


def cmd_stream(path, start=0, end=None, every=0):
    """A norn's stream of thought: one line block whenever its action or
    attention changes, Reward or Punishment arrives, or a decision link is
    strengthened -- or every `every` ticks.  Each block: place and strongest
    drives; what it perceives; the concepts firing, with what they mean now;
    how the actions score; what it chose; Reward and Punishment; the
    links strengthened since the last block, by meaning."""
    end = end if end else None
    learned = collections.Counter()
    state = {"key": None, "last_tick": None}

    def on_decision(replay, frame, event, meaning):
        if event.kind in ("reinforce", "ltw step") and not event.dreaming:
            _, neuron, rule, _ = event.key
            sign = "+" if rule == 0 else "-"
            learned[f"{sign}{action_name(neuron)} when [{describe_meaning(meaning)}]"] += 1

    def on_frame(replay, frame):
        if frame.tick < start or (end is not None and frame.tick > end):
            learned.clear()
            return
        layout = replay.layout
        chems = frame.lobe_chemicals
        reward = chems.get((DECISION, 0), 0)
        punishment = chems.get((DECISION, 1), 0)
        key = (frame.action, frame.attention_slot)
        due = every and (state["last_tick"] is None or frame.tick - state["last_tick"] >= every)
        if not (key != state["key"] or reward or punishment or learned or due):
            return
        state["key"], state["last_tick"] = key, frame.tick
        perceived = [replay.perception[n] for n in range(layout.lobes[PERCEPTION].neuron_count)
                     if frame.neurons[(PERCEPTION, n)][1] and not replay.perception[n].startswith("drive")]
        concepts = sorted(((frame.neurons[(CONCEPT, n)][1], n)
                           for n in range(layout.lobes[CONCEPT].neuron_count)
                           if frame.neurons[(CONCEPT, n)][1]), reverse=True)
        thoughts = []
        for strength, n in concepts:
            meaning = describe_meaning(replay.meaning.get(n, ("?",)))
            if meaning not in thoughts:
                thoughts.append(meaning)
            if len(thoughts) == 5:
                break
        scores = sorted(((frame.neurons[(DECISION, a)][0], a) for a in range(len(ACTIONS))),
                        reverse=True)[:3]
        flags = " asleep" if frame.flags & 1 else ""
        flags += " dreaming" if frame.flags & 2 else ""
        print(f"tick {frame.tick} at {frame.x},{frame.y}{flags}  feels: "
              f"{', '.join(top_drives(frame_drives(frame))) or 'nothing'}")
        print(f"   perceives: {'; '.join(perceived) or 'nothing'}  |  attends: {attended(frame)}")
        print(f"   thinks: {' | '.join(thoughts) or 'nothing'}")
        print(f"   weighs: {', '.join(f'{action_name(a)} {v}' for v, a in scores)}  ->  "
              f"does {action_name(frame.action)}"
              + (f"  REWARD {reward}" if reward else "") + (f"  PUNISHMENT {punishment}" if punishment else ""))
        if frame.votes:
            for line in explain_choice(replay, frame):
                print("   because: " + line)
        if learned:
            print("   learns: " + "; ".join(f"{k} x{v}" for k, v in learned.most_common(4)))
            learned.clear()

    BrainReplay(path).run(on_decision=on_decision, on_frame=on_frame)


def cmd_causes(path, top=12, window=8):
    """Why the norn was rewarded or punished, and what it learned from it
    (format 7).  For each onset of Reward or Punishment at the decision lobe:
    what it was doing and attending, and which drive changed most over the
    previous `window` frames; then which meaning -> action links that
    Reward or Punishment strengthened (reinforce events) until it ended."""
    history = collections.deque(maxlen=window + 1)
    episodes = collections.Counter()
    taught = collections.defaultdict(collections.Counter)
    current = {"reward": None, "punishment": None}

    def cause_of(frame, which):
        drives = frame_drives(frame)
        before = frame_drives(history[0]) if history else drives
        deltas = sorted(((drives[i] - before[i], DRIVES[i]) for i in range(len(DRIVES))),
                        key=lambda d: abs(d[0]), reverse=True)
        change = (f"{deltas[0][1]} {'fell' if deltas[0][0] < 0 else 'rose'} {abs(deltas[0][0])}"
                  if deltas and deltas[0][0] else "no drive change")
        return f"{which} while {action_name(frame.action)} -> {attended(frame)}; {change}"

    def on_frame(replay, frame):
        if not frame.lobe_chemicals:
            return
        for which, slot in (("reward", 0), ("punishment", 1)):
            level = frame.lobe_chemicals.get((DECISION, slot), 0)
            if level and current[which] is None:
                current[which] = cause_of(frame, which)
                episodes[current[which]] += 1
            elif not level:
                current[which] = None
        history.append(frame)

    def on_decision(replay, frame, event, meaning):
        if event.kind != "reinforce" or event.dreaming:
            return
        _, neuron, rule, _ = event.key
        which = "reward" if rule == 0 else "punishment"
        cause = current[which]
        if cause is not None:
            sign = "+" if rule == 0 else "-"
            taught[cause][f"{sign}{action_name(neuron)} when [{describe_meaning(meaning)}]"] += 1

    replay = BrainReplay(path)
    version = replay_version(path)
    if version < 7:
        raise SystemExit("causes needs format 7 or later (drives and lobe chemical inputs)")
    if version >= 9:
        return traced_causes(path, top)
    replay.run(on_decision=on_decision, on_frame=on_frame)
    print(f"ticks {replay.first_tick}-{replay.last_tick}: {sum(episodes.values())} reward and "
          f"punishment onsets")
    for cause, count in episodes.most_common(top):
        print(f"\n{count:5d} x {cause}")
        for link, n in taught[cause].most_common(4):
            print(f"         strengthened {link} ({n} reinforcements)")


def explain_choice(replay, frame, top=3):
    """Why the decision lobe chose this action and the attention lobe this
    object (format 10 votes): lines of text."""
    lines = []
    votes = frame.votes.get(DECISION)
    if votes:
        scores = []
        for action, (excite, inhibit) in enumerate(votes.sums[:len(ACTIONS)]):
            scores.append((frame.neurons[(DECISION, action)][0], action, excite, inhibit))
        scores.sort(reverse=True)
        pushes = collections.defaultdict(collections.Counter)  # action -> label -> amount
        raw = collections.Counter()                             # (action, rule) -> raw total
        for (lobe, neuron, rule, dendrite), value in votes.contributions.items():
            raw[(neuron, rule)] += value
        for (lobe, neuron, rule, dendrite), value in votes.contributions.items():
            source = frame.dendrites[(lobe, neuron, rule, dendrite)][0]
            meaning = describe_meaning(replay.meaning.get(source, ("?",))) \
                if source != 0xffff else "none"
            # Each link's share of the sum the lobe used (after clamping and
            # its input gain), so the shares add up to "for" and "against".
            used = votes.sums[neuron][rule] if neuron < len(votes.sums) else 0
            pushes[neuron][("+" if rule == 0 else "-", meaning)] += \
                value * used / raw[(neuron, rule)]
        chosen = frame.action
        for state, action, excite, inhibit in [s for s in scores if s[1] == chosen] + \
                [s for s in scores if s[1] != chosen][:1]:
            reasons = sorted(pushes[action].items(), key=lambda kv: -kv[1])[:top]
            text = ", ".join(f"{sign}{amount:.0f} [{meaning}]" for (sign, meaning), amount in reasons
                             if amount >= 0.5)
            head = "chose" if action == chosen else "next best"
            # The state rule adds this update's for and against to what the
            # neuron already held (and the engine boosts the action under
            # way), so a score also carries the past.
            carried = state - excite + inhibit
            lines.append(f"{head} {action_name(action)} (score {state} = carried {carried}"
                         f" + for {excite} - against {inhibit}): {text or 'no links firing'}")
    votes = frame.votes.get(ATTENTION)
    if votes and frame.attention_slot >= 0:
        slot = frame.attention_slot
        found = collections.Counter()
        raw = collections.Counter()
        for (lobe, neuron, rule, dendrite), value in votes.contributions.items():
            if neuron == slot:
                raw[rule] += value
        for (lobe, neuron, rule, dendrite), value in votes.contributions.items():
            if neuron != slot:
                continue
            source = frame.dendrites[(lobe, neuron, rule, dendrite)][0]
            label = target_label(replay.layout, replay.perception, ATTENTION, rule, source)
            used = votes.sums[neuron][rule] if neuron < len(votes.sums) else 0
            found[("+" if rule == 0 else "-", label)] += value * used / raw[rule]
        reasons = ", ".join(f"{sign}{amount:.0f} {label}" for (sign, label), amount
                            in found.most_common(top) if amount >= 0.5)
        lines.append(f"attends {attended(frame)}: {reasons or 'no inputs firing'}")
    return lines


def cmd_why(path, start=0, end=None):
    """At each change of action or attention: why (explain_choice)."""
    if replay_version(path) < 10:
        raise SystemExit("why needs format 10 or later (votes)")
    end = end if end else None
    state = {"key": None}

    def on_frame(replay, frame):
        if frame.tick < start or (end is not None and frame.tick > end) or not frame.votes:
            return
        key = (frame.action, frame.attention_slot)
        if key == state["key"]:
            return
        state["key"] = key
        print(f"tick {frame.tick} feels: {', '.join(top_drives(frame_drives(frame))) or 'nothing'}")
        for line in explain_choice(replay, frame):
            print("   " + line)

    BrainReplay(path).run(on_frame=on_frame)


def cmd_goals(path):
    """The goal-direction matrix: per attention category, the drives it has
    relieved (weight > 0), when each first appeared, and its final weight."""
    if replay_version(path) < 10:
        raise SystemExit("goals needs format 10 or later")
    first, final = {}, ()
    for _, frame in recording(path):
        if not frame.goal_matrix:
            continue
        final = frame.goal_matrix
        for index, value in enumerate(final):
            if value > 0 and index not in first:
                first[index] = frame.tick
    rows = collections.defaultdict(list)
    for index, value in enumerate(final):
        if value > 0:
            rows[index // 16].append((value, DRIVES[index % 16] if index % 16 < len(DRIVES)
                                      else f"drive {index % 16}", first.get(index)))
    for row in sorted(rows, key=lambda r: -max(v for v, _, _ in rows[r])):
        items = sorted(rows[row], reverse=True)
        print(f"{category_name(row):16s} relieves " + ", ".join(
            f"{drive} {value} (from tick {tick})" for value, drive, tick in items))


def cmd_words(path):
    if replay_version(path) < 10:
        raise SystemExit("words needs format 10 or later")
    last = {}
    for _, frame in recording(path):
        for index, word in frame.words.items():
            if last.get(index) != word:
                if index in last:
                    heard, said, strength = word
                    print(f"tick {frame.tick}: word {index} '{heard}' -> says '{said}' "
                          f"(reinforcement {strength})")
                last[index] = word
    print("at the end: " + "; ".join(f"'{h}' -> '{s}' {r}" for h, s, r in
                                    sorted(last.values(), key=lambda w: -w[2])[:20] if h))


def cmd_instincts(path):
    """Each instinct dreamed: the three inputs it replays, the action it
    links them to and the chemical it rewards or punishes with; then the
    decision links its dream events changed, by meaning."""
    if replay_version(path) < 10:
        raise SystemExit("instincts needs format 10 or later")
    import chemical_provenance
    if CHEMICALS_FILE and not chemical_provenance.CHEMICAL_NAMES:
        chemical_provenance.load_chemical_names(CHEMICALS_FILE)
    described = {}
    changed = collections.defaultdict(collections.Counter)

    def on_frame(replay, frame):
        for instinct in frame.instincts:
            # Lobe 0 marks an unused input (Instinct::run_dream_step).
            inputs = [f"{replay.layout.lobes[lobe].name} {item_name(lobe, neuron)}"
                      for lobe, neuron in instinct.inputs
                      if 0 < lobe < len(replay.layout.lobes)]
            described[instinct.number] = (
                f"{' & '.join(inputs)} -> {action_name(instinct.decision_neuron)} with "
                f"{chemical_provenance.chemical_name(instinct.chemical)} {instinct.concentration}")

    def on_decision(replay, frame, event, meaning):
        if event.instinct is not None and event.kind not in ("reset",):
            _, neuron, rule, _ = event.key
            sign = "+" if rule == 0 else "-"
            changed[event.instinct][
                f"{event.kind} {sign}{action_name(neuron)} when [{describe_meaning(meaning)}]"] += 1

    BrainReplay(path).run(on_decision=on_decision, on_frame=on_frame)
    for number in sorted(described):
        print(f"instinct {number}: {described[number]}")
        for link, count in changed[number].most_common(4):
            print(f"      {link} x{count}")
        if not changed[number]:
            print("      changed no decision link")


def traced_causes(path, top):
    """Format 9: credit each decision reinforcement to the root origins of
    the chemical behind it (type 0's chem 0, type 1's chem 1), as the
    recorded chemical flows trace them."""
    import chemical_provenance
    from chemical_provenance import Provenance, brain_inputs, describe_mix
    if CHEMICALS_FILE and not chemical_provenance.CHEMICAL_NAMES:
        chemical_provenance.load_chemical_names(CHEMICALS_FILE)
    provenance = Provenance()
    state = {"tick": None, "inputs": {}}
    credit = collections.defaultdict(collections.Counter)  # origin -> link -> share
    weight = collections.Counter()                          # origin -> reinforcements
    onsets = collections.defaultdict(collections.Counter)   # which -> origin -> share
    current = {"reward": False, "punishment": False}

    def advance(frame):
        if state["tick"] != frame.tick:
            provenance.update(frame)
            if frame.chemistry:
                state["inputs"] = brain_inputs(frame.chemistry)
            state["tick"] = frame.tick

    def on_decision(replay, frame, event, meaning):
        advance(frame)
        if event.kind != "reinforce" or event.dreaming:
            return
        _, neuron, rule, _ = event.key
        chemical = state["inputs"].get((DECISION, rule))
        if chemical is None:
            return
        sign = "+" if rule == 0 else "-"
        link = f"{sign}{action_name(neuron)} when [{describe_meaning(meaning)}]"
        for origin, share in provenance.mix(chemical).items():
            credit[origin][link] += share
            weight[origin] += share

    def on_frame(replay, frame):
        advance(frame)
        for which, slot in (("reward", 0), ("punishment", 1)):
            level = frame.lobe_chemicals.get((DECISION, slot), 0)
            chemical = state["inputs"].get((DECISION, slot))
            if level and not current[which] and chemical is not None:
                for origin, share in provenance.mix(chemical).items():
                    onsets[which][origin] += share
            current[which] = bool(level)

    replay = BrainReplay(path)
    replay.run(on_decision=on_decision, on_frame=on_frame)
    print(f"ticks {replay.first_tick}-{replay.last_tick}")
    for which in ("reward", "punishment"):
        total = sum(onsets[which].values())
        if total:
            print(f"\n{which} onsets: {total:.0f}; made from: "
                  + describe_mix({o: v / total for o, v in onsets[which].most_common()}, 5))
    total = sum(weight.values())
    print(f"\n{total:.0f} decision reinforcements, credited to where their chemical came from:")
    for origin, value in weight.most_common(top):
        print(f"\n{value * 100 / total:5.1f}%  {origin}")
        for link, share in credit[origin].most_common(4):
            print(f"         taught {link} ({share:.1f})")


def replay_version(path):
    for layout, _ in read_recording(path):
        return layout.version
    return 0


if __name__ == "__main__":
    signal.signal(signal.SIGPIPE, signal.SIG_DFL)
    commands = {"perception": cmd_perception, "concepts": cmd_concepts,
                "rules": cmd_rules, "timeline": cmd_timeline,
                "learning": cmd_learning, "meanings": cmd_meanings,
                "beliefs": cmd_beliefs, "teaching": cmd_teaching,
                "stream": cmd_stream, "causes": cmd_causes, "why": cmd_why,
                "goals": cmd_goals, "words": cmd_words, "instincts": cmd_instincts}
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
