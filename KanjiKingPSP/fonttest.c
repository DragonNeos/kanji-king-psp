/*
 * fonttest.c
 * This file is used to display the PSP's internal font (pgf and bwfon firmware files)
 * intraFont Version 0.31 by BenHur - http://www.psp-programming.com/benhur
 *
 * Uses parts of pgeFont by InsertWittyName - http://insomniac.0x89.org
 *
 * This work is licensed under the Creative Commons Attribution-Share Alike 3.0 License.
 * See LICENSE for more details.
 *
 */

#include <pspkernel.h>
#include <stdio.h>
#include <string.h>
#include <pspgu.h>
#include <pspgum.h>
#include <pspdisplay.h>
//#include <time.h>     //only needed for the blinking effect
//#include <pspdebug.h> //only needed for indicating loading progress

#include <oslib/intraFont/intraFont.h>

#include <pspctrl.h>

#include <malloc.h>
//#include <stdio.h>

//#include <ft2build.h>
//#include FT_FREETYPE_H
//#include <SDL/SDL_ttf.h>
//#include "GL/gl.h"

PSP_MODULE_INFO("intraFontTest", PSP_MODULE_USER, 1, 1);

static int running = 1;
static unsigned int __attribute__((aligned(16))) list[262144];

int exit_callback(int arg1, int arg2, void *common) {
	running = 0;
	return 0;
}

int CallbackThread(SceSize args, void *argp) {
	int cbid = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
	sceKernelRegisterExitCallback(cbid);
	sceKernelSleepThreadCB();
	return 0;
}

int SetupCallbacks(void) {
	int thid = sceKernelCreateThread("CallbackThread", CallbackThread, 0x11, 0xFA0, PSP_THREAD_ATTR_USER, 0);
	if (thid >= 0) sceKernelStartThread(thid, 0, 0);
	return thid;
}

#define BUF_WIDTH (512)
#define SCR_WIDTH (480)
#define SCR_HEIGHT (272)


//-----------------------------------------------------------------------------
void DrawString(const char *str, float x, float y)
{



}

//#include <c++/4.3.5/string>

//-----------------------------------------------------------------------------
int main() {
//	pspDebugScreenInit();
	SetupCallbacks();

	SceCtrlData pad_data;
	sceCtrlSetSamplingCycle(0);
	sceCtrlSetSamplingMode(1);

//----
/*	FT_Library	ftLib;
	FT_Face		face;
	FT_Error	error;

	error = FT_Init_FreeType(&ftLib);
	if(error)
		pspDebugScreenPrintf("FT_Init_FreeType = %d", error);

	error = FT_New_Face(ftLib, "ipagui.ttf", 0, &face);

	if(error)
		pspDebugScreenPrintf("FT_New_Face = %d", error);
*/

//----
	SceUID in = sceIoOpen("heisig.utf-8.txt", PSP_O_RDONLY, 0777);
//	FILE  *in = fopen("heisig.txt", "r");
//	std::string heisig;
	char *heisig = NULL;
	size_t heisig_size = 0;
	if(in)
	{
		SceOff bytes = sceIoLseek(in, 0, SEEK_END);
//		fseek(in, 0, SEEK_END);
//		long bytes = ftell(in);
//		fseek(in, 0, SEEK_SET);
		sceIoLseek(in, 0, SEEK_SET);

		if((heisig = malloc(bytes+1)))
		{
			heisig_size = sceIoRead(in, heisig, bytes);
//			heisig_size = fread(heisig, 1, bytes, in);
		}
//		fclose(in);
		sceIoClose(in);
		heisig[bytes] = 0;
	}

	//----
	// Colors
	enum colors {
		RED =	0xFF0000FF,
		GREEN =	0xFF00FF00,
		BLUE =	0xFFFF0000,
		WHITE =	0xFFFFFFFF,
		LITEGRAY = 0xFFBFBFBF,
		GRAY =  0xFF7F7F7F,
		DARKGRAY = 0xFF3F3F3F,		
		BLACK = 0xFF000000,
	};


	// Init intraFont library
	intraFontInit();

	// Load fonts
/*	intraFont* ltn[16];                                         //latin fonts (large/small, with/without serif, regular/italic/bold/italic&bold)

	for (int i = 0; i < 16; i++)
	{
		char file[40];
		sprintf(file, "flash0:/font/ltn%d.pgf", i); 
		ltn[i] = intraFontLoad(file,0);                                             //<- this is where the actual loading happens 
		intraFontSetStyle(ltn[i], 1.0f, WHITE, BLACK, 0);
	}

	ltn[8] = intraFontLoad("flash0:/font/ltn8.pgf",0);
	intraFontSetStyle(ltn[8], 1.0f, WHITE, BLACK, 0);
*/
	intraFont* jpn0 = intraFontLoad("flash0:/font/jpn0.pgf", INTRAFONT_STRING_UTF8);
//	intraFontSetStyle(jpn0, 1.0f, WHITE, BLACK, 0);

//	intraFont* kr0 = intraFontLoad("flash0:/font/kr0.pgf", INTRAFONT_STRING_UTF8);
//	intraFontSetStyle(kr0, 1.0f, WHITE, BLACK, 0);

//	intraFont* arib = intraFontLoad("flash0:/font/arib.pgf",0);
//	intraFontSetStyle(arib, 1.0f, WHITE, DARKGRAY, 0);


//	intraFont* chn = intraFontLoad("flash0:/font/gb3s1518.bwfon", 0);
//	intraFontSetStyle(chn, 1.0f, WHITE, DARKGRAY, 0);


	// Set alternative fonts that are used in case a certain char does not exist in the main font
//	intraFontSetAltFont(ltn[8], jpn0);                     //japanese font is used for chars that don't exist in latin
//	intraFontSetAltFont(jpn0, chn);                        //chinese font (bwfon) is used for chars that don't exist in japanese (and latin)
//	intraFontSetAltFont(jpn0, kr0);                        //korean font is used for chars that don't exist in chinese (and jap and ltn)
//	intraFontSetAltFont(kr0, arib);                        //symbol font is used for chars that don't exist in korean (and chn, jap & ltn)
	// NB: This is an extreme case - usually you need only one alternative font (e.g. japanese & chinese)
	// Also: if one of the fonts failed to load (e.g. chn) the chain breaks and all subequent fonts are not used (e.g. kr0 and arib)
	
	// Init GU    
	sceGuInit();
	sceGuStart(GU_DIRECT, list);

	sceGuDrawBuffer(GU_PSM_8888, (void*)0, BUF_WIDTH);
	sceGuDispBuffer(SCR_WIDTH, SCR_HEIGHT, (void*)0x88000, BUF_WIDTH);
	sceGuDepthBuffer((void*)0x110000, BUF_WIDTH);
 
	sceGuOffset(2048 - (SCR_WIDTH/2), 2048 - (SCR_HEIGHT/2));
	sceGuViewport(2048, 2048, SCR_WIDTH, SCR_HEIGHT);
	sceGuDepthRange(65535, 0);
	sceGuScissor(0, 0, SCR_WIDTH, SCR_HEIGHT);
	sceGuEnable(GU_SCISSOR_TEST);
	sceGuDepthFunc(GU_GEQUAL);
	sceGuEnable(GU_DEPTH_TEST);
	sceGuFrontFace(GU_CW);
	sceGuShadeModel(GU_SMOOTH);
	sceGuEnable(GU_CULL_FACE);
	sceGuEnable(GU_CLIP_PLANES);
	sceGuEnable(GU_BLEND);
	sceGuBlendFunc(GU_ADD, GU_SRC_ALPHA, GU_ONE_MINUS_SRC_ALPHA, 0, 0);
	sceGuFinish();
	sceGuSync(0,0);

	sceDisplayWaitVblankStart();
	sceGuDisplay(GU_TRUE);

	char tmp[64];
	
	while(running)
	{
		sceGuStart(GU_DIRECT, list);

		sceGumMatrixMode(GU_PROJECTION);
		sceGumLoadIdentity();
		sceGumPerspective( 75.0f, 16.0f/9.0f, 0.5f, 1000.0f);

		sceGumMatrixMode(GU_VIEW);
		sceGumLoadIdentity();

		sceGumMatrixMode(GU_MODEL);
		sceGumLoadIdentity();
		
		sceGuClearColor(BLACK);
		sceGuClearDepth(0);
		sceGuClear(GU_COLOR_BUFFER_BIT|GU_DEPTH_BUFFER_BIT);

	//----
		sceCtrlReadBufferPositive(&pad_data, 1);
		if(pad_data.Buttons & PSP_CTRL_CIRCLE)
		{
			intraFontPrint(jpn0, 40.0f, 40.0f, "下の成績 [げのせいせき] \n/schlechte Zensur/Note im unteren Bereich/");
		}

		if(pad_data.Buttons & PSP_CTRL_CROSS)
		{
//			int k = 0;
			char *ptr = &heisig[0];
			float y;
			int l = 0;
			for(y = 0.0f; y <= 272.0f; y += 18.0f)
			{
				for(l = 0; ptr[l]!='	';)
					l++;
				intraFontPrintEx(jpn0, 20.0f, y, ptr, 1);
				ptr += l+1;
				for(l = 0; ptr[l]!='	';)
					l++;
				intraFontPrintEx(jpn0, 60.0f, y, ptr, l);
				ptr += l+1;
				for(l = 0; ptr[l]!='\n';)
					l++;
				intraFontPrintEx(jpn0, 220.0f, y, ptr, l);
				ptr += l+1;

//				intraFontPrintEx(jpn0, 10.0f, y, tmp, sprintf(tmp, "%d", heisig[k++]));

			}
		}


		// End drawing
		sceGuFinish();
		sceGuSync(0,0);
		
		// Swap buffers (waiting for vsync)
		sceDisplayWaitVblankStart();
		sceGuSwapBuffers();
	}
	

	free(heisig);

	intraFontUnload(jpn0);

	// Shutdown font library
	intraFontShutdown();

	sceKernelExitGame();

	return 0;
}
