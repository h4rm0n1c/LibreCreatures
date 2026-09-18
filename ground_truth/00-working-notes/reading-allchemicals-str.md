# Reading `allchemicals.str` (and why chemical names were wrong twice)

`allchemicals.str` in the install root is a plain MFC string table:

    count: u16                  (256)
    entries[count]:
        length: u8              (0xff then a u16 for a long string)
        bytes:  length          latin-1

Entry `i` is chemical `i`, so index 0 is `<NONE>`. A correct parse consumes the
file exactly -- 1973 of 1973 bytes for the shipped copy. If a parse leaves
trailing bytes, the names are shifted and every lookup after the first long
string is wrong.

    python3 - allchemicals.str <<'PY'
    import sys, struct
    d = open(sys.argv[1], 'rb').read()
    n = struct.unpack_from('<H', d, 0)[0]; p = 2; names = []
    for _ in range(n):
        l = d[p]; p += 1
        if l == 255:
            l = struct.unpack_from('<H', d, p)[0]; p += 2
        names.append(d[p:p+l].decode('latin1')); p += l
    assert p == len(d), (p, len(d))
    PY

`re_work/tools/parse_gene.py`'s `chem_name()` already agrees with this.

Names that have been misread in this project's notes:

    52  ConASH            (was written up as "Antibody 3")
    53  DecASH1
    243 Antibody 3        -- the real one; 240..244 are Antibody 0..4

Landmarks worth knowing by number: 1 Pain, 2 Need for Pleasure, 3 Hunger,
4 Coldness, 5 Hotness, 6 Tiredness, 7 Sleepiness, 8 Loneliness, 9 Crowded,
10 Fear, 11 Boredom, 12 Anger, 13 Sex Drive; 35 Hunger Decrease (Saccharin),
49 Reward, 50 Punishment, 51 Reinforcement, 54/55 Reward/Punish Echo,
56 Ageing, 57 Starch, 58 Glucose, 59 Glycogen, 69 Adrenaline.

Related: [norns-do-not-feed-themselves.md](norns-do-not-feed-themselves.md).
