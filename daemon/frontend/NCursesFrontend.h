/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2004 Sven Henkel <sidddy@users.sourceforge.net>
 *  Copyright (C) 2007-2016 Andrey Prygunkov <hugbug@users.sourceforge.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#ifndef NCURSESFRONTEND_H
#define NCURSESFRONTEND_H

#ifndef DISABLE_CURSES

#include <string>
#include "Frontend.h"
#include "Log.h"
#include "DownloadInfo.h"

extern const int NCURSES_COLORPAIR_TEXT;
extern const int NCURSES_COLORPAIR_INFO;
extern const int NCURSES_COLORPAIR_WARNING;
extern const int NCURSES_COLORPAIR_ERROR;
extern const int NCURSES_COLORPAIR_DEBUG;
extern const int NCURSES_COLORPAIR_DETAIL;
extern const int NCURSES_COLORPAIR_STATUS;
extern const int NCURSES_COLORPAIR_KEYBAR;
extern const int NCURSES_COLORPAIR_INFOLINE;
extern const int NCURSES_COLORPAIR_TEXTHIGHL;
extern const int NCURSES_COLORPAIR_CURSOR;
extern const int NCURSES_COLORPAIR_HINT;

extern const int MAX_SCREEN_WIDTH;

class NCursesFrontend : public Frontend
{
public:
	NCursesFrontend();
	virtual ~NCursesFrontend();

protected:
	virtual void Run();

private:
	enum EInputMode
	{
		normal,
		editQueue,
		downloadRate
	};

	bool m_useColor = true;
	int m_dataUpdatePos = 0;
	bool m_updateNextTime = false;
	int m_screenHeight = 0;
	int m_screenWidth = 0;
	int m_queueWinTop = 0;
	int m_queueWinHeight = 0;
	int m_queueWinClientHeight = 0;
	int m_messagesWinTop = 0;
	int m_messagesWinHeight = 0;
	int m_messagesWinClientHeight = 0;
	int m_selectedQueueEntry = 0;
	int m_lastEditEntry = -1;
	bool m_lastPausePars = false;
	int m_queueScrollOffset = 0;
	std::string m_hint;
	time_t m_startHint;
	int m_colWidthFiles;
	int m_colWidthTotal;
	int m_colWidthLeft;

	// Inputting numbers
	int m_inputNumberIndex = 0;
	int m_inputValue;
#ifdef WIN32
	const wchar_t* m_messageTypes[5] = {
		L"INFO    ",
		L"WARNING ",
		L"ERROR   ",
		L"DEBUG   ",
		L"DETAIL  "
	};
#else
	const char* m_messageTypes[5] = {
		"INFO    ",
		"WARNING ",
		"ERROR   ",
		"DEBUG   ",
		"DETAIL  "
	};
#endif
	const int m_messageColorTypes[5] = { 
		NCURSES_COLORPAIR_INFO, 
		NCURSES_COLORPAIR_WARNING,
		NCURSES_COLORPAIR_ERROR, 
		NCURSES_COLORPAIR_DEBUG, 
		NCURSES_COLORPAIR_DETAIL 
	};

#ifdef WIN32
	std::vector<CHAR_INFO> m_screenBuffer;
	std::vector<CHAR_INFO> m_oldScreenBuffer;
	std::vector<WORD> m_colorAttr;
#else
	void* m_window; // WINDOW*
#endif

	EInputMode m_inputMode = normal;
	bool m_showNzbname;
	bool m_showTimestamp;
	bool m_groupFiles;
	int m_queueWindowPercentage = 50;

#ifdef WIN32
	void init_pair(int colorNumber, WORD wForeColor, WORD wBackColor);
	void PlotText(const wchar_t* string, int row, int pos, int colorPair, bool blink);
#else
	void PlotText(const char*  string, int row, int pos, int colorPair, bool blink);
#endif
	void PlotLine(const char*  string, int row, int pos, int colorPair);
	void PrintMessages();
	void PrintQueue();
	void PrintFileQueue();
	void PrintFilename(FileInfo* fileInfo, int row, bool selected);
	void PrintGroupQueue();
	void ResetColWidths();
	void PrintGroupname(NzbInfo* nzbInfo, int row, bool selected, bool calcColWidth);
	void PrintTopHeader(char* header, int lineNr, bool upTime);
	int PrintMessage(Message& msg, int row, int maxLines);
	void PrintKeyInputBar();
	void PrintStatus();
	void UpdateInput(int initialKey);
	void Update(int key);
	void SetCurrentQueueEntry(int entry);
	void CalcWindowSizes();
	void RefreshScreen();
	int ReadConsoleKey();
	int CalcQueueSize();
	void NeedUpdateData();
	bool EditQueue(DownloadQueue::EEditAction action, int offset);
	void SetHint(const char* hint);
};

#endif

#endif
