/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2025 Denis <denis@nzbget.com>
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

#include "nzbget.h"

#include "DownloadQueueValidator.h"
#include "Options.h"
#include "Validators.h"
#include <memory>
#include <string>

namespace SystemHealth::DownloadQueue
{
DownloadQueueValidator::DownloadQueueValidator(const Options& options) : m_options(options)
{
	m_validators.reserve(17);
	m_validators.push_back(std::make_unique<FlushQueueValidator>(options));
	m_validators.push_back(std::make_unique<ContinuePartialValidator>(options));
	m_validators.push_back(std::make_unique<PropagationDelayValidator>(options));
	m_validators.push_back(std::make_unique<ArticleCacheValidator>(options));
	m_validators.push_back(std::make_unique<DirectWriteValidator>(options));
	m_validators.push_back(std::make_unique<WriteBufferValidator>(options));
	m_validators.push_back(std::make_unique<FileNamingValidator>(options));
	m_validators.push_back(std::make_unique<RenameAfterUnpackValidator>(options));
	m_validators.push_back(std::make_unique<RenameIgnoreExtValidator>(options));
	m_validators.push_back(std::make_unique<ReorderFilesValidator>(options));
	m_validators.push_back(std::make_unique<PostStrategyValidator>(options));
	m_validators.push_back(std::make_unique<DiskSpaceValidator>(options));
	m_validators.push_back(std::make_unique<NzbCleanupDiskValidator>(options));
	m_validators.push_back(std::make_unique<KeepHistoryValidator>(options));
	m_validators.push_back(std::make_unique<FeedHistoryValidator>(options));
	m_validators.push_back(std::make_unique<SkipWriteValidator>(options));
	m_validators.push_back(std::make_unique<RawArticleValidator>(options));
}

Status FlushQueueValidator::Validate() const
{
	if (m_options.GetFlushQueue() && m_options.GetSkipWrite())
	{
		return Status::Warning(std::string(Options::FLUSHQUEUE) + " is enabled while " +
							   std::string(Options::SKIPWRITE) +
							   " is enabled; flushed data may not be written to disk.");
	}
	return Status::Ok();
}

Status ContinuePartialValidator::Validate() const
{
	if (m_options.GetContinuePartial() && m_options.GetDirectWrite())
	{
		return Status::Info(std::string(Options::CONTINUEPARTIAL) + " is enabled alongside " +
							std::string(Options::DIRECTWRITE) +
							". Behavior depends on direct write semantics.");
	}
	return Status::Ok();
}

Status PropagationDelayValidator::Validate() const
{
	return CheckPositiveNum(Options::PROPAGATIONDELAY, m_options.GetPropagationDelay());
}

Status ArticleCacheValidator::Validate() const
{
	int val = m_options.GetArticleCache();
	Status s = CheckPositiveNum(Options::ARTICLECACHE, val);
	if (!s.IsOk()) return s;

	// Check for 32-bit architecture limit (1900 MB)
	if (sizeof(void*) == 4 && val > 1900)
	{
		return Status::Error(std::string(Options::ARTICLECACHE) +
							 " cannot exceed 1900 MB on 32-bit systems.");
	}
	return Status::Ok();
}

Status DirectWriteValidator::Validate() const
{
	if (m_options.GetDirectWrite() && m_options.GetWriteBuffer() == 0)
	{
		return Status::Warning(std::string(Options::DIRECTWRITE) + " is enabled but " +
							   std::string(Options::WRITEBUFFER) +
							   " is 0; performance may be poor.");
	}
	return Status::Ok();
}

Status WriteBufferValidator::Validate() const
{
	int val = m_options.GetWriteBuffer();
	Status s = CheckPositiveNum(Options::WRITEBUFFER, val);
	if (!s.IsOk()) return s;

	// Warn if buffer is excessively large (e.g., > 100MB per connection)
	if (val > 102400)
	{
		return Status::Warning(
			std::string(Options::WRITEBUFFER) +
			" is very large (>100MB). This is per-connection and may exhaust memory.");
	}
	return Status::Ok();
}

Status FileNamingValidator::Validate() const
{
	auto val = m_options.GetFileNaming();
	if (val != Options::EFileNaming::nfAuto && val != Options::EFileNaming::nfArticle &&
		val != Options::EFileNaming::nfNzb)
	{
		return Status::Error("Invalid value for " + std::string(Options::FILENAMING) +
							 ". Must be 'auto', 'article', or 'nzb'.");
	}
	return Status::Ok();
}

Status RenameAfterUnpackValidator::Validate() const
{
	if (m_options.GetRenameAfterUnpack() && !m_options.GetUnpack())
	{
		return Status::Info(
			std::string(Options::RENAMEAFTERUNPACK) +
			" is enabled but Unpack is disabled; renaming after unpack will not occur.");
	}
	return Status::Ok();
}

Status RenameIgnoreExtValidator::Validate() const
{
	std::string_view v = m_options.GetRenameIgnoreExt();
	if (v.empty()) return Status::Ok();
	if (v.size() > 512)
		return Status::Warning(std::string(Options::RENAMEIGNOREEXT) + " is unusually long.");
	return Status::Ok();
}

Status ReorderFilesValidator::Validate() const
{
	if (m_options.GetReorderFiles() && m_options.GetFileNaming() == Options::EFileNaming::nfNzb)
	{
		return Status::Info(std::string(Options::REORDERFILES) +
							" is enabled but file naming mode 'nzb' is selected; reordering may "
							"have limited effect.");
	}
	return Status::Ok();
}

Status PostStrategyValidator::Validate() const
{
	auto val = m_options.GetPostStrategy();
	if (val != Options::EPostStrategy::ppSequential && val != Options::EPostStrategy::ppBalanced &&
		val != Options::EPostStrategy::ppAggressive && val != Options::EPostStrategy::ppRocket)
	{
		return Status::Error("Invalid value for " + std::string(Options::POSTSTRATEGY) + ".");
	}
	return Status::Ok();
}

Status DiskSpaceValidator::Validate() const
{
	int val = m_options.GetDiskSpace();
	Status s = CheckPositiveNum(Options::DISKSPACE, val);
	if (!s.IsOk()) return s;

	// 0 means disabled, which is fine.
	// If enabled but very low (e.g. < 50MB), it might be too late to pause effectively.
	if (val > 0 && val < 50)
	{
		return Status::Warning(std::string(Options::DISKSPACE) +
							   " is set very low (<50MB). Downloads may fill disk before pausing.");
	}
	return Status::Ok();
}

Status NzbCleanupDiskValidator::Validate() const
{
	if (m_options.GetNzbCleanupDisk() && m_options.GetDiskSpace() == 0)
	{
		return Status::Info(std::string(Options::NZBCLEANUPDISK) +
							" is enabled but DiskSpace enforcement is disabled; cleanup thresholds "
							"won't be applied.");
	}
	return Status::Ok();
}

Status KeepHistoryValidator::Validate() const
{
	return CheckPositiveNum(Options::KEEPHISTORY, m_options.GetKeepHistory());
}

Status FeedHistoryValidator::Validate() const
{
	return CheckPositiveNum(Options::FEEDHISTORY, m_options.GetFeedHistory());
}

Status SkipWriteValidator::Validate() const
{
	if (m_options.GetSkipWrite())
	{
		return Status::Warning(std::string(Options::SKIPWRITE) +
							   " is enabled! Downloaded data will NOT be saved to disk.");
	}
	return Status::Ok();
}
Status RawArticleValidator::Validate() const
{
	if (m_options.GetRawArticle())
	{
		return Status::Warning(
			std::string(Options::RAWARTICLE) +
			" is enabled! Articles will be saved in raw format (unusable for normal files).");
	}
	return Status::Ok();
}
}  // namespace SystemHealth::DownloadQueue
