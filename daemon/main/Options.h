/*
 *  This file is part of nzbget. See <https://nzbget.com>.
 *
 *  Copyright (C) 2004 Sven Henkel <sidddy@users.sourceforge.net>
 *  Copyright (C) 2007-2019 Andrey Prygunkov <hugbug@users.sourceforge.net>
 *  Copyright (C) 2024-2025 Denis <denis@nzbget.com>
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


#ifndef OPTIONS_H
#define OPTIONS_H

#include <string_view>
#include "NString.h"
#include "Thread.h"
#include "Util.h"
#include "FeedInfo.h"


class Options
{
public:
 // Program options
	static constexpr std::string_view CONFIGFILE = "ConfigFile";
	static constexpr std::string_view APPBIN = "AppBin";
	static constexpr std::string_view APPDIR = "AppDir";
	static constexpr std::string_view APPVERSION = "Version";
	static constexpr std::string_view MAINDIR = "MainDir";
	static constexpr std::string_view DESTDIR = "DestDir";
	static constexpr std::string_view INTERDIR = "InterDir";
	static constexpr std::string_view TEMPDIR = "TempDir";
	static constexpr std::string_view QUEUEDIR = "QueueDir";
	static constexpr std::string_view NZBDIR = "NzbDir";
	static constexpr std::string_view WEBDIR = "WebDir";
	static constexpr std::string_view CONFIGTEMPLATE = "ConfigTemplate";
	static constexpr std::string_view SCRIPTDIR = "ScriptDir";
	static constexpr std::string_view REQUIREDDIR = "RequiredDir";
	static constexpr std::string_view LOGFILE = "LogFile";
	static constexpr std::string_view WRITELOG = "WriteLog";
	static constexpr std::string_view ROTATELOG = "RotateLog";
	static constexpr std::string_view APPENDCATEGORYDIR = "AppendCategoryDir";
	static constexpr std::string_view LOCKFILE = "LockFile";
	static constexpr std::string_view DAEMONUSERNAME = "DaemonUsername";
	static constexpr std::string_view OUTPUTMODE = "OutputMode";
	static constexpr std::string_view DUPECHECK = "DupeCheck";
	static constexpr std::string_view DOWNLOADRATE = "DownloadRate";
	static constexpr std::string_view CONTROLIP = "ControlIp";
	static constexpr std::string_view CONTROLPORT = "ControlPort";
	static constexpr std::string_view CONTROLUSERNAME = "ControlUsername";
	static constexpr std::string_view CONTROLPASSWORD = "ControlPassword";
	static constexpr std::string_view RESTRICTEDUSERNAME = "RestrictedUsername";
	static constexpr std::string_view RESTRICTEDPASSWORD = "RestrictedPassword";
	static constexpr std::string_view ADDUSERNAME = "AddUsername";
	static constexpr std::string_view ADDPASSWORD = "AddPassword";
	static constexpr std::string_view FORMAUTH = "FormAuth";
	static constexpr std::string_view SECURECONTROL = "SecureControl";
	static constexpr std::string_view SECUREPORT = "SecurePort";
	static constexpr std::string_view SECURECERT = "SecureCert";
	static constexpr std::string_view SECUREKEY = "SecureKey";
	static constexpr std::string_view CERTSTORE = "CertStore";
	static constexpr std::string_view CERTCHECK = "CertCheck";
	static constexpr std::string_view AUTHORIZEDIP = "AuthorizedIP";
	static constexpr std::string_view ARTICLETIMEOUT = "ArticleTimeout";
	static constexpr std::string_view ARTICLEREADCHUNKSIZE = "ArticleReadChunkSize";
	static constexpr std::string_view URLTIMEOUT = "UrlTimeout";
	static constexpr std::string_view REMOTETIMEOUT = "RemoteTimeout";
	static constexpr std::string_view FLUSHQUEUE = "FlushQueue";
	static constexpr std::string_view NZBLOG = "NzbLog";
	static constexpr std::string_view RAWARTICLE = "RawArticle";
	static constexpr std::string_view SKIPWRITE = "SkipWrite";
	static constexpr std::string_view ARTICLERETRIES = "ArticleRetries";
	static constexpr std::string_view ARTICLEINTERVAL = "ArticleInterval";
	static constexpr std::string_view URLRETRIES = "UrlRetries";
	static constexpr std::string_view URLINTERVAL = "UrlInterval";
	static constexpr std::string_view CONTINUEPARTIAL = "ContinuePartial";
	static constexpr std::string_view URLCONNECTIONS = "UrlConnections";
	static constexpr std::string_view LOGBUFFER = "LogBuffer";
	static constexpr std::string_view INFOTARGET = "InfoTarget";
	static constexpr std::string_view WARNINGTARGET = "WarningTarget";
	static constexpr std::string_view ERRORTARGET = "ErrorTarget";
	static constexpr std::string_view DEBUGTARGET = "DebugTarget";
	static constexpr std::string_view DETAILTARGET = "DetailTarget";
	static constexpr std::string_view PARCHECK = "ParCheck";
	static constexpr std::string_view PARREPAIR = "ParRepair";
	static constexpr std::string_view PARSCAN = "ParScan";
	static constexpr std::string_view PARQUICK = "ParQuick";
	static constexpr std::string_view POSTSTRATEGY = "PostStrategy";
	static constexpr std::string_view FILENAMING = "FileNaming";
	static constexpr std::string_view RENAMEAFTERUNPACK = "RenameAfterUnpack";
	static constexpr std::string_view RENAMEIGNOREEXT = "RenameIgnoreExt";
	static constexpr std::string_view PARRENAME = "ParRename";
	static constexpr std::string_view PARBUFFER = "ParBuffer";
	static constexpr std::string_view PARTHREADS = "ParThreads";
	static constexpr std::string_view RARRENAME = "RarRename";
	static constexpr std::string_view HEALTHCHECK = "HealthCheck";
	static constexpr std::string_view DIRECTRENAME = "DirectRename";
	static constexpr std::string_view HARDLINKING = "HardLinking";
	static constexpr std::string_view HARDLINKINGIGNOREEXT = "HardLinkingIgnoreExt";
	static constexpr std::string_view UMASK = "UMask";
	static constexpr std::string_view UPDATEINTERVAL = "UpdateInterval";
	static constexpr std::string_view CURSESNZBNAME = "CursesNzbName";
	static constexpr std::string_view CURSESTIME = "CursesTime";
	static constexpr std::string_view CURSESGROUP = "CursesGroup";
	static constexpr std::string_view CRCCHECK = "CrcCheck";
	static constexpr std::string_view DIRECTWRITE = "DirectWrite";
	static constexpr std::string_view WRITEBUFFER = "WriteBuffer";
	static constexpr std::string_view NZBDIRINTERVAL = "NzbDirInterval";
	static constexpr std::string_view NZBDIRFILEAGE = "NzbDirFileAge";
	static constexpr std::string_view DISKSPACE = "DiskSpace";
	static constexpr std::string_view CRASHTRACE = "CrashTrace";
	static constexpr std::string_view CRASHDUMP = "CrashDump";
	static constexpr std::string_view PARPAUSEQUEUE = "ParPauseQueue";
	static constexpr std::string_view SCRIPTPAUSEQUEUE = "ScriptPauseQueue";
	static constexpr std::string_view NZBCLEANUPDISK = "NzbCleanupDisk";
	static constexpr std::string_view PARTIMELIMIT = "ParTimeLimit";
	static constexpr std::string_view KEEPHISTORY = "KeepHistory";
	static constexpr std::string_view UNPACK = "Unpack";
	static constexpr std::string_view DIRECTUNPACK = "DirectUnpack";
	static constexpr std::string_view USETEMPUNPACKDIR = "UseTempUnpackDir";
	static constexpr std::string_view UNPACKCLEANUPDISK = "UnpackCleanupDisk";
	static constexpr std::string_view UNRARCMD = "UnrarCmd";
	static constexpr std::string_view SEVENZIPCMD = "SevenZipCmd";
	static constexpr std::string_view UNPACKPASSFILE = "UnpackPassFile";
	static constexpr std::string_view UNPACKPAUSEQUEUE = "UnpackPauseQueue";
	static constexpr std::string_view SCRIPTORDER = "ScriptOrder";
	static constexpr std::string_view EXTENSIONS = "Extensions";
	static constexpr std::string_view EXTCLEANUPDISK = "ExtCleanupDisk";
	static constexpr std::string_view PARIGNOREEXT = "ParIgnoreExt";
	static constexpr std::string_view UNPACKIGNOREEXT = "UnpackIgnoreExt";
	static constexpr std::string_view FEEDHISTORY = "FeedHistory";
	static constexpr std::string_view URLFORCE = "UrlForce";
	static constexpr std::string_view TIMECORRECTION = "TimeCorrection";
	static constexpr std::string_view PROPAGATIONDELAY = "PropagationDelay";
	static constexpr std::string_view ARTICLECACHE = "ArticleCache";
	static constexpr std::string_view EVENTINTERVAL = "EventInterval";
	static constexpr std::string_view SHELLOVERRIDE = "ShellOverride";
	static constexpr std::string_view MONTHLYQUOTA = "MonthlyQuota";
	static constexpr std::string_view QUOTASTARTDAY = "QuotaStartDay";
	static constexpr std::string_view DAILYQUOTA = "DailyQuota";
	static constexpr std::string_view REORDERFILES = "ReorderFiles";
	static constexpr std::string_view UPDATECHECK = "UpdateCheck";

// obsolete options
	static constexpr std::string_view POSTLOGKIND = "PostLogKind";
	static constexpr std::string_view NZBLOGKIND = "NZBLogKind";
	static constexpr std::string_view RETRYONCRCERROR = "RetryOnCrcError";
	static constexpr std::string_view ALLOWREPROCESS = "AllowReProcess";
	static constexpr std::string_view POSTPROCESS = "PostProcess";
	static constexpr std::string_view LOADPARS = "LoadPars";
	static constexpr std::string_view THREADLIMIT = "ThreadLimit";
	static constexpr std::string_view PROCESSLOGKIND = "ProcessLogKind";
	static constexpr std::string_view APPENDNZBDIR = "AppendNzbDir";
	static constexpr std::string_view RENAMEBROKEN = "RenameBroken";
	static constexpr std::string_view MERGENZB = "MergeNzb";
	static constexpr std::string_view STRICTPARNAME = "StrictParName";
	static constexpr std::string_view RELOADURLQUEUE = "ReloadUrlQueue";
	static constexpr std::string_view RELOADPOSTQUEUE = "ReloadPostQueue";
	static constexpr std::string_view NZBPROCESS = "NZBProcess";
	static constexpr std::string_view NZBADDEDPROCESS = "NZBAddedProcess";
	static constexpr std::string_view CREATELOG = "CreateLog";
	static constexpr std::string_view RESETLOG = "ResetLog";
	static constexpr std::string_view PARCLEANUPQUEUE = "ParCleanupQueue";
	static constexpr std::string_view DELETECLEANUPDISK = "DeleteCleanupDisk";
	static constexpr std::string_view HISTORYCLEANUPDISK = "HistoryCleanupDisk";
	static constexpr std::string_view SCANSCRIPT = "ScanScript";
	static constexpr std::string_view QUEUESCRIPT = "QueueScript";
	static constexpr std::string_view FEEDSCRIPT = "FeedScript";
	static constexpr std::string_view DECODE = "Decode";
	static constexpr std::string_view SAVEQUEUE = "SaveQueue";
	static constexpr std::string_view RELOADQUEUE = "ReloadQueue";
	static constexpr std::string_view TERMINATETIMEOUT = "TerminateTimeout";
	static constexpr std::string_view ACCURATERATE = "AccurateRate";
	static constexpr std::string_view CREATEBROKENLOG = "CreateBrokenLog";
	static constexpr std::string_view BROKENLOG = "BrokenLog";

	enum EWriteLog
	{
		wlNone,
		wlAppend,
		wlReset,
		wlRotate
	};
	enum EMessageTarget
	{
		mtNone,
		mtScreen,
		mtLog,
		mtBoth
	};
	enum EOutputMode
	{
		omLoggable,
		omColored,
		omNCurses
	};
	enum EParCheck
	{
		pcAuto,
		pcAlways,
		pcForce,
		pcManual
	};
	enum EParScan
	{
		psLimited,
		psExtended,
		psFull,
		psDupe
	};
	enum EHealthCheck
	{
		hcPause,
		hcDelete,
		hcPark,
		hcNone
	};
	enum ESchedulerCommand
	{
		scPauseDownload,
		scUnpauseDownload,
		scPausePostProcess,
		scUnpausePostProcess,
		scDownloadRate,
		scScript,
		scProcess,
		scPauseScan,
		scUnpauseScan,
		scActivateServer,
		scDeactivateServer,
		scFetchFeed
	};
	enum EPostStrategy
	{
		ppSequential,
		ppBalanced,
		ppAggressive,
		ppRocket
	};
	enum EFileNaming
	{
		nfAuto,
		nfArticle,
		nfNzb
	};
	enum ECertVerifLevel
	{
		cvNone,
		cvMinimal,
		cvStrict,
		Count
	};

	class OptEntry
	{
	public:
		OptEntry(const char* name, const char* value) :
			m_name(name), m_value(value) {}
		void SetName(const char* name) { m_name = name; }
		const char* GetName() { return m_name; }
		void SetValue(const char* value);
		const char* GetValue() { return m_value; }
		const char* GetDefValue() { return m_defValue; }
		int GetLineNo() { return m_lineNo; }
		bool Restricted();

	private:
		CString m_name;
		CString m_value;
		CString m_defValue;
		int m_lineNo = 0;

		void SetLineNo(int lineNo) { m_lineNo = lineNo; }

		friend class Options;
	};

	typedef std::deque<OptEntry> OptEntriesBase;

	class OptEntries: public OptEntriesBase
	{
	public:
		OptEntry* FindOption(const char* name);
	};

	typedef GuardedPtr<OptEntries> GuardedOptEntries;

	typedef std::vector<CString> NameList;
	typedef std::vector<const char*> CmdOptList;

	class Category
	{
	public:
		Category(const char* name, const char* destDir, bool unpack, const char* extensions) :
			m_name(name), m_destDir(destDir), m_unpack(unpack), m_extensions(extensions) {}
		const char* GetName() { return m_name; }
		const char* GetDestDir() { return m_destDir; }
		bool GetUnpack() { return m_unpack; }
		const char* GetExtensions() { return m_extensions; }
		NameList* GetAliases() { return &m_aliases; }

	private:
		CString m_name;
		CString m_destDir;
		bool m_unpack;
		CString m_extensions;
		NameList m_aliases;
	};

	typedef std::deque<Category> CategoriesBase;

	class Categories: public CategoriesBase
	{
	public:
		Category* FindCategory(const char* name, bool searchAliases);
	};

	class Extender
	{
	public:
		virtual void AddNewsServer(int id, bool active, const char* name, const char* host,
			int port, int ipVersion, const char* user, const char* pass, bool joinGroup,
			bool tls, const char* cipher, int maxConnections, int retention,
			int level, int group, bool optional, unsigned int certVerificationfLevel) = 0;
		virtual void AddFeed(
			[[maybe_unused]] int id,
			[[maybe_unused]] const char* name,
			[[maybe_unused]] const char* url,
			[[maybe_unused]] int interval,
			[[maybe_unused]] const char* filter,
			[[maybe_unused]] bool backlog,
			[[maybe_unused]] bool pauseNzb,
			[[maybe_unused]] const char* category,
			[[maybe_unused]] FeedInfo::CategorySource categorySource,
			[[maybe_unused]] int priority,
			[[maybe_unused]] const char* extensions
		) { }
		virtual void AddTask([[maybe_unused]] int id, [[maybe_unused]] int hours, [[maybe_unused]] int minutes,
			[[maybe_unused]] int weekDaysBits, [[maybe_unused]] ESchedulerCommand command,
			[[maybe_unused]] const char* param) {}
		virtual void SetupFirstStart() {}
	};

	Options(const char* exeName, const char* configFilename, bool noConfig,
		CmdOptList* commandLineOptions, Extender* extender);
	Options(CmdOptList* commandLineOptions, Extender* extender);
	~Options();

	static bool SplitOptionString(const char* option, CString& optName, CString& optValue);
	static void ConvertOldOptions(OptEntries* optEntries);
	bool GetFatalError() { return m_fatalError; }
	GuardedOptEntries GuardOptEntries() { return GuardedOptEntries(&m_optEntries, &m_optEntriesMutex); }
	void CreateSchedulerTask(int id, const char* time, const char* weekDays,
		ESchedulerCommand command, const char* param);

	// Options
	const char* GetConfigFilename() const { return m_configFilename; }
	bool GetConfigErrors() const { return m_configErrors; }
	const char* GetMainDir() const { return m_mainDir; }
	const char* GetAppDir() const { return m_appDir; }
	const char* GetDestDir() const { return m_destDir; }
	const char* GetInterDir() const { return m_interDir; }
	const char* GetTempDir() const { return m_tempDir; }
	const char* GetQueueDir() const { return m_queueDir; }
	const char* GetNzbDir() const { return m_nzbDir; }
	const char* GetWebDir() const { return m_webDir; }
	const char* GetConfigTemplate() const { return m_configTemplate; }
	const char* GetScriptDir() const { return m_scriptDir; }
	const char* GetRequiredDir() const { return m_requiredDir; }
	bool GetNzbLog() const { return m_nzbLog; }
	EMessageTarget GetInfoTarget() const { return m_infoTarget; }
	EMessageTarget GetWarningTarget() const { return m_warningTarget; }
	EMessageTarget GetErrorTarget() const { return m_errorTarget; }
	EMessageTarget GetDebugTarget() const { return m_debugTarget; }
	EMessageTarget GetDetailTarget() const { return m_detailTarget; }
	int GetArticleTimeout() const { return m_articleTimeout; }
	int GetArticleReadChunkSize() const { return m_articleReadChunkSize; }
	int GetUrlTimeout() const { return m_urlTimeout; }
	int GetRemoteTimeout() const { return m_remoteTimeout; }
	bool GetRawArticle() const { return m_rawArticle; }
	bool GetSkipWrite() const { return m_skipWrite; }
	bool GetAppendCategoryDir() const { return m_appendCategoryDir; }
	bool GetContinuePartial() const { return m_continuePartial; }
	int GetArticleRetries() const { return m_articleRetries; }
	int GetArticleInterval() const { return m_articleInterval; }
	int GetUrlRetries() const { return m_urlRetries; }
	int GetUrlInterval() const { return m_urlInterval; }
	bool GetFlushQueue() const { return m_flushQueue; }
	bool GetDupeCheck() const { return m_dupeCheck; }
	const char* GetControlIp() const { return m_controlIp; }
	const char* GetControlUsername() const { return m_controlUsername; }
	const char* GetControlPassword() const { return m_controlPassword; }
	const char* GetRestrictedUsername() const { return m_restrictedUsername; }
	const char* GetRestrictedPassword() const { return m_restrictedPassword; }
	const char* GetAddUsername() const { return m_addUsername; }
	const char* GetAddPassword() const { return m_addPassword; }
	int GetControlPort() const { return m_controlPort; }
	bool GetFormAuth() const { return m_formAuth; }
	bool GetSecureControl() const { return m_secureControl; }
	int GetSecurePort() const { return m_securePort; }
	const char* GetSecureCert() const { return m_secureCert; }
	const char* GetSecureKey() const { return m_secureKey; }
	const char* GetCertStore() const { return m_certStore; }
	bool GetCertCheck() const { return m_certCheck; }
	const char* GetAuthorizedIp() const { return m_authorizedIp; }
	const char* GetLockFile() const { return m_lockFile; }
	const char* GetDaemonUsername() const { return m_daemonUsername; }
	EOutputMode GetOutputMode() const { return m_outputMode; }
	int GetUrlConnections() const { return m_urlConnections; }
	int GetLogBuffer() const { return m_logBuffer; }
	EWriteLog GetWriteLog() const { return m_writeLog; }
	const char* GetLogFile() const { return m_logFile; }
	int GetRotateLog() const { return m_rotateLog; }
	EParCheck GetParCheck() const { return m_parCheck; }
	bool GetParRepair() const { return m_parRepair; }
	EParScan GetParScan() const { return m_parScan; }
	bool GetParQuick() const { return m_parQuick; }
	EPostStrategy GetPostStrategy() const { return m_postStrategy; }
	bool GetParRename() const { return m_parRename; }
	int GetParBuffer() const { return m_parBuffer; }
	int GetParThreads() const { return m_parThreads; }
	bool GetRarRename() const { return m_rarRename; }
	EHealthCheck GetHealthCheck() const { return m_healthCheck; }
	const char* GetScriptOrder() const { return m_scriptOrder; }
	const char* GetExtensions() const { return m_extensions; }
	int GetUMask() const { return m_umask; }
	int GetUpdateInterval() const { return m_updateInterval; }
	bool GetCursesNzbName() const { return m_cursesNzbName; }
	bool GetCursesTime() const { return m_cursesTime; }
	bool GetCursesGroup() const { return m_cursesGroup; }
	bool GetCrcCheck() const { return m_crcCheck; }
	bool GetDirectWrite() const { return m_directWrite; }
	int GetWriteBuffer() const { return m_writeBuffer; }
	int GetNzbDirInterval() const { return m_nzbDirInterval; }
	int GetNzbDirFileAge() const { return m_nzbDirFileAge; }
	int GetDiskSpace() const { return m_diskSpace; }
	bool GetTls() const { return m_tls; }
	bool GetCrashTrace() const { return m_crashTrace; }
	bool GetCrashDump() const { return m_crashDump; }
	bool GetParPauseQueue() const { return m_parPauseQueue; }
	bool GetScriptPauseQueue() const { return m_scriptPauseQueue; }
	bool GetNzbCleanupDisk() const { return m_nzbCleanupDisk; }
	int GetParTimeLimit() const { return m_parTimeLimit; }
	int GetKeepHistory() const { return m_keepHistory; }
	bool GetUnpack() const { return m_unpack; }
	bool GetDirectUnpack() const { return m_directUnpack; }
	bool GetUseTempUnpackDir() const { return m_useTempUnpackDir; }
	bool GetUnpackCleanupDisk() const { return m_unpackCleanupDisk; }
	const char* GetUnrarCmd() const { return m_unrarCmd; }
	const char* GetSevenZipCmd() const { return m_sevenZipCmd; }
	const char* GetUnpackPassFile() const { return m_unpackPassFile; }
	bool GetUnpackPauseQueue() const { return m_unpackPauseQueue; }
	const char* GetExtCleanupDisk() const { return m_extCleanupDisk; }
	const char* GetParIgnoreExt() const { return m_parIgnoreExt; }
	const char* GetRenameIgnoreExt() const { return m_renameIgnoreExt; }
	bool GetRenameAfterUnpack() const { return m_renameAfterUnpack; }
	const char* GetUnpackIgnoreExt() const { return m_unpackIgnoreExt; }
	int GetFeedHistory() const { return m_feedHistory; }
	bool GetUrlForce() const { return m_urlForce; }
	int GetTimeCorrection() const { return m_timeCorrection; }
	int GetPropagationDelay() const { return m_propagationDelay; }
	int GetArticleCache() const { return m_articleCache; }
	int GetEventInterval() const { return m_eventInterval; }
	const std::string& GetShellOverride() const { return m_shellOverride; }
	int GetMonthlyQuota() const { return m_monthlyQuota; }
	int GetQuotaStartDay() const { return m_quotaStartDay; }
	int GetDailyQuota() const { return m_dailyQuota; }
	bool GetDirectRename() const { return m_directRename; }
	bool GetHardLinking() const { return m_hardLinking; }
	const char* GetHardLinkingIgnoreExt() const { return m_hardLinkingIgnoreExt; }
	bool GetReorderFiles() const { return m_reorderFiles; }
	EFileNaming GetFileNaming() const { return m_fileNaming; }
	int GetDownloadRate() const { return m_downloadRate; }

	Categories* GetCategories() { return &m_categories; }
	Category* FindCategory(const char* name, bool searchAliases) { return m_categories.FindCategory(name, searchAliases); }

	// Current state
	void SetServerMode(bool serverMode) { m_serverMode = serverMode; }
	bool GetServerMode() const { return m_serverMode; }
	void SetDaemonMode(bool daemonMode) { m_daemonMode = daemonMode; }
	bool GetDaemonMode() const { return m_daemonMode; }
	void SetRemoteClientMode(bool remoteClientMode) { m_remoteClientMode = remoteClientMode; }
	bool GetRemoteClientMode() const { return m_remoteClientMode; }

private:
	void CheckDirs();

	OptEntries m_optEntries;
	Mutex m_optEntriesMutex;
	Categories m_categories;
	bool m_noDiskAccess = false;
	bool m_noConfig = false;
	bool m_fatalError = false;
	Extender* m_extender;

	// Options
	bool m_configErrors = false;
	int m_configLine = 0;
	CString m_appDir;
	CString m_configFilename;
	CString m_mainDir;
	CString m_destDir;
	CString m_interDir;
	CString m_tempDir;
	CString m_queueDir;
	CString m_nzbDir;
	CString m_webDir;
	CString m_configTemplate;
	CString m_scriptDir;
	CString m_requiredDir;
	EMessageTarget m_infoTarget = mtScreen;
	EMessageTarget m_warningTarget = mtScreen;
	EMessageTarget m_errorTarget = mtScreen;
	EMessageTarget m_debugTarget = mtNone;
	EMessageTarget m_detailTarget = mtScreen;
	bool m_skipWrite = false;
	bool m_rawArticle = false;
	bool m_nzbLog = false;
	int m_articleTimeout = 0;
	int m_articleReadChunkSize = 4;
	int m_urlTimeout = 0;
	int m_remoteTimeout = 0;
	bool m_appendCategoryDir = false;
	bool m_continuePartial = false;
	int m_articleRetries = 0;
	int m_articleInterval = 0;
	int m_urlRetries = 0;
	int m_urlInterval = 0;
	bool m_flushQueue = false;
	bool m_dupeCheck = false;
	CString m_controlIp;
	CString m_controlUsername;
	CString m_controlPassword;
	CString m_restrictedUsername;
	CString m_restrictedPassword;
	CString m_addUsername;
	CString m_addPassword;
	bool m_formAuth = false;
	int m_controlPort = 0;
	bool m_secureControl = false;
	int m_securePort = 0;
	CString m_secureCert;
	CString m_secureKey;
	CString m_certStore;
	bool m_certCheck = false;
	CString m_authorizedIp;
	CString m_lockFile;
	CString m_daemonUsername;
	EOutputMode m_outputMode = omLoggable;
	int m_urlConnections = 0;
	int m_logBuffer = 0;
	EWriteLog m_writeLog = wlAppend;
	int m_rotateLog = 0;
	CString m_logFile;
	EParCheck m_parCheck = pcManual;
	bool m_parRepair = false;
	EParScan m_parScan = psLimited;
	bool m_parQuick = true;
	EPostStrategy m_postStrategy = ppSequential;
	bool m_parRename = false;
	int m_parBuffer = 0;
	int m_parThreads = 0;
	bool m_rarRename = false;
	bool m_directRename = false;
	bool m_hardLinking = false;
	CString m_hardLinkingIgnoreExt;
	EHealthCheck m_healthCheck = hcNone;
	CString m_extensions;
	CString m_scriptOrder;
	int m_umask = 0;
	int m_updateInterval = 0;
	bool m_cursesNzbName = false;
	bool m_cursesTime = false;
	bool m_cursesGroup = false;
	bool m_crcCheck = false;
	bool m_directWrite = false;
	int m_writeBuffer = 0;
	int m_nzbDirInterval = 0;
	int m_nzbDirFileAge = 0;
	int m_diskSpace = 0;
	bool m_tls = false;
	bool m_crashTrace = false;
	bool m_crashDump = false;
	bool m_parPauseQueue = false;
	bool m_scriptPauseQueue = false;
	bool m_nzbCleanupDisk = false;
	int m_parTimeLimit = 0;
	int m_keepHistory = 0;
	bool m_unpack = false;
	bool m_directUnpack = false;
	bool m_useTempUnpackDir = true;
	bool m_unpackCleanupDisk = false;
	CString m_unrarCmd;
	CString m_sevenZipCmd;
	CString m_unpackPassFile;
	bool m_unpackPauseQueue;
	CString m_extCleanupDisk;
	CString m_parIgnoreExt;
	CString m_unpackIgnoreExt;
	int m_feedHistory = 0;
	bool m_urlForce = false;
	int m_timeCorrection = 0;
	int m_propagationDelay = 0;
	int m_articleCache = 0;
	int m_eventInterval = 0;
	std::string m_shellOverride;
	int m_monthlyQuota = 0;
	int m_quotaStartDay = 0;
	int m_dailyQuota = 0;
	bool m_reorderFiles = false;
	EFileNaming m_fileNaming = nfArticle;
	bool m_renameAfterUnpack = true;
	CString m_renameIgnoreExt;
	int m_downloadRate = 0;

	// Application mode
	bool m_serverMode = false;
	bool m_daemonMode = false;
	bool m_remoteClientMode = false;

	void Init(const char* exeName, const char* configFilename, bool noConfig,
		CmdOptList* commandLineOptions, bool noDiskAccess, Extender* extender);
	void InitDefaults();
	void InitOptions();
	void InitOptFile();
	void InitServers();
	void InitCategories();
	void InitScheduler();
	void InitFeeds();
	void InitCommandLineOptions(CmdOptList* commandLineOptions);
	void CheckOptions();
	int ParseEnumValue(const char* OptName, int argc, const char* argn[], const int argv[]);
	int ParseIntValue(const char* OptName, int base);
	FeedInfo::CategorySource ParseCategorySource(const char* value);
	OptEntry* FindOption(const char* optname);
	const char* GetOption(const char* optname);
	void SetOption(const char* optname, const char* value);
	bool SetOptionString(const char* option);
	bool ValidateOptionName(const char* optname, const char* optvalue);
	void LoadConfigFile();
	void CheckDir(CString& dir, const char* optionName, const char* parentDir,
		bool allowEmpty, bool create);
	bool ParseTime(const char* time, int* hours, int* minutes);
	bool ParseWeekDays(const char* weekDays, int* weekDaysBits);
	void ConfigError(const char* msg, ...);
	void ConfigWarn(const char* msg, ...);
	void LocateOptionSrcPos(const char *optionName);
	static void ConvertOldOption(CString& option, CString& value);
	static void MergeOldScriptOption(OptEntries* optEntries, const char* optname, bool mergeCategories);
	static bool HasScript(const char* scriptList, const char* scriptName);
};

extern Options* g_Options;

#endif
