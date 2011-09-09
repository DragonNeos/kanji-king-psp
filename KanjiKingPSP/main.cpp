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
//#include <stdio.h>
//#include <string.h>
#include <pspgu.h>
#include <pspgum.h>
#include <pspdisplay.h>
//#include <time.h>     //only needed for the blinking effect
//#include <pspdebug.h> //only needed for indicating loading progress

//#include <oslib/intraFont/intraFont.h>

#include <pspctrl.h>
#include <psprtc.h>

#include <math.h>

#include "ttf.h"
#include "SimpleRNG.h"

PSP_MODULE_INFO("Kanji King", PSP_MODULE_USER, 1, 1);

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

#define	UNICODE_RADICALS	0x2F00		// first codepoint of all 214 Kangxi radicals
#define	UNICODE_CJK			0x4E00		// first codepoint of all Kanji

SimpleRNG rng;

//-------------------------------------------------------------------------------------------------
//	@return random number in [0,1)
inline double randd()
{
//	static const double f = 1.0 / double(RAND_MAX+1);
//	return 4.656612873077392578125e-10 * rand();
	return rng.rand();
}

//	@return random number in [0,limit)
int rand(int limit)
{
	return int(randd() * double(limit));
}

BYTE randSquared()
{
	const	u32	u = rng.randUint(),
					a = 0xFFFFu &  u,
					b = 0xFFFFu & (u >> 16),
					s = a * b;

	return 5.960646379255e-8	 * s;			//	= 256 / (0xFFFF * 0xFFFF + 1) * s
}

//-------------------------------------------------------------------------------------------------
//	@return the substring after the first n encountered delimiters or after the first newline, or the position of the first encountered NULL character.
const char *advance(const char *ptr, char delimiter = '	', int n = 1)
{
	for(; *ptr && n; ptr++)
	{
		if(*ptr == delimiter)
			n--;
		if(*ptr == '\n' || n == 0)
			return ptr + 1;
	}

	return ptr;
}

//-------------------------------------------------------------------------------------------------
u32 ColorLevel(BYTE b)
{
	const int	L = 2 * int(b);
	const BYTE	R = min(255, 510 - L),
					G = min(255, L),
					B = 0x0F;
	return 0xFF000000 | (B<<16) | (G<<8) | R;
}

double ProbLevel(BYTE b)
{
//	return 0.00390625 * (257.0 / (1.0 + double(b)) - 1.0);
	return 0.00390625 * (256 - int(b));
}

//-------------------------------------------------------------------------------------------------
int main()
{
	u64 tick1, tick2;
	const double TickToSeconds = 1.0 / sceRtcGetTickResolution();
	sceRtcGetCurrentTick(&tick1);
//	srand((unsigned)last_tick);

//----
	SetupCallbacks();

	SceCtrlData pad_data;
	sceCtrlSetSamplingCycle(0);
	sceCtrlSetSamplingMode(1);

	FT_Init_FreeType(&FTLib);

//----
//	Load resources
	const string	heisig(file("data/heisig.txt")),				// kanji	german	english
						radikale(file("data/radikale.txt")),		// 214 radicals with german meanings
						kanjidic(file("data/kanjidic.txt")),		// 214 radicals + >3000 kanji (all <= 0xFFFF)
						components(file("data/heisig.components.txt")),
						vocab(file("data/vocab.txt")),
						francais(file("data/french.txt"));

	if(heisig == false)
		sceKernelExitGame();

	int		nKanji		= 0,				// # heisig kanji
				nVocab		= 0,				// # vocab lines (= items)
				nFrancais	= 0,
				testLimitK	= 0,				// Kanji with this or higher indices are not tested
				testLimitV	= 0,				// Vocab ...
				testLimitF	= 0,				// Francais ...
				select		= 0,				// 3rd (=middle) kanji displayed in list mode
				prevSelect	= -1,
				nKanjidic	= 0,				// # kanjidic lines (= kanji)
				cursor		= 0,				// Selected item, when scrolling through stuff
				prevCursor	= 0,
				scrollIdx	= 0,				// First displayed line, when not everything fits on sceen
				cycleIdx		= 0;				// Kanji which is selected from vocabulary

//----
//	Decode UTF-8
	UTF8		utf8;
	u32		codepoint = 0;
	bool		newline = true,
				newKanji = true;

//	Temporariliy use the VRAM to collect pointers to all Kanjis = newlines
	const char	**tmp1 = (const char**) VRAM,				//  512 kiB
					**tmp2 = tmp1 + 512*1024,					//  512 kiB
					*cPtr = &components[0];
	u16			*tmp3 = (u16*) SCRATCHPAD,					//   16 kiB -> 8192 entries
					*tmp4 = (u16*) TMP_VRAM;					//  960 kiB

	for(const char *s = &heisig[0]; *s; ++s)
	{
		if(newline)
		{
			newline = false;

			tmp1[nKanji] = s;					// pointer to the kanji	german	english
			tmp2[nKanji] = cPtr;				// pointer to the components

			cPtr = advance(cPtr, '\n');
		}

		if(!utf8.decode(&codepoint, (BYTE)*s))
		{
			if(newKanji)
			{
				newKanji = false;
				tmp3[nKanji] = codepoint;
				nKanji++;
			}

		//	New character decoded
			newKanji = newline = (codepoint == '\n');
		}
	}

//	Copy tmp pointers to proper vector
	const vec<const char*>	kanjiPointer(nKanji, tmp1);
	const vec<const char*>	compoPointer(nKanji, tmp2);
	const vec<u16>				kanjiUnicode(nKanji, tmp3);
			vec<u16>				kanjiDicLink(nKanji);				// heisig kanji index -> kanjidic line
			vec<BYTE>			kanjiVocabCount(nKanji);			// # of vocab entries with this kanji
	u16	totalVocabLinks = 0;

//----
//	Collect vocab lines for each kanji into tmp4
//	Max entries per kanji = 128 -> max. nKanji = 3840
	kanjiVocabCount.zero();

//	Count vocab lines and collect pointers to each line in TMP_VRAM
	for(const char *s = &vocab[0]; *s; s = advance(s, '\n'))
	{
	// pointer to the vocab line
		tmp1[nVocab] = s;

	//	decode entry and check for kanji
		utf8.state = 0;

		for(; s[0] && s[0]!='	'; ++s)
			if(!utf8.decode(&codepoint, (BYTE)*s))
			{
			//	New character
				for(int i = 0; i < nKanji; i++)
				{
				// scratchpad = faster?
				//	if((u16)codepoint == kanjiUnicode[i])
					if((u16)codepoint == tmp3[i] && kanjiVocabCount[i] < 128)
					{
						tmp4[128*i + kanjiVocabCount[i]++] = nVocab;			// record line number
						totalVocabLinks++;
					}
				}
			}

		nVocab++;
	}

//	Copy tmp pointers to proper vector
//	Consolidate tmp4 into main memory
	vec<const char*>	vocabPointer(nVocab);						// pointer to all vocab lines
	vec<u16>				kanjiVocabLines(totalVocabLinks),		// kanjiVocabCount vocab lines for each kanji
							kanjiVocabIndex(nKanji);					// offset into kanjiVocabLines for each kanji
//	reverse vocab direction, because JLPT4 data is at the end of the file.
	for(int i = 0; i < nVocab; i++)
		vocabPointer[i] = tmp1[nVocab-1 - i];

	u16 offset = 0;
	for(int i = 0; i < nKanji; i++)
	{
		kanjiVocabIndex[i] = offset;
		for(BYTE n = 0; n < kanjiVocabCount[i]; n++)
			kanjiVocabLines[offset+n] = nVocab-1 - tmp4[128*i+kanjiVocabCount[i]-1-n];
//		memcpy(&kanjiVocabLines[offset], &tmp4[128*i], kanjiVocabCount[i]*sizeof(u16));
		offset += kanjiVocabCount[i];
	}

//----
//	Collect kanjidic codepoints and newlines
	for(const char *s = &kanjidic[0]; *s; s = advance(s, '\n'))
	{
		tmp1[nKanjidic] = s;					// pointer to the kanjidic line

		for(utf8.state = 0; utf8.decode(&codepoint, (BYTE)*s); ++s)
			;

		tmp3[nKanjidic++] = codepoint;	// codepoint of the kanji on this line
	}

//----
//	Make links for all heisig kanji
	for(int i = 0; i < nKanji; i++)
	{
		u16	code = kanjiUnicode[i];
		int	link = 0;
		for(; link < nKanjidic; link++)
			if(code == tmp3[link])
				break;

		kanjiDicLink[i] = link;
	}

//	Copy tmp pointers to proper vector
	const vec<const char*>	kanjidicPointer(nKanjidic, tmp1);
	const vec<u16>				kanjidicUnicode(nKanjidic, tmp3);

//----
//	Collect francais newlines
	for(const char *s = &francais[0]; *s; s = advance(s, '\n'))
		tmp1[nFrancais++] = s;				// pointer to the francais line
//	testLimitF = nFrancais;					// test everything from the beginning

//	Copy tmp pointers to proper vector
	const vec<const char*>	francaisPointer(nFrancais, tmp1);

//----
//	Load progress
	vec<BYTE>	levelKanji(nKanji),
					levelVocab(nVocab),
					levelFranc(nFrancais);
	{
		levelKanji.zero();
		levelVocab.zero();
		levelFranc.zero();
		file	a("kanji.stats"),
				b("vocab.stats"),
				c("franc.stats");
		a.read(levelKanji.data, levelKanji.length*sizeof(BYTE));
		b.read(levelVocab.data, levelVocab.length*sizeof(BYTE));
		c.read(levelFranc.data, levelFranc.length*sizeof(BYTE));
	}

//----
//	Font font1("fonts/meiryo.ttc");
//	Font font1("fonts/ipagui.ttf", 42);
	Font font1("fonts/ipaexg.ttf", 42);
	Font font2("fonts/ipam.ttf", 42);
//	Font font2("fonts/Cyberbit.ttf", 42);
//	Font font2("fonts/mingliu.ttc", 42);
//	Font font2("fonts/HanaMinA.ttf", 42);
	Font font3("fonts/kaiu.ttf", 42);

//----
//	Seed RNG
	sceRtcGetCurrentTick(&tick2);
	rng.seed(tick1, tick2);
	tick1 = 0;
	
//----
//	Init GU
	sceGuInit();
	sceGuStart(GU_DIRECT, list);

	void *framebuffer = 0x0;
	sceGuDrawBuffer(GU_PSM_8888, framebuffer, BUF_WIDTH);
//	sceGuDispBuffer(SCR_WIDTH, SCR_HEIGHT, framebuffer, BUF_WIDTH);				// single buffer
	sceGuDispBuffer(SCR_WIDTH, SCR_HEIGHT, (void*)0x88000, BUF_WIDTH);			// double buffer
	sceGuOffset(2048 - (SCR_WIDTH/2), 2048 - (SCR_HEIGHT/2));
	sceGuViewport(2048, 2048, SCR_WIDTH, SCR_HEIGHT);
	sceGuScissor(0, 0, SCR_WIDTH, SCR_HEIGHT);
	sceGuEnable(GU_SCISSOR_TEST);
	sceGuDisable(GU_DEPTH_TEST);
	sceGuClearColor(BLACK);
	sceGuFinish();
//	sceGuSync(0,0);
	sceGuDisplay(GU_TRUE);

//----
//	Games state and logic
	unsigned	currButtons	= 0,
				prevButtons	= 0,
				pressed		= 0,				// just pressed buttons
				realeased	= 0;				// just released buttons
//				infoMode		= 0;				// what is displayed on the main screens right hand side
	int		currMode		= -1,				// current "screen" - determines what is displayed and how buttons work
				prevMode		= 0,				// still displayed previous frame
				testMode		= 0,				// what is being tested: kanji or vocab or francais?
				prevTestMode= 0,
				q0				= -1,				// current  kanji or vocab index = question
				q1				= -1;				// previous kanji or vocab index = answer
	BYTE		prevLevel	= 0;				// To restore the level after a mistaken answer
	bool		german		= false,			// Which language to use for translations: english or german?
				correct		= false;			// if q1 was answered correctly
	Font		*font			= &font1;
	u16		q1Kanji[8],						// unicode codepoints of kanji in q1
				q1KanjiCount= 0;
	bool		modeUsed[3]	= {false,false,false};

//----
	while(running)
	{
	//----
	//	Input
		sceCtrlReadBufferPositive(&pad_data, 1);

		currButtons = pad_data.Buttons;
		unsigned
		changed		= currButtons ^ prevButtons;			// buttons that were either pressed or released
		pressed		= currButtons & changed;				// buttons that were just pressed
		realeased	= prevButtons & changed;				// buttons that were just released

	//----
		if(pressed && currMode == 100)
		{
			pressed  = 0;
			currMode = 0;
		}

	//----
		if(pressed & PSP_CTRL_DOWN)		cursor+=1;
		if(pressed & PSP_CTRL_UP)			cursor-=1;
		if(pressed & PSP_CTRL_RIGHT)		cursor+=7;
		if(pressed & PSP_CTRL_LEFT)		cursor-=7;
		if(currMode == 0
		&&	pressed & PSP_CTRL_SELECT)
		{
			testMode++;
			if(testMode > 2)
				testMode = 0;
		}
		if(currMode == 1
		&&	pressed & PSP_CTRL_SELECT)
		{
			currMode = 5;
			pressed = 0;
		}
		const bool	v	= (testMode == 1),
						fr	= (testMode == 2);
		if(currMode == 1
		||	currMode == 3)						select += cursor - prevCursor;
		if(pressed & PSP_CTRL_RTRIGGER)	currMode == 2 ? scrollIdx++ : german = !german;
		if(pressed & PSP_CTRL_TRIANGLE)	scrollIdx++;
		if(pressed & PSP_CTRL_CROSS)
		if(currMode==3)
			pressed |=PSP_CTRL_START;
		if(pressed & PSP_CTRL_START)
		{
			switch(currMode)
			{
			case 0:	currMode = 3;
						cycleIdx = 0;	break;
			default:	cycleIdx++;		break;
			}
		}
		if(pressed & PSP_CTRL_CIRCLE)
		{
			switch(currMode)
			{
			case 1:	currMode = 2;	break;
			case 3:	currMode = 2;	break;
			}
		}
		if(pressed & PSP_CTRL_CROSS)
		{
			switch(currMode)
			{
			case 1:	currMode = 3;	break;
			case 2:	currMode = 3;	break;
			}
		}
		if(pressed & PSP_CTRL_SQUARE)
		{
			switch(currMode)
			{
			case 1:	currMode = 0;	break;
			default:	currMode = 1;	break;
			}
		}
		if(pressed & PSP_CTRL_LTRIGGER)
		{
			if			(font==&font1)	font = &font2;
			else if	(font==&font2)	font = &font3;
			else if	(font==&font3)	font = &font1;
		}


	//----
	//	shortcuts
		vec<BYTE> &level	= fr ? levelFranc : v ? levelVocab : levelKanji;
		int &testLimit		= fr ? testLimitF : v ? testLimitV : testLimitK;
		const int &limit	= fr ? nFrancais  : v ? nVocab     : nKanji;
		const vec<const char*> &src = fr ? francaisPointer : v ? vocabPointer : kanjiPointer;

	//----
	//	Draw only if necessary
		if(pressed || currMode!=prevMode)
		{
		//----
		//	Clear sreen
			sceGuStart(GU_DIRECT, list);
			sceGuClear(GU_COLOR_BUFFER_BIT);
			sceGuFinish();

		//	Test mode?
			if(testMode!=prevTestMode)
			{
				prevMode = 0;
				currMode =
				q0 =
				q1 = -1;
			}

			if(currMode == 0)
			{
				if(pressed & PSP_CTRL_RIGHT)	testLimit = min(testLimit+ 10, limit);
				if(pressed & PSP_CTRL_LEFT)	testLimit = max(testLimit- 10, 12);
				if(pressed & PSP_CTRL_UP)		testLimit = min(testLimit+100, limit);
				if(pressed & PSP_CTRL_DOWN)	testLimit = max(testLimit-100, 12);
			}

		//----
		//	Reset mode
			if(currMode == 5)
			{
				if(currButtons == (PSP_CTRL_LTRIGGER|PSP_CTRL_RTRIGGER))
				{
					level.zero();
					q0 =
					q1 = -1;
					currMode = 1;
				}
				else if(pressed && (currButtons & (PSP_CTRL_LTRIGGER|PSP_CTRL_RTRIGGER)) == 0)
					currMode = 1;
				else
				{
					font1.print(framebuffer, 32.0f, 100.0f, "Press L+R to DELETE current progress.", 0, 20);
					font1.print(framebuffer, 32.0f, 200.0f, "Press any other key to cancel.", 0, 20);
				}
			}

		//----
		//	Vocab mode
			if(currMode == 2)
			{
				if(currMode != prevMode)
					cursor = 0;

				if(select >= 0)
				{
					int	count			= kanjiVocabCount[select] + 1,
							prevIndex	= prevCursor % count,
							index			= cursor     % count;
					if(prevIndex != index)
						scrollIdx = 0;
					while(index < 0)
						index += count;

					float	x =   8.0f,
							y = 140.0f;
					font1.setSize(42);
					if(index == count-1)
					{
						font1.print(framebuffer, x, 50.0f, kanjiUnicode[select]);
						font1.print(framebuffer, x, y, advance(kanjidicPointer[kanjiDicLink[select]],'	', 8), '	', 18);
						sprintf((char*)SCRATCHPAD, "0 / %u", count-1);
						font1.print(framebuffer, 400.0f, 54.0f, (char*)SCRATCHPAD);
					}
					else
					{
						int	lineNumber	= kanjiVocabLines[kanjiVocabIndex[select]+index];

						const char	*ptr	= vocabPointer[lineNumber];
						ptr = font1.print(framebuffer, x,      50.0f, ptr, '	', 42);
						ptr = font1.print(framebuffer, x,     100.0f, ptr, '	');
								font1.print(framebuffer, 400.0f, 32.0f, "JLPT", 0, 18);
						ptr = font1.print(framebuffer, 444.0f, 32.0f, ptr, '	');
						sprintf((char*)SCRATCHPAD, "%u / %u", index+1, count-1);
								font1.print(framebuffer, 400.0f, 54.0f, (char*)SCRATCHPAD);
						bool ger = german;
						int records = 1;
						if(ger)
						{
							const char *p2 = advance(ptr);
							if(p2[0] == '?')
								ger = false;		// No german definition -> temporarily switch to english
							else
							{
							//	count # of meanings
								for(ptr = p2; *ptr != '\n'; ptr++)
									if(*ptr == '\x1E')
										records++;
								ptr = p2;
							}
						}
						if(ger)
						{
							int	pages = 1 + (records-1) / 5,
									page	= scrollIdx % pages,
									skip	= 5 * page;
							ptr = advance(ptr, '\x1E', skip);
							if(pages > 1)
							{
								font1.setSize(20);
								if(page == pages-1)	font1.print(framebuffer, -10.0f, 132.0f, 0x2191);
								if(page < pages-1)	font1.print(framebuffer, -10.0f, 268.0f, 0x2193);
							}
						}
						while(ptr[-1]!='\n')
						{
							ptr = font1.print(framebuffer, x, y, ptr, ger ? '\x1E' : '	', 18, ger ? 15.0f/18.0f : 1.5f);
											
							if(!ger || (y += 30.0f) > 270.0f)
								break;
						}
					}
				}
				else
				{
				//	No Vocab data
//					font1.setSize(18);
//					font1.print(framebuffer, 400.0f, 32.0f, "no data");
//					font1.print(framebuffer, 400.0f, 54.0f, "0 / 0");
//					currMode = 1;
				}
			}


		//----
		//	Buffer all kanji in select
			const bool jump = (currMode==3 && prevMode==1 && v && (pressed & PSP_CTRL_CROSS));
			if(jump)
			{
				utf8.state = 0;
				q1KanjiCount = 0;
				const char *str = src[select];
				while((codepoint = utf8.decode(&str)))
					if(codepoint == '	')
						break;
					else
					if(codepoint >= UNICODE_CJK)		// Is Kanji?
						q1Kanji[q1KanjiCount++] = codepoint;
			}

		//----
		//	Jump to q1 entry or kanji
			if(currMode == 1 || currMode == 3)
			if(prevMode == 0 || jump || (pressed & PSP_CTRL_START))
			{
				if(!v || fr || (currMode == 1 && !jump))
				{
					if(q1 > 0)	select = q1;
					else			select = testLimitK-5;
				}
				else if(q1KanjiCount)
				{
					codepoint = q1Kanji[cycleIdx % q1KanjiCount];
					for(int i = 0; i < nKanji; i++)
						if(kanjiUnicode[i]==codepoint)
						{
//							prevSelect = select;
							select = i;
							break;
						}
				}
			}

		//----
		//	Restore old list position
			if(currMode==1 && (prevMode==3||prevMode==2) && v)
			if(prevSelect >= 0)
				select = prevSelect;

		//----
		//	List mode
			if(currMode == 1)
			{
				if(select < 0)			select += limit;
				if(select >= limit)	select -= limit;

				for(int k = 0; k < 7; k++)
				{
					const int   i = select - 3 + k;
					if(i < 0 || i >= limit)
						continue;
					const float y = 38.85f * k + 30.0f;
					const char *ptr = src[i];
					font->color =
					font1.color = k==3 ? WHITE : ColorLevel(level[i]);
					font1.setSize(20);
					sprintf((char*)SCRATCHPAD, "#%i", i+1);
							font1.print(framebuffer,   10.0f, y, (char*)SCRATCHPAD);
					if(v||fr)
					{
						 font->print(framebuffer,  96.0f, y, ptr, '	', 20);
					}
					else
					{
						ptr = font->print(framebuffer,   96.0f, y + (font==&font1 ? 3.0f : font==&font2 ? 3.0f : 0.0f), ptr, '	', 38);
						ptr = font1.print(framebuffer,  160.0f, y, german ? advance(ptr) : ptr, '	', 20);
//								font1.color = ColorLevel(levelKanji[i]);
					}
						sprintf((char*)SCRATCHPAD, "%i%%", int(0.392156862745098*level[i]+0.5));
								font1.print(framebuffer,  400.0f, y, (char*)SCRATCHPAD);
				}

				font->color =
				font1.color = WHITE;
				prevSelect = select;
			}


		//----
		//	Testing mode
			if(currMode == 0)
			{
			//----
			//	Revise answer from correct to false
				if(q1 >= 0)
				if(pressed & PSP_CTRL_TRIANGLE)					// restore backup
				{
					correct = !correct;
					level[q1] = correct ? prevLevel : 0.6666666666 * prevLevel;
					q0 = -1;
					sceRtcGetCurrentTick(&tick1);
				}

			//----
			//	Solve kanji
				if(currMode == prevMode && q0 >= 0)
				if(pressed & (PSP_CTRL_CROSS|PSP_CTRL_CIRCLE))
				{
				//	Adjust kanji grade, taking quickness into account
					correct = pressed & PSP_CTRL_CIRCLE;
					q1 = q0;
					prevLevel = level[q1];					// backup level

					sceRtcGetCurrentTick(&tick2);
					const double s = TickToSeconds * (tick2 - tick1);

					if(correct)	level[q1] = min(255.0, 0.6666666666 * prevLevel + 0.3333333333 * 255.0 * exp((v?-0.05:-0.1)*s));
					else			level[q1] = 0.6666666666 * prevLevel;

				//----
				//	Buffer all kanji in q1
					utf8.state = 0;
					q1KanjiCount = 0;
					const char *str = src[q1];
					while((codepoint = utf8.decode(&str)))
						if(codepoint == '	')
							break;
						else
						if(codepoint >= UNICODE_CJK)		// Is Kanji?
							q1Kanji[q1KanjiCount++] = codepoint;

				//----
				//	Adjust kanji in vocab
					if(v)
					{
						for(int i = 0; i < nKanji; i++)
						for(u16 n = 0; n < q1KanjiCount; n++)
							if(kanjiUnicode[i]==q1Kanji[n])
							{
								if(correct)	level[i] = max(255, int(level[i]) +  1);
								else			level[i] = min(  0, int(level[i]) - 10);
							}
					}

				//----
					q0 = -1;
					sceRtcGetCurrentTick(&tick1);

				//----
				//	Will save data to memorystick, when quitting
					modeUsed[testMode] = true;
				}

				if(q0 >= 0 && currMode != prevMode)
				{
					q0 = -1;
					tick1 = 0;
//					sceRtcGetCurrentTick(&tick1);
				}

			//----
			//	Draw solution (translation)
				if(q1 >= 0)
				{
					const char *ptr = src[q1];

					font->color =
					font1.color = correct ? 0xFFDFFF60 : 0xFF7CF1FF;
					if(fr)
					{
						ptr = font1.print(framebuffer, 4.0f, 160.0f, ptr, '	',  24);	// francais
						ptr = font1.print(framebuffer, 4.0f, 200.0f, ptr, '\n',   24);	// german
					}
					else if(v)
					{
						ptr = font1.print(framebuffer, 4.0f, 150.0f, ptr, '	', 40);	// expression
						ptr = font1.print(framebuffer, 4.0f, 200.0f, ptr, '	', 40);	// hiragana
					// translation
						ptr+=2;
						if(german)
							ptr = advance(ptr);
						ptr = font1.print(framebuffer, 4.0f, 235.0f, ptr, german?'\x1E':'	', 24, 1.2f);
					}
					else
					{
					//	Draw kanji
						ptr = font->print(framebuffer, 250.0f, font == &font1 ? 60.0f : font == &font2 ? 59.0f : 54.0f, ptr, '	', 60);

					//	Translation
						if(german)
							ptr = advance(ptr);
					//	int len = advance(ptr) - ptr;
					//	ptr = font1.print(framebuffer, 332.0f - 7.0f*len, 256.0f, ptr, '	', 24);
						ptr = font1.print(framebuffer, 250.0f, 88.0f, ptr, '	', 21);
					}

					if(!v && !fr)
					{
					//	Statistics
						font1.setSize(18);
						font1.color = ColorLevel(level[q1]);
						sprintf((char*)SCRATCHPAD, "#%i", q1+1);
						font1.print(framebuffer, 340.0f, 40.0f, (char*)SCRATCHPAD);
						sprintf((char*)SCRATCHPAD, "%i%%", q1>=0 ? int(0.392156862745098*level[q1]+0.5) : 0);
						font1.print(framebuffer, 340.0f, 60.0f, (char*)SCRATCHPAD);
						font->color =
						font1.color = WHITE;

					//	kanjidic data
						ptr = advance(kanjidicPointer[kanjiDicLink[q1]],'	',6);
						float	x = 250.0f,
								y =  90.0f;
						do
						{
							ptr = font1.print(framebuffer, x, y+=26.0f, ptr, ' ', 21, -2.0f);
						}while(ptr[-1]!='	');
						float	tmp = y;
						x = 340.0f;
						y =  90.0f;
						do
						{
							ptr = font1.print(framebuffer, x, y+=26.0f, ptr, ' ', 21, -2.0f);
						}while(ptr[-1]!='	');
						
							ptr = font1.print(framebuffer, 250.0f, max(tmp,y)+26.0f, ptr, '	', 18, 1.2f);
					}

					font->color =
					font1.color = WHITE;
				}
			}

		//----
		//	Components mode
			if(currMode == 3)
			{
			//	Draw kanji + components
				const char	*ptrComp = compoPointer[select];
						
				for(float y = 68.0f; ; y += 50.0f)
				{
					utf8.state	= 0;
					u32	*comp	= (u32*) SCRATCHPAD,
							count	= 1,
							phon	= 0,
							idx	= 0;
					for(const char *s = ptrComp; *s && s[0]!=' ' && s[0]!='\n'; ++s)
						if(!utf8.decode(&codepoint, (BYTE)*s))
						{
							if(codepoint >= UNICODE_CJK)		// Is Kanji?
								comp[idx++] = codepoint;
							if(codepoint == 0x273D)				// '*' = phonetic component
								phon = comp[idx-1];
							if(codepoint == 0x279E)				// '->' = original component
								count++;
						}

DRAW_COMP:
				//----
				//	Find component meaning
					if(count)
					{
						int			kanjidicIdx		= 0;
						const char	*meaning			= NULL,
										*japName			= NULL,	// japanese name
										*stats			= NULL,
										*on				= NULL,
										*kun				= NULL;
					//	Check heisig meaning
						for(int i = 0; meaning==NULL && i<nKanji; i++)
						{
							for(u32 k = 0; k < count; k++)
								if(kanjiUnicode[i] == comp[k])
								{
									meaning = advance(kanjiPointer[i]);
									if(german)
										meaning = advance(meaning);

								//----
//									kanjidicIdx = kanjiDicLink[i];
									break;
								}
						}

					//	Check radikale.txt
						int rad = 0;
						utf8.state = 0;
						for(const char *s = &radikale[0]; meaning==NULL && german && *s; ++s, ++rad)
						{
							if(!utf8.decode(&codepoint, (BYTE)*s)
							&&	codepoint >= UNICODE_CJK)		// Is Kanji?
							for(u32 k = 0; k < count; k++)
								if(codepoint == comp[k])
								{
									meaning = advance(s);
//									kanjidicIdx = rad;
								}
						}

					//	Check kanjidic.txt
						for( ; kanjidicIdx < nKanjidic; kanjidicIdx++)
						{
							for(u32 k = 0; k < count; k++)
								if(kanjidicUnicode[kanjidicIdx] == comp[k])
								{
									const char *mng;
									stats		= advance(kanjidicPointer[kanjidicIdx]);
									on			= advance(stats, '	', 5);
									kun		= advance(on);
									mng		= advance(kun);
									if(kanjidicIdx < 214)
										japName	= advance(mng);
									if(meaning == NULL)
									{
										meaning = mng;
								//		if(german && kanjidicIdx < 214)
								//			meaning = advance(advance(&radikale[0], '\n', kanjidicIdx));
									}
//									if(japName[-1]!='	')
//										japName = NULL;			// unknown japanese name
									break;
								}
						}

						font->setSize(42);
						font->color = y == 68.0f ? 0xFF80C6FF : phon==comp[0] ? 0xFF80D9AE : WHITE;
						font->print(framebuffer, 8.0f, y, comp[0]);
						font->color = WHITE;

						if(stats && y == 68.0f)
						{
						// get radical index
							int rad = 0;
							const char *s = advance(stats);
							if(s[-1] == '	')
							{
							//	Modify the original string, to avoid a memcpy
								((char*)s)[-1] = 0x0;
								rad = atoi(stats);
								((char*)s)[-1] = '	';
							}

								font1.color = 0x55FFFFFF;	font1.setSize(11);
								font1.print(framebuffer,   8.0f, 14.0f, "radical");
								font1.print(framebuffer, 130.0f, 14.0f, "strokes",  0x0);
								font1.print(framebuffer, 218.0f, 14.0f, "freq",     0x0);
								font1.print(framebuffer, 318.0f, 14.0f, "grade",    0x0);
								font1.print(framebuffer, 408.0f, 14.0f, "JLPT",     0x0);
								font1.color = 0xAAFFFFFF;	font1.setSize(16);
								font1.print(framebuffer,  50.0f, 16.0f, UNICODE_RADICALS+rad-1);
								font1.print(framebuffer,  72.0f, 16.0f, stats,    '	');
							s=	font1.print(framebuffer, 171.0f, 16.0f, s,         '	');
							s=	font1.print(framebuffer, 244.0f, 16.0f, s,         '	');
							s=	font1.print(framebuffer, 352.0f, 16.0f, s,         '	');
							s=	font1.print(framebuffer, 437.0f, 16.0f, s,         '	');
								font1.color = WHITE;
						}
						font1.color = phon==comp[0] ? 0xFF80D9AE : WHITE;
						if(on)							font1.print(framebuffer,  68.0f, y,       on,      '	', 18, -2.0f);
						font1.color = WHITE;
						if(kun && y == 68.0f)		font1.print(framebuffer,  68.0f, y-25.0f, kun,     '	', 18, -2.0f);
						if(meaning)						font1.print(framebuffer, 250.0f, y,       meaning, '	', 18);
						if(japName && y != 68.0f)	font1.print(framebuffer, 250.0f, y-25.0f, japName, '\n', 18);

						if(phon && phon != comp[0])
						{
							y += 50.0f;
							comp[0] = phon;
							count = 1;
							goto DRAW_COMP;
						}
					}


					ptrComp = advance(ptrComp, ' ');
					if(ptrComp[-1] == '\n')
						break;
				}
	
			}

		//----
		//	Level up
			if(currMode == 100)
			{
				font1.setSize(88);
				font1.color = WHITE;
				font1.print(framebuffer, 45.0f,  98.0f, "Level Up!");
				font1.color = 0xFF8356FF;
				font1.print(framebuffer, 42.0f,  95.0f, "Level Up!");
				font1.setSize(176);
				sprintf((char*)SCRATCHPAD, "%i", testLimit/10);
				font1.color = WHITE;
				font1.print(framebuffer, 103.0f, 260.0f, (char*)SCRATCHPAD);
				font1.color = 0xFFFFB566;
				font1.print(framebuffer, 100.0f, 257.0f, (char*)SCRATCHPAD);
				font1.color = WHITE;
			}

		//----
		//	Swap buffers
			if(currMode >= 0)
			{
				sceDisplayWaitVblankStart();
				framebuffer = sceGuSwapBuffers();
				prevMode		= currMode;
			}
			else
			{
			//	Restore previous mode, e.g. for initialization
				swap(currMode, prevMode);
			}

		//----
		//	Update testLimit
		//	We're doing it now, to not slow things down
			u32 sum = 0;
			for(size_t i = 0; i < level.length; i++)
				sum += level[i];

		//----
		// Each kanji with a rating of 155 (=5.0s reaction time) unlocks a new kanji
			int newLimit = min(max(testLimit, int(20.0 + 0.006465574372532171 * sum)), limit);
			if(testLimit && ((newLimit)/10) > ((testLimit)/10))
				currMode = 100;
			testLimit = newLimit;

		//----
		//	Debug info: draw on display buffer
			if(currMode == 1 || (currMode == 0 && pressed&(PSP_CTRL_LEFT|PSP_CTRL_RIGHT|PSP_CTRL_UP|PSP_CTRL_DOWN)))
			{
				sprintf((char*)SCRATCHPAD, "%i / %i", testLimit, limit);
				font1.color = 0x55FFFFFF;
				font1.print((void*)(framebuffer ? NULL : 0x88000), 4.0f, 8.0f, (char*)SCRATCHPAD, 0, 8);
				font1.color = WHITE;
			}

		}//if state changed
		else
		{
			sceDisplayWaitVblankStart();	//Sleep(1);
		}

	//----
	//	Big kanji (question)
		if(currMode == 0)
		{
			bool resetTick1 = false;
		//----
		//	Select new kanji
			sceRtcGetCurrentTick(&tick2);
			const double s = TickToSeconds * (tick2 - tick1);
			if(q0 < 0 && s >= (correct||currMode!=prevMode ? 0.0 : 5.0))
			{
				while(q0 < 0 || q0 == q1)
				{
					int i = rand(testLimit);
					if(randSquared() < 255 - level[i])
						q0 = i;
				}

				resetTick1 = true;
			}
		//----
		//	Draw kanji
			if(q0 >= 0)
			if(resetTick1 || pressed || currMode!=prevMode)
			{
				void *currBuffer = (void*) (framebuffer ? 0x0 : 0x88000);
				const char *ptr = src[q0];
				size_t len = 0;
				if(fr)
					len = (advance(ptr, '	') - ptr) - 1;

				font->print(currBuffer, 4.0f, (fr||v) ? 88.0f : font==&font1 ? 200.0f : font==&font2 ? 200.0f : 180.0f, ptr, '	', fr ? (len>16?32:48) : v ? (utf8.strlen(ptr,'	')>6?40:80) : 200);

				if(resetTick1);
					sceRtcGetCurrentTick(&tick1);
			}
		}

		prevTestMode = testMode;
		prevButtons = currButtons;
		prevCursor = cursor;

	}//while(running)


//----
	FT_Done_FreeType(FTLib);

//----
//	Save progress
	{
		if(modeUsed[0])	file("kanji.stats", PSP_O_CREAT|PSP_O_WRONLY).write(levelKanji.data, levelKanji.length*sizeof(BYTE));
		if(modeUsed[1])	file("vocab.stats", PSP_O_CREAT|PSP_O_WRONLY).write(levelVocab.data, levelVocab.length*sizeof(BYTE));
		if(modeUsed[2])	file("franc.stats", PSP_O_CREAT|PSP_O_WRONLY).write(levelFranc.data, levelFranc.length*sizeof(BYTE));
	}

	sceKernelExitGame();

	return 0;
}
