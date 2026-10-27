# /// script
# requires-python = ">=3.12"
# dependencies = []
# ///
"""Generate a deterministic synthetic screenplay in Fountain for workload tests.

The text is nonsense assembled from an original word list, so the output can be
committed or shared freely. The element mix approximates a dialogue-heavy feature
and includes every element the export profile covers. Same arguments, same bytes.

    uv run tools/generate_script.py --scenes 300 --seed 1 > fixtures/generated/feature-200.fountain
"""

import argparse
import random

WORDS = (
    "harbor lantern ledger window copper station quiet orchard engine signal letter "
    "ladder corridor shadow morning winter river paper motor glass basement island "
    "bridge kitchen market radio hallway garden ticket rooftop pocket mirror stair "
    "carefully slowly suddenly almost never always again still only nearly perhaps "
    "opens closes lifts drops turns waits listens watches answers follows carries "
    "steady narrow distant heavy bright broken empty careful patient restless "
    "the a his her their every another this that some no one".split()
)
NAMES = ["MARA", "THEO", "JUNE", "ODELL", "PRIYA", "WALT", "INES", "BASIL"]
PLACES = ["LIGHTHOUSE", "FERRY TERMINAL", "BOARDING HOUSE", "RADIO SHACK", "CANNERY", "CHAPEL", "SEAWALL"]
TIMES = ["DAY", "NIGHT", "DAWN", "DUSK", "CONTINUOUS", "LATER"]
TRANSITIONS = ["CUT TO:", "DISSOLVE TO:", "SMASH CUT TO:", "> FADE OUT."]
EXTENSIONS = ["V.O.", "O.S."]
PARENTHETICALS = ["quietly", "beat", "not looking up", "into the radio", "to Mara", "laughing despite herself"]


def sentence(rng, low, high, emphasis=0.0):
    words = [rng.choice(WORDS) for _ in range(rng.randint(low, high))]
    if rng.random() < emphasis:
        i = rng.randrange(len(words))
        marker = rng.choice(["*", "**", "_"])
        words[i] = f"{marker}{words[i]}{marker}"
    text = " ".join(words)
    return text[0].upper() + text[1:] + rng.choice([".", ".", ".", "?", "!"])


def paragraph(rng, sentences, emphasis=0.0):
    return " ".join(sentence(rng, 4, 14, emphasis) for _ in range(sentences))


def speech(rng, name, *, dual=False):
    cue = name
    if rng.random() < 0.08:
        cue += f" ({rng.choice(EXTENSIONS)})"
    lines = [cue + (" ^" if dual else "")]
    if rng.random() < 0.15:
        lines.append(f"({rng.choice(PARENTHETICALS)})")
    lines.append(paragraph(rng, rng.randint(1, 3 if dual else 4), emphasis=0.05))
    if not dual and rng.random() < 0.05:
        lines.append(f"({rng.choice(PARENTHETICALS)})")
        lines.append(paragraph(rng, rng.randint(1, 2)))
    return lines


def scene(rng, number):
    out = [f"{rng.choice(['INT.', 'EXT.'])} {rng.choice(PLACES)} - {rng.choice(TIMES)}", ""]
    out += [paragraph(rng, rng.randint(1, 4), emphasis=0.03), ""]
    cast = rng.sample(NAMES, rng.randint(2, 3))
    for _ in range(rng.randint(4, 12)):
        roll = rng.random()
        if roll < 0.02:
            first, second = rng.sample(cast, 2)
            out += speech(rng, first) + [""] + speech(rng, second, dual=True) + [""]
        elif roll < 0.18:
            out += [paragraph(rng, rng.randint(1, 3)), ""]
        elif roll < 0.19:
            out += [f"~{sentence(rng, 4, 8)}", ""]
        else:
            out += speech(rng, rng.choice(cast)) + [""]
    if rng.random() < 0.01:
        out += [f"> {rng.choice(['INTERMISSION', 'END OF PART ONE'])} <", ""]
    if rng.random() < 0.10:
        out += [rng.choice(TRANSITIONS), ""]
    if number % 97 == 96:
        out += ["===", ""]
    return out


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--scenes", type=int, required=True)
    parser.add_argument("--seed", type=int, default=1)
    args = parser.parse_args()
    rng = random.Random(args.seed)
    out = [
        f"Title: Synthetic Workload {args.scenes}",
        "Credit: Written by",
        "Author: Apollo Generator",
        f"Draft date: seed {args.seed}",
        "",
        "!FADE IN:",
        "",
    ]
    for number in range(args.scenes):
        out += scene(rng, number)
    print("\n".join(out).rstrip() + "\n")


if __name__ == "__main__":
    main()
