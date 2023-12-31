/*
 * Definitions file parser for Build
 * by Jonathon Fowler (jf@jonof.id.au)
 * Remixed substantially by Ken Silverman
 * See the included license file "BUILDLIC.TXT" for license info.
 */

#include "build.h"
#include "compat.h"
#include "baselayer.h"
#include "scriptfile.h"
#include "lz4.h"
#include "common_build.h"

enum {
	T_EOF = -2,
	T_ERROR = -1,
	T_INCLUDE = 0,
	T_DEFINE,
	T_DEFINETEXTURE,
	T_DEFINESKYBOX,
	T_DEFINETINT,
	T_DEFINEMODEL,
	T_DEFINEMODELFRAME,
	T_DEFINEMODELANIM,
	T_DEFINEMODELSKIN,
	T_SELECTMODELSKIN,
	T_DEFINEVOXEL,
	T_DEFINEVOXELTILES,
	T_MODEL,
	T_FILE,
	T_SCALE,
	T_SHADE,
	T_FRAME,
	T_ANIM,
	T_SKIN,
	T_SURF,
	T_TILE,
	T_TILE0,
	T_TILE1,
	T_FRAME0,
	T_FRAME1,
	T_FPS,
	T_FLAGS,
	T_PAL,
	T_HUD,
	T_XADD,
	T_YADD,
	T_ZADD,
	T_ANGADD,
	T_FLIPPED,
	T_HIDE,
	T_NOBOB,
	T_NODEPTH,
	T_VOXEL,
	T_SKYBOX,
	T_FRONT,T_RIGHT,T_BACK,T_LEFT,T_TOP,T_BOTTOM,
	T_TINT,T_RED,T_GREEN,T_BLUE,
	T_TEXTURE,T_ALPHACUT,T_NOCOMPRESS,
	T_UNDEFMODEL,T_UNDEFMODELRANGE,T_UNDEFMODELOF,T_UNDEFTEXTURE,T_UNDEFTEXTURERANGE,
    T_TILEFROMTEXTURE, T_XOFFSET, T_YOFFSET,
    T_ORIGSIZEX, T_ORIGSIZEY,

};

typedef struct { char *text; int tokenid; } tokenlist;
static tokenlist basetokens[] = {
	{ "include",         T_INCLUDE          },
	{ "#include",        T_INCLUDE          },
	{ "define",          T_DEFINE           },
	{ "#define",         T_DEFINE           },

	// deprecated style
	{ "definetexture",   T_DEFINETEXTURE    },
	{ "defineskybox",    T_DEFINESKYBOX     },
	{ "definetint",      T_DEFINETINT       },
	{ "definemodel",     T_DEFINEMODEL      },
	{ "definemodelframe",T_DEFINEMODELFRAME },
	{ "definemodelanim", T_DEFINEMODELANIM  },
	{ "definemodelskin", T_DEFINEMODELSKIN  },
	{ "selectmodelskin", T_SELECTMODELSKIN  },
	{ "definevoxel",     T_DEFINEVOXEL      },
	{ "definevoxeltiles",T_DEFINEVOXELTILES },

	// new style
	{ "model",             T_MODEL             },
	{ "voxel",             T_VOXEL             },
	{ "skybox",            T_SKYBOX            },
	{ "tint",              T_TINT              },
	{ "texture",           T_TEXTURE           },
	{ "tile",              T_TEXTURE           },
	{ "undefmodel",        T_UNDEFMODEL        },
	{ "undefmodelrange",   T_UNDEFMODELRANGE   },
	{ "undefmodelof",      T_UNDEFMODELOF      },
	{ "undeftexture",      T_UNDEFTEXTURE      },
	{ "undeftexturerange", T_UNDEFTEXTURERANGE },

    { "tilefromtexture", T_TILEFROMTEXTURE  },
};

static tokenlist modeltokens[] = {
	{ "scale",  T_SCALE  },
	{ "shade",  T_SHADE  },
	{ "zadd",   T_ZADD   },
	{ "frame",  T_FRAME  },
	{ "anim",   T_ANIM   },
	{ "skin",   T_SKIN   },
	{ "hud",    T_HUD    },
};

static tokenlist modelframetokens[] = {
	{ "frame",  T_FRAME   },
	{ "name",   T_FRAME   },
	{ "tile",   T_TILE   },
	{ "tile0",  T_TILE0  },
	{ "tile1",  T_TILE1  },
};

static tokenlist modelanimtokens[] = {
	{ "frame0", T_FRAME0 },
	{ "frame1", T_FRAME1 },
	{ "fps",    T_FPS    },
	{ "flags",  T_FLAGS  },
};

static tokenlist modelskintokens[] = {
	{ "pal",    T_PAL    },
	{ "file",   T_FILE   },
	{ "surf",   T_SURF   },
	{ "surface",T_SURF   },
};

static tokenlist modelhudtokens[] = {
	{ "tile",   T_TILE   },
	{ "tile0",  T_TILE0  },
	{ "tile1",  T_TILE1  },
	{ "xadd",   T_XADD   },
	{ "yadd",   T_YADD   },
	{ "zadd",   T_ZADD   },
	{ "angadd", T_ANGADD },
	{ "hide",   T_HIDE   },
	{ "nobob",  T_NOBOB  },
	{ "flipped",T_FLIPPED},
	{ "nodepth",T_NODEPTH},
};

static tokenlist voxeltokens[] = {
	{ "tile",   T_TILE   },
	{ "tile0",  T_TILE0  },
	{ "tile1",  T_TILE1  },
	{ "scale",  T_SCALE  },
};

static tokenlist skyboxtokens[] = {
	{ "tile"   ,T_TILE   },
	{ "pal"    ,T_PAL    },
	{ "ft"     ,T_FRONT  },{ "front"  ,T_FRONT  },{ "forward",T_FRONT  },
	{ "rt"     ,T_RIGHT  },{ "right"  ,T_RIGHT  },
	{ "bk"     ,T_BACK   },{ "back"   ,T_BACK   },
	{ "lf"     ,T_LEFT   },{ "left"   ,T_LEFT   },{ "lt"     ,T_LEFT   },
	{ "up"     ,T_TOP    },{ "top"    ,T_TOP    },{ "ceiling",T_TOP    },{ "ceil"   ,T_TOP    },
	{ "dn"     ,T_BOTTOM },{ "bottom" ,T_BOTTOM },{ "floor"  ,T_BOTTOM },{ "down"   ,T_BOTTOM }
}; 

static tokenlist tinttokens[] = {
	{ "pal",   T_PAL },
	{ "red",   T_RED   },{ "r", T_RED },
	{ "green", T_GREEN },{ "g", T_GREEN },
	{ "blue",  T_BLUE  },{ "b", T_BLUE },
	{ "flags", T_FLAGS }
};

static tokenlist texturetokens[] = {
	{ "pal",   T_PAL  },
};
static tokenlist texturetokens_pal[] = {
	{ "file",      T_FILE },{ "name", T_FILE },
	{ "alphacut",  T_ALPHACUT },
	{ "nocompress",T_NOCOMPRESS },
    { "orig_sizex", T_ORIGSIZEX },
    { "orig_sizey", T_ORIGSIZEY },
};


static int getatoken(scriptfile *sf, tokenlist *tl, int ntokens)
{
	char *tok;
	int i;

	if (!sf) return T_ERROR;
	tok = scriptfile_gettoken(sf);
	if (!tok) return T_EOF;

	for(i=0;i<ntokens;i++) {
		if (!Bstrcasecmp(tok, tl[i].text))
			return tl[i].tokenid;
	}

	return T_ERROR;
}

static int lastmodelid = -1, lastvoxid = -1, modelskin = -1, lastmodelskin = -1, seenframe = 0;
extern int nextvoxid;

static const char *skyfaces[6] = {
	"front face", "right face", "back face",
	"left face", "top face", "bottom face"
};

static void tile_from_truecolpic(int tile, const palette_t *picptr, int alphacut)
{
    const int xsiz = tilesizx[tile], ysiz = tilesizy[tile];
    int i, j;

    char *ftd = (char *)Bmalloc(xsiz*ysiz);

    faketiledata[tile] = (char *)Bmalloc(xsiz*ysiz + 400);

    for (i=xsiz-1; i>=0; i--)
    {
        for (j=ysiz-1; j>=0; j--)
        {
            const palette_t *col = &picptr[j*xsiz+i];
            if (col->f < alphacut) { ftd[i*ysiz+j] = 255; continue; }
            ftd[i*ysiz+j] = getclosestcol(col->b>>2,col->g>>2,col->r>>2);
        }
        //                initprintf("\n %d %d %d %d",col->r,col->g,col->b,col->f);
    }

    faketilesiz[tile] = LZ4_compress(ftd, faketiledata[tile], xsiz*ysiz);
    Bfree(ftd);
}

static int defsparser(scriptfile *script)
{
	int tokn;
	char *cmdtokptr;
	while (1) {
		tokn = getatoken(script,basetokens,sizeof(basetokens)/sizeof(tokenlist));
		cmdtokptr = script->ltextptr;
		switch (tokn) {
			case T_ERROR:
				initprintf("Error on line %s:%d.\n", script->filename,scriptfile_getlinum(script,cmdtokptr));
				break;
			case T_EOF:
				return(0);
			case T_INCLUDE:
				{
					char *fn;
					if (!scriptfile_getstring(script,&fn)) {
						scriptfile *included;

						included = scriptfile_fromfile(fn);
						if (!included) {
							initprintf("Warning: Failed including %s on line %s:%d\n",
									fn, script->filename,scriptfile_getlinum(script,cmdtokptr));
						} else {
							defsparser(included);
							scriptfile_close(included);
						}
					}
					break;
				}
			case T_DEFINE:
				{
					char *name;
					int number;

					if (scriptfile_getstring(script,&name)) break;
					if (scriptfile_getsymbol(script,&number)) break;

					if (scriptfile_addsymbolvalue(name,number) < 0)
						initprintf("Warning: Symbol %s was NOT redefined to %d on line %s:%d\n",
								name,number,script->filename,scriptfile_getlinum(script,cmdtokptr));
					break;
				}

				// OLD (DEPRECATED) DEFINITION SYNTAX
			case T_DEFINETEXTURE:
				{
					int tile,pal,fnoo;
					char *fn;

					if (scriptfile_getsymbol(script,&tile)) break;
					if (scriptfile_getsymbol(script,&pal))  break;
					if (scriptfile_getnumber(script,&fnoo)) break; //x-center
					if (scriptfile_getnumber(script,&fnoo)) break; //y-center
					if (scriptfile_getnumber(script,&fnoo)) break; //x-size
					if (scriptfile_getnumber(script,&fnoo)) break; //y-size
					if (scriptfile_getstring(script,&fn))  break;
					hicsetsubsttex(tile,pal,fn,-1.0,0,-1,-1);
				}
				break;
			case T_DEFINESKYBOX:
				{
					int tile,pal,i;
					char *fn[6];

					if (scriptfile_getsymbol(script,&tile)) break;
					if (scriptfile_getsymbol(script,&pal)) break;
					if (scriptfile_getsymbol(script,&i)) break; //future expansion
					for (i=0;i<6;i++) if (scriptfile_getstring(script,&fn[i])) break; //grab the 6 faces
					if (i < 6) break;
					hicsetskybox(tile,pal,fn);
				}
				break;
			case T_DEFINETINT:
				{
					int pal, r,g,b,f;

					if (scriptfile_getsymbol(script,&pal)) break;
					if (scriptfile_getnumber(script,&r)) break;
					if (scriptfile_getnumber(script,&g)) break;
					if (scriptfile_getnumber(script,&b)) break;
					if (scriptfile_getnumber(script,&f)) break; //effects
					hicsetpalettetint(pal,r,g,b,f);
				}
				break;
			case T_DEFINEMODEL:
				{
					char *modelfn;
					double scale;
					int shadeoffs;

					if (scriptfile_getstring(script,&modelfn)) break;
					if (scriptfile_getdouble(script,&scale)) break;
					if (scriptfile_getnumber(script,&shadeoffs)) break;

#if defined(POLYMOST) && defined(USE_OPENGL)
					lastmodelid = md_loadmodel(modelfn);
					if (lastmodelid < 0) {
						initprintf("Failure loading MD2/MD3 model \"%s\"\n", modelfn);
						break;
					}
					md_setmisc(lastmodelid,(float)scale, shadeoffs,0.0);
#endif
					modelskin = lastmodelskin = 0;
					seenframe = 0;
				}
				break;
			case T_DEFINEMODELFRAME:
				{
					char *framename, happy=1;
					int ftilenume, ltilenume, tilex;

					if (scriptfile_getstring(script,&framename)) break;
					if (scriptfile_getnumber(script,&ftilenume)) break; //first tile number
					if (scriptfile_getnumber(script,&ltilenume)) break; //last tile number (inclusive)
					if (ltilenume < ftilenume) {
						initprintf("Warning: backwards tile range on line %s:%d\n", script->filename, scriptfile_getlinum(script,cmdtokptr));
						tilex = ftilenume;
						ftilenume = ltilenume;
						ltilenume = tilex;
					}

					if (lastmodelid < 0) {
						initprintf("Warning: Ignoring frame definition.\n");
						break;
					}
#if defined(POLYMOST) && defined(USE_OPENGL)
					for (tilex = ftilenume; tilex <= ltilenume && happy; tilex++) {
						switch (md_defineframe(lastmodelid, framename, tilex, max(0,modelskin))) {
							case 0: break;
							case -1: happy = 0; break; // invalid model id!?
							case -2: initprintf("Invalid tile number on line %s:%d\n",
										 script->filename, scriptfile_getlinum(script,cmdtokptr));
								 happy = 0;
								 break;
							case -3: initprintf("Invalid frame name on line %s:%d\n",
										 script->filename, scriptfile_getlinum(script,cmdtokptr));
								 happy = 0;
								 break;
						}
					}
#endif
					seenframe = 1;
				}
				break;
			case T_DEFINEMODELANIM:
				{
					char *startframe, *endframe;
					int flags;
					double dfps;

					if (scriptfile_getstring(script,&startframe)) break;
					if (scriptfile_getstring(script,&endframe)) break;
					if (scriptfile_getdouble(script,&dfps)) break; //animation frame rate
					if (scriptfile_getnumber(script,&flags)) break;

					if (lastmodelid < 0) {
						initprintf("Warning: Ignoring animation definition.\n");
						break;
					}
#if defined(POLYMOST) && defined(USE_OPENGL)
					switch (md_defineanimation(lastmodelid, startframe, endframe, (int)(dfps*(65536.0*.001)), flags)) {
						case 0: break;
						case -1: break; // invalid model id!?
						case -2: initprintf("Invalid starting frame name on line %s:%d\n",
									 script->filename, scriptfile_getlinum(script,cmdtokptr));
							 break;
						case -3: initprintf("Invalid ending frame name on line %s:%d\n",
									 script->filename, scriptfile_getlinum(script,cmdtokptr));
							 break;
						case -4: initprintf("Out of memory on line %s:%d\n",
									 script->filename, scriptfile_getlinum(script,cmdtokptr));
							 break;
					}
#endif
				}
				break;
			case T_DEFINEMODELSKIN:
				{
					int palnum, palnumer;
					char *skinfn;
					
					if (scriptfile_getsymbol(script,&palnum)) break;
					if (scriptfile_getstring(script,&skinfn)) break; //skin filename

					// if we see a sequence of definemodelskin, then a sequence of definemodelframe,
					// and then a definemodelskin, we need to increment the skin counter.
					//
					// definemodel "mymodel.md2" 1 1
					// definemodelskin 0 "normal.png"   // skin 0
					// definemodelskin 21 "normal21.png"
					// definemodelframe "foo" 1000 1002   // these use skin 0
					// definemodelskin 0 "wounded.png"   // skin 1
					// definemodelskin 21 "wounded21.png"
					// definemodelframe "foo2" 1003 1004   // these use skin 1
					// selectmodelskin 0         // resets to skin 0
					// definemodelframe "foo3" 1005 1006   // these use skin 0
					if (seenframe) { modelskin = ++lastmodelskin; }
					seenframe = 0;

#if defined(POLYMOST) && defined(USE_OPENGL)
					switch (md_defineskin(lastmodelid, skinfn, palnum, max(0,modelskin), 0)) {
						case 0: break;
						case -1: break; // invalid model id!?
						case -2: initprintf("Invalid skin filename on line %s:%d\n",
									 script->filename, scriptfile_getlinum(script,cmdtokptr));
							 break;
						case -3: initprintf("Invalid palette number on line %s:%d\n",
									 script->filename, scriptfile_getlinum(script,cmdtokptr));
							 break;
						case -4: initprintf("Out of memory on line %s:%d\n",
									 script->filename, scriptfile_getlinum(script,cmdtokptr));
							 break;
					}
#endif               
				}
				break;
			case T_SELECTMODELSKIN:
				{
					if (scriptfile_getsymbol(script,&modelskin)) break;
				}
				break;
			case T_DEFINEVOXEL:
				{
					char *fn;

					if (scriptfile_getstring(script,&fn)) break; //voxel filename

					if (nextvoxid == MAXVOXELS) {
						initprintf("Maximum number of voxels already defined.\n");
						break;
					}

#ifdef SUPERBUILD
					if (qloadkvx(nextvoxid, fn)) {
						initprintf("Failure loading voxel file \"%s\"\n",fn);
						break;
					}

					lastvoxid = nextvoxid++;
#endif
				}
				break;
			case T_DEFINEVOXELTILES:
				{
					int ftilenume, ltilenume, tilex;

					if (scriptfile_getnumber(script,&ftilenume)) break; //1st tile #
					if (scriptfile_getnumber(script,&ltilenume)) break; //last tile #

					if (ltilenume < ftilenume) {
						initprintf("Warning: backwards tile range on line %s:%d\n",
								script->filename, scriptfile_getlinum(script,cmdtokptr));
						tilex = ftilenume;
						ftilenume = ltilenume;
						ltilenume = tilex;
					}
					if (ltilenume < 0 || ftilenume >= MAXTILES) {
						initprintf("Invalid tile range on line %s:%d\n",
								script->filename, scriptfile_getlinum(script,cmdtokptr));
						break;
					}

					if (lastvoxid < 0) {
						initprintf("Warning: Ignoring voxel tiles definition.\n");
						break;
					}
#ifdef SUPERBUILD
					for (tilex = ftilenume; tilex <= ltilenume; tilex++) {
						tiletovox[tilex] = lastvoxid;
					}
#endif
				}
				break;

				// NEW (ENCOURAGED) DEFINITION SYNTAX
			case T_MODEL:
				{
					char *modelend, *modelfn;
					double scale=1.0, mzadd=0.0;
					int shadeoffs=0;

					modelskin = lastmodelskin = 0;
					seenframe = 0;

					if (scriptfile_getstring(script,&modelfn)) break;

#if defined(POLYMOST) && defined(USE_OPENGL)
					lastmodelid = md_loadmodel(modelfn);
					if (lastmodelid < 0) {
						initprintf("Failure loading MD2/MD3 model \"%s\"\n", modelfn);
						break;
					}
#endif
					if (scriptfile_getbraces(script,&modelend)) break;
					while (script->textptr < modelend) {
						switch (getatoken(script,modeltokens,sizeof(modeltokens)/sizeof(tokenlist))) {
							//case T_ERROR: initprintf("Error on line %s:%d in model tokens\n", script->filename,script->linenum); break;
							case T_SCALE: scriptfile_getdouble(script,&scale); break;
							case T_SHADE: scriptfile_getnumber(script,&shadeoffs); break;
							case T_ZADD:  scriptfile_getdouble(script,&mzadd); break;
							case T_FRAME:
							{
								char *frametokptr = script->ltextptr;
								char *frameend, *framename = 0, happy=1;
								int ftilenume = -1, ltilenume = -1, tilex = 0;

								if (scriptfile_getbraces(script,&frameend)) break;
								while (script->textptr < frameend) {
									switch(getatoken(script,modelframetokens,sizeof(modelframetokens)/sizeof(tokenlist))) {
										case T_FRAME: scriptfile_getstring(script,&framename); break;
										case T_TILE:  scriptfile_getsymbol(script,&ftilenume); ltilenume = ftilenume; break;
										case T_TILE0: scriptfile_getsymbol(script,&ftilenume); break; //first tile number
										case T_TILE1: scriptfile_getsymbol(script,&ltilenume); break; //last tile number (inclusive)
									}
								}

								if (ftilenume < 0) initprintf("Error: missing 'first tile number' for frame definition near line %s:%d\n", script->filename, scriptfile_getlinum(script,frametokptr)), happy = 0;
								if (ltilenume < 0) initprintf("Error: missing 'last tile number' for frame definition near line %s:%d\n", script->filename, scriptfile_getlinum(script,frametokptr)), happy = 0;
								if (!happy) break;

								if (ltilenume < ftilenume) {
									initprintf("Warning: backwards tile range on line %s:%d\n", script->filename, scriptfile_getlinum(script,frametokptr));
									tilex = ftilenume;
									ftilenume = ltilenume;
									ltilenume = tilex;
								}

								if (lastmodelid < 0) {
									initprintf("Warning: Ignoring frame definition.\n");
									break;
								}
#if defined(POLYMOST) && defined(USE_OPENGL)
								for (tilex = ftilenume; tilex <= ltilenume && happy; tilex++) {
									switch (md_defineframe(lastmodelid, framename, tilex, max(0,modelskin))) {
										case 0: break;
										case -1: happy = 0; break; // invalid model id!?
										case -2: initprintf("Invalid tile number on line %s:%d\n",
													 script->filename, scriptfile_getlinum(script,frametokptr));
											 happy = 0;
											 break;
										case -3: initprintf("Invalid frame name on line %s:%d\n",
													 script->filename, scriptfile_getlinum(script,frametokptr));
											 happy = 0;
											 break;
									}
								}
#endif
								seenframe = 1;
								}
								break;
							case T_ANIM:
							{
								char *animtokptr = script->ltextptr;
								char *animend, *startframe = 0, *endframe = 0, happy=1;
								int flags = 0;
								double dfps = 1.0;

								if (scriptfile_getbraces(script,&animend)) break;
								while (script->textptr < animend) {
									switch(getatoken(script,modelanimtokens,sizeof(modelanimtokens)/sizeof(tokenlist))) {
										case T_FRAME0: scriptfile_getstring(script,&startframe); break;
										case T_FRAME1: scriptfile_getstring(script,&endframe); break;
										case T_FPS: scriptfile_getdouble(script,&dfps); break; //animation frame rate
										case T_FLAGS: scriptfile_getsymbol(script,&flags); break;
									}
								}

								if (!startframe) initprintf("Error: missing 'start frame' for anim definition near line %s:%d\n", script->filename, scriptfile_getlinum(script,animtokptr)), happy = 0;
								if (!endframe) initprintf("Error: missing 'end frame' for anim definition near line %s:%d\n", script->filename, scriptfile_getlinum(script,animtokptr)), happy = 0;
								if (!happy) break;
								
								if (lastmodelid < 0) {
									initprintf("Warning: Ignoring animation definition.\n");
									break;
								}
#if defined(POLYMOST) && defined(USE_OPENGL)
								switch (md_defineanimation(lastmodelid, startframe, endframe, (int)(dfps*(65536.0*.001)), flags)) {
									case 0: break;
									case -1: break; // invalid model id!?
									case -2: initprintf("Invalid starting frame name on line %s:%d\n",
												 script->filename, scriptfile_getlinum(script,animtokptr));
										 break;
									case -3: initprintf("Invalid ending frame name on line %s:%d\n",
												 script->filename, scriptfile_getlinum(script,animtokptr));
										 break;
									case -4: initprintf("Out of memory on line %s:%d\n",
												 script->filename, scriptfile_getlinum(script,animtokptr));
										 break;
								}
#endif
							} break;
							case T_SKIN:
							{
								char *skintokptr = script->ltextptr;
								char *skinend, *skinfn = 0;
								int palnum = 0, surfnum = 0;

								if (scriptfile_getbraces(script,&skinend)) break;
								while (script->textptr < skinend) {
									switch(getatoken(script,modelskintokens,sizeof(modelskintokens)/sizeof(tokenlist))) {
										case T_PAL: scriptfile_getsymbol(script,&palnum); break;
										case T_FILE: scriptfile_getstring(script,&skinfn); break; //skin filename
										case T_SURF: scriptfile_getnumber(script,&surfnum); break;
									}
								}

								if (!skinfn) {
										initprintf("Error: missing 'skin filename' for skin definition near line %s:%d\n", script->filename, scriptfile_getlinum(script,skintokptr));
										break;
								}

								if (seenframe) { modelskin = ++lastmodelskin; }
								seenframe = 0;

#if defined(POLYMOST) && defined(USE_OPENGL)
								switch (md_defineskin(lastmodelid, skinfn, palnum, max(0,modelskin), surfnum)) {
									case 0: break;
									case -1: break; // invalid model id!?
									case -2: initprintf("Invalid skin filename on line %s:%d\n",
												 script->filename, scriptfile_getlinum(script,skintokptr));
										 break;
									case -3: initprintf("Invalid palette number on line %s:%d\n",
												 script->filename, scriptfile_getlinum(script,skintokptr));
										 break;
									case -4: initprintf("Out of memory on line %s:%d\n",
												 script->filename, scriptfile_getlinum(script,skintokptr));
										 break;
								}
#endif
							} break;
							case T_HUD:
							{
								char *hudtokptr = script->ltextptr;
								char happy=1, *frameend;
								int ftilenume = -1, ltilenume = -1, tilex = 0, flags = 0;
								double xadd = 0.0, yadd = 0.0, zadd = 0.0, angadd = 0.0;

								if (scriptfile_getbraces(script,&frameend)) break;
								while (script->textptr < frameend) {
									switch(getatoken(script,modelhudtokens,sizeof(modelhudtokens)/sizeof(tokenlist))) {
										case T_TILE:  scriptfile_getsymbol(script,&ftilenume); ltilenume = ftilenume; break;
										case T_TILE0: scriptfile_getsymbol(script,&ftilenume); break; //first tile number
										case T_TILE1: scriptfile_getsymbol(script,&ltilenume); break; //last tile number (inclusive)
										case T_XADD:  scriptfile_getdouble(script,&xadd); break;
										case T_YADD:  scriptfile_getdouble(script,&yadd); break;
										case T_ZADD:  scriptfile_getdouble(script,&zadd); break;
										case T_ANGADD:scriptfile_getdouble(script,&angadd); break;
										case T_HIDE:    flags |= 1; break;
										case T_NOBOB:   flags |= 2; break;
										case T_FLIPPED: flags |= 4; break;
										case T_NODEPTH: flags |= 8; break;
									}
								}

								if (ftilenume < 0) initprintf("Error: missing 'first tile number' for hud definition near line %s:%d\n", script->filename, scriptfile_getlinum(script,hudtokptr)), happy = 0;
								if (ltilenume < 0) initprintf("Error: missing 'last tile number' for hud definition near line %s:%d\n", script->filename, scriptfile_getlinum(script,hudtokptr)), happy = 0;
								if (!happy) break;

								if (ltilenume < ftilenume) {
									initprintf("Warning: backwards tile range on line %s:%d\n", script->filename, scriptfile_getlinum(script,hudtokptr));
									tilex = ftilenume;
									ftilenume = ltilenume;
									ltilenume = tilex;
								}

								if (lastmodelid < 0) {
									initprintf("Warning: Ignoring frame definition.\n");
									break;
								}
#if defined(POLYMOST) && defined(USE_OPENGL)
								for (tilex = ftilenume; tilex <= ltilenume && happy; tilex++) {
									switch (md_definehud(lastmodelid, tilex, xadd, yadd, zadd, angadd, flags)) {
										case 0: break;
										case -1: happy = 0; break; // invalid model id!?
										case -2: initprintf("Invalid tile number on line %s:%d\n",
												script->filename, scriptfile_getlinum(script,hudtokptr));
											happy = 0;
											break;
										case -3: initprintf("Invalid frame name on line %s:%d\n",
												script->filename, scriptfile_getlinum(script,hudtokptr));
											happy = 0;
											break;
									}
								}
#endif
							} break;
						}
					}

#if defined(POLYMOST) && defined(USE_OPENGL)
					md_setmisc(lastmodelid,(float)scale,shadeoffs,(float)mzadd);
#endif

					modelskin = lastmodelskin = 0;
					seenframe = 0;

				}
				break;
			case T_VOXEL:
				{
					char *voxeltokptr = script->ltextptr;
					char *fn, *modelend;
					int tile0 = MAXTILES, tile1 = -1, tilex = -1;

					if (scriptfile_getstring(script,&fn)) break; //voxel filename
					if (nextvoxid == MAXVOXELS) { initprintf("Maximum number of voxels already defined.\n"); break; }
#ifdef SUPERBUILD
					if (qloadkvx(nextvoxid, fn)) { initprintf("Failure loading voxel file \"%s\"\n",fn); break; }
					lastvoxid = nextvoxid++;
#endif

					if (scriptfile_getbraces(script,&modelend)) break;
					while (script->textptr < modelend) {
						switch (getatoken(script,voxeltokens,sizeof(voxeltokens)/sizeof(tokenlist))) {
							//case T_ERROR: initprintf("Error on line %s:%d in voxel tokens\n", script->filename,linenum); break;
							case T_TILE:  
								scriptfile_getsymbol(script,&tilex);
#ifdef SUPERBUILD
								if ((unsigned long)tilex < MAXTILES) tiletovox[tilex] = lastvoxid;
								else initprintf("Invalid tile number on line %s:%d\n",script->filename, scriptfile_getlinum(script,voxeltokptr));
#endif
								break;
							case T_TILE0: 
								scriptfile_getsymbol(script,&tile0); break; //1st tile #
							case T_TILE1:
								scriptfile_getsymbol(script,&tile1);
								if (tile0 > tile1)
								{
									initprintf("Warning: backwards tile range on line %s:%d\n", script->filename, scriptfile_getlinum(script,voxeltokptr));
									tilex = tile0; tile0 = tile1; tile1 = tilex;
								}
								if ((tile1 < 0) || (tile0 >= MAXTILES))
									{ initprintf("Invalid tile range on line %s:%d\n",script->filename, scriptfile_getlinum(script,voxeltokptr)); break; }
#ifdef SUPERBUILD
								for(tilex=tile0;tilex<=tile1;tilex++) tiletovox[tilex] = lastvoxid;
#endif
								break; //last tile number (inclusive)
							case T_SCALE: {
								double scale=1.0;
								scriptfile_getdouble(script,&scale);
#ifdef SUPERBUILD
								voxscale[lastvoxid] = 65536*scale;
#endif
								break;
							}
						}
					}
					lastvoxid = -1;
				}
				break;
			case T_SKYBOX:
				{
					char *skyboxtokptr = script->ltextptr;
					char *fn[6] = {0,0,0,0,0,0}, *modelend, happy=1;
					int i, tile = -1, pal = 0;

					if (scriptfile_getbraces(script,&modelend)) break;
					while (script->textptr < modelend) {
						switch (getatoken(script,skyboxtokens,sizeof(skyboxtokens)/sizeof(tokenlist))) {
							//case T_ERROR: initprintf("Error on line %s:%d in skybox tokens\n",script->filename,linenum); break;
							case T_TILE:  scriptfile_getsymbol(script,&tile ); break;
							case T_PAL:   scriptfile_getsymbol(script,&pal  ); break;
							case T_FRONT: scriptfile_getstring(script,&fn[0]); break;
							case T_RIGHT: scriptfile_getstring(script,&fn[1]); break;
							case T_BACK:  scriptfile_getstring(script,&fn[2]); break;
							case T_LEFT:  scriptfile_getstring(script,&fn[3]); break;
							case T_TOP:   scriptfile_getstring(script,&fn[4]); break;
							case T_BOTTOM:scriptfile_getstring(script,&fn[5]); break;
						}
					}

					if (tile < 0) initprintf("Error: missing 'tile number' for skybox definition near line %s:%d\n", script->filename, scriptfile_getlinum(script,skyboxtokptr)), happy=0;
					for (i=0;i<6;i++) {
							if (!fn[i]) initprintf("Error: missing '%s filename' for skybox definition near line %s:%d\n", skyfaces[i], script->filename, scriptfile_getlinum(script,skyboxtokptr)), happy = 0;
					}

					if (!happy) break;
					
					hicsetskybox(tile,pal,fn);
				}
				break;
			case T_TINT:
				{
					char *tinttokptr = script->ltextptr;
					int red=255, green=255, blue=255, pal=-1, flags=0;
					char *tintend;

					if (scriptfile_getbraces(script,&tintend)) break;
					while (script->textptr < tintend) {
						switch (getatoken(script,tinttokens,sizeof(tinttokens)/sizeof(tokenlist))) {
							case T_PAL:   scriptfile_getsymbol(script,&pal);   break;
							case T_RED:   scriptfile_getnumber(script,&red);   red   = min(255,max(0,red));   break;
							case T_GREEN: scriptfile_getnumber(script,&green); green = min(255,max(0,green)); break;
							case T_BLUE:  scriptfile_getnumber(script,&blue);  blue  = min(255,max(0,blue));  break;
							case T_FLAGS: scriptfile_getsymbol(script,&flags); break;
						}
					}

					if (pal < 0) {
							initprintf("Error: missing 'palette number' for tint definition near line %s:%d\n", script->filename, scriptfile_getlinum(script,tinttokptr));
							break;
					}

					hicsetpalettetint(pal,red,green,blue,flags);
				}
				break;
			case T_TEXTURE:
				{
					char *texturetokptr = script->ltextptr, *textureend;
					int tile=-1;

					if (scriptfile_getsymbol(script,&tile)) break;
					if (scriptfile_getbraces(script,&textureend)) break;
					while (script->textptr < textureend) {
						switch (getatoken(script,texturetokens,sizeof(texturetokens)/sizeof(tokenlist))) {
							case T_PAL: {
								char *paltokptr = script->ltextptr, *palend;
								int pal=-1;
								char *fn = NULL;
								double alphacut = -1.0;
								char flags = 0;
                                int sizex = -1, sizey = -1;
								if (scriptfile_getsymbol(script,&pal)) break;
								if (scriptfile_getbraces(script,&palend)) break;
								while (script->textptr < palend) {
									switch (getatoken(script,texturetokens_pal,sizeof(texturetokens_pal)/sizeof(tokenlist))) {
										case T_FILE:     scriptfile_getstring(script,&fn); break;
										case T_ALPHACUT: scriptfile_getdouble(script,&alphacut); break;
										case T_NOCOMPRESS: flags |= 1; break;
                                        case T_ORIGSIZEX:
                                            scriptfile_getsymbol(script, &sizex);
                                            break;
                                        case T_ORIGSIZEY:
                                            scriptfile_getsymbol(script, &sizey);
                                            break;
										default: break;
									}
								}

								if ((unsigned)tile > (unsigned)MAXTILES) break;	// message is printed later
								if ((unsigned)pal > (unsigned)MAXPALOOKUPS) {
									initprintf("Error: missing or invalid 'palette number' for texture definition near "
												"line %s:%d\n", script->filename, scriptfile_getlinum(script,paltokptr));
									break;
								}
								if (!fn) {
									initprintf("Error: missing 'file name' for texture definition near line %s:%d\n",
												script->filename, scriptfile_getlinum(script,paltokptr));
									break;
								}
								hicsetsubsttex(tile,pal,fn,alphacut,flags, sizex, sizey);
							} break;
							default: break;
						}
					}
					
					if ((unsigned)tile >= (unsigned)MAXTILES) {
						initprintf("Error: missing or invalid 'tile number' for texture definition near line %s:%d\n",
									script->filename, scriptfile_getlinum(script,texturetokptr));
						break;
					}
				}
				break;

			case T_UNDEFMODEL:
			case T_UNDEFMODELRANGE:
				{
					int r0,r1;
						
					if (scriptfile_getsymbol(script,&r0)) break;
					if (tokn == T_UNDEFMODELRANGE) {
						if (scriptfile_getsymbol(script,&r1)) break;
						if (r1 < r0) {
							int t = r1;
							r1 = r0;
							r0 = t;
							initprintf("Warning: backwards tile range on line %s:%d\n", script->filename, scriptfile_getlinum(script,cmdtokptr));
						}
						if (r0 < 0 || r1 >= MAXTILES) {
							initprintf("Error: invalid tile range on line %s:%d\n", script->filename, scriptfile_getlinum(script,cmdtokptr));
							break;
						}
					} else {
						r1 = r0;
						if ((unsigned)r0 >= (unsigned)MAXTILES) {
							initprintf("Error: invalid tile number on line %s:%d\n", script->filename, scriptfile_getlinum(script,cmdtokptr));
							break;
						}
					}
#if defined(POLYMOST) && defined(USE_OPENGL)
					for (; r0 <= r1; r0++) md_undefinetile(r0);
#endif
				}
				break;

			case T_UNDEFMODELOF:
				{
					int mid,r0;

					if (scriptfile_getsymbol(script,&r0)) break;
					if ((unsigned)r0 >= (unsigned)MAXTILES) {
						initprintf("Error: invalid tile number on line %s:%d\n", script->filename, scriptfile_getlinum(script,cmdtokptr));
						break;
					}

#if defined(POLYMOST) && defined(USE_OPENGL)
					mid = md_tilehasmodel(r0);
					if (mid < 0) break;

					md_undefinemodel(mid);
#endif
				}
				break;

			case T_UNDEFTEXTURE:
			case T_UNDEFTEXTURERANGE:
				{
					int r0,r1,i;

					if (scriptfile_getsymbol(script,&r0)) break;
					if (tokn == T_UNDEFTEXTURERANGE) {
						if (scriptfile_getsymbol(script,&r1)) break;
						if (r1 < r0) {
							int t = r1;
							r1 = r0;
							r0 = t;
							initprintf("Warning: backwards tile range on line %s:%d\n", script->filename, scriptfile_getlinum(script,cmdtokptr));
						}
						if (r0 < 0 || r1 >= MAXTILES) {
							initprintf("Error: invalid tile range on line %s:%d\n", script->filename, scriptfile_getlinum(script,cmdtokptr));
							break;
						}
					} else {
						r1 = r0;
						if ((unsigned)r0 >= (unsigned)MAXTILES) {
							initprintf("Error: invalid tile number on line %s:%d\n", script->filename, scriptfile_getlinum(script,cmdtokptr));
							break;
						}
					}

					for (; r0 <= r1; r0++)
						for (i=MAXPALOOKUPS-1; i>=0; i--)
							hicclearsubst(r0,i);
				}
				break;

            case T_TILEFROMTEXTURE:
                {
                    char *texturetokptr = script->ltextptr, *textureend, *fn = NULL;
                    int tile = -1;
                    int alphacut = 255;
                    int xoffset = 0, yoffset = 0;

                    static const tokenlist tilefromtexturetokens[] =
                    {
                        { "file",            T_FILE },
                        { "name",            T_FILE },
                        { "alphacut",        T_ALPHACUT },
                        { "xoffset",         T_XOFFSET },
                        { "xoff",            T_XOFFSET },
                        { "yoffset",         T_YOFFSET },
                        { "yoff",            T_YOFFSET },
                    };

                    if (scriptfile_getsymbol(script,&tile)) break;
                    if (scriptfile_getbraces(script,&textureend)) break;
                    while (script->textptr < textureend)
                    {
                        int token = getatoken(script,tilefromtexturetokens,sizeof(tilefromtexturetokens)/sizeof(tokenlist));
                        switch (token)
                        {
                        case T_FILE:
                            scriptfile_getstring(script,&fn); break;
                        case T_ALPHACUT:
                            scriptfile_getsymbol(script,&alphacut); break;
                        case T_XOFFSET:
                            scriptfile_getsymbol(script,&xoffset); break;
                        case T_YOFFSET:
                            scriptfile_getsymbol(script,&yoffset); break;
                        default:
                            break;
                        }
                    }

                    if ((unsigned)tile >= MAXTILES)
                    {
                        initprintf("Error: missing or invalid 'tile number' for texture definition near line %s:%d\n",
                                   script->filename, scriptfile_getlinum(script,texturetokptr));
                        break;
                    }

                    if (!fn)
                    {
                        initprintf("Error: missing 'file name' for tilefromtexture definition near line %s:%d\n",
                                   script->filename, scriptfile_getlinum(script,texturetokptr));
                        break;
                    }

                    if (check_file_exist(fn))
                        break;

                    alphacut = clamp(alphacut, 0, 255);

                    {
                        int xsiz, ysiz, j;
                        palette_t *picptr = NULL;

                        kpzload(fn, (intptr_t *)&picptr, &j, &xsiz, &ysiz);
        //                initprintf("\ngot bpl %d xsiz %d ysiz %d",bpl,xsiz,ysiz);

                        if (!picptr)
                            break;

                        if (xsiz <= 0 || ysiz <= 0)
                            break;

                        xoffset = clamp(xoffset, -128, 127)&255;
                        yoffset = clamp(yoffset, -128, 127)&255;

                        set_picsizanm(tile, xsiz, ysiz, (picanm[tile]&0xff0000ff)+(xoffset<<8)+(yoffset<<16));

                        tile_from_truecolpic(tile, picptr, alphacut);

                        Bfree(picptr);
                    }
                }
                break;

			default:
				initprintf("Unknown token.\n"); break;
		}
	}
	return 0;
}


int loaddefinitionsfile(char *fn)
{
	scriptfile *script;

	script = scriptfile_fromfile(fn);
	if (!script) return -1;

	defsparser(script);

	scriptfile_close(script);
	scriptfile_clearsymbols();

	return 0;
}

// vim:ts=4:
