#include <pspkernel.h>
#include <pspgu.h>
#include <pspdisplay.h>
#include <pspctrl.h>
#include <psprtc.h>
#include <pspmoduleinfo.h>
#include <psppower.h>

#include "ttf.h"
#include "SimpleRNG.h"

PSP_MODULE_INFO("Kanji King", PSP_MODULE_USER, 1, 4);

static bool running = true;
static bool suspend = false;

static unsigned int __attribute__((aligned(16))) list[262144];

int exit_callback(int arg1, int arg2, void *common) {
	running = 0;
	return 0;
}


//-------------------------------------------------------------------------------------------------
int power_callback(int unknown, int pwrflags, void *common)
{
    /* check for power switch and suspending as one is manual and the other automatic */
    if (pwrflags & PSP_POWER_CB_POWER_SWITCH || pwrflags & PSP_POWER_CB_SUSPENDING) {
	// suspending
		  suspend = true;
    } else if (pwrflags & PSP_POWER_CB_RESUMING) {
	// resuming from suspend mode
	 } else if (pwrflags & PSP_POWER_CB_RESUME_COMPLETE) {
	// resume complete
		  suspend = false;
    } else if (pwrflags & PSP_POWER_CB_STANDBY) {
	// entering standby mode
    } else {
	// Unhandled power event
    }
//    sceDisplayWaitVblankStart();

        return 0;
}

/* Callback thread */
int CallbackThread(SceSize args, void *argp)
{
    int cbid;
    cbid = sceKernelCreateCallback("Exit Callback", exit_callback, NULL);
    sceKernelRegisterExitCallback(cbid);
    cbid = sceKernelCreateCallback("Power Callback", power_callback, NULL);
    scePowerRegisterCallback(0, cbid);
    sceKernelSleepThreadCB();

        return 0;
}

/* Sets up the callback thread and returns its thread id */
int SetupCallbacks(void)
{
    int thid = 0;
    thid = sceKernelCreateThread("update_thread", CallbackThread, 0x11, 0xFA0, 0, 0);
    if (thid >= 0)
        sceKernelStartThread(thid, 0, 0);
    return thid;
}



//-------------------------------------------------------------------------------------------------
#define	UNICODE_RAD_SUPP	0x2E80		// first codepoint of CJK Radicals Supplement
#define	UNICODE_RADICALS	0x2F00		// first codepoint of 214 Kangxi Radicals
#define	UNICODE_CJK			0x4E00		// first codepoint of CJK Unified Ideographs

SimpleRNG rng;

//-------------------------------------------------------------------------------------------------
//	@return random number in [0,1)
inline double randd()
{
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
/*	const int	L = 2 * int(b);
	const BYTE	R = min(255, 510 - L),
					G = min(255, L),
					B = 0x0F;
*/
	float	rgb[3],
//			hue = 2.0f/3.0f * BYTE_TO_FLOAT * b;
			hue = 0.5 * BYTE_TO_FLOAT * b;
	hue += 5.0f/6.0f;
//	hue += 0.75f;
	if(hue >  1.0f)
		hue -= 1.0f;

	HSVtoRGB(hue, 1.0, 1.0, rgb);

	const BYTE	R = 255.0f*rgb[0],
					G = 255.0f*rgb[1],
					B = 255.0f*rgb[2];

	return 0xFF000000 | (B<<16) | (G<<8) | R;
}

//-------------------------------------------------------------------------------------------------
int repetition = 5;

float ProbTable[256];

void UpdateProbTable()
{
	repetition = min(max(repetition,1),10);

	const float y = exp((10-repetition)/3.0f - 1.0f);

	for(int i = 0; i < 256; i++)
		ProbTable[i] = pow(1.0f - float(i)/255.0f, y);
}
/*
double ProbLevel(BYTE b)
{
//	double rep_num = exp(repetition);
// b				= [0,255]
// repetition	= [1,10]
	double d = double(b);
//	return 0.00390625 * (257.0 / (1.0 + double(b)) - 1.0);
//	return 0.00390625 * (256 - int(b));
	return rep_num / (rep_num + d*d);
}
*/

//-------------------------------------------------------------------------------------------------
u16	tmpKanji[8],							// unicode codepoints of all kanji in a string
		tmpKanjiCount = 0;

void CacheKanji(const char *str, const u16 *filter = NULL, int filterCount = 0)
{
	UTF8 utf8;
	u32 codepoint;
	tmpKanjiCount = 0;

	while((codepoint = utf8.decode(&str)) && tmpKanjiCount < 8)
		if(codepoint == '	')
			break;
		else
		if(codepoint >= UNICODE_CJK 		// Is Kanji?
		&&	codepoint <= 0x0000FFFF)
		{
			if(filter==NULL)
					tmpKanji[tmpKanjiCount++] = codepoint;
			else
				for(int i = 0; i < filterCount; i++)
					if(filter[i]==codepoint)
					{
						tmpKanji[tmpKanjiCount++] = codepoint;
						break;
					}		
		}
}


//-------------------------------------------------------------------------------------------------
int main()
{
	u64 tick1, tick2;
	const double TickToSeconds = 1.0 / sceRtcGetTickResolution();
	sceRtcGetCurrentTick(&tick1);
	UpdateProbTable();

	SetupCallbacks();

	SceCtrlData pad_data;
	sceCtrlSetSamplingCycle(0);
	sceCtrlSetSamplingMode(1);

//----
//	toggle power locking
	scePowerLock(0);

//----
	FT_Init_FreeType(&FTLib);

//----
//	Load resources
	const string	heisig(file("data/kanji.txt")),				// kanji+components	english	german
						radikale(file("data/radikale.txt")),		// 214 radicals with german meanings
						kanjidic(file("data/kanjidic.txt")),		// 214 radicals + >12000 kanji (all <= 0xFFFF)
						vocab(file("data/vocab.txt")),
						kana(file("data/kana.txt"));					// 274 hiragana + katakana
//						francais(file("data/french.txt"));

	if(heisig == false)
		sceKernelExitGame();

	int		nKanji		= 0,				// # heisig kanji
				nVocab		= 0,				// # vocab lines (= items)
				nKana			= 0,				// # kana lines
				testLimitK	= 0,				// Kanji with this or higher indices are not tested
				testLimitV	= 0,				// Vocab ...
				testLimitA	= 0,				// Kana  ...
				autoLimit	= 0,				// automatically determined minimum test limit
				select		= -1,				// 3rd (=middle) kanji displayed in list mode
				listSelect	= -1,				// index of the vocab list
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
		for(; link < nKanjidic; link++)
			if(code == tmp4[link])
				break;

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

//----
	const int nFonts = 6;
	const char *fNames[nFonts] = {"fonts/meiryo.ttc",
											"fonts/ipam.ttf",
											"fonts/epmarugo.ttf",
											"fonts/kaiu.ttf",
											"fonts/epgyosho.ttf",
											"fonts/KanjiStrokeOrders_v3.000.ttf"};

//	Font font1("fonts/ipaexg.ttf", 42);
	Font font1(fNames[0]);
	Font font2(fNames[1]);
	Font font3(fNames[2]);
	Font font4(fNames[3]);
	Font font5(fNames[4]);
	Font font6(fNames[5]);
	Font *fonts[nFonts] = {&font1, &font2, &font3, &font4, &font5, &font6};
	int selectedFont = 0;

//----
//	Game state and logic
	unsigned	currButtons	= 0,
				prevButtons	= 0,
				pressed		= 0,				// just pressed buttons
				released		= 0,				// just released buttons
				prevPressed	= 0,
				repeatPress	= 0;				// was the same button pressed repeatedly?
	int		currMode		= -1,				// current "screen" - determines what is displayed and how buttons work
				prevMode		= 0,				// still displayed previous frame
				testMode		= 0,				// what is being tested: kanji or vocab or kana?
				prevTestMode= 0,
				q0				= -1,				// current  kanji or vocab index = question
				q1				= -1;				// previous kanji or vocab index = answer
	BYTE		prevLevel	= 0;				// To restore the level after a mistaken answer
	bool		german		= false,			// Which language to use for translations: english or german?
				correct		= false,			// if q1 was answered correctly
				autoPlay		= false;			// cycle kanji without user input?
	bool		modeUsed[3]	= {false,false,false};

	vec<BYTE>	levelKanji(nKanji),
					levelVocab(nVocab),
					levelKana(nKana);
//----
//	Load progress
	{
		levelKanji.zero();
		levelVocab.zero();
		levelKana.zero();
		file vStats("vocab.stats");
		file kStats("kana.stats");
		file jStats("kanji.stats");
		if(vStats)
			vStats.read(levelVocab.data, levelVocab.length*sizeof(BYTE));
		if(kStats)
			kStats.read(levelKana.data,  levelKana.length*sizeof(BYTE));
		if(jStats)
		{
			vec<u16>	stats(jStats);

		//----
		//	load language and font options
			if(stats[0]=='g')	{	german			= stats[1];								}
			if(stats[2]=='r')	{	repetition		= stats[3];	UpdateProbTable();	}
			if(stats[4]=='f')	{	selectedFont	= stats[5];								}
			if(stats[6]=='1')	{	testLimitK		= stats[7];								}
			if(stats[8]=='2')	{	testLimitV		= stats[9];								}
			if(stats[10]=='3'){	testLimitA		= stats[11];							}

		//----
		//	compare unicode codepoints
			for(int i = 0; i < nKanji; i++)
			for(size_t j = 0; j < stats.length/2; j++)
				if(kanjiUnicode[i] == stats[j*2])
				{
					levelKanji[i] = stats[j*2+1];
					break;
				}
		}
	}

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
	while(running)
	{
		if(suspend)
		{
		//	Close the fonts (files)
			for(int i = 0; i < nFonts; i++)
				fonts[i]->close();

			scePowerUnlock(0);

			while(suspend)
				sceDisplayWaitVblankStart();

			scePowerLock(0);

		//	Reopen the fonts (files)
			for(int i = 0; i < nFonts; i++)
				fonts[i]->open(fNames[i]);

		// reset question
			if(currMode == 0)
				currMode = -1;
		}

	//----
	//	Input
		sceCtrlReadBufferPositive(&pad_data, 1);

		currButtons = pad_data.Buttons;
		unsigned
		changed		= currButtons ^ prevButtons;		// buttons that were either pressed or released
		pressed		= currButtons & changed;			// buttons that were just pressed
		released		= prevButtons & changed;			// buttons that were just released

		if(pressed)
		{
			if(pressed==prevPressed)
				repeatPress++;
			else
				repeatPress = 0;
			prevPressed	= pressed;
		}

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
		if(pressed & PSP_CTRL_SELECT)
		{
			switch(currMode)
			{
			case 0:
				testMode++;
				if(testMode > 2)
					testMode = 0;
			break;
			case 1:
				currMode = 5;
				pressed = 0;
			break;
			case 3:
			case 4:
				autoPlay = !autoPlay;
			break;
			}
		}

		if(currMode <= 1)
			cycleIdx = 0;

		if(currMode == 1
		||	currMode == 3
		||	currMode == 4)													select += cursor - prevCursor;
		if(pressed & PSP_CTRL_RTRIGGER)								german = !german;
		if((currMode == 3 && (pressed & PSP_CTRL_SELECT))
		||	(currMode == 4 && (pressed & PSP_CTRL_TRIANGLE)))	scrollIdx++;
		if(currMode == 3 && (pressed & PSP_CTRL_CROSS))			cycleIdx++;
		if(pressed & PSP_CTRL_START)
		{
			if(currMode == 0)
				currMode = 3;
			else if(currMode==3)
				cycleIdx++;
			else
				currMode = 0;
		}

		if((pressed & PSP_CTRL_TRIANGLE) && (currMode == 1 || currMode == 2 || currMode == 3))
		{
			currMode = 4;
			scrollIdx = 0;
		}
		else if((pressed & PSP_CTRL_CIRCLE) && (currMode == 1 || currMode == 3 || currMode == 4))
		{
			currMode = 2;
		}
		else if((pressed & PSP_CTRL_CROSS) && (currMode == 1 || currMode == 2 || currMode == 4))
		{
			currMode = 3;
		}
		else if(pressed & PSP_CTRL_SQUARE)
		{
			switch(currMode)
			{
			case 1:	currMode = 0;	break;
			default:	currMode = 1;	break;
			}
		}

		if(pressed & PSP_CTRL_LTRIGGER)
			selectedFont++;
		if(selectedFont >= nFonts-1)
			selectedFont = 0;
		Font *const font = fonts[selectedFont];

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
			prevMode	= -1;
			currMode	= 0;
			select	=
			listSelect =
			q0	=
			q1	= -1;
		}

		if(currMode==3 && currMode!=prevMode && prevMode!=4)
			cycleIdx = 0;

	//----
		if(currMode!=3 && currMode!=4)
			autoPlay = false;
		if(autoPlay)
		{
		//----
		//	Select new kanji
			sceRtcGetCurrentTick(&tick2);
			const double s = TickToSeconds * (tick2 - tick1);
			if(s >= 5.0)
			{
				tick1 = tick2;
				if(++select == limit)
					select = 0;

				pressed |= 0x4000000;		// not a button, just to enter the drawing routine
			}
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

		//----
		//	Kana have no vocab or components
			if(ka && (currMode == 2 || currMode == 3 || currMode == 4))
				currMode = 1;

			if(currMode == 0)
			{
				if(pressed & PSP_CTRL_RIGHT)	{	repetition= min(repetition+ 1, 10);	UpdateProbTable();						}
				if(pressed & PSP_CTRL_LEFT)	{	repetition= max(repetition- 1,  1);	UpdateProbTable();						}
				if(pressed & PSP_CTRL_UP)		{	testLimit = min(testLimit+(1<<repeatPress), limit);							}
				if(pressed & PSP_CTRL_DOWN)	{	testLimit = max(testLimit-(1<<repeatPress), autoLimit ? autoLimit : 20);}
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
		//	Return from 'quick' mode 3 after cycling all kanji
			if(currMode == 3 && cycleIdx >= max(tmpKanjiCount,u16(1)) && (pressed & PSP_CTRL_START))
				currMode = 0;

		//----
		//	Jump to q1 entry or kanji
			if(currMode)
			{
				if(prevMode==0)
					listSelect = select = (q1 >= 0 ? q1 : testLimit-4);

				if(tmpKanjiCount)
				{
					if((v && prevMode<=1 && (currMode==2||currMode==3||currMode==4))
					||	(currMode==3&&prevMode!=1&&prevMode!=4 && (pressed & (PSP_CTRL_START|PSP_CTRL_CROSS))))
					{
						codepoint = tmpKanji[cycleIdx % tmpKanjiCount];
						for(int i = 0; i < nKanji; i++)
							if(kanjiUnicode[i]==codepoint)
							{
								select = i;
								break;
							}
					}
				}
				else
				{
					if((v||ka) && currMode > 1 && currMode < 5)
						currMode = prevMode;
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
				//----
				// Find correct entry
					if(v && listSelect>0 && (pressed&PSP_CTRL_CIRCLE))
					{
						for(int i = 0; i < kanjiVocabCount[select]; i++)
						{
							int lineNumber	= kanjiVocabLines[kanjiVocabIndex[select]+i];
							if(vocabPointer[lineNumber] == src[listSelect])
							{
								cursor = i;
								break;
							}
						}
					}

				//----
					int	count			= kanjiVocabCount[select] + 1,
							prevIndex	= prevCursor % count,
							index			= cursor     % count;
					if(prevIndex != index)
						scrollIdx = 0;
					while(index < 0)
						index += count;


				//----
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

						tmpKanji[0] = kanjiUnicode[select];
						tmpKanjiCount = 1;
					}
					else
					{
						int	lineNumber	= kanjiVocabLines[kanjiVocabIndex[select]+index];

						const char	*ptr	= vocabPointer[lineNumber];

						CacheKanji(ptr, kanjiUnicode, nKanji);
						cycleIdx = 0;

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
		//	Stroke order mode
			if(currMode == 4)
			{
				Font &f = (scrollIdx & 1) ? *font : font6;
				f.setSize(240);
				f.print(framebuffer, 0, 220, kanjiUnicode[select]);

				float	x = 250.0f, y = 90.0f;
				font1.color = ColorLevel(levelKanji[select]) & 0xC0FFFFFF;
				font1.print(framebuffer, x, 68.0f, advance(german ? advance(kanjiPointer[select]) : kanjiPointer[select]), '	', 16);

			//	kanjidic data
				const char *ptr = advance(kanjidicPointer[kanjiDicLink[select]],'	',6);
				do
				{
					ptr = font1.print(framebuffer, x, y+=24.0f, ptr, ' ', 18, -2.0f);
				}while(ptr[-1]!='	');
				x = 340.0f;
				y =  90.0f;
				do
				{
					ptr = font1.print(framebuffer, x, y+=24.0f, ptr, ' ', 18, -2.0f);
				}while(ptr[-1]!='	');
						
//					ptr = font1.print(framebuffer, x, max(tmp,y)+26.0f, ptr, '	', 18, 1.2f);

				font1.color = WHITE;
			}

		//----
		//	List mode
			if(currMode == 1)
			{
			//	Restore old list position
				if(v && listSelect >= 0 && prevMode!=1)
					select = listSelect;

				if(select < 0)			select += limit;
				if(select >= limit)	select -= limit;

				for(int k = 0; k < 7; k++)
				{
					const int   i = select - 3 + k;
					if(i < 0 || i >= limit)
						continue;
					const float y = 38.85f * k + 30.0f;
					const char *ptr = src[i];
					if(k==3)
					{
						if(v)
							CacheKanji(ptr);
						else
						{
							tmpKanjiCount = 1;
							tmpKanji[0] = kanjiUnicode[i];
						}
					}

					font->color =
					font1.color = k==3 ? WHITE : ColorLevel(level[i]);
					font1.setSize(20);
					sprintf((char*)SCRATCHPAD, "#%i", i+1);
								font1.print(framebuffer,   10.0f, y, (char*)SCRATCHPAD);
					sprintf((char*)SCRATCHPAD, "%i%%", int(0.392156862745098*level[i]+0.5));
								font1.print(framebuffer,  400.0f, y, (char*)SCRATCHPAD);

						ptr = font->print(framebuffer,   96.0f, y + (font==&font3 ? 2.0f : font==&font4 ? 0.0f : 3.0f), ptr, v||ka?'	':' ', 38, -2.0f);
					if(!v)
					{
						if(!ka)
							ptr = advance(german ? advance(src[i]) : src[i]);
						ptr = font1.print(framebuffer, ka ? 240.0f : 160.0f, y, ptr, '	', ka?26:20);
					}
				}

				font->color =
				font1.color = WHITE;
				listSelect = select;
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
					tmpKanjiCount = 0;
					if(v)
						CacheKanji(src[q1], kanjiUnicode, nKanji);
/*
				//----
				//	Adjust kanji in vocab
					if(v)
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
						font->print(framebuffer, x, font == &font1 ? 60.0f : font == &font2 ? 59.0f : font == &font4 ? 54.0f : 60.0f, kanjiUnicode[q1]);

					//	Translation
						ptr = advance(german ? advance(ptr) : ptr);
						ptr = font1.print(framebuffer, x, 88.0f, ptr, '	', 21);

					//	Statistics
						font1.setSize(18);
						font1.color = ColorLevel(level[q1]) & 0xC0FFFFFF;
						sprintf((char*)SCRATCHPAD, "#%i", q1+1);
						font1.print(framebuffer, 340.0f, 40.0f, (char*)SCRATCHPAD);
						sprintf((char*)SCRATCHPAD, "%i%%", q1>=0 ? int(0.392156862745098*level[q1]+0.5) : 0);
						font1.print(framebuffer, 340.0f, 60.0f, level[q1] > prevLevel ? 0x2191 : level[q1] == prevLevel ? 0x2192 : 0x2193);
						font1.print(framebuffer, 360.0f, 60.0f, (char*)SCRATCHPAD);
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

			// set list index
//				listSelect = select = (q1 >= 0 ? q1 : testLimit-4);
			}

		//----
		//	Components mode
			if(currMode == 3)
			{
				Font &fnt = *font;

			//	Draw kanji + components
				const char *s = kanjiPointer[select];
						
				for(float y = 68.0f; y < 272.0f; y += 50.0f)
				{
					utf8.state	= 0;
					u32	*comp	= (u32*) SCRATCHPAD,
							count	= 0,
							phon	= 0,
							trad	= 0,
							prev	= 0;

					for( ; *s && s[0]!=' ' && s[0]!='\n'; s++)
						if(!utf8.decode(&codepoint, (BYTE)*s))
						{
							if(codepoint >= UNICODE_RAD_SUPP)// Is Kanji?
							{
								prev =
								comp[count++] = codepoint;
							}
							else
							if(codepoint == 0x273D && prev)	// '*' = phonetic component
							{
								phon = prev;
								if(count>1)
									count--;
							}
							else
							if(codepoint == 0x2744 && prev)	// '*' = traditional form of the character
							{
								trad = prev;
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
						u32 code = comp[0];
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

						if(trad==code)
							y = 268.0f;

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
							Font &f = font1;
						//	Font &f = font2;
								f.color = 0x55FFFFFF;
								f.setSize(11);
								f.print(framebuffer,   8.0f, 14.0f, "radical");
								f.print(framebuffer, 130.0f, 14.0f, "strokes",  0x0);
								f.print(framebuffer, 218.0f, 14.0f, "freq",     0x0);
								f.print(framebuffer, 318.0f, 14.0f, "grade",    0x0);
								f.print(framebuffer, 408.0f, 14.0f, "JLPT",     0x0);
								f.color = 0xAAFFFFFF;	f.setSize(16);
								f.print(framebuffer,  50.0f, 16.0f, UNICODE_RADICALS+rad-1);
								f.print(framebuffer,  72.0f, 16.0f, stats,    '	');
							s=	f.print(framebuffer, 171.0f, 16.0f, s,         '	');
							s=	f.print(framebuffer, 244.0f, 16.0f, s,         '	');
							s=	f.print(framebuffer, 352.0f, 16.0f, s,         '	');
							s=	f.print(framebuffer, 437.0f, 16.0f, s,         '	');
								f.color = WHITE;
						}

						Font &f = fnt.findChar(code) ? fnt : font1.findChar(code) ? font1 : font4;
						f.setSize(42);
						f.color = font1.color = (y == 68.0f ? 0xFF80C6FF : phon ? 0xFF80D9AE : trad==code ? 0xFFFF99CC : WHITE);
						f.print(framebuffer, 8.0f, y, code);
						if(on)							font1.print(framebuffer,  68.0f, y,       on,      '	', 18, -2.0f);
						f.color = font1.color = WHITE;
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

					if(s[-1] == '\n')
						break;
				}
			}//components mode

		//----
		//	autoPlay
			if(currMode==3 || currMode==4)
			{
				font1.setSize(21);
				font1.color = autoPlay ? 0x80FFFFFF : 0x40FFFFFF;
				font1.print(framebuffer, 411.0f, 268.0f, autoPlay ? 0x25BA : 0x25BB);
//				font1.print(framebuffer, 411.0f, 268.0f, autoPlay ? 0x25B6 : 0x25B7);
				font1.color = WHITE;
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
			u32 sumA = 0;
			double sumB = 0;
			int newLimit = testLimit;
			for(size_t i = 0; i < level.length; i++)
			{
				if(level[i] >= 128)
					sumA++;
				sumB += 0.00392156862745098 * level[i];

			//----
			// Each kanji with a rating of 128 (= 50% = 5.0s reaction time) unlocks a new kanji
			//	and every 16 perfect kanji unlock an additional one.
				autoLimit = 20 + sumA + 0.0625*sumB;
				if(autoLimit > limit)
				{
					newLimit = limit;
					break;
				}
				if(i >= (size_t)autoLimit)
					break;
				else
					newLimit = max(testLimit, autoLimit);
			}
			if(testLimit && ((newLimit)/10) > ((testLimit)/10))
				currMode = 100;
			testLimit = newLimit;

		//----
		//	Debug info: draw on display buffer
			if(currMode != 0)
			{
//				sprintf((char*)SCRATCHPAD, "%i / %i", select, listSelect);
//				sprintf((char*)SCRATCHPAD, "%i / %i", testLimit, limit);
//				sprintf((char*)SCRATCHPAD, "%i , %i, %i", nKanji, nTest, count2);
//				font1.color = 0x80FFFFFF;
//				font1.print((void*)(framebuffer ? NULL : 0x88000), 4.0f, 8.0f, (char*)SCRATCHPAD, 0, 8);
//				sprintf((char*)SCRATCHPAD, "%i / %i", cycleIdx, tmpKanjiCount);
//				font1.print((void*)(framebuffer ? NULL : 0x88000), 4.0f, 270.0f, (char*)SCRATCHPAD, 0, 8);
//				font1.color = WHITE;
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
					if(ProbTable[level[i]] > rng.randf())
						q0 = i;
				}

				resetTick1 = true;
			}

			void *currBuffer = (void*) (framebuffer ? 0x0 : 0x88000);

		//----
			if(switchedTestMode || (pressed & (PSP_CTRL_UP|PSP_CTRL_DOWN)))
			{
				sprintf((char*)SCRATCHPAD, "Learning %s :  %i / %i", testMode==0 ? "Kanji" : testMode==1 ? "Vocabulary" : "Hiragana & Katakana", testLimit, limit);
				font1.setSize(21);
				font1.color = testMode==0 ? 0xFFFF8766 : testMode==1 ?  0xFF5FCC20 : 0xFF549BFF;
				font1.print(currBuffer, 5.0f, 265.0f, (char*)SCRATCHPAD);
				font1.color = WHITE;
			}
		//----
			if(pressed & (PSP_CTRL_LEFT|PSP_CTRL_RIGHT))
			{
				sprintf((char*)SCRATCHPAD, "review :  %i", repetition);
				font1.setSize(21);
				font1.color = WHITE;
				font1.print(currBuffer, 5.0f, 265.0f, (char*)SCRATCHPAD);
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

				font->print(currBuffer, ka?42.0f:5.0f, v||ka ? 88.0f : font == &font4 ? 180.0f : 200.0f, ptr, v||ka?'	':' ', v ? (utf8.strlen(ptr,'	')>6?40:80) : ka?80:200, -2.0f);
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
			vec<u16>	stats(12+nKanji*2);

		//----
		//	save options
			stats[0] = 'g';	stats[1] = german;
			stats[2] = 'r';	stats[3] = repetition;
			stats[4] = 'f';	stats[5] = selectedFont;
			stats[6] = '1';	stats[7] = testLimitK;
			stats[8] = '2';	stats[9] = testLimitV;
			stats[10]= '3';	stats[11]= testLimitA;

		//----
		//	save kanji level
			for(int i = 0; i < nKanji; i++)
			{
				stats[12+i*2]   = kanjiUnicode[i];
				stats[12+i*2+1] = levelKanji[i];
			}
								file("kanji.stats",	PSP_O_CREAT|PSP_O_WRONLY).write(stats.data,			stats.length*sizeof(u16));
		}
		if(modeUsed[1])	file("vocab.stats",	PSP_O_CREAT|PSP_O_WRONLY).write(levelVocab.data,	nVocab*sizeof(BYTE));
		if(modeUsed[2])	file("kana.stats",	PSP_O_CREAT|PSP_O_WRONLY).write(levelKana.data,		nKana *sizeof(BYTE));
	}

	sceKernelExitGame();

	return 0;
}
