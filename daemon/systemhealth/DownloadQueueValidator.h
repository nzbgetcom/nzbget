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

#ifndef DOWNLOAD_QUEUE_VALIDATOR_H
#define DOWNLOAD_QUEUE_VALIDATOR_H

#include "SectionValidator.h"
#include "Options.h"

namespace SystemHealth::DownloadQueue
{
class DownloadQueueValidator final : public SectionValidator
{
public:
	explicit DownloadQueueValidator(const Options& options);
	std::string_view GetName() const override { return "DownloadQueue"; }

private:
	const Options& m_options;
};

class FlushQueueValidator : public Validator
{
public:
	explicit FlushQueueValidator(const Options& options) : m_options(options) {}
	std::string_view GetName() const override { return Options::FLUSHQUEUE; }
	Status Validate() const override;

private:
	const Options& m_options;
};

class ContinuePartialValidator : public Validator
{
public:
	explicit ContinuePartialValidator(const Options& options) : m_options(options) {}
	std::string_view GetName() const override { return Options::CONTINUEPARTIAL; }
	Status Validate() const override;

private:
	const Options& m_options;
};

class PropagationDelayValidator : public Validator
{
public:
	explicit PropagationDelayValidator(const Options& options) : m_options(options) {}
	std::string_view GetName() const override { return Options::PROPAGATIONDELAY; }
	Status Validate() const override;

private:
	const Options& m_options;
};

class ArticleCacheValidator : public Validator
{
public:
	explicit ArticleCacheValidator(const Options& options) : m_options(options) {}
	std::string_view GetName() const override { return Options::ARTICLECACHE; }
	Status Validate() const override;

private:
	const Options& m_options;
};

class DirectWriteValidator : public Validator
{
public:
	explicit DirectWriteValidator(const Options& options) : m_options(options) {}
	std::string_view GetName() const override { return Options::DIRECTWRITE; }
	Status Validate() const override;

private:
	const Options& m_options;
};

class WriteBufferValidator : public Validator
{
public:
	explicit WriteBufferValidator(const Options& options) : m_options(options) {}
	std::string_view GetName() const override { return Options::WRITEBUFFER; }
	Status Validate() const override;

private:
	const Options& m_options;
};

class FileNamingValidator : public Validator
{
public:
	explicit FileNamingValidator(const Options& options) : m_options(options) {}
	std::string_view GetName() const override { return Options::FILENAMING; }
	Status Validate() const override;

private:
	const Options& m_options;
};

class RenameAfterUnpackValidator : public Validator
{
public:
	explicit RenameAfterUnpackValidator(const Options& options) : m_options(options) {}
	std::string_view GetName() const override { return Options::RENAMEAFTERUNPACK; }
	Status Validate() const override;

private:
	const Options& m_options;
};

class RenameIgnoreExtValidator : public Validator
{
public:
	explicit RenameIgnoreExtValidator(const Options& options) : m_options(options) {}
	std::string_view GetName() const override { return Options::RENAMEIGNOREEXT; }
	Status Validate() const override;

private:
	const Options& m_options;
};

class ReorderFilesValidator : public Validator
{
public:
	explicit ReorderFilesValidator(const Options& options) : m_options(options) {}
	std::string_view GetName() const override { return Options::REORDERFILES; }
	Status Validate() const override;

private:
	const Options& m_options;
};

class PostStrategyValidator : public Validator
{
public:
	explicit PostStrategyValidator(const Options& options) : m_options(options) {}
	std::string_view GetName() const override { return Options::POSTSTRATEGY; }
	Status Validate() const override;

private:
	const Options& m_options;
};

class DiskSpaceValidator : public Validator
{
public:
	explicit DiskSpaceValidator(const Options& options) : m_options(options) {}
	std::string_view GetName() const override { return Options::DISKSPACE; }
	Status Validate() const override;

private:
	const Options& m_options;
};

class NzbCleanupDiskValidator : public Validator
{
public:
	explicit NzbCleanupDiskValidator(const Options& options) : m_options(options) {}
	std::string_view GetName() const override { return Options::NZBCLEANUPDISK; }
	Status Validate() const override;

private:
	const Options& m_options;
};

class KeepHistoryValidator : public Validator
{
public:
	explicit KeepHistoryValidator(const Options& options) : m_options(options) {}
	std::string_view GetName() const override { return Options::KEEPHISTORY; }
	Status Validate() const override;

private:
	const Options& m_options;
};

class FeedHistoryValidator : public Validator
{
public:
	explicit FeedHistoryValidator(const Options& options) : m_options(options) {}
	std::string_view GetName() const override { return Options::FEEDHISTORY; }
	Status Validate() const override;

private:
	const Options& m_options;
};

class SkipWriteValidator : public Validator
{
public:
	explicit SkipWriteValidator(const Options& options) : m_options(options) {}
	std::string_view GetName() const override { return Options::SKIPWRITE; }
	Status Validate() const override;

private:
	const Options& m_options;
};

class RawArticleValidator : public Validator
{
public:
	explicit RawArticleValidator(const Options& options) : m_options(options) {}
	std::string_view GetName() const override { return Options::RAWARTICLE; }
	Status Validate() const override;

private:
	const Options& m_options;
};

}  // namespace SystemHealth::DownloadQueue

#endif
