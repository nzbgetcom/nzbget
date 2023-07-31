/*
 *  This file is part of nzbget. See <http://nzbget.com>.
 *
 *  Copyright (C) 2023 nzbget.com <nzbget@nzbget.com>
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


#ifndef LOGGABLEFRONTEND_H
#define LOGGABLEFRONTEND_H

#include "Frontend.h"
#include "Log.h"

class LoggableFrontend : public Frontend
{
protected:
	virtual void Run();
	virtual void Update();
	virtual void BeforePrint() {};
	virtual void BeforeExit() {};
	virtual void PrintMessage(Message& message);
	virtual void PrintStatus() {};
	virtual void PrintSkip();
};

#endif
