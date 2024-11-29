/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2024 Denis <denis@nzbget.com>
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
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */


#ifndef POST_UNPACK_H
#define POST_UNPACK_H

#include <string>
#include "Thread.h"
#include "ScriptController.h"
#include "DownloadInfo.h"

namespace PostUnpackRenamer
{
	class Controller final : public Thread, public ScriptController
	{
	public:
		void Run() override;
		static void StartJob(PostInfo* postInfo);

	protected:
		void AddMessage(Message::EKind kind, const char* text) override;

	private:
		PostInfo* m_postInfo;
		std::string m_name;
		std::string m_dstDir;
		bool RenameFiles(const std::string& dir, const std::string& nameToRename);
	};
}

#endif
