/* OSDSYS menu rendering + localization symbols (PRELIMINARY)
 * From the 2026-08-22 reversing session.
 * See docs/menu-rendering-reverse.md in the osdsys repo.
 *
 * Confidence tiers:
 *   [ok]  semantics verified from the disassembly
 *   [tnt] tentative - role plausible but not fully traced
 *
 * Apply: File > Script file... (IDA 7.x)
 * Names are descriptive, not official; rename freely.
 */

#include <idc.idc>

static main()
{
	/* ================= string/localization API ================= */

	MakeName(0x002037B8, "osdGetLanguage");			// [ok] returns the current language index
	MakeName(0x00204170, "osdSelectStringTable");		// [ok] caches osdLanguageTables[lang] into osdCurStringTable
	MakeName(0x002041B8, "osdGetString");			// [ok] returns osdCurStringTable[id]

	/* ================= menu packet layer (0x240xxx) ================= */

	MakeName(0x00240038, "menuPktFlagReset");		// [ok] clears gp-29672 flag
	MakeName(0x00240040, "menuPacketInit");			// [tnt] calls 0x23bff0, sets flags
	MakeName(0x00240068, "menuBrowserState");		// [ok] disc-state machine (0x1f0000b0 states 5/8)
	MakeName(0x00240178, "menuBrowserInput");		// [ok] pad-code 0x808f watcher, 121-frame counter
	MakeName(0x002403C8, "menuPktOpen");			// [ok] ctx 0x295AD0+idx*32, scratch 0x70000000+idx*0x2000
	MakeName(0x00240438, "menuPktKick");			// [ok] sync + IRQ bit on head tag + sceDmaSend
	MakeName(0x002404C0, "menuPktClose");			// [tnt]
	MakeName(0x002407A8, "menuPktSync1");			// [tnt]
	MakeName(0x002409A0, "menuPktSync2");			// [tnt]
	MakeName(0x00240B48, "menuPktGsData");			// [tnt] sceVif1PkAddGsData x2
	MakeName(0x00240E68, "menuPktBuilder");			// [tnt]

	/* ================= menu element renderer ================= */

	MakeName(0x00245038, "menuDrawElement");		// [ok] desc 0x295D10 x352, GIF tag 0x295D00, data 0x295CD0
	MakeName(0x00245308, "menuElementClear");		// [tnt] clears element anim floats

	/* ================= menu texture uploader ================= */

	MakeName(0x00246D28, "menuUploadTexture");		// [ok] 28B descs 0x296160, CLUT+image
	MakeName(0x00246DE8, "menuUploadTextureLoadImage");	// [ok] file RPC 0x24dce0 + sceVif1PkRefLoadImage
	MakeName(0x00246F30, "menuUploadTexturePair");		// [tnt] two LoadImage uploads
	MakeName(0x00247070, "menuLoadTextures");		// [tnt] upload driver

	/* ================= menu screens (string consumers) ================= */

	MakeName(0x00224630, "menuScreenRenderer");		// [ok] 15x getString, 3x loadImage_Resource, widgets
	MakeName(0x0022AC20, "menuWidgetDraw");			// [tnt] per-item draw, 9x from menuScreenRenderer
	MakeName(0x0022AC48, "menuWidgetDraw2");		// [tnt] 3x from menuScreenRenderer
	MakeName(0x00238228, "menuScreenDraw1");		// [tnt] caller of menuDrawElement
	MakeName(0x002437A0, "menuScreenDraw2");		// [tnt] caller of menuDrawElement
	MakeName(0x00248370, "menuScreenDraw3");		// [tnt] caller of menuDrawElement
	MakeName(0x002051F8, "menuOptionsStrings");		// [tnt] Disc Speed / Diagnosis IDs directly

	/* ================= menu element/texture/string tables ================= */

	MakeName(0x0026ECC0, "osdLanguageTables");		// [ok] 9 ptrs: lang -> string table
	MakeName(0x0026EDE0, "osdCurStringTable");		// [ok] cached by osdSelectStringTable
	MakeName(0x002972A8, "osdStringsJP");			// [ok] Shift-JIS block
	MakeName(0x00298B08, "osdStringsEN");			// [ok] strings 0x299000-0x29a100
	MakeName(0x0029A2A4, "osdStringsFR");			// [ok] strings 0x29a5a0+
	MakeName(0x0029A0E8, "osdStringsTbl2");			// [tnt] 0x26ecc0[2]
	MakeName(0x0029B9D8, "osdStringsTbl3");			// [tnt] 0x26ecc0[6]
	MakeName(0x0029D1D0, "osdStringsTbl4");			// [tnt] 0x26ecc0[4]
	MakeName(0x0029EA88, "osdStringsTbl5");			// [tnt] 0x26ecc0[5]
	MakeName(0x002A0250, "osdStringsTbl6");			// [tnt] 0x26ecc0[7]
	MakeName(0x002A1B30, "osdStringsTbl7");			// [tnt] 0x26ecc0[3]
	MakeName(0x002A4380, "configTitleRecords");		// [tnt] 32B {flags,x,titleID,subID} per lang/state
	MakeName(0x00295AD0, "menuPktCtxs");			// [ok] 2 x 32B sceVif1Pk contexts
	MakeName(0x00295CD0, "menuElemGsData");			// [ok] 2 x 16B GS reg data templates
	MakeName(0x00295D00, "menuElemGifTag");			// [ok] GIF tag template qword
	MakeName(0x00295D10, "menuElements");			// [ok] 352B element descriptors
	MakeName(0x00296160, "menuTexDescs");			// [ok] 28B texture descriptors
	MakeName(0x00284100, "menuElemVertTable");		// [tnt] static verts, 2nd transform in menuDrawElement

	/* ================= menu globals (gp-relative, absolute addrs) ================= */

	MakeName(0x0027BA7C, "menuCurPacket");			// [ok] gp-29600: current VIF1 packet ctx ptr
	MakeName(0x0027BA54, "menuDmaChan");			// [ok] gp-29640: sceDmaChan* for the kick
	MakeName(0x0027BA80, "menuPktBufIdx");			// [ok] gp-29596: u8 double-buffer index
	MakeName(0x0027BA84, "menuInputState");			// [ok] gp-29592: pad state (0x808f checks)
	MakeName(0x0027BA24, "menuPktBusy");			// [ok] gp-29688: u8 in-flight flag
	MakeName(0x0027BA34, "menuFlag1");			// [ok] gp-29672
	MakeName(0x0027BA38, "menuCounter121");			// [ok] gp-29668: 0..121 counter
	MakeName(0x0027BA3C, "menuFlag27");			// [ok] gp-29664
	MakeName(0x0027BAE4, "menuElemIndex");			// [ok] gp-29496: menuDrawElement's descriptor index
	MakeName(0x0027BB00, "menuFlag2");			// [ok] gp-29468
	MakeName(0x0027BB4C, "menuFlag3");			// [ok] gp-29392
	MakeName(0x0027BDD0, "menuState");			// [ok] gp-28748: browser state machine state

	/* ================= comments on the key functions ================= */

	SetFunctionCmt(0x002403C8, "menuPktOpen: opens a VIF1 packet. Selects ctx 0x295AD0+idx*32 and scratch 0x70000000+idx*0x2000 from the double-buffer byte (gp-29596); sceVif1PkInit/PkReset/PktCnt(0)/OpenDirectCode(0); stores ctx to gp-29600.", 0);
	SetFunctionCmt(0x00240438, "menuPktKick: closes the chain (PkCloseGifTag/CloseDirectCode/End/Terminate), sceDmaSync, ORs 0x80000000 (IRQ) into the head tag QWC word and passes it as sceDmaSend's qwc arg; flips the double-buffer byte.", 0);
	SetFunctionCmt(0x00245038, "menuDrawElement: renders one menu element. Desc = 0x295D10 + *(gp-29496)*352 (244(el)=float pos, 48(el)=verts); matrix via sceVu0RotMatrix/TransMatrix/MulMatrix; GIF tag from 0x295D00; GS data from 0x295CD0; kicks via menuPktKick.", 0);
	SetFunctionCmt(0x00246D28, "menuUploadTexture: uploads a menu texture (CLUT + image) from 28B descriptor 0x296160 + (a1&0xffff)*28 via menuUploadTextureLoadImage twice.", 0);
	SetFunctionCmt(0x00246DE8, "menuUploadTextureLoadImage: file RPC (0x24dce0) + sceVif1PkInit/PkReset/PkRefLoadImage/PkEnd/PkTerminate + sceDmaSync/Send - one LoadImage packet.", 0);
	SetFunctionCmt(0x00204170, "osdSelectStringTable: caches osdLanguageTables[lang] (0x26ecc0) into osdCurStringTable (0x26ede0); lang from osdGetLanguage.", 0);
	SetFunctionCmt(0x002041B8, "osdGetString: returns osdCurStringTable[id] for the current language; ids 85/86 gated on 0x204318 (version check) via a struct at +340/+344.", 0);
	SetFunctionCmt(0x00224630, "menuScreenRenderer: a full menu screen - 15x osdGetString, 5x osdSelectStringTable, 3x loadImage_Resource, 9x menuWidgetDraw + 3x menuWidgetDraw2, 6x 0x21dc88.", 0);
	SetFunctionCmt(0x00240068, "menuBrowserState: disc-state machine - watches 0x1f0000b0 for states 5/8, calls 0x2036b0/0x23bfd0/0x23bff0, transitions menuState (0/1/2).", 0);
}
