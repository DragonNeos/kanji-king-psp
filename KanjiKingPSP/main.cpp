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

#define	UNICODE_RAD_SUPP	0x2E80		// first codepoint of CJK Radicals Supplement
#define	UNICODE_RADICALS	0x2F00		// first codepoint of 214 Kangxi Radicals
#define	UNICODE_CJK			0x4E00		// first codepoint of CJK Unified Ideographs

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
	const string	heisig(file("data/heisig.txt")),				// kanji+components	english	german	heisigIndex
						radikale(file("data/radikale.txt")),		// 214 radicals with german meanings
						kanjidic(file("data/kanjidic.txt")),		// 214 radicals + >12000 kanji (all <= 0xFFFF)
						vocab(file("data/vocab.txt")),
						kana(file("data/kana.txt"));					// 274 hiragana + katakana
//						francais(file("data/french.txt"));

	if(heisig == false)
		sceKernelExitGame();

	int		nKanji		= 0,				// # heisig kanji
//				nTest			= 0,
				nVocab		= 0,				// # vocab lines (= items)
				nKana			= 0,				// # kana lines
				testLimitK	= 0,				// Kanji with this or higher indices are not tested
				testLimitV	= 0,				// Vocab ...
				testLimitA	= 0,				// Kana  ...
				autoLimit	= 0,				// automatically determined minimum test limit
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

//	Temporariliy use the VRAM to collect pointers to all Kanjis = newlines
	const char	**tmp1 = (const char**) VRAM;				//  512 kiB
//					**tmp2 = tmp1 + 512*1024;					//  512 kiB
//					*cPtr = &components[0];
	u16			*tmp3 = (u16*) SCRATCHPAD,					//   16 kiB -> 8192 entries
					*tmp4 = (u16*) TMP_VRAM;					//  960 kiB

	for(const char *s = &heisig[0]; *s; s = advance(s,'\n'))
	{
		utf8.state = 0;
		tmp1[nKanji] = s;
		tmp3[nKanji] = utf8.decode(&s);
		nKanji++;
	}

//	Copy tmp pointers to proper vector
	const vec<const char*>	kanjiPointer(nKanji, tmp1);
//	const vec<const char*>	compoPointer(nKanji, tmp2);
			vec<const char*>	radikPointer(214);
	const vec<u16>				kanjiUnicode(nKanji, tmp3);
			vec<u16>				kanjiDicLink(nKanji);				// heisig kanji index -> kanjidic line
			vec<BYTE>			kanjiVocabCount(nKanji);			// # of vocab entries with this kanji
	u16	totalVocabLinks = 0;

//----
//	Collect radikal lines
	{
		int i = 0;
		for(const char *s = &radikale[0]; *s; s = advance(s, '\n'))
			radikPointer[i++] = s;
	}

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
	for(const char *s = &kanjidic[0]; /*nKanjidic < 8192 &&*/ *s; s = advance(s, '\n'))
	{
		tmp1[nKanjidic] = s;					// pointer to the kanjidic line

		for(utf8.state = 0; utf8.decode(&codepoint, (BYTE)*s); ++s)
			;

		if(codepoint <= 0xFFFF)
			tmp4[nKanjidic++] = codepoint;// codepoint of the kanji on this line
	}

//----
//	Make links for all heisig kanji
	for(int i = 0; i < nKanji; i++)
	{
		u16	code = kanjiUnicode[i];
		int	link = 0;
//		bool	found = false;
		for(; link < nKanjidic; link++)
			if(code == tmp4[link])
			{
//				found = true;
				break;
			}

//		if(!found)
//			nTest = i;
		kanjiDicLink[i] = link;
	}

//	Copy tmp pointers to proper vector
	const vec<const char*>	kanjidicPointer(nKanjidic, tmp1);
	const vec<u16>				kanjidicUnicode(nKanjidic, tmp4);

//----
//	Collect kana newlines
	for(const char *s = &kana[0]; *s; s = advance(s, '\n'))
		tmp1[nKana++] = s;					// pointer to the kana line

//	Copy tmp pointers to proper vector
	const vec<const char*>	kanaPointer(nKana, tmp1);

	vec<BYTE>	levelKanji(nKanji),
					levelVocab(nVocab),
					levelKana(nKana);
//----
//	Load progress
//	int count2 = 0;
	{
		levelKanji.zero();
		levelVocab.zero();
		levelKana.zero();
	//	file("kanji.stats").read(levelKanji.data, levelKanji.length*sizeof(BYTE));
		file("vocab.stats").read(levelVocab.data, levelVocab.length*sizeof(BYTE));
		file("kana.stats").read (levelKana.data,  levelKana.length*sizeof(BYTE));
		vec<u16>	stats(file("kanji.stats"));

	//----
	//	compatibility to v1.1
		if(stats.length == 1021)
		{
			const BYTE *level = (BYTE*)&stats[0];
			for(int i = 0, j = 0; i < nKanji && j < 2042; i++)
			{
				const char *nl = advance(kanjiPointer[i],'\n');
				if(nl[-3] != '.')
				{
					levelKanji[i] = level[j++];
//					count2++;
				}
			}
		}
		else
		{
		//----
		//	compare unicode codepoints
			for(int i = 0; i < nKanji; i++)
			for(size_t j = 0; j < stats.length/2; j++)
				if(kanjiUnicode[i] == stats[j*2])
				{
					levelKanji[i] = stats[j*2+1];
					break;
				}
//			memcpy(&levelKanji[0], &tmpKanji[0], min(levelKanji.length,tmpKanji.length));
		}
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
	int		currMode		= -1,				// current "screen" - determines what is displayed and how buttons work
				prevMode		= 0,				// still displayed previous frame
				testMode		= 0,				// what is being tested: kanji or vocab or kana?
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
//	bool		switchedTestMode = false;

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
		if(currMode == 1
		||	currMode == 3)						select += cursor - prevCursor;
		if(pressed & PSP_CTRL_RTRIGGER)	german = !german;
		if(pressed & PSP_CTRL_TRIANGLE)	scrollIdx++;
		if(((pressed & PSP_CTRL_CIRCLE) && currMode==2)
		||	((pressed & PSP_CTRL_CROSS)  && currMode==3))
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
	//	setup references to applicable data, depending on selected mode
		const bool	v	= (testMode == 1),					// vocab mode
						ka	= (testMode == 2);					// kana mode
		vec<BYTE>					&level		= ka ? levelKana   : v ? levelVocab   : levelKanji;
		int							&testLimit	= ka ? testLimitA  : v ? testLimitV   : testLimitK;
		const int					&limit		= ka ? nKana       : v ? nVocab       : nKanji;
		const vec<const char*>	&src			= ka ? kanaPointer : v ? vocabPointer : kanjiPointer;
		const bool switchedTestMode = (testMode!=prevTestMode) || currMode==-1;

	//	Changed test mode?
		if(switchedTestMode)
		{
			prevMode = -1;
			currMode = 0;
			q0 =
			q1 = -1;
		}

	//----
	//	Draw only if necessary
		if(pressed || currMode!=prevMode)
		{
		//----
		//	Clear sreen
			sceGuStart(GU_DIRECT, list);
			sceGuClear(GU_COLOR_BUFFER_BIT);
			sceGuFinish();

			if(prevMode == 0)
				prevSelect = q1;				// remember list index

		//----
		//	Kana have no vocab or components
			if(ka && (currMode == 2 || currMode == 3))
				currMode = 1;

			if(currMode == 0)
			{
				if(pressed & PSP_CTRL_RIGHT)	testLimit = min(testLimit+ 10, limit);
				if(pressed & PSP_CTRL_LEFT)	testLimit = max(testLimit- 10, autoLimit ? autoLimit : 12);
				if(pressed & PSP_CTRL_UP)		testLimit = min(testLimit+100, limit);
				if(pressed & PSP_CTRL_DOWN)	testLimit = max(testLimit-100, autoLimit ? autoLimit : 12);
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
		//	Buffer all kanji in select
			{
				const bool jump = (prevMode==1 && (currMode==2||currMode==3) && v && (pressed & (PSP_CTRL_CROSS|PSP_CTRL_CIRCLE)));
				if(jump)
				{
					utf8.state = 0;
					q1KanjiCount = 0;
	//				cursor =
	//				scrollIdx =
					cycleIdx = 0;
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
				if(currMode)
				if(prevMode == 0 || jump || (pressed & PSP_CTRL_START))
				{
					if(!v || ka || (currMode == 1 && !jump))
					{
						if(q1 > 0)	select = q1;
						else			select = testLimit-4;
					}
					else if(q1KanjiCount)
					{
						codepoint = q1Kanji[cycleIdx % q1KanjiCount];
						for(int i = 0; i < nKanji; i++)
							if(kanjiUnicode[i]==codepoint)
							{
								select = i;
								break;
							}
					}
					else
						currMode = prevMode;
				}
			}

		//----
		//	Restore old list position
			if(currMode==1 && (prevMode==3||prevMode==2) && v)
			if(prevSelect >= 0)
				select = prevSelect;

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
								font1.print(framebuffer, 400.0f, 32.0f, "JLPT", 0, 18);
					font->setSize(42);
					if(index == count-1)
					{
						const char	*ptr	= kanjidicPointer[kanjiDicLink[select]];
								font->print(framebuffer,      x, 50.0f, kanjiUnicode[select]);
						ptr = font1.print(framebuffer, 444.0f, 32.0f, advance(ptr,'	', 5), '	', 18);
						ptr = font1.print(framebuffer,      x,     y, advance(ptr,'	', 2), '	', 18);
						sprintf((char*)SCRATCHPAD, "0 / %u", count-1);
						font1.print(framebuffer, 400.0f, 54.0f, (char*)SCRATCHPAD);
					}
					else
					{
						int	lineNumber	= kanjiVocabLines[kanjiVocabIndex[select]+index];

						const char	*ptr	= vocabPointer[lineNumber];
						ptr = font->print(framebuffer, x,      50.0f, ptr, '	', 42);
						ptr = font->print(framebuffer, x,     100.0f, ptr, '	');
						ptr = font1.print(framebuffer, 444.0f, 32.0f, ptr, '	', 18);
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
			}//Vocab mode

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
					sprintf((char*)SCRATCHPAD, "%i%%", int(0.392156862745098*level[i]+0.5));
								font1.print(framebuffer,  400.0f, y, (char*)SCRATCHPAD);

						ptr = font->print(framebuffer,   96.0f, y + (font==&font1 ? 3.0f : font==&font2 ? 3.0f : 0.0f), ptr, v||ka?'	':' ', 38, -2.0f);
					if(!v)
					{
						if(!ka)
							ptr = advance(german ? advance(src[i]) : src[i]);
						ptr = font1.print(framebuffer, ka ? 240.0f : 160.0f, y, ptr, '	', ka?26:20);
					}
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
/*					if(v)
					{
						for(u16 n = 0; n < q1KanjiCount; n++)
						for(int i = 0; i < nKanji; i++)
							if(kanjiUnicode[i]==q1Kanji[n])
							{
								if(correct)	levelKanji[i] = max(255, int(levelKanji[i]) +  1);
								else			levelKanji[i] = min(  0, int(levelKanji[i]) - 10);
							}
					}
*/
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
					float x = v ? 5.0f : ka ? 42.0f : 250.0f,
							y = ka ? 185.0f : 150.0f;

					if(v)
					{
						ptr = font1.print(framebuffer, x, y, ptr, '	', 40);	// expression
						y += 50.0f;
					}

					if(v||ka)
					{
						ptr = font1.print(framebuffer, x, y, ptr, '	', 40);	// hiragana
					// translation
						if(v)
						{
							ptr+=2;
							if(german)
								ptr = advance(ptr);
						}
					//	int len = advance(ptr) - ptr;
						ptr = font1.print(framebuffer, x, 235.0f, ptr, german&&!ka?'\x1E':'	', ka?40:24, 1.2f);
					}
					else
					{
					//	Draw kanji
						font->setSize(60);
						font->print(framebuffer, x, font == &font1 ? 60.0f : font == &font2 ? 59.0f : 54.0f, kanjiUnicode[q1]);

					//	Translation
						ptr = advance(german ? advance(ptr) : ptr);
						ptr = font1.print(framebuffer, x, 88.0f, ptr, '	', 21);
					}

					if(!v && !ka)
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
						float	y =  90.0f;
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
			// font2 doesn't support some radical components
				Font &fnt = font == &font2? font1 : *font;

			//	Draw kanji + components
				const char *s = kanjiPointer[select];
						
				for(float y = 68.0f; ; y += 50.0f)
				{
					utf8.state	= 0;
					u32	*comp	= (u32*) SCRATCHPAD,
							count	= 0,
							phon	= 0,
							prev	= 0;

					for( ; *s && s[0]!=' ' && s[0]!='\n'; s++)
						if(!utf8.decode(&codepoint, (BYTE)*s))
						{
							if(codepoint >= UNICODE_RAD_SUPP)// Is Kanji?
							{
								comp[count++] = codepoint;
								prev = codepoint;
							}
							else
							if(codepoint == 0x273D && prev)	// '*' = phonetic component
							{
								phon = prev;
								if(count>1)
									count--;
							}
//							if(codepoint == 0x279E)				// '->' = original component
//								count++;
						}

					s++;
DRAW_COMP:
				//----
				//	Find component meaning
					if(count)
					{
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
									break;
								}
						}

					//	Check radikale.txt
						if(german && meaning==NULL)
						{
							for(u32 k = 0; k < count; k++)
							for(u32 r = 0; r < 214; r++)
							{
								utf8.state = 0;
								for(const char *s = radikPointer[r];;)
								{
									codepoint = utf8.decode(&s);
									if(codepoint < UNICODE_RAD_SUPP)
										break;
									if(codepoint == comp[k])
									{
										meaning = advance(s);
										goto FOUND_RADIKAL;
									}
								}
							}
						}
FOUND_RADIKAL:

					//	Check kanjidic.txt
						u16 code = comp[0];
						for(u32 k = 0; k < count; k++)
						{
							const char *mng = NULL;
							for(int kanjidicIdx = 0; kanjidicIdx < nKanjidic; kanjidicIdx++)
								if(kanjidicUnicode[kanjidicIdx] == comp[k])
								{
									code		= comp[k];
									stats		= advance(kanjidicPointer[kanjidicIdx]);
									on			= advance(stats, '	', 5);
									kun		= advance(on);
									mng		= advance(kun);
									japName	= advance(mng);
									break;
								}

							if(mng)
							{
								if(meaning == NULL)
									meaning = mng;
								break;
							}
						}

						fnt.setSize(42);
						fnt.color = y == 68.0f ? 0xFF80C6FF : phon ? 0xFF80D9AE : WHITE;
						fnt.print(framebuffer, 8.0f, y, code);
						fnt.color = WHITE;

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
						font1.color = phon ? 0xFF80D9AE : WHITE;
						if(on)							font1.print(framebuffer,  68.0f, y,       on,      '	', 18, -2.0f);
						font1.color = WHITE;
						if(kun && y == 68.0f)		font1.print(framebuffer,  68.0f, y-22.0f, kun,     '	', 18, -2.0f);
						if(meaning)						font1.print(framebuffer, 250.0f, y,       meaning, '	', (advance(meaning,'	') - meaning) > 30 ? 10 : 16, -3.0f);
						if(japName && y != 68.0f)	font1.print(framebuffer, 250.0f, y-22.0f, japName, '\n', 18, -2.0f);

						if(phon && phon != code)
						{
							y += 50.0f;
							comp[0] = phon;
							count = 1;
							goto DRAW_COMP;
						}
					}


				//	ptrComp = advance(ptrComp, ' ');
					if(s[-1] == '\n')
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
				sprintf((char*)SCRATCHPAD, "%i", testLimit/10 - 1);
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
			autoLimit = min(int(20.0 + 0.006465574372532171 * sum), limit);
			int newLimit = max(testLimit, autoLimit);
			if(testLimit && ((newLimit)/10) > ((testLimit)/10))
				currMode = 100;
			testLimit = newLimit;

		//----
		//	Debug info: draw on display buffer
			if(currMode == 1)
			{
				sprintf((char*)SCRATCHPAD, "%i / %i", testLimit, limit);
//				sprintf((char*)SCRATCHPAD, "%i , %i, %i", nKanji, nTest, count2);
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
			const double s = switchedTestMode ? 5.0 : TickToSeconds * (tick2 - tick1);
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

			void *currBuffer = (void*) (framebuffer ? 0x0 : 0x88000);

		//----
			if(switchedTestMode || (pressed & (PSP_CTRL_UP|PSP_CTRL_RIGHT|PSP_CTRL_DOWN|PSP_CTRL_LEFT)))
			{
				sprintf((char*)SCRATCHPAD, "Learning %s :  %i / %i", testMode==0 ? "Kanji" : testMode==1 ? "Vocabulary" : "Hiragana & Katakana", testLimit, limit);
				font1.setSize(21);
				font1.color = testMode==0 ? 0xFFFF8766 : testMode==1 ?  0xFF5FCC20 : 0xFF549BFF;
				font1.print(currBuffer, 5.0f, 265.0f, (char*)SCRATCHPAD);
				font1.color = WHITE;
			}

		//----
		//	Draw kanji
			if(q0 >= 0)
			if(resetTick1 || pressed || currMode!=prevMode)
			{
				const char *ptr = src[q0];
//				size_t len = 0;
//				if(fr)
//					len = (advance(ptr, '	') - ptr) - 1;

				font->print(currBuffer, ka?42.0f:5.0f, v||ka ? 88.0f : font==&font1 ? 200.0f : font==&font2 ? 200.0f : 180.0f, ptr, v||ka?'	':' ', v ? (utf8.strlen(ptr,'	')>6?40:80) : ka?80:200, -2.0f);
			//----
				if(resetTick1);
					sceRtcGetCurrentTick(&tick1);

			}
		}

//		if(prevTestMode != testMode)
//			switchedTestMode = true;
		prevTestMode = testMode;
		prevButtons = currButtons;
		prevCursor = cursor;

	}//while(running)


//----
	FT_Done_FreeType(FTLib);

//----
//	Save progress
	{
		if(modeUsed[0])
		{
			vec<u16>	stats(nKanji*2);
			for(int i = 0; i < nKanji; i++)
			{
				stats[i*2]   = kanjiUnicode[i];
				stats[i*2+1] = levelKanji[i];
			}
								file("kanji.stats",	PSP_O_CREAT|PSP_O_WRONLY).write(stats.data,			nKanji*2*sizeof(u16));
		}
		if(modeUsed[1])	file("vocab.stats",	PSP_O_CREAT|PSP_O_WRONLY).write(levelVocab.data,	nVocab*sizeof(BYTE));
		if(modeUsed[2])	file("kana.stats",	PSP_O_CREAT|PSP_O_WRONLY).write(levelKana.data,		nKana *sizeof(BYTE));
	}

	sceKernelExitGame();

	return 0;
}
