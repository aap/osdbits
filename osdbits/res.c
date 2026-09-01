#include "inc.h"
#include "res.h"

#include "res/FNTASCII_EXP.inc"
#include "res/FNTEXOSD_EXP.inc"
#include "res/TEXCMARU_EXP.inc"
#include "res/TEXCBLUR_EXP.inc"
#include "res/TEXCBUMP_EXP.inc"
#include "res/TEXCFLOW_EXP.inc"
#include "res/TEXCKABE_EXP.inc"
#include "res/TEXCNAVI_EXP.inc"
#include "res/TEXCREFA_EXP.inc"
#include "res/TEXOBLPR_EXP.inc"
#include "res/TEXOBLP_EXP.inc"
#include "res/TEXOCRBL_EXP.inc"
#include "res/TEXOCRLE_EXP.inc"
#include "res/TEXOFLAR_EXP.inc"
#include "res/TEXOFOG0_EXP.inc"
#include "res/TEXOFOG1_EXP.inc"
#include "res/TEXOFOG2_EXP.inc"
#include "res/TEXOFOG3_EXP.inc"
#include "res/TEXOFOG4_EXP.inc"
#include "res/TEXOPNGD_EXP.inc"
#include "res/TEXOPNGE_EXP.inc"
#include "res/TEXOPNGF_EXP.inc"
#include "res/TEXOPNGG_EXP.inc"
#include "res/TEXOPNGI_EXP.inc"
#include "res/TEXOPNGJ_EXP.inc"
#include "res/TEXOPNGP_EXP.inc"
#include "res/TEXOPNGS_EXP.inc"
#include "res/TEXOREF_EXP.inc"
#include "res/TEXOSCE_EXP.inc"
#include "res/TEXOWAL0_EXP.inc"

enum ResourceType
{
	RES_TYPE0,
	RES_FILE,
	RES_DIR,
	RES_SUBFILE,
	RES_COMPSUBFILE,
};

typedef struct Resource Resource;
struct Resource
{
	char *name;
	u8 *data;
	u32 size;
	i32 type;
};

Resource resources[] = {
	{ "FONTM",	nil, 0, RES_FILE },
	{ "FNTIMAGE",	nil, 0, RES_DIR },
	{ "FNTASCII",	nil, 0, RES_COMPSUBFILE },
	{ "FNTEX000",	nil, 0, RES_COMPSUBFILE },
	{ "FNTEX001",	nil, 0, RES_COMPSUBFILE },
	{ "FNTEXOSD",	nil, 0, RES_COMPSUBFILE },
	{ "SNDIMAGE",	nil, 0, RES_DIR },
	{ "SNDBOOTH",	nil, 0, RES_COMPSUBFILE },
	{ "SNDBOOTB",	nil, 0, RES_COMPSUBFILE },
	{ "SNDBOOTS",	nil, 0, RES_COMPSUBFILE },
	{ "SNDTNNLS",	nil, 0, RES_COMPSUBFILE },
	{ "SNDCLOKS",	nil, 0, RES_COMPSUBFILE },
	{ "SNDTM30S",	nil, 0, RES_COMPSUBFILE },
	{ "SNDTM60S",	nil, 0, RES_COMPSUBFILE },
	{ "SNDOSDDH",	nil, 0, RES_COMPSUBFILE },
	{ "SNDOSDDB",	nil, 0, RES_COMPSUBFILE },
	{ "SNDLOGOS",	nil, 0, RES_COMPSUBFILE },
	{ "SNDWARNS",	nil, 0, RES_COMPSUBFILE },
	{ "SNDRCLKS",	nil, 0, RES_COMPSUBFILE },
	{ "TEXIMAGE",	nil, 0, RES_DIR },
	{ "TEXOPNGD",	nil, 0, RES_COMPSUBFILE },
	{ "TEXOPNGE",	nil, 0, RES_COMPSUBFILE },
	{ "TEXOPNGF",	nil, 0, RES_COMPSUBFILE },
	{ "TEXOPNGG",	nil, 0, RES_COMPSUBFILE },
	{ "TEXOPNGI",	nil, 0, RES_COMPSUBFILE },
	{ "TEXOPNGJ",	nil, 0, RES_COMPSUBFILE },
	{ "TEXOPNGP",	nil, 0, RES_COMPSUBFILE },
	{ "TEXOPNGS",	nil, 0, RES_COMPSUBFILE },
	{ "TEXOWAL0",	nil, 0, RES_COMPSUBFILE },
	{ "TEXOBLP",	nil, 0, RES_COMPSUBFILE },
	{ "TEXOCRLE",	nil, 0, RES_COMPSUBFILE },
	{ "TEXOFOG1",	nil, 0, RES_COMPSUBFILE },
	{ "TEXOFOG4",	nil, 0, RES_COMPSUBFILE },
	{ "TEXOREF",	nil, 0, RES_COMPSUBFILE },
	{ "TEXOBLPR",	nil, 0, RES_COMPSUBFILE },
	{ "TEXOFLAR",	nil, 0, RES_COMPSUBFILE },
	{ "TEXOFOG2",	nil, 0, RES_COMPSUBFILE },
	{ "TEXOSCE",	nil, 0, RES_COMPSUBFILE },
	{ "TEXOCRBL",	nil, 0, RES_COMPSUBFILE },
	{ "TEXOFOG0",	nil, 0, RES_COMPSUBFILE },
	{ "TEXOFOG3",	nil, 0, RES_COMPSUBFILE },
	{ "TEXCKLGN",	nil, 0, RES_COMPSUBFILE },
	{ "TEXCKLGP",	nil, 0, RES_COMPSUBFILE },
	{ "TEXCKLFN",	nil, 0, RES_COMPSUBFILE },
	{ "TEXCKLFP",	nil, 0, RES_COMPSUBFILE },
	{ "TEXCFLOW",	nil, 0, RES_COMPSUBFILE },
	{ "TEXCKABE",	nil, 0, RES_COMPSUBFILE },
	{ "TEXCBUMP",	nil, 0, RES_COMPSUBFILE },
	{ "TEXCBINV",	nil, 0, RES_COMPSUBFILE },
	{ "TEXCSMOK",	nil, 0, RES_COMPSUBFILE },
	{ "TEXCREFA",	nil, 0, RES_COMPSUBFILE },
	{ "TEXCNAVI",	nil, 0, RES_COMPSUBFILE },
	{ "TEXCBLUR",	nil, 0, RES_COMPSUBFILE },
	{ "TEXCSTSL",	nil, 0, RES_COMPSUBFILE },
	{ "TEXCMARU",	nil, 0, RES_COMPSUBFILE },
	{ "TEXBNAV1",	nil, 0, RES_COMPSUBFILE },
	{ "TEXBNAV2",	nil, 0, RES_COMPSUBFILE },
	{ "TEXBARRW",	nil, 0, RES_COMPSUBFILE },
	{ "TEXBBTTN",	nil, 0, RES_COMPSUBFILE },
	{ "TEXBCPAR",	nil, 0, RES_COMPSUBFILE },
	{ "TEXBCDPB",	nil, 0, RES_COMPSUBFILE },
	{ "TEXBOVAL",	nil, 0, RES_COMPSUBFILE },
	{ "TEXBICHI",	nil, 0, RES_COMPSUBFILE },
	{ "ICOIMAGE",	nil, 0, RES_DIR },
	{ "ICOBDISC",	nil, 0, RES_COMPSUBFILE },
	{ "ICOBCDDA",	nil, 0, RES_COMPSUBFILE },
	{ "ICOBPS1M",	nil, 0, RES_COMPSUBFILE },
	{ "ICOBPS2M",	nil, 0, RES_COMPSUBFILE },
	{ "ICOBPKST",	nil, 0, RES_COMPSUBFILE },
	{ "ICOBPS1D",	nil, 0, RES_COMPSUBFILE },
	{ "ICOBPS2D",	nil, 0, RES_COMPSUBFILE },
	{ "ICOBDVDD",	nil, 0, RES_COMPSUBFILE },
	{ "ICOBYSYS",	nil, 0, RES_COMPSUBFILE },
	{ "ICOBFSCE",	nil, 0, RES_COMPSUBFILE },
	{ "ICOBFNOR",	nil, 0, RES_COMPSUBFILE },
	{ "ICOBFBRK",	nil, 0, RES_COMPSUBFILE },
	{ "ICOBQUES",	nil, 0, RES_COMPSUBFILE },
	{ "TZLIST",	nil, 0, RES_FILE }
};

void
LoadResources(void)
{
	// totally fake, but good enough for now
	resources[RESID_TEXOPNGD].data = TEXOPNGD_EXP;
	resources[RESID_TEXOPNGD].size = TEXOPNGD_EXP_len;
	resources[RESID_TEXOPNGE].data = TEXOPNGE_EXP;
	resources[RESID_TEXOPNGE].size = TEXOPNGE_EXP_len;
	resources[RESID_TEXOPNGF].data = TEXOPNGF_EXP;
	resources[RESID_TEXOPNGF].size = TEXOPNGF_EXP_len;
	resources[RESID_TEXOPNGG].data = TEXOPNGG_EXP;
	resources[RESID_TEXOPNGG].size = TEXOPNGG_EXP_len;
	resources[RESID_TEXOPNGI].data = TEXOPNGI_EXP;
	resources[RESID_TEXOPNGI].size = TEXOPNGI_EXP_len;
	resources[RESID_TEXOPNGJ].data = TEXOPNGJ_EXP;
	resources[RESID_TEXOPNGJ].size = TEXOPNGJ_EXP_len;
	resources[RESID_TEXOPNGP].data = TEXOPNGP_EXP;
	resources[RESID_TEXOPNGP].size = TEXOPNGP_EXP_len;
	resources[RESID_TEXOPNGS].data = TEXOPNGS_EXP;
	resources[RESID_TEXOPNGS].size = TEXOPNGS_EXP_len;
	resources[RESID_TEXOWAL0].data = TEXOWAL0_EXP;
	resources[RESID_TEXOWAL0].size = TEXOWAL0_EXP_len;
	resources[RESID_TEXOBLP ].data = TEXOBLP_EXP;
	resources[RESID_TEXOBLP ].size = TEXOBLP_EXP_len;
	resources[RESID_TEXOCRLE].data = TEXOCRLE_EXP;
	resources[RESID_TEXOCRLE].size = TEXOCRLE_EXP_len;
	resources[RESID_TEXOFOG1].data = TEXOFOG1_EXP;
	resources[RESID_TEXOFOG1].size = TEXOFOG1_EXP_len;
	resources[RESID_TEXOFOG4].data = TEXOFOG4_EXP;
	resources[RESID_TEXOFOG4].size = TEXOFOG4_EXP_len;
	resources[RESID_TEXOREF ].data = TEXOREF_EXP;
	resources[RESID_TEXOREF ].size = TEXOREF_EXP_len;
	resources[RESID_TEXOBLPR].data = TEXOBLPR_EXP;
	resources[RESID_TEXOBLPR].size = TEXOBLPR_EXP_len;
	resources[RESID_TEXOFLAR].data = TEXOFLAR_EXP;
	resources[RESID_TEXOFLAR].size = TEXOFLAR_EXP_len;
	resources[RESID_TEXOFOG2].data = TEXOFOG2_EXP;
	resources[RESID_TEXOFOG2].size = TEXOFOG2_EXP_len;
	resources[RESID_TEXOSCE ].data = TEXOSCE_EXP;
	resources[RESID_TEXOSCE ].size = TEXOSCE_EXP_len;
	resources[RESID_TEXOCRBL].data = TEXOCRBL_EXP;
	resources[RESID_TEXOCRBL].size = TEXOCRBL_EXP_len;
	resources[RESID_TEXOFOG0].data = TEXOFOG0_EXP;
	resources[RESID_TEXOFOG0].size = TEXOFOG0_EXP_len;
	resources[RESID_TEXOFOG3].data = TEXOFOG3_EXP;
	resources[RESID_TEXOFOG3].size = TEXOFOG3_EXP_len;
	/* the menu background scene's orb sprites (menu.c) */
	resources[RESID_TEXCBLUR].data = TEXCBLUR_EXP;
	resources[RESID_TEXCBLUR].size = TEXCBLUR_EXP_len;
	resources[RESID_TEXCNAVI].data = TEXCNAVI_EXP;
	resources[RESID_TEXCNAVI].size = TEXCNAVI_EXP_len;
	/* the menu background's wall texture (menuback.c) */
	resources[RESID_TEXCKABE].data = TEXCKABE_EXP;
	resources[RESID_TEXCKABE].size = TEXCKABE_EXP_len;
	resources[RESID_TEXCBUMP].data = TEXCBUMP_EXP;
	resources[RESID_TEXCBUMP].size = TEXCBUMP_EXP_len;
	/* TEXC slot 0 - the deferred rod bloom's environment map
	 * (menuconfig.c).  Its partner TEXCBINV (slot 3) is NOT a resource
	 * here: the two blobs are exact bitwise complements of each other
	 * (verified byte for byte over all 4096), so DecodeBump() derives it. */
	resources[RESID_TEXCFLOW].data = TEXCFLOW_EXP;
	resources[RESID_TEXCFLOW].size = TEXCFLOW_EXP_len;
	/* the OSD text engine's Latin font page (menutext.c) */
	resources[RESID_FNTASCII].data = FNTASCII_EXP;
	resources[RESID_FNTASCII].size = FNTASCII_EXP_len;
	/* its symbol page - the kind-2 glyphs the \7oNNN escape draws, of
	 * which the config screen's page marker is glyph 20 (menutext.c) */
	resources[RESID_FNTEXOSD].data = FNTEXOSD_EXP;
	resources[RESID_FNTEXOSD].size = FNTEXOSD_EXP_len;
	/* the four shape-button icons the hint bar draws (menutext.c) */
	resources[RESID_TEXCMARU].data = TEXCMARU_EXP;
	resources[RESID_TEXCMARU].size = TEXCMARU_EXP_len;
	/* the cubes' spherical environment map (menuconfig.c) */
	resources[RESID_TEXCREFA].data = TEXCREFA_EXP;
	resources[RESID_TEXCREFA].size = TEXCREFA_EXP_len;
}

u8*
GetResourceData(int resid)
{
	return resources[resid].data;
}

u32
GetResourceSize(int resid)
{
	return resources[resid].size;
}
