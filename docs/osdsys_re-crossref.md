# osdsys_re symbol cross-reference

[osdsys_re](https://github.com/ps2re/osdsys_re) (uyjulian et al.) is a
splat-based project on **HDDOSD 1.10U** (unpacked from SUDC4) - symbol
database only, no decompiled source yet.  In the opening module its
addresses sit at a constant **+0x8f30** above our retail image, verified
on several anchors (OpeningDrawOpeningScene 0x221cb0 <-> 0x218d80,
OpeningInitTowersFog 0x221d30 <-> 0x218e00, InitLightsCubes 0x2209e0 <->
0x217ab0).  The two binaries differ, so treat the computed retail
addresses below as candidates, not facts - a few interior helpers are
known to be off (their vif1SetCLAMP_1 maps to 0x212b70 where our image
has it at 0x212bd0).

Notable names for functions we have not traced yet:

- `OpeningInitIllegalScene` = retail ~0x21b570 (size 0xd8)
- `OpeningDrawIllegalScene` = retail ~0x21b648 (size 0x48 - tiny, so it
  must delegate to the shared opening draw code)
- `OpeningProcessInner`     = retail ~0x215fd0 (size 0x6f4) - the big
  state switch our Process() stubs as a TODO
- `opening_transition_to_clock` = retail ~0x211fb0, and the whole clock
  module after 0x21c940

Opening-module function list (HDDOSD address, name, computed retail):

    0x0021aaa8 module_opening_getdesc                   ~0x211b78
    0x0021aab8 module_opening_getversion                ~0x211b88
    0x0021aac8 module_opening_prepare                   ~0x211b98
    0x0021ab38 module_opening_setup                     ~0x211c08
    0x0021ab88 module_dummy_setup                       ~0x211c58
    0x0021abd0 module_opening_thread_proc               ~0x211ca0
    0x0021ac50 OpeningInit                              ~0x211d20
    0x0021acd8 OpeningProcess                           ~0x211da8
    0x0021ad58 OpeningDrawEnd                           ~0x211e28
    0x0021ada0 OpeningDoOpeningIllegal                  ~0x211e70
    0x0021ae30 opening_thread_set_vars                  ~0x211f00
    0x0021ae78 opening_thread_set_vars_2                ~0x211f48
    0x0021aee0 opening_transition_to_clock              ~0x211fb0
    0x0021b128 OpeningInitRender                        ~0x2121f8
    0x0021b1a8 gsAllocExtraBuffers                      ~0x212278
    0x0021b208 OpeningInitTextures                      ~0x2122d8
    0x0021b2f8 InitDMA                                  ~0x2123c8
    0x0021b360 pktSetFlatRect                           ~0x212430
    0x0021b4c8 pktSetSCISSOR_1                          ~0x212598
    0x0021b528 pktSetCLAMP_1                            ~0x2125f8
    0x0021b570 InitDoubleBuffer                         ~0x212640
    0x0021b5d8 vif1Pad                                  ~0x2126a8
    0x0021b678 vif1Begin                                ~0x212748
    0x0021b6f0 vif1End                                  ~0x2127c0
    0x0021b768 pktSetAD                                 ~0x212838
    0x0021b7a0 pktSetTexRect                            ~0x212870
    0x0021b970 pktSetTEST_1                             ~0x212a40
    0x0021b9c8 pktSetAlphaBlend                         ~0x212a98
    0x0021ba60 vif1SetAD                                ~0x212b30
    0x0021baa0 vif1SetCLAMP_1                           ~0x212b70
    0x0021bb20 vif1SetTEST_1                            ~0x212bf0
    0x0021bbc0 vif1SetAlphaBlend                        ~0x212c90
    0x0021bc10 vif1SetSCISSOR_1                         ~0x212ce0
    0x0021bc40 vif1SetTexRect                           ~0x212d10
    0x0021bcb0 vif1SetFlatRect                          ~0x212d80
    0x0021bd10 vif1SetFramebuffer                       ~0x212de0
    0x0021be48 sprAlloc                                 ~0x212f18
    0x0021bed0 sprGetFreeSize                           ~0x212fa0
    0x0021bee8 sprInitAlloc                             ~0x212fb8
    0x0021bf30 sprAllocChains                           ~0x213000
    0x0021bfa8 sprGetChainBuffer                        ~0x213078
    0x0021bfd0 sprInitChains                            ~0x2130a0
    0x0021bfe0 sprSetBasePtr                            ~0x2130b0
    0x0021bff0 sprInit                                  ~0x2130c0
    0x0021c010 InitSPR                                  ~0x2130e0
    0x0021c068 sprTransformVertex                       ~0x213138
    0x0021c108 psmToBppEE                               ~0x2131d8
    0x0021c160 psmToBppGS                               ~0x213230
    0x0021c1b0 OpeningUploadImage                       ~0x213280
    0x0021c360 GetTexExponent                           ~0x213430
    0x0021c3a8 gsAllocBuffer                            ~0x213478
    0x0021c440 OpeningInitTexture                       ~0x213510
    0x0021cb68 vif1SetTextureMIP                        ~0x213c38
    0x0021cda8 vif1SetTexture                           ~0x213e78
    0x0021cdb8 gsInitAlloc                              ~0x213e88
    0x0021cdf0 vif1SetZTest                             ~0x213ec0
    0x0021ce38 vif1SetZWrite                            ~0x213f08
    0x0021ced8 vif1SetXYOffset                          ~0x213fa8
    0x0021de88 OpeningDoText                            ~0x214f58
    0x0021e168 OpeningDrawFog                           ~0x215238
    0x0021ee48 OpeningInitAnimation                     ~0x215f18
    0x0021ef00 OpeningProcessInner                      ~0x215fd0
    0x0021f5f8 OpeningDrawLights                        ~0x2166c8
    0x002209e0 InitLightsCubes                          ~0x217ab0
    0x00220ce8 OpeningDrawLightsAndCubes                ~0x217db8
    0x00221bb8 OpeningInitOpeningScene                  ~0x218c88
    0x00221cb0 OpeningDrawOpeningScene                  ~0x218d80
    0x00221d30 OpeningInitTowersFog                     ~0x218e00
    0x00222180 OpeningDoOpening                         ~0x219250
    0x00222e70 OpeningDoIllegalDisc                     ~0x219f40
    0x002244a0 OpeningInitIllegalScene                  ~0x21b570
    0x00224578 OpeningDrawIllegalScene                  ~0x21b648
    0x00225728 module_opening_225728                    ~0x21c7f8
    0x00225870 module_clock_getdesc                     ~0x21c940
    0x00225880 module_clock_getversion                  ~0x21c950
    0x00225890 module_clock_prepare                     ~0x21c960
    0x00225900 module_clock_setup                       ~0x21c9d0
    0x00225950 clock_stuff1                             ~0x21ca20
    0x002259b8 clock_input_check_handler_p6_p7_tgt      ~0x21ca88
    0x00225d30 module_clock_thread_proc                 ~0x21ce00
    0x00225d98 module_clock_225D98                      ~0x21ce68
    0x00225db0 module_clock_init_resources              ~0x21ce80
    0x00225e80 clock_orb_rendering_func                 ~0x21cf50
    0x00225f38 module_clock_225F38                      ~0x21d008
    0x00226000 module_clock_226000                      ~0x21d0d0
