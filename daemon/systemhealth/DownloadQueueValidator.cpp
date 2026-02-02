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
		return Status::Warning("'" + std::string(Options::FLUSHQUEUE) + "' is enabled while '" +
							   std::string(Options::SKIPWRITE) +
							   "' is enabled: flushed data may not be written to disk");
	}
	return Status::Ok();
}

Status ContinuePartialValidator::Validate() const
{
	if (m_options.GetContinuePartial())
		return Status::Info("Disabling '" + std::string(Options::CONTINUEPARTIAL) +
							"' may slightly reduce disk access and is recommended on fast "
							"connections");

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

	if (val == 0)
		return Status::Warning(
			"'" + std::string(Options::ARTICLECACHE) +
			"' is disabled. Enabling it is recommended to reduce disk fragmentation");

	// Check for 32-bit architecture limit (1900 MB)
	if (sizeof(void*) == 4 && val > 1900)
		return Status::Error("'" + std::string(Options::ARTICLECACHE) +
							 "' cannot exceed 1900 MB on 32-bit systems");

	if (m_options.GetDirectWrite())
	{
		if (val < 50)
		{
			return Status::Warning("It's recommended to set '" +
								   std::string(Options::ARTICLECACHE) + "' at least 50MB");
		}
		else
		{
			return Status::Ok();
		}
	}
	else if (val < 200)
	{
		return Status::Warning(
			"A cache under 200 MB is likely too small to hold complete files, "
			"forcing writes to the temporary directory and degrading performance");
	}

	return Status::Ok();
}

Status DirectWriteValidator::Validate() const
{
	if (!m_options.GetDirectWrite() && !m_options.GetRawArticle())
		return Status::Warning(
			"'" + std::string(Options::DIRECTWRITE) +
			"' is disabled. "
			"Articles will be written to the temporary directory first, then copied to the "
			"destination. "
			"Enabling this option is usually recommended to reduce disk I/O usage");

	return Status::Ok();
}

Status WriteBufferValidator::Validate() const
{
	int val = m_options.GetWriteBuffer();
	Status s = CheckPositiveNum(Options::WRITEBUFFER, val);
	if (!s.IsOk()) return s;

	if (val == 0)
	{
		return Status::Warning(
			"'" + std::string(Options::WRITEBUFFER) +
			"' is set to '0'. "
			"This uses the default system buffer, which is often too small and inefficient");
	}

	if (val < 1024)
	{
		return Status::Warning(
			"'" + std::string(Options::WRITEBUFFER) +
			"' is very low. "
			"At least 1024 KB is recommended for systems with sufficient memory");
	}

	// Warn if buffer is excessively large (e.g., > 100MB per connection)
	if (val > 102400)
	{
		return Status::Warning(
			"'" + std::string(Options::WRITEBUFFER) +
			"' is very large (>100MB). This is per-connection and may exhaust memory");
	}
	return Status::Ok();
}

Status FileNamingValidator::Validate() const
{
	const auto val = m_options.GetFileNaming();
	switch (val)
	{
		case Options::nfAuto:
			return Status::Ok();

		case Options::EFileNaming::nfNzb:
			return Status::Info("'" + std::string(Options::FILENAMING) +
								"' is set to 'Nzb'. "
								"Files will be named strictly based on the NZB content. "
								"Obfuscated releases often result in meaningless filenames with "
								"this setting. 'Auto' is recommended");

		case Options::EFileNaming::nfArticle:
			return Status::Info("'" + std::string(Options::FILENAMING) +
								"' is set to 'Article'. "
								"Files will be named using subject headers from the articles. "
								"If headers are malformed or missing, filenames will be incorrect. "
								"'Auto' is recommended");

		default:
			return Status::Ok();
	}
}

Status RenameAfterUnpackValidator::Validate() const { return Status::Ok(); }

Status RenameIgnoreExtValidator::Validate() const { return Status::Ok(); }

Status ReorderFilesValidator::Validate() const { return Status::Ok(); }

Status PostStrategyValidator::Validate() const
{
	const auto strategy = m_options.GetPostStrategy();
	switch (strategy)
	{
		case Options::EPostStrategy::ppBalanced:
			return Status::Ok();

		case Options::EPostStrategy::ppSequential:
			return Status::Info("'" + std::string(Options::POSTSTRATEGY) +
								"' is set to 'Sequential'. "
								"Safe for low-end hardware, but may be slower.");

		case Options::EPostStrategy::ppAggressive:
			return Status::Info(
				"'" + std::string(Options::POSTSTRATEGY) +
				"' is set to 'Aggressive'. "
				"Ensure you have a multi-core CPU and fast storage (SSD) to prevent bottlenecks");

		case Options::EPostStrategy::ppRocket:
			return Status::Info("'" + std::string(Options::POSTSTRATEGY) +
								"' is set to 'Rocket'. "
								"This requires high-end hardware (NVMe SSD, many CPU cores)");

		default:
			return Status::Ok();
	}
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
		return Status::Warning("'" + std::string(Options::DISKSPACE) +
							   "' is set very low (<50MB). Downloads may fill disk before pausing");
	}
	return Status::Ok();
}

Status NzbCleanupDiskValidator::Validate() const
{
	if (!m_options.GetNzbCleanupDisk())
		return Status::Info(
			"'" + std::string(Options::NZBCLEANUPDISK) +
			"' is disabled. "
			"Source NZB files will remain in the incoming directory after processing");

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
		return Status::Warning("'" + std::string(Options::SKIPWRITE) +
							   "' is enabled: downloaded data will NOT be saved to disk");
	}
	return Status::Ok();
}

Status RawArticleValidator::Validate() const
{
	if (m_options.GetRawArticle())
	{
		return Status::Warning(
			"'" + std::string(Options::RAWARTICLE) +
			"' is enabled: articles will be saved in raw format (unusable for normal files)");
	}
	return Status::Ok();
}
}  // namespace SystemHealth::DownloadQueue
