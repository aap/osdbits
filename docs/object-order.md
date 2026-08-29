# object order of the opening module

What the binary says about the original translation-unit structure of
the opening (0x211d30..0x21c910), from a scan of every function's
gp-relative data references (2026-08-28).

## Method

For each function (bounds from the IDB), collect every gp-relative
access and split it into three classes:

- **lit** — the float literal pool (~0x2a7040..0x2a7280; the opening's entries start at 0x2a7088): every float
  constant a function loads gp-relative.
- **sd** — initialized scalar data, `.sdata` (0x2a7700..0x2a7a00).
- **sb** — zero-initialized scalars, `.sbss` (0x2a7f50..).

## Findings

1. **The float literal pool is strictly monotone in function order
   across the whole module** — every function's literals sit directly
   after the previous function's, no sharing, no resets.  Literals are
   NOT deduplicated at any level: DrawIllegalFog alone references two
   separate copies of 6.2831855 (0x2a70a4 and 0x2a70ac) for its two
   wrap loops, and pi exists at least four times in the pool (fog,
   anim, flare, fades).  So float constants are per-use-site `.lit4`
   entries, emitted in code order.
2. `.sdata` and `.sbss` are likewise allocated in first-use order
   along the code (7700-7714 main loop vars, 7720s DMA, 7730s double
   buffer, 7750s SPR alloc, 7788 text step, 7794 anim go-flag, ...,
   7808 the cube callback pointer; same monotone story in `.sbss`).
3. **There is no padding anywhere inside the module**: every
   inter-function gap is 0 or 4 bytes (8-byte function alignment).
   The first real gaps (36/188/400 bytes) appear only after
   sub_21c7a8 (0x21c8ec), where the next module (makeThreadU etc.)
   begins.

## Consequence

Within the opening module, the binary fully determines the ORDER of
functions and of data declarations - and determines nothing else.
Translation-unit boundaries leave no observable trace: no section
alignment padding, no literal-pool dedup domains, no `.sdata`
clustering beyond code order.  A single giant opening.c and any
sensible multi-file split produce the identical image, as long as

- concatenated function order equals binary order, and
- each datum is declared before/near its first-using function so the
  `.lit4`/`.sdata`/`.sbss` emission order matches.

So the source split is OUR choice.  Natural seams (also the matching/
workspace TU assignments, roughly):

    main.c    0x211d30  OpeningThread, InitOpening, ProcessOpening,
                        DrawEnd, main loop, type helpers, post-loop
                        CDVD normalize, fixed-point sin/cos helpers
    gs.c      0x212258  render/texture/DMA init, pkt/vif1 helpers,
                        SPR alloc, GS alloc, texture upload
    blit.c    0x214050  extra-buffer blits, feedback blur, black bars,
                        fade rect, SCE + illegal text, DoText,
                        initTextShit
    fog.c     0x214f78  InitFog, DrawFog, illegal fog init + draw
    anim.c    0x215f18  camera state machine + integration
    cube.c    0x2166c8  DrawLights, colour/ST callback, DrawCube,
                        InitLightsCubes, DrawLightsAndCubes
    towers.c  0x217e30  height grid, tower chain patchers, DrawTowers,
                        trail functions
    scene.c   0x218c88  InitOpeningScene, DrawOpeningScene,
                        InitTowersFog, DoOpening
    illegal.c 0x219748  illegal cube callback + draw + placement,
                        flare subsystem, fades, illegal scene
    cubedraw. 0x21b798  cube UV builders, 24/32 texture helper,
                        CubeTextureFuckery, DrawTexturedQuad, the
                        screen capture

The module ends at 0x21c910; whatever follows is a separate part of
the OSD and a separate ordering problem.
