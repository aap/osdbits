# automated variant search (the "campaign" toolkit)

Grew out of an experiment wiring ~/othersrc/n64-decomp-workbench's
sweep generators to ee-gcc (2026-08-29).  Verdict: the workbench's own
generator space (all within-statement edits) closed exactly ZERO
register-allocation ties over 17.6k variants; what wins ties in gcc
2.9 is STATEMENT PLACEMENT and declaration facts, so the keeper is our
own 90-line statement permuter.  Scoreboard from the first run:
ProcessOpeningAnimation 446/446 (found automatically: `rotation +=`
belongs AFTER the position[] integrations - 101 builds, 1.3s),
OpeningThread 32/32 (missing InitOpening prototype - found by reading
implicit-declaration warnings, no mutator has that move), sub_215798
149->156 (the countdown IS gcc loop reversal of an upward loop, and
illegalFogState is indexed [k], not walked).

Tools (all score with check.py's exact criterion via score1.py;
EETOOLS must be /usr/local/sce/ee/gcc/bin - freesce objcopy has no -j):

    score1.py    check.py's comparison as an importable library
    stmtperm.py  SRC.c LO..HI FUNCTAB [--full] [--jobs N]
                 relocate/permute whole top-level statements in a
                 line range - THE tie-breaker that works
    search.py    beam search over the workbench generator families
                 (needs PYTHONPATH=~/othersrc/n64-decomp-workbench/src)
    runall.py    compile+rank a directory of variants (~10ms/variant)
    gen_all.sh   dump every workbench CLI generator's variants

Copy the TU under test into this directory (a *-functions.txt per
target keeps runs fast); never point the tools at ../src while another
agent owns the file.
