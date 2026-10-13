#!/usr/bin/env python3
"""Read the brain-related genes of a C1 genome (.gen).

    genome_brain.py lobes GENOME [ALLCHEMICALS.STR]    each lobe gene: size, what
                                                    copies to perception, its
                                                    state rule and both dendrite
                                                    types' rules, readably
    genome_brain.py senses GENOME [ALLCHEMICALS.STR]   each stimulus gene: which
                                                    general sense neuron it
                                                    fires and what chemicals
                                                    it gives

As a module: general_sense_names(path) and lobe_rules(path) for decoders.

Layouts follow LibreCreatures' loaders: Lobe::load_genome and
DendriteType::load_from_genome (src/c1/brain), the stimulus genes in
Creature's genome load (creature.cpp), receptor genes in Biochemistry.  A
gene is "gene", family, subtype, sequence, load pass, switch-on stage,
flags (10 bytes), then its payload up to the next "gene" or "gend".  Only
genes that switch on at stage 0 (birth) are read.
"""
import os
import struct
import sys

STANDARD_LOBES = ["perception", "drive", "stimulus source", "verb", "noun",
                  "general sense", "decision", "attention", "concept"]

# SVRule tokens by their official names, as the C1 Genetics Kit shows them
# (LibreCreatures SVRuleToken, src/c1/brain/rules.hpp): 1..21 operands,
# 22..27 operators.
REGISTERS = {1: "0", 2: "1", 3: "64", 4: "255", 5: "chem0", 6: "chem1",
             7: "chem2", 8: "chem3", 9: "state", 10: "output", 11: "thres",
             12: "type0", 13: "type1", 14: "anded0", 15: "anded1",
             16: "input", 17: "conduct", 18: "suscept", 19: "STW",
             20: "LTW", 21: "strength"}
OPERATORS = {22: "TRUE", 23: "PLUS", 24: "MINUS", 25: "TIMES", 26: "INCR", 27: "DECR"}

# The CAOS manual's Stimulus table: the built-in stimulus numbers.
STIMULI = ["disappointment", "pointer pats me", "creature pats me",
           "pointer slaps me", "creature slaps me", "it is approaching",
           "it is retreating", "I bump into wall", "object comes into view",
           "unrecognised word", "heard user speak", "heard creature speak",
           "I am quiescent", "I've activated 1", "I've activated 2",
           "I've deactivated", "I am approaching", "I have retreated",
           "I have got", "I have dropped", "I've stated need", "I am resting",
           "I am sleeping", "I am travelling", "laying", "involuntary action",
           "travelling (old)", "I have retreated (2)"] + [
           f"involuntary action {i}" for i in range(8)]


def genes(source):
    """Yield (family, subtype, stage, payload, load_pass, flags) for every
    gene.  `source` is a .gen path or the file's bytes.  Family is taken
    modulo 3, as the game does."""
    data = source if isinstance(source, (bytes, bytearray)) else open(source, "rb").read()
    starts = []
    position = data.find(b"gene")
    end = data.find(b"gend")
    while position != -1 and (end == -1 or position < end):
        starts.append(position)
        position = data.find(b"gene", position + 4)
    for index, start in enumerate(starts):
        stop = starts[index + 1] if index + 1 < len(starts) else (end if end != -1 else len(data))
        header = data[start:start + 10]
        if len(header) < 10:
            break
        yield header[4] % 3, header[5], header[8], data[start + 10:stop], header[7], header[9]


def applies(flags, sex):
    """Whether a gene applies to a creature of `sex` (1 male, 2 female;
    None: either).  Genome flags 0x08 male, 0x10 female, neither: both."""
    if sex is None or not flags & 0x18:
        return True
    return bool((flags & 0x08 and sex == 1) or (flags & 0x10 and sex == 2))


def read_chemical_names(path):
    """allchemicals.str: u16 count, then one MFC string per chemical number
    (u8 length, or 0xFF and a u16 length).  The game's chemicals.str is a
    different, shorter list; chemical numbers index allchemicals.str."""
    if not path or not os.path.exists(path):
        return {}
    data = open(path, "rb").read()
    count = struct.unpack_from("<H", data, 0)[0]
    names, position = {}, 2
    for number in range(count):
        length = data[position]
        position += 1
        if length == 0xFF:
            length = struct.unpack_from("<H", data, position)[0]
            position += 2
        names[number] = data[position:position + length].decode("latin-1")
        position += length
    return names


def expression(tokens):
    """An 8-byte rule as text, e.g. "type 0 - type 1"."""
    words = []
    for raw in tokens:
        token = raw % 30 if raw > 29 else raw
        if token == 0:
            break
        if token in OPERATORS:
            words.append(OPERATORS[token])
        else:
            words.append(REGISTERS.get(token, f"r{token}"))
    return " ".join(words) or "(none)"


def parse_rule(payload, offset):
    fields = list(payload[offset:offset + 15])
    rule = {
        "source lobe": fields[0], "dendrites": (fields[1], fields[2]),
        "LTW range": (fields[5], fields[6]), "strength range": (fields[7], fields[8]),
        "mode": fields[9], "susceptibility decay": fields[10],
        "STW relax": fields[11], "LTW step every": fields[12],
        "growth every": fields[13],
    }
    offset += 14
    rule["growth rule"] = expression(payload[offset:offset + 8]); offset += 8
    rule["decay every"] = payload[offset]; offset += 1
    rule["decay rule"] = expression(payload[offset:offset + 8]); offset += 8
    rule["susceptibility rule"] = expression(payload[offset:offset + 8]); offset += 8
    rule["reinforcement rule"] = expression(payload[offset:offset + 8]); offset += 8
    return rule, offset


def lobe_rules(source, sex=None):
    """The lobes as the game builds them at birth (Brain::load_genome): every
    stage-0 brain-family gene that applies to the sex, any subtype, those
    with load pass 0 first and then the rest, numbered in that order."""
    found = [(load_pass, payload) for family, subtype, stage, payload, load_pass, flags
             in genes(source) if family == 0 and stage == 0 and applies(flags, sex)]
    ordered = [p for lp, p in found if lp == 0] + [p for lp, p in found if lp != 0]
    lobes = []
    for payload in ordered:
        lobe = {"grid": tuple(payload[0:4]), "perception copy": payload[4] % 3,
                "threshold": payload[5], "relax": payload[6], "baseline": payload[7],
                "input gain": payload[8], "state rule": expression(payload[9:17]),
                "winner takes all": payload[17], "types": []}
        offset = 18
        for _ in range(2):
            rule, offset = parse_rule(payload, offset)
            lobe["types"].append(rule)
        lobes.append(lobe)
    return lobes


def brain_receptors(path, max_stage=0, sex=None):
    """(lobe, locus, chemical) for each receptor on a brain lobe switched on
    by life stage `max_stage`."""
    found = []
    for family, subtype, stage, payload, _, flags in genes(path):
        if (family != 1 or subtype % 5 != 0 or stage > max_stage or len(payload) < 4
                or not applies(flags, sex)):
            continue
        domain = payload[0] if payload[0] < 2 else payload[0] & 1
        if domain == 0:
            found.append((payload[1], payload[2], payload[3]))
    return found


def stimulus_genes(path, max_stage=0, sex=None):
    """Each stimulus gene switched on by life stage `max_stage`: number,
    sense neuron, chemicals.  A later gene for the same stimulus replaces
    an earlier one, as the game reloads them."""
    found = {}
    for family, subtype, stage, payload, _, flags in genes(path):
        if (family != 2 or subtype % 7 != 0 or stage > max_stage or len(payload) < 13
                or not applies(flags, sex)):
            continue
        number = payload[0] % 36
        chemicals = [(payload[5 + 2 * i], payload[6 + 2 * i]) for i in range(4)]
        found[number] = {"stimulus": number, "significance": payload[1],
                          "sense neuron": payload[2], "intensity": payload[3],
                          "flags": payload[4], "chemicals": chemicals}
    return list(found.values())


def general_sense_names(path, max_stage=0, sex=None):
    """General sense neuron -> the stimuli that fire it, by name."""
    names = {}
    for gene in stimulus_genes(path, max_stage, sex):
        label = STIMULI[gene["stimulus"]] if gene["stimulus"] < len(STIMULI) else f"stimulus {gene['stimulus']}"
        names.setdefault(gene["sense neuron"], []).append(label)
    return {neuron: " / ".join(labels) for neuron, labels in names.items()}


def chemical_inputs(path, chemicals=None, max_stage=0, sex=None):
    """(lobe, chem slot 0..3) -> chemical name, from receptors on loci 13..16."""
    chemicals = chemicals or {}
    found = {}
    for lobe, locus, chemical in brain_receptors(path, max_stage, sex):
        if 13 <= locus <= 16:
            found[(lobe, locus - 13)] = chemicals.get(chemical, f"chemical {chemical}")
    return found


def cmd_lobes(path, chemicals_path=None):
    chemicals = read_chemical_names(chemicals_path)
    inputs = chemical_inputs(path, chemicals)
    for index, lobe in enumerate(lobe_rules(path)):
        name = STANDARD_LOBES[index] if index < len(STANDARD_LOBES) else f"lobe {index}"
        x, y, w, h = lobe["grid"]
        chems = ", ".join(f"chem {slot} = {inputs[(index, slot)]}"
                          for slot in range(4) if (index, slot) in inputs)
        print(f"lobe {index} {name}: {w}x{h} at {x},{y}; perception copy {lobe['perception copy']}; "
              f"threshold {lobe['threshold']}, input gain {lobe['input gain']}")
        print(f"   state rule: {lobe['state rule']}")
        if chems:
            print(f"   chemical inputs: {chems}")
        for t, rule in enumerate(lobe["types"]):
            if rule["dendrites"] == (0, 0):
                continue
            src = rule["source lobe"]
            src_name = STANDARD_LOBES[src] if src < len(STANDARD_LOBES) else f"lobe {src}"
            print(f"   type {t}: from {src_name}, {rule['dendrites'][0]}-{rule['dendrites'][1]} "
                  f"dendrites, LTW {rule['LTW range']}, strength {rule['strength range']}")
            print(f"      suscept svrule:   {rule['susceptibility rule']}  (relax suscept "
                  f"{rule['susceptibility decay']}, relax STW {rule['STW relax']})")
            print(f"      reinforce svrule: {rule['reinforcement rule']}")
            print(f"      gain svrule: {rule['growth rule']} (gain rate {rule['growth every']}); "
                  f"lose svrule: {rule['decay rule']} (lose rate {rule['decay every']}); "
                  f"LTW gain rate {rule['LTW step every']}; migration {rule['mode']}")


def cmd_senses(path, chemicals_path=None):
    chemicals = read_chemical_names(chemicals_path)
    for gene in sorted(stimulus_genes(path), key=lambda g: (g["sense neuron"], g["stimulus"])):
        label = STIMULI[gene["stimulus"]] if gene["stimulus"] < len(STIMULI) else f"stimulus {gene['stimulus']}"
        chems = ", ".join(f"{chemicals.get(c, c)} {a}" for c, a in gene["chemicals"] if c)
        print(f"sense {gene['sense neuron']:3d}  <- stimulus {gene['stimulus']:2d} {label:24s} {chems}")


if __name__ == "__main__":
    commands = {"lobes": cmd_lobes, "senses": cmd_senses}
    if len(sys.argv) < 3 or sys.argv[1] not in commands:
        print(__doc__)
        sys.exit(2)
    commands[sys.argv[1]](*sys.argv[2:4])
