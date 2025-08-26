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


#include "nzbget.h"
#include "Util.h"
#include "FileSystem.h"
#include "Options.h"
#include "Log.h"
#include "MessageBase.h"
#include "DownloadInfo.h"

const char* BoolNames[] = { "yes", "no", "true", "false", "1", "0", "on", "off", "enable", "disable", "enabled", "disabled" };
const int BoolValues[] = { 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0 };
const int BoolCount = 12;

#ifndef WIN32
const char* PossibleConfigLocations[] =
	{
		"~/.nzbget",
		"/etc/nzbget.conf",
		"/usr/etc/nzbget.conf",
		"/usr/local/etc/nzbget.conf",
		"/opt/etc/nzbget.conf",
		"~/usr/etc/nzbget.conf",
		nullptr
	};
#endif

void Options::OptEntry::SetValue(const char* value)
{
	m_value = value;
	if (!m_defValue)
	{
		m_defValue = value;
	}
}

bool Options::OptEntry::Restricted()
{
	BString<1024> loName = *m_name;
	for (char* p = loName; *p; p++) *p = tolower(*p); // convert string to lowercase

	bool restricted = !strcasecmp(m_name, CONTROLIP.data()) ||
		!strcasecmp(m_name, CONTROLPORT.data()) ||
		!strcasecmp(m_name, FORMAUTH.data()) ||
		!strcasecmp(m_name, SECURECONTROL.data()) ||
		!strcasecmp(m_name, SECUREPORT.data()) ||
		!strcasecmp(m_name, SECURECERT.data()) ||
		!strcasecmp(m_name, SECUREKEY.data()) ||
		!strcasecmp(m_name, CERTSTORE.data()) ||
		!strcasecmp(m_name, CERTCHECK.data()) ||
		!strcasecmp(m_name, AUTHORIZEDIP.data()) ||
		!strcasecmp(m_name, DAEMONUSERNAME.data()) ||
		!strcasecmp(m_name, UMASK.data()) ||
		strchr(m_name, ':') ||			// All extension script options
		strstr(loName, "username") ||	// ServerX.Username, ControlUsername, etc.
		strstr(loName, "password");		// ServerX.Password, ControlPassword, etc.

	return restricted;
}

Options::OptEntry* Options::OptEntries::FindOption(const char* name)
{
	if (!name)
	{
		return nullptr;
	}

	for (OptEntry& optEntry : this)
	{
		if (!strcasecmp(optEntry.GetName(), name))
		{
			return &optEntry;
		}
	}

	return nullptr;
}


Options::Category* Options::Categories::FindCategory(const char* name, bool searchAliases)
{
	if (!name)
	{
		return nullptr;
	}

	for (Category& category : this)
	{
		if (!strcasecmp(category.GetName(), name))
		{
			return &category;
		}
	}

	if (searchAliases)
	{
		for (Category& category : this)
		{
			for (CString& alias : category.GetAliases())
			{
				WildMask mask(alias);
				if (mask.Match(name))
				{
					return &category;
				}
			}
		}
	}

	return nullptr;
}


Options::Options(const char* exeName, const char* configFilename, bool noConfig,
	CmdOptList* commandLineOptions, Extender* extender)
{
	Init(exeName, configFilename, noConfig, commandLineOptions, false, extender);
}

Options::Options(CmdOptList* commandLineOptions, Extender* extender)
{
	Init("nzbget/nzbget", nullptr, true, commandLineOptions, true, extender);
}

void Options::Init(const char* exeName, const char* configFilename, bool noConfig,
	CmdOptList* commandLineOptions, bool noDiskAccess, Extender* extender)
{
	g_Options = this;
	m_extender = extender;
	m_noDiskAccess = noDiskAccess;
	m_noConfig = noConfig;
	m_configFilename = configFilename;

	SetOption(CONFIGFILE.data(), "");

	CString filename;
	if (m_noDiskAccess)
	{
		filename = exeName;
	}
	else
	{
		filename = FileSystem::GetExeFileName(exeName);
	}
	FileSystem::NormalizePathSeparators(filename);
	SetOption(APPBIN.data(), filename);
	char* end = strrchr(filename, PATH_SEPARATOR);
	if (end) *end = '\0';
	SetOption(APPDIR.data(), filename);
	m_appDir = *filename;

	SetOption(APPVERSION.data(), Util::VersionRevision());

	InitDefaults();

	InitOptFile();
	if (m_fatalError)
	{
		return;
	}

	if (commandLineOptions)
	{
		InitCommandLineOptions(commandLineOptions);
	}

	if (!m_configFilename && !noConfig)
	{
		printf("No configuration-file found\n");
#ifdef WIN32
		printf("Please put configuration-file \"nzbget.conf\" into the directory with exe-file\n");
#else
		printf("Please use option \"-c\" or put configuration-file in one of the following locations:\n");
		int p = 0;
		while (const char* filename = PossibleConfigLocations[p++])
		{
			printf("%s\n", filename);
		}
#endif
		m_fatalError = true;
		return;
	}

	ConvertOldOptions(&m_optEntries);

	CheckDirs();
	InitOptions();
	CheckOptions();

	InitServers();
	InitCategories();
	InitScheduler();
	InitFeeds();
}

Options::~Options()
{
	g_Options = nullptr;
}

void Options::ConfigError(const char* msg, ...)
{
	char tmp2[1024];

	va_list ap;
	va_start(ap, msg);
	vsnprintf(tmp2, 1024, msg, ap);
	tmp2[1024-1] = '\0';
	va_end(ap);

	printf("%s(%i): %s\n", m_configFilename ? FileSystem::BaseFileName(m_configFilename) : "<noconfig>", m_configLine, tmp2);
	error("%s(%i): %s", m_configFilename ? FileSystem::BaseFileName(m_configFilename) : "<noconfig>", m_configLine, tmp2);

	m_configErrors = true;
}

void Options::ConfigWarn(const char* msg, ...)
{
	char tmp2[1024];

	va_list ap;
	va_start(ap, msg);
	vsnprintf(tmp2, 1024, msg, ap);
	tmp2[1024-1] = '\0';
	va_end(ap);

	printf("%s(%i): %s\n", FileSystem::BaseFileName(m_configFilename), m_configLine, tmp2);
	warn("%s(%i): %s", FileSystem::BaseFileName(m_configFilename), m_configLine, tmp2);
}

void Options::LocateOptionSrcPos(const char *optionName)
{
	OptEntry* optEntry = FindOption(optionName);
	if (optEntry)
	{
		m_configLine = optEntry->GetLineNo();
	}
	else
	{
		m_configLine = 0;
	}
}

void Options::InitDefaults()
{
#ifdef WIN32
	SetOption(MAINDIR.data(), "${AppDir}");
	SetOption(WEBDIR.data(), "${AppDir}\\webui");
	SetOption(CONFIGTEMPLATE.data(), "${AppDir}\\nzbget.conf.template");
#else
	SetOption(MAINDIR.data(), "~/downloads");
	SetOption(WEBDIR.data(), "");
	SetOption(CONFIGTEMPLATE.data(), "");
#endif
	SetOption(TEMPDIR.data(), "${MainDir}/tmp");
	SetOption(DESTDIR.data(), "${MainDir}/dst");
	SetOption(INTERDIR.data(), "");
	SetOption(QUEUEDIR.data(), "${MainDir}/queue");
	SetOption(NZBDIR.data(), "${MainDir}/nzb");
	SetOption(LOCKFILE.data(), "${MainDir}/nzbget.lock");
	SetOption(LOGFILE.data(), "${MainDir}/nzbget.log");
	SetOption(SCRIPTDIR.data(), "${MainDir}/scripts");
	SetOption(REQUIREDDIR.data(), "");
	SetOption(WRITELOG.data(), "append");
	SetOption(ROTATELOG.data(), "3");
	SetOption(APPENDCATEGORYDIR.data(), "yes");
#ifdef DISABLE_CURSES
	SetOption(OUTPUTMODE.data(), "color");
#else
	SetOption(OUTPUTMODE.data(), "curses");
#endif
	SetOption(DUPECHECK.data(), "yes");
	SetOption(DOWNLOADRATE.data(), "0");
	SetOption(CONTROLIP.data(), "0.0.0.0");
	SetOption(CONTROLUSERNAME.data(), "nzbget");
	SetOption(CONTROLPASSWORD.data(), "tegbzn6789");
	SetOption(RESTRICTEDUSERNAME.data(), "");
	SetOption(RESTRICTEDPASSWORD.data(), "");
	SetOption(ADDUSERNAME.data(), "");
	SetOption(ADDPASSWORD.data(), "");
	SetOption(CONTROLPORT.data(), "6789");
	SetOption(FORMAUTH.data(), "no");
	SetOption(SECURECONTROL.data(), "no");
	SetOption(SECUREPORT.data(), "6791");
	SetOption(SECURECERT.data(), "");
	SetOption(SECUREKEY.data(), "");
	SetOption(CERTSTORE.data(), "");
	SetOption(CERTCHECK.data(), "no");
	SetOption(AUTHORIZEDIP.data(), "");
	SetOption(ARTICLETIMEOUT.data(), "60");
	SetOption(ARTICLEREADCHUNKSIZE.data(), "4");
	SetOption(URLTIMEOUT.data(), "60");
	SetOption(REMOTETIMEOUT.data(), "90");
	SetOption(FLUSHQUEUE.data(), "yes");
	SetOption(NZBLOG.data(), "yes");
	SetOption(RAWARTICLE.data(), "no");
	SetOption(SKIPWRITE.data(), "no");
	SetOption(ARTICLERETRIES.data(), "3");
	SetOption(ARTICLEINTERVAL.data(), "10");
	SetOption(URLRETRIES.data(), "3");
	SetOption(URLINTERVAL.data(), "10");
	SetOption(CONTINUEPARTIAL.data(), "no");
	SetOption(URLCONNECTIONS.data(), "4");
	SetOption(LOGBUFFER.data(), "1000");
	SetOption(INFOTARGET.data(), "both");
	SetOption(WARNINGTARGET.data(), "both");
	SetOption(ERRORTARGET.data(), "both");
	SetOption(DEBUGTARGET.data(), "none");
	SetOption(DETAILTARGET.data(), "both");
	SetOption(PARCHECK.data(), "auto");
	SetOption(PARREPAIR.data(), "yes");
	SetOption(PARSCAN.data(), "extended");
	SetOption(PARQUICK.data(), "yes");
	SetOption(POSTSTRATEGY.data(), "sequential");
	SetOption(FILENAMING.data(), "article");
	SetOption(RENAMEAFTERUNPACK.data(), "yes");
	SetOption(RENAMEIGNOREEXT.data(), ".zip, .7z, .rar, .par2");
	SetOption(PARRENAME.data(), "yes");
	SetOption(PARBUFFER.data(), "16");
	SetOption(PARTHREADS.data(), "0");
	SetOption(RARRENAME.data(), "yes");
	SetOption(HEALTHCHECK.data(), "none");
	SetOption(DIRECTRENAME.data(), "no");
	SetOption(HARDLINKING.data(), "no");
	SetOption(HARDLINKINGIGNOREEXT.data(), ".zip, .7z, .rar, *.7z.###, *.r##");
	SetOption(SCRIPTORDER.data(), "");
	SetOption(EXTENSIONS.data(), "");
	SetOption(DAEMONUSERNAME.data(), "root");
	SetOption(UMASK.data(), "1000");
	SetOption(UPDATEINTERVAL.data(), "200");
	SetOption(CURSESNZBNAME.data(), "yes");
	SetOption(CURSESTIME.data(), "no");
	SetOption(CURSESGROUP.data(), "no");
	SetOption(CRCCHECK.data(), "yes");
	SetOption(DIRECTWRITE.data(), "yes");
	SetOption(WRITEBUFFER.data(), "0");
	SetOption(NZBDIRINTERVAL.data(), "5");
	SetOption(NZBDIRFILEAGE.data(), "60");
	SetOption(DISKSPACE.data(), "250");
	SetOption(CRASHTRACE.data(), "no");
	SetOption(CRASHDUMP.data(), "no");
	SetOption(PARPAUSEQUEUE.data(), "no");
	SetOption(SCRIPTPAUSEQUEUE.data(), "no");
	SetOption(NZBCLEANUPDISK.data(), "no");
	SetOption(PARTIMELIMIT.data(), "0");
	SetOption(KEEPHISTORY.data(), "7");
	SetOption(UNPACK.data(), "no");
	SetOption(DIRECTUNPACK.data(), "no");
	SetOption(USETEMPUNPACKDIR.data(), "yes");
	SetOption(UNPACKCLEANUPDISK.data(), "no");
#ifdef WIN32
	SetOption(UNRARCMD.data(), "unrar.exe");
	SetOption(SEVENZIPCMD.data(), "7z.exe");
#else
	SetOption(UNRARCMD.data(), "unrar");
	SetOption(SEVENZIPCMD.data(), "7z");
#endif
	SetOption(UNPACKPASSFILE.data(), "");
	SetOption(UNPACKPAUSEQUEUE.data(), "no");
	SetOption(EXTCLEANUPDISK.data(), "");
	SetOption(PARIGNOREEXT.data(), "");
	SetOption(UNPACKIGNOREEXT.data(), "");
	SetOption(FEEDHISTORY.data(), "7");
	SetOption(URLFORCE.data(), "yes");
	SetOption(TIMECORRECTION.data(), "0");
	SetOption(PROPAGATIONDELAY.data(), "0");
	SetOption(ARTICLECACHE.data(), "0");
	SetOption(EVENTINTERVAL.data(), "0");
	SetOption(SHELLOVERRIDE.data(), "");
	SetOption(MONTHLYQUOTA.data(), "0");
	SetOption(QUOTASTARTDAY.data(), "1");
	SetOption(DAILYQUOTA.data(), "0");
	SetOption(REORDERFILES.data(), "no");
	SetOption(UPDATECHECK.data(), "none");
}

void Options::InitOptFile()
{
	if (!m_configFilename && !m_noConfig)
	{
		// search for config file in default locations
#ifdef WIN32
		BString<1024> filename("%s\\nzbget.conf", *m_appDir);

		if (!FileSystem::FileExists(filename))
		{
			char appDataPath[MAX_PATH];
			SHGetFolderPath(nullptr, CSIDL_COMMON_APPDATA, nullptr, 0, appDataPath);
			filename.Format("%s\\NZBGet\\nzbget.conf", appDataPath);

			if (m_extender && !FileSystem::FileExists(filename))
			{
				m_extender->SetupFirstStart();
			}
		}

		if (FileSystem::FileExists(filename))
		{
			m_configFilename = filename;
		}
#else
		// look in the exe-directory first
		BString<1024> filename("%s/nzbget.conf", *m_appDir);

		if (FileSystem::FileExists(filename))
		{
			m_configFilename = filename;
		}
		else
		{
			int p = 0;
			while (const char* altfilename = PossibleConfigLocations[p++])
			{
				// substitute HOME-variable
				filename = FileSystem::ExpandHomePath(altfilename);

				if (FileSystem::FileExists(filename))
				{
					m_configFilename = *filename;
					break;
				}
			}
		}
#endif
	}

	if (m_configFilename)
	{
		// normalize path in filename
		CString filename = FileSystem::ExpandFileName(m_configFilename);

#ifndef WIN32
		// substitute HOME-variable
		filename = FileSystem::ExpandHomePath(filename);
#endif

		m_configFilename = *filename;

		SetOption(CONFIGFILE.data(), m_configFilename);
		LoadConfigFile();
	}
}

void Options::CheckDir(CString& dir, const char* optionName,
	const char* parentDir, bool allowEmpty, bool create)
{
	const char* tempdir = GetOption(optionName);

	if (m_noDiskAccess)
	{
		dir = tempdir;
		return;
	}

	if (Util::EmptyStr(tempdir))
	{
		if (!allowEmpty)
		{
			ConfigError("Invalid value for option \"%s\": <empty>", optionName);
		}
		dir = "";
		return;
	}

	dir = tempdir;
	FileSystem::NormalizePathSeparators((char*)dir);
	if (!Util::EmptyStr(dir) && dir[dir.Length() - 1] == PATH_SEPARATOR)
	{
		// remove trailing slash
		dir[dir.Length() - 1] = '\0';
	}

	if (!(dir[0] == PATH_SEPARATOR || dir[0] == ALT_PATH_SEPARATOR ||
		(dir[0] && dir[1] == ':')) &&
		!Util::EmptyStr(parentDir))
	{
		// convert relative path to absolute path
		int plen = strlen(parentDir);

		BString<1024> usedir2;
		if (parentDir[plen-1] == PATH_SEPARATOR || parentDir[plen-1] == ALT_PATH_SEPARATOR)
		{
			usedir2.Format("%s%s", parentDir, *dir);
		}
		else
		{
			usedir2.Format("%s%c%s", parentDir, PATH_SEPARATOR, *dir);
		}

		FileSystem::NormalizePathSeparators((char*)usedir2);
		dir = usedir2;
		SetOption(optionName, usedir2);
	}

	// Ensure the dir is created
	CString errmsg;
	if (create && !FileSystem::ForceDirectories(dir, errmsg))
	{
		ConfigError("Invalid value for option \"%s\" (%s): %s", optionName, *dir, *errmsg);
	}
}

void Options::CheckDirs()
{
	const char* mainDir = GetOption(MAINDIR.data());

	CheckDir(m_destDir, DESTDIR.data(), mainDir, false, false);
	CheckDir(m_interDir, INTERDIR.data(), mainDir, true, false);
	CheckDir(m_tempDir, TEMPDIR.data(), mainDir, false, true);
	CheckDir(m_queueDir, QUEUEDIR.data(), mainDir, false, true);
	CheckDir(m_webDir, WEBDIR.data(), nullptr, true, false);
	CheckDir(m_scriptDir, SCRIPTDIR.data(), mainDir, true, false);
	CheckDir(m_nzbDir, NZBDIR.data(), mainDir, false, true);
}

void Options::InitOptions()
{
	m_requiredDir = GetOption(REQUIREDDIR.data());

	m_configTemplate		= GetOption(CONFIGTEMPLATE.data());
	m_scriptOrder			= GetOption(SCRIPTORDER.data());
	m_extensions			= GetOption(EXTENSIONS.data());
	m_controlIp				= GetOption(CONTROLIP.data());
	m_controlUsername		= GetOption(CONTROLUSERNAME.data());
	m_controlPassword		= GetOption(CONTROLPASSWORD.data());
	m_restrictedUsername	= GetOption(RESTRICTEDUSERNAME.data());
	m_restrictedPassword	= GetOption(RESTRICTEDPASSWORD.data());
	m_addUsername			= GetOption(ADDUSERNAME.data());
	m_addPassword			= GetOption(ADDPASSWORD.data());
	m_secureCert			= GetOption(SECURECERT.data());
	m_secureKey				= GetOption(SECUREKEY.data());
	m_certStore				= GetOption(CERTSTORE.data());
	m_authorizedIp			= GetOption(AUTHORIZEDIP.data());
	m_lockFile				= GetOption(LOCKFILE.data());
	m_daemonUsername		= GetOption(DAEMONUSERNAME.data());
	m_logFile				= GetOption(LOGFILE.data());
	m_unrarCmd				= GetOption(UNRARCMD.data());
	m_sevenZipCmd			= GetOption(SEVENZIPCMD.data());
	m_unpackPassFile		= GetOption(UNPACKPASSFILE.data());
	m_extCleanupDisk		= GetOption(EXTCLEANUPDISK.data());
	m_parIgnoreExt			= GetOption(PARIGNOREEXT.data());
	m_unpackIgnoreExt		= GetOption(UNPACKIGNOREEXT.data());

	const char* shellOverride = GetOption(SHELLOVERRIDE.data());
	m_shellOverride	= shellOverride ? shellOverride : "";

	m_renameIgnoreExt 	    = GetOption(RENAMEIGNOREEXT.data());

	m_downloadRate			= ParseIntValue(DOWNLOADRATE.data(), 10) * 1024;
	m_articleTimeout		= ParseIntValue(ARTICLETIMEOUT.data(), 10);
	m_articleReadChunkSize  = ParseIntValue(ARTICLEREADCHUNKSIZE.data(), 10) * 1024;
	m_urlTimeout			= ParseIntValue(URLTIMEOUT.data(), 10);
	m_remoteTimeout			= ParseIntValue(REMOTETIMEOUT.data(), 10);
	m_articleRetries		= ParseIntValue(ARTICLERETRIES.data(), 10);
	m_articleInterval		= ParseIntValue(ARTICLEINTERVAL.data(), 10);
	m_urlRetries			= ParseIntValue(URLRETRIES.data(), 10);
	m_urlInterval			= ParseIntValue(URLINTERVAL.data(), 10);
	m_controlPort			= ParseIntValue(CONTROLPORT.data(), 10);
	m_securePort			= ParseIntValue(SECUREPORT.data(), 10);
	m_urlConnections		= ParseIntValue(URLCONNECTIONS.data(), 10);
	m_logBuffer				= ParseIntValue(LOGBUFFER.data(), 10);
	m_rotateLog				= ParseIntValue(ROTATELOG.data(), 10);
	m_umask					= ParseIntValue(UMASK.data(), 8);
	m_updateInterval		= ParseIntValue(UPDATEINTERVAL.data(), 10);
	m_writeBuffer			= ParseIntValue(WRITEBUFFER.data(), 10);
	m_nzbDirInterval		= ParseIntValue(NZBDIRINTERVAL.data(), 10);
	m_nzbDirFileAge			= ParseIntValue(NZBDIRFILEAGE.data(), 10);
	m_diskSpace				= ParseIntValue(DISKSPACE.data(), 10);
	m_parTimeLimit			= ParseIntValue(PARTIMELIMIT.data(), 10);
	m_keepHistory			= ParseIntValue(KEEPHISTORY.data(), 10);
	m_feedHistory			= ParseIntValue(FEEDHISTORY.data(), 10);
	m_timeCorrection		= ParseIntValue(TIMECORRECTION.data(), 10);
	if (-24 <= m_timeCorrection && m_timeCorrection <= 24)
	{
		m_timeCorrection *= 60;
	}
	m_timeCorrection *= 60;
	m_propagationDelay		= ParseIntValue(PROPAGATIONDELAY.data(), 10) * 60;
	m_articleCache			= ParseIntValue(ARTICLECACHE.data(), 10);
	m_eventInterval			= ParseIntValue(EVENTINTERVAL.data(), 10);
	m_parBuffer				= ParseIntValue(PARBUFFER.data(), 10);
	m_parThreads			= ParseIntValue(PARTHREADS.data(), 10);
	m_monthlyQuota			= ParseIntValue(MONTHLYQUOTA.data(), 10);
	m_quotaStartDay			= ParseIntValue(QUOTASTARTDAY.data(), 10);
	m_dailyQuota			= ParseIntValue(DAILYQUOTA.data(), 10);

	m_nzbLog				= (bool)ParseEnumValue(NZBLOG.data(), BoolCount, BoolNames, BoolValues);
	m_appendCategoryDir		= (bool)ParseEnumValue(APPENDCATEGORYDIR.data(), BoolCount, BoolNames, BoolValues);
	m_continuePartial		= (bool)ParseEnumValue(CONTINUEPARTIAL.data(), BoolCount, BoolNames, BoolValues);
	m_flushQueue			= (bool)ParseEnumValue(FLUSHQUEUE.data(), BoolCount, BoolNames, BoolValues);
	m_dupeCheck				= (bool)ParseEnumValue(DUPECHECK.data(), BoolCount, BoolNames, BoolValues);
	m_parRepair				= (bool)ParseEnumValue(PARREPAIR.data(), BoolCount, BoolNames, BoolValues);
	m_parQuick				= (bool)ParseEnumValue(PARQUICK.data(), BoolCount, BoolNames, BoolValues);
	m_parRename				= (bool)ParseEnumValue(PARRENAME.data(), BoolCount, BoolNames, BoolValues);
	m_rarRename				= (bool)ParseEnumValue(RARRENAME.data(), BoolCount, BoolNames, BoolValues);
	m_directRename			= (bool)ParseEnumValue(DIRECTRENAME.data(), BoolCount, BoolNames, BoolValues);
	m_hardLinking			= (bool)ParseEnumValue(HARDLINKING.data(), BoolCount, BoolNames, BoolValues);
	m_hardLinkingIgnoreExt = GetOption(HARDLINKINGIGNOREEXT.data());
	m_cursesNzbName			= (bool)ParseEnumValue(CURSESNZBNAME.data(), BoolCount, BoolNames, BoolValues);
	m_cursesTime			= (bool)ParseEnumValue(CURSESTIME.data(), BoolCount, BoolNames, BoolValues);
	m_cursesGroup			= (bool)ParseEnumValue(CURSESGROUP.data(), BoolCount, BoolNames, BoolValues);
	m_crcCheck				= (bool)ParseEnumValue(CRCCHECK.data(), BoolCount, BoolNames, BoolValues);
	m_directWrite			= (bool)ParseEnumValue(DIRECTWRITE.data(), BoolCount, BoolNames, BoolValues);
	m_rawArticle			= (bool)ParseEnumValue(RAWARTICLE.data(), BoolCount, BoolNames, BoolValues);
	m_skipWrite				= (bool)ParseEnumValue(SKIPWRITE.data(), BoolCount, BoolNames, BoolValues);
	m_crashTrace			= (bool)ParseEnumValue(CRASHTRACE.data(), BoolCount, BoolNames, BoolValues);
	m_crashDump				= (bool)ParseEnumValue(CRASHDUMP.data(), BoolCount, BoolNames, BoolValues);
	m_parPauseQueue			= (bool)ParseEnumValue(PARPAUSEQUEUE.data(), BoolCount, BoolNames, BoolValues);
	m_scriptPauseQueue		= (bool)ParseEnumValue(SCRIPTPAUSEQUEUE.data(), BoolCount, BoolNames, BoolValues);
	m_nzbCleanupDisk		= (bool)ParseEnumValue(NZBCLEANUPDISK.data(), BoolCount, BoolNames, BoolValues);
	m_formAuth				= (bool)ParseEnumValue(FORMAUTH.data(), BoolCount, BoolNames, BoolValues);
	m_secureControl			= (bool)ParseEnumValue(SECURECONTROL.data(), BoolCount, BoolNames, BoolValues);
	m_unpack				= (bool)ParseEnumValue(UNPACK.data(), BoolCount, BoolNames, BoolValues);
	m_directUnpack			= (bool)ParseEnumValue(DIRECTUNPACK.data(), BoolCount, BoolNames, BoolValues);
	m_useTempUnpackDir		= (bool)ParseEnumValue(USETEMPUNPACKDIR.data(), BoolCount, BoolNames, BoolValues);
	m_unpackCleanupDisk		= (bool)ParseEnumValue(UNPACKCLEANUPDISK.data(), BoolCount, BoolNames, BoolValues);
	m_unpackPauseQueue		= (bool)ParseEnumValue(UNPACKPAUSEQUEUE.data(), BoolCount, BoolNames, BoolValues);
	m_urlForce				= (bool)ParseEnumValue(URLFORCE.data(), BoolCount, BoolNames, BoolValues);
	m_certCheck				= (bool)ParseEnumValue(CERTCHECK.data(), BoolCount, BoolNames, BoolValues);
	m_reorderFiles			= (bool)ParseEnumValue(REORDERFILES.data(), BoolCount, BoolNames, BoolValues);
	m_renameAfterUnpack     = (bool)ParseEnumValue(RENAMEAFTERUNPACK.data(), BoolCount, BoolNames, BoolValues);

	const char* OutputModeNames[] = { "loggable", "logable", "log", "colored", "color", "ncurses", "curses" };
	const int OutputModeValues[] = { omLoggable, omLoggable, omLoggable, omColored, omColored, omNCurses, omNCurses };
	const int OutputModeCount = 7;
	m_outputMode = (EOutputMode)ParseEnumValue(OUTPUTMODE.data(), OutputModeCount, OutputModeNames, OutputModeValues);

	const char* ParCheckNames[] = { "auto", "always", "force", "manual" };
	const int ParCheckValues[] = { pcAuto, pcAlways, pcForce, pcManual };
	const int ParCheckCount = 6;
	m_parCheck = (EParCheck)ParseEnumValue(PARCHECK.data(), ParCheckCount, ParCheckNames, ParCheckValues);

	const char* ParScanNames[] = { "limited", "extended", "full", "dupe" };
	const int ParScanValues[] = { psLimited, psExtended, psFull, psDupe };
	const int ParScanCount = 4;
	m_parScan = (EParScan)ParseEnumValue(PARSCAN.data(), ParScanCount, ParScanNames, ParScanValues);

	const char* PostStrategyNames[] = { "sequential", "balanced", "aggressive", "rocket" };
	const int PostStrategyValues[] = { ppSequential, ppBalanced, ppAggressive, ppRocket };
	const int PostStrategyCount = 4;
	m_postStrategy = (EPostStrategy)ParseEnumValue(POSTSTRATEGY.data(), PostStrategyCount, PostStrategyNames, PostStrategyValues);

	const char* FileNamingNames[] = { "auto", "article", "nzb" };
	const int FileNamingValues[] = { nfAuto, nfArticle, nfNzb };
	const int FileNamingCount = 4;
	m_fileNaming = (EFileNaming)ParseEnumValue(FILENAMING.data(), FileNamingCount, FileNamingNames, FileNamingValues);

	const char* HealthCheckNames[] = { "pause", "delete", "park", "none" };
	const int HealthCheckValues[] = { hcPause, hcDelete, hcPark, hcNone };
	const int HealthCheckCount = 4;
	m_healthCheck = (EHealthCheck)ParseEnumValue(HEALTHCHECK.data(), HealthCheckCount, HealthCheckNames, HealthCheckValues);

	const char* TargetNames[] = { "screen", "log", "both", "none" };
	const int TargetValues[] = { mtScreen, mtLog, mtBoth, mtNone };
	const int TargetCount = 4;
	m_infoTarget = (EMessageTarget)ParseEnumValue(INFOTARGET.data(), TargetCount, TargetNames, TargetValues);
	m_warningTarget = (EMessageTarget)ParseEnumValue(WARNINGTARGET.data(), TargetCount, TargetNames, TargetValues);
	m_errorTarget = (EMessageTarget)ParseEnumValue(ERRORTARGET.data(), TargetCount, TargetNames, TargetValues);
	m_debugTarget = (EMessageTarget)ParseEnumValue(DEBUGTARGET.data(), TargetCount, TargetNames, TargetValues);
	m_detailTarget = (EMessageTarget)ParseEnumValue(DETAILTARGET.data(), TargetCount, TargetNames, TargetValues);

	const char* WriteLogNames[] = { "none", "append", "reset", "rotate" };
	const int WriteLogValues[] = { wlNone, wlAppend, wlReset, wlRotate };
	const int WriteLogCount = 4;
	m_writeLog = (EWriteLog)ParseEnumValue(WRITELOG.data(), WriteLogCount, WriteLogNames, WriteLogValues);
}

int Options::ParseEnumValue(const char* OptName, int argc, const char * argn[], const int argv[])
{
	OptEntry* optEntry = FindOption(OptName);
	if (!optEntry)
	{
		ConfigError("Undefined value for option \"%s\"", OptName);
		return argv[0];
	}

	int defNum = 0;

	for (int i = 0; i < argc; i++)
	{
		if (!strcasecmp(optEntry->GetValue(), argn[i]))
		{
			// normalizing option value in option list, for example "NO" -> "no"
			for (int j = 0; j < argc; j++)
			{
				if (argv[j] == argv[i])
				{
					if (strcmp(argn[j], optEntry->GetValue()))
					{
						optEntry->SetValue(argn[j]);
					}
					break;
				}
			}

			return argv[i];
		}

		if (!strcasecmp(optEntry->GetDefValue(), argn[i]))
		{
			defNum = i;
		}
	}

	m_configLine = optEntry->GetLineNo();
	ConfigError("Invalid value for option \"%s\": \"%s\"", OptName, optEntry->GetValue());
	optEntry->SetValue(argn[defNum]);
	return argv[defNum];
}

int Options::ParseIntValue(const char* OptName, int base)
{
	OptEntry* optEntry = FindOption(OptName);
	if (!optEntry)
	{
		ConfigError("Undefined value for option \"%s\"", OptName);
		return 0;
	}

	char *endptr;
	int val = strtol(optEntry->GetValue(), &endptr, base);

	if (endptr && *endptr != '\0')
	{
		m_configLine = optEntry->GetLineNo();
		ConfigError("Invalid value for option \"%s\": \"%s\"", OptName, optEntry->GetValue());
		optEntry->SetValue(optEntry->GetDefValue());
		val = strtol(optEntry->GetDefValue(), nullptr, base);
	}

	return val;
}

void Options::SetOption(const char* optname, const char* value)
{
	OptEntry* optEntry = FindOption(optname);
	if (!optEntry)
	{
		m_optEntries.emplace_back(optname, nullptr);
		optEntry = &m_optEntries.back();
	}

	CString curvalue;

#ifndef WIN32
	if (value && (value[0] == '~') && (value[1] == '/'))
	{
		if (m_noDiskAccess)
		{
			curvalue = value;
		}
		else
		{
			curvalue = FileSystem::ExpandHomePath(value);
		}
	}
	else
#endif
	{
		curvalue = value;
	}

	optEntry->SetLineNo(m_configLine);

	// expand variables
	while (const char* dollar = strstr(curvalue, "${"))
	{
		const char* end = strchr(dollar, '}');
		if (end)
		{
			int varlen = (int)(end - dollar - 2);
			BString<100> variable;
			variable.Set(dollar + 2, varlen);
			const char* varvalue = GetOption(variable);
			if (varvalue)
			{
				curvalue.Replace((int)(dollar - curvalue), 2 + varlen + 1, varvalue);
			}
			else
			{
				break;
			}
		}
		else
		{
			break;
		}
	}

	optEntry->SetValue(curvalue);
}

Options::OptEntry* Options::FindOption(const char* optname)
{
	OptEntry* optEntry = m_optEntries.FindOption(optname);

	// normalize option name in option list; for example "server1.joingroup" -> "Server1.JoinGroup"
	if (optEntry && strcmp(optEntry->GetName(), optname))
	{
		optEntry->SetName(optname);
	}

	return optEntry;
}

const char* Options::GetOption(const char* optname)
{
	OptEntry* optEntry = FindOption(optname);
	if (optEntry)
	{
		if (optEntry->GetLineNo() > 0)
		{
			m_configLine = optEntry->GetLineNo();
		}
		return optEntry->GetValue();
	}
	return nullptr;
}

void Options::InitServers()
{
	int n = 1;
	while (true)
	{
		const char* nactive = GetOption(BString<100>("Server%i.Active", n));
		bool active = true;
		if (nactive)
		{
			active = (bool)ParseEnumValue(BString<100>("Server%i.Active", n), BoolCount, BoolNames, BoolValues);
		}

		const char* nname = GetOption(BString<100>("Server%i.Name", n));
		const char* nlevel = GetOption(BString<100>("Server%i.Level", n));
		const char* ngroup = GetOption(BString<100>("Server%i.Group", n));
		const char* nhost = GetOption(BString<100>("Server%i.Host", n));
		const char* nport = GetOption(BString<100>("Server%i.Port", n));
		const char* nusername = GetOption(BString<100>("Server%i.Username", n));
		const char* npassword = GetOption(BString<100>("Server%i.Password", n));

		const char* njoingroup = GetOption(BString<100>("Server%i.JoinGroup", n));
		bool joinGroup = false;
		if (njoingroup)
		{
			joinGroup = (bool)ParseEnumValue(BString<100>("Server%i.JoinGroup", n), BoolCount, BoolNames, BoolValues);
		}

		const char* noptional = GetOption(BString<100>("Server%i.Optional", n));
		bool optional = false;
		if (noptional)
		{
			optional = (bool)ParseEnumValue(BString<100>("Server%i.Optional", n), BoolCount, BoolNames, BoolValues);
		}

		const char* ntls = GetOption(BString<100>("Server%i.Encryption", n));
		bool tls = false;
		if (ntls)
		{
			tls = (bool)ParseEnumValue(BString<100>("Server%i.Encryption", n), BoolCount, BoolNames, BoolValues);
#ifdef DISABLE_TLS
			if (tls)
			{
				ConfigError("Invalid value for option \"%s\": program was compiled without TLS/SSL-support",
					*BString<100>("Server%i.Encryption", n));
				tls = false;
			}
#endif
			m_tls |= tls;
		}

		const char* ncertveriflevel = GetOption(BString<100>("Server%i.CertVerification", n));
		int certveriflevel = ECertVerifLevel::cvStrict;
		if (ncertveriflevel)
		{
			const char* CertVerifNames[] = { "none", "minimal", "strict" };
			const int CertVerifValues[] = { ECertVerifLevel::cvNone, ECertVerifLevel::cvMinimal, ECertVerifLevel::cvStrict };
			const int CertVerifCount = ECertVerifLevel::Count;
			certveriflevel = ParseEnumValue(BString<100>("Server%i.CertVerification", n), CertVerifCount, CertVerifNames, CertVerifValues);
		}

		const char* nipversion = GetOption(BString<100>("Server%i.IpVersion", n));
		int ipversion = 0;
		if (nipversion)
		{
			const char* IpVersionNames[] = {"auto", "ipv4", "ipv6"};
			const int IpVersionValues[] = {0, 4, 6};
			const int IpVersionCount = 3;
			ipversion = ParseEnumValue(BString<100>("Server%i.IpVersion", n), IpVersionCount, IpVersionNames, IpVersionValues);
		}

		const char* ncipher = GetOption(BString<100>("Server%i.Cipher", n));
		const char* nconnections = GetOption(BString<100>("Server%i.Connections", n));
		const char* nretention = GetOption(BString<100>("Server%i.Retention", n));

		bool definition = nactive || nname || nlevel || ngroup || nhost || nport || noptional ||
			nusername || npassword || nconnections || njoingroup || ntls || ncipher || nretention;
		bool completed = nhost && nport && nconnections;

		if (!definition)
		{
			break;
		}

		if (completed)
		{
			if (m_extender)
			{
				m_extender->AddNewsServer(n, active, nname,
					nhost,
					nport ? atoi(nport) : 119,
					ipversion,
					nusername, npassword,
					joinGroup, tls, ncipher,
					nconnections ? atoi(nconnections) : 1,
					nretention ? atoi(nretention) : 0,
					nlevel ? atoi(nlevel) : 0,
					ngroup ? atoi(ngroup) : 0,
					optional, certveriflevel);
			}
		}
		else
		{
			ConfigError("Server definition not complete for \"Server%i\"", n);
		}

		n++;
	}
}

void Options::InitCategories()
{
	int n = 1;
	while (true)
	{
		const char* nname = GetOption(BString<100>("Category%i.Name", n));
		const char* ndestdir = GetOption(BString<100>("Category%i.DestDir", n));

		const char* nunpack = GetOption(BString<100>("Category%i.Unpack", n));
		bool unpack = true;
		if (nunpack)
		{
			unpack = (bool)ParseEnumValue(BString<100>("Category%i.Unpack", n), BoolCount, BoolNames, BoolValues);
		}

		const char* nextensions = GetOption(BString<100>("Category%i.Extensions", n));
		const char* naliases = GetOption(BString<100>("Category%i.Aliases", n));

		bool definition = nname || ndestdir || nunpack || nextensions || naliases;
		bool completed = nname && strlen(nname) > 0;

		if (!definition)
		{
			break;
		}

		if (completed)
		{
			CString destDir;
			if (ndestdir && ndestdir[0] != '\0')
			{
				CheckDir(destDir, BString<100>("Category%i.DestDir", n), m_destDir, false, false);
			}

			m_categories.emplace_back(nname, destDir, unpack, nextensions);
			Category& category = m_categories.back();

			// split Aliases into tokens and create items for each token
			if (naliases)
			{
				Tokenizer tok(naliases, ",;");
				while (const char* aliasName = tok.Next())
				{
					category.GetAliases()->push_back(aliasName);
				}
			}
		}
		else
		{
			ConfigError("Category definition not complete for \"Category%i\"", n);
		}

		n++;
	}
}

void Options::InitFeeds()
{
	int n = 1;
	while (true)
	{
		const char* nname = GetOption(BString<100>("Feed%i.Name", n));
		const char* nurl = GetOption(BString<100>("Feed%i.URL", n));
		const char* nfilter = GetOption(BString<100>("Feed%i.Filter", n));
		const char* ncategory = GetOption(BString<100>("Feed%i.Category", n));
		const char* nextensions = GetOption(BString<100>("Feed%i.Extensions", n));

		const char* nbacklog = GetOption(BString<100>("Feed%i.Backlog", n));
		bool backlog = true;
		if (nbacklog)
		{
			backlog = (bool)ParseEnumValue(BString<100>("Feed%i.Backlog", n), BoolCount, BoolNames, BoolValues);
		}

		const char* npausenzb = GetOption(BString<100>("Feed%i.PauseNzb", n));
		bool pauseNzb = false;
		if (npausenzb)
		{
			pauseNzb = (bool)ParseEnumValue(BString<100>("Feed%i.PauseNzb", n), BoolCount, BoolNames, BoolValues);
		}

		const char* ncategorySource = GetOption(BString<100>("Feed%i.CategorySource", n));
		const auto categorySource = ParseCategorySource(ncategorySource);

		const char* ninterval = GetOption(BString<100>("Feed%i.Interval", n));
		const char* npriority = GetOption(BString<100>("Feed%i.Priority", n));

		bool definition = nname || nurl || nfilter || ncategory || nbacklog || npausenzb ||
			ninterval || npriority || nextensions;
		bool completed = nurl;

		if (!definition)
		{
			break;
		}

		if (completed)
		{
			if (m_extender)
			{
				m_extender->AddFeed(
					n,
					nname,
					nurl,
					ninterval ? atoi(ninterval) : 0,
					nfilter,
					backlog,
					pauseNzb,
					ncategory,
					categorySource,
					npriority ? atoi(npriority) : 0,
					nextensions
				);
			}
		}
		else
		{
			ConfigError("Feed definition not complete for \"Feed%i\"", n);
		}

		n++;
	}
}

void Options::InitScheduler()
{
	for (int n = 1; ; n++)
	{
		const char* time = GetOption(BString<100>("Task%i.Time", n));
		const char* weekDays = GetOption(BString<100>("Task%i.WeekDays", n));
		const char* command = GetOption(BString<100>("Task%i.Command", n));
		const char* downloadRate = GetOption(BString<100>("Task%i.DownloadRate", n));
		const char* process = GetOption(BString<100>("Task%i.Process", n));
		const char* param = GetOption(BString<100>("Task%i.Param", n));

		if (Util::EmptyStr(param) && !Util::EmptyStr(process))
		{
			param = process;
		}
		if (Util::EmptyStr(param) && !Util::EmptyStr(downloadRate))
		{
			param = downloadRate;
		}

		bool definition = time || weekDays || command || downloadRate || param;
		bool completed = time && command;

		if (!definition)
		{
			break;
		}

		if (!completed)
		{
			ConfigError("Task definition not complete for \"Task%i\"", n);
			continue;
		}

		const char* CommandNames[] = { "pausedownload", "pause", "unpausedownload", "resumedownload", "unpause", "resume",
			"pausepostprocess", "unpausepostprocess", "resumepostprocess", "pausepost", "unpausepost", "resumepost",
			"downloadrate", "setdownloadrate", "rate", "speed", "script", "process", "pausescan", "unpausescan", "resumescan",
			"activateserver", "activateservers", "deactivateserver", "deactivateservers", "fetchfeed", "fetchfeeds" };
		const int CommandValues[] = { scPauseDownload, scPauseDownload, scUnpauseDownload,
			scUnpauseDownload, scUnpauseDownload, scUnpauseDownload,
			scPausePostProcess, scUnpausePostProcess, scUnpausePostProcess,
			scPausePostProcess, scUnpausePostProcess, scUnpausePostProcess,
			scDownloadRate, scDownloadRate, scDownloadRate, scDownloadRate,
			scScript, scProcess, scPauseScan, scUnpauseScan, scUnpauseScan,
			scActivateServer, scActivateServer, scDeactivateServer,
			scDeactivateServer, scFetchFeed, scFetchFeed };
		const int CommandCount = 27;
		ESchedulerCommand taskCommand = (ESchedulerCommand)ParseEnumValue(
			BString<100>("Task%i.Command", n), CommandCount, CommandNames, CommandValues);

		if (param && strlen(param) > 0 && taskCommand == scProcess &&
			Util::SplitCommandLine(param).empty())
		{
			ConfigError("Invalid value for option \"Task%i.Param\"", n);
			continue;
		}

		if (taskCommand == scDownloadRate)
		{
			if (param)
			{
				char* err;
				int downloadRateVal = strtol(param, &err, 10);
				if (!err || *err != '\0' || downloadRateVal < 0)
				{
					ConfigError("Invalid value for option \"Task%i.Param\": \"%s\"", n, downloadRate);
					continue;
				}
			}
			else
			{
				ConfigError("Task definition not complete for \"Task%i\". Option \"Task%i.Param\" is missing", n, n);
				continue;
			}
		}

		if ((taskCommand == scScript ||
			 taskCommand == scProcess ||
			 taskCommand == scActivateServer ||
			 taskCommand == scDeactivateServer ||
			 taskCommand == scFetchFeed) &&
			Util::EmptyStr(param))
		{
			ConfigError("Task definition not complete for \"Task%i\". Option \"Task%i.Param\" is missing", n, n);
			continue;
		}

		CreateSchedulerTask(n, time, weekDays, taskCommand, param);
	}
}

void Options::CreateSchedulerTask(int id, const char* time, const char* weekDays,
	ESchedulerCommand command, const char* param)
{
	if (!id)
	{
		m_configLine = 0;
	}

	int weekDaysVal = 0;
	if (weekDays && !ParseWeekDays(weekDays, &weekDaysVal))
	{
		ConfigError("Invalid value for option \"Task%i.WeekDays\": \"%s\"", id, weekDays);
		return;
	}

	int hours, minutes;
	Tokenizer tok(time, ";,");
	while (const char* oneTime = tok.Next())
	{
		if (!ParseTime(oneTime, &hours, &minutes))
		{
			ConfigError("Invalid value for option \"Task%i.Time\": \"%s\"", id, oneTime);
			return;
		}

		if (m_extender)
		{
			if (hours == -2)
			{
				for (int everyHour = 0; everyHour < 24; everyHour++)
				{
					m_extender->AddTask(id, everyHour, minutes, weekDaysVal, command, param);
				}
			}
			else
			{
				m_extender->AddTask(id, hours, minutes, weekDaysVal, command, param);
			}
		}
	}
}

bool Options::ParseTime(const char* time, int* hours, int* minutes)
{
	if (!strcmp(time, "*"))
	{
		*hours = -1;
		*minutes = 0;
		return true;
	}

	int colons = 0;
	const char* p = time;
	while (*p)
	{
		if (!strchr("0123456789: *", *p))
		{
			return false;
		}
		if (*p == ':')
		{
			colons++;
		}
		p++;
	}

	if (colons != 1)
	{
		return false;
	}

	const char* colon = strchr(time, ':');
	if (!colon)
	{
		return false;
	}

	if (time[0] == '*')
	{
		*hours = -2;
	}
	else
	{
		*hours = atoi(time);
		if (*hours < 0 || *hours > 23)
		{
			return false;
		}
	}

	if (colon[1] == '*')
	{
		return false;
	}
	*minutes = atoi(colon + 1);
	if (*minutes < 0 || *minutes > 59)
	{
		return false;
	}

	return true;
}

bool Options::ParseWeekDays(const char* weekDays, int* weekDaysBits)
{
	*weekDaysBits = 0;
	const char* p = weekDays;
	int firstDay = 0;
	bool range = false;
	while (*p)
	{
		if (strchr("1234567", *p))
		{
			int day = *p - '0';
			if (range)
			{
				if (day <= firstDay || firstDay == 0)
				{
					return false;
				}
				for (int i = firstDay; i <= day; i++)
				{
					*weekDaysBits |= 1 << (i - 1);
				}
				firstDay = 0;
			}
			else
			{
				*weekDaysBits |= 1 << (day - 1);
				firstDay = day;
			}
			range = false;
		}
		else if (*p == ',')
		{
			range = false;
		}
		else if (*p == '-')
		{
			range = true;
		}
		else if (*p == ' ')
		{
			// skip spaces
		}
		else
		{
			return false;
		}
		p++;
	}
	return true;
}

void Options::LoadConfigFile()
{
	SetOption(CONFIGFILE.data(), m_configFilename);

	DiskFile infile;

	if (!infile.Open(m_configFilename, DiskFile::omRead))
	{
		ConfigError("Could not open file %s", *m_configFilename);
		m_fatalError = true;
		return;
	}

	m_configLine = 0;
	int bufLen = (int)FileSystem::FileSize(m_configFilename) + 1;
	CharBuffer buf(bufLen);

	int line = 0;
	while (infile.ReadLine(buf, buf.Size() - 1))
	{
		m_configLine = ++line;

		if (buf[0] != 0 && buf[strlen(buf)-1] == '\n')
		{
			buf[strlen(buf)-1] = 0; // remove traling '\n'
		}
		if (buf[0] != 0 && buf[strlen(buf)-1] == '\r')
		{
			buf[strlen(buf)-1] = 0; // remove traling '\r' (for windows line endings)
		}

		if (buf[0] == 0 || buf[0] == '#' || strspn(buf, " ") == strlen(buf))
		{
			continue;
		}

		SetOptionString(buf);
	}

	infile.Close();

	m_configLine = 0;
}

void Options::InitCommandLineOptions(CmdOptList* commandLineOptions)
{
	for (const char* option : *commandLineOptions)
	{
		SetOptionString(option);
	}
}

bool Options::SetOptionString(const char* option)
{
	CString optname;
	CString optvalue;

	if (!SplitOptionString(option, optname, optvalue))
	{
		ConfigError("Invalid option \"%s\"", option);
		return false;
	}

	bool ok = ValidateOptionName(optname, optvalue);
	if (ok)
	{
		SetOption(optname, optvalue);
	}
	else
	{
		ConfigError("Invalid option \"%s\"", *optname);
	}

	return ok;
}

/*
 * Splits option string into name and value;
 * Converts old names and values if necessary;
 * Returns true if the option string has name and value;
 */
bool Options::SplitOptionString(const char* option, CString& optName, CString& optValue)
{
	const char* eq = strchr(option, '=');
	if (!eq || eq == option)
	{
		return false;
	}

	optName.Set(option, (int)(eq - option));
	optValue.Set(eq + 1);

	ConvertOldOption(optName, optValue);

	return true;
}

bool Options::ValidateOptionName(const char* optname, const char* optvalue)
{
	if (!strcasecmp(optname, CONFIGFILE.data()) || !strcasecmp(optname, APPBIN.data()) ||
		!strcasecmp(optname, APPDIR.data()) || !strcasecmp(optname, APPVERSION.data()))
	{
		// read-only options
		return false;
	}

	const char* v = GetOption(optname);
	if (v)
	{
		// it's predefined option, OK
		return true;
	}

	if (!strncasecmp(optname, "server", 6))
	{
		char* p = (char*)optname + 6;
		while (*p >= '0' && *p <= '9') p++;
		if (p &&
			(!strcasecmp(p, ".active") || !strcasecmp(p, ".name") ||
			!strcasecmp(p, ".level") || !strcasecmp(p, ".host") ||
			!strcasecmp(p, ".port") || !strcasecmp(p, ".username") ||
			!strcasecmp(p, ".password") || !strcasecmp(p, ".joingroup") ||
			!strcasecmp(p, ".encryption") || !strcasecmp(p, ".connections") ||
			!strcasecmp(p, ".cipher") || !strcasecmp(p, ".group") ||
			!strcasecmp(p, ".retention") || !strcasecmp(p, ".optional") ||
			!strcasecmp(p, ".notes") || !strcasecmp(p, ".ipversion") ||
			!strcasecmp(p, ".certverification")))
		{
			return true;
		}
	}

	if (!strncasecmp(optname, "task", 4))
	{
		char* p = (char*)optname + 4;
		while (*p >= '0' && *p <= '9') p++;
		if (p && (!strcasecmp(p, ".time") || !strcasecmp(p, ".weekdays") ||
			!strcasecmp(p, ".command") || !strcasecmp(p, ".param") ||
			!strcasecmp(p, ".downloadrate") || !strcasecmp(p, ".process")))
		{
			return true;
		}
	}

	if (!strncasecmp(optname, "category", 8))
	{
		char* p = (char*)optname + 8;
		while (*p >= '0' && *p <= '9') p++;
		if (p && (!strcasecmp(p, ".name") || !strcasecmp(p, ".destdir") || !strcasecmp(p, ".extensions") ||
			!strcasecmp(p, ".unpack") || !strcasecmp(p, ".aliases")))
		{
			return true;
		}
	}

	if (!strncasecmp(optname, "feed", 4))
	{
		char* p = (char*)optname + 4;
		while (*p >= '0' && *p <= '9') p++;
		if (p && (!strcasecmp(p, ".name") || !strcasecmp(p, ".url") || !strcasecmp(p, ".interval") ||
			 !strcasecmp(p, ".filter") || !strcasecmp(p, ".backlog") || !strcasecmp(p, ".pausenzb") ||
			 !strcasecmp(p, ".category") || !strcasecmp(p, ".categorySource") || !strcasecmp(p, ".priority") || 
			 !strcasecmp(p, ".extensions")))
		{
			return true;
		}
	}

	// scripts options
	if (strchr(optname, ':'))
	{
		return true;
	}

	// print warning messages for obsolete options
	if (!strcasecmp(optname, RETRYONCRCERROR.data()) ||
		!strcasecmp(optname, ALLOWREPROCESS.data()) ||
		!strcasecmp(optname, LOADPARS.data()) ||
		!strcasecmp(optname, THREADLIMIT.data()) ||
		!strcasecmp(optname, POSTLOGKIND.data()) ||
		!strcasecmp(optname, NZBLOGKIND.data()) ||
		!strcasecmp(optname, PROCESSLOGKIND.data()) ||
		!strcasecmp(optname, APPENDNZBDIR.data()) ||
		!strcasecmp(optname, RENAMEBROKEN.data()) ||
		!strcasecmp(optname, MERGENZB.data()) ||
		!strcasecmp(optname, STRICTPARNAME.data()) ||
		!strcasecmp(optname, RELOADURLQUEUE.data()) ||
		!strcasecmp(optname, RELOADPOSTQUEUE.data()) ||
		!strcasecmp(optname, PARCLEANUPQUEUE.data()) ||
		!strcasecmp(optname, DELETECLEANUPDISK.data()) ||
		!strcasecmp(optname, HISTORYCLEANUPDISK.data()) ||
		!strcasecmp(optname, SAVEQUEUE.data()) ||
		!strcasecmp(optname, RELOADQUEUE.data()) ||
		!strcasecmp(optname, TERMINATETIMEOUT.data()) ||
		!strcasecmp(optname, ACCURATERATE.data()) ||
		!strcasecmp(optname, CREATEBROKENLOG.data()) ||
		!strcasecmp(optname, BROKENLOG.data()))
	{
		ConfigWarn("Option \"%s\" is obsolete, ignored", optname);
		return true;
	}

	if (!strcasecmp(optname, POSTPROCESS.data()) ||
		!strcasecmp(optname, NZBPROCESS.data()) ||
		!strcasecmp(optname, NZBADDEDPROCESS.data()))
	{
		if (optvalue && strlen(optvalue) > 0)
		{
			ConfigError("Option \"%s\" is obsolete, ignored, use \"%s\" and \"%s\" instead",
				optname, SCRIPTDIR, EXTENSIONS);
		}
		return true;
	}

	if (!strcasecmp(optname, SCANSCRIPT.data()) ||
		!strcasecmp(optname, QUEUESCRIPT.data()) ||
		!strcasecmp(optname, FEEDSCRIPT.data()))
	{
		// will be automatically converted into "Extensions"
		return true;
	}

	if (!strcasecmp(optname, CREATELOG.data()) || !strcasecmp(optname, RESETLOG.data()))
	{
		ConfigWarn("Option \"%s\" is obsolete, ignored, use \"%s\" instead", optname, WRITELOG.data());
		return true;
	}

	return false;
}

void Options::ConvertOldOption(CString& option, CString& value)
{
	// for compatibility with older versions accept old option names

	if (!strcasecmp(option, "$MAINDIR"))
	{
		option = "MainDir";
	}

	if (!strcasecmp(option, "ServerIP"))
	{
		option = "ControlIP";
	}

	if (!strcasecmp(option, "ServerPort"))
	{
		option = "ControlPort";
	}

	if (!strcasecmp(option, "ServerPassword"))
	{
		option = "ControlPassword";
	}

	if (!strcasecmp(option, "PostPauseQueue"))
	{
		option = "ScriptPauseQueue";
	}

	if (!strcasecmp(option, "ParCheck") && !strcasecmp(value, "yes"))
	{
		value = "always";
	}

	if (!strcasecmp(option, "ParCheck") && !strcasecmp(value, "no"))
	{
		value = "auto";
	}

	if (!strcasecmp(option, "ParScan") && !strcasecmp(value, "auto"))
	{
		value = "extended";
	}

	if (!strcasecmp(option, "DefScript") || !strcasecmp(option, "PostScript"))
	{
		option = "Extensions";
	}

	int nameLen = strlen(option);
	if (!strncasecmp(option, "Category", 8) &&
		((nameLen > 10 && !strcasecmp(option + nameLen - 10, ".DefScript")) ||
		 (nameLen > 11 && !strcasecmp(option + nameLen - 11, ".PostScript"))))
	{
		option.Replace(".DefScript", ".Extensions");
		option.Replace(".PostScript", ".Extensions");
	}
	if (!strncasecmp(option, "Feed", 4) && nameLen > 11 && !strcasecmp(option + nameLen - 11, ".FeedScript"))
	{
		option.Replace(".FeedScript", ".Extensions");
	}

	if (!strcasecmp(option, "WriteBufferSize"))
	{
		option = "WriteBuffer";
		int val = strtol(value, nullptr, 10);
		val = val == -1 ? 1024 : val / 1024;
		value.Format("%i", val);
	}

	if (!strcasecmp(option, "ConnectionTimeout"))
	{
		option = "ArticleTimeout";
	}

	if (!strcasecmp(option, "Retries"))
	{
		option = "ArticleRetries";
	}

	if (!strcasecmp(option, "RetryInterval"))
	{
		option = "ArticleInterval";
	}

	if (!strcasecmp(option, "DumpCore"))
	{
		option = CRASHDUMP.data();
	}

	if (!strcasecmp(option, DECODE.data()))
	{
		option = RAWARTICLE.data();
		value = !strcasecmp(value, "no") ? "yes" : "no";
	}

	if (!strcasecmp(option, "LogBufferSize"))
	{
		option = LOGBUFFER.data();
	}
}

void Options::CheckOptions()
{
#ifdef DISABLE_PARCHECK
	if (m_parCheck != pcManual)
	{
		LocateOptionSrcPos(PARCHECK.data());
		ConfigError("Invalid value for option \"%s\": program was compiled without parcheck-support", PARCHECK.data());
	}
	if (m_parRename)
	{
		LocateOptionSrcPos(PARRENAME.data());
		ConfigError("Invalid value for option \"%s\": program was compiled without parcheck-support", PARRENAME.data());
	}
	if (m_directRename)
	{
		LocateOptionSrcPos(DIRECTRENAME.data());
		ConfigError("Invalid value for option \"%s\": program was compiled without parcheck-support", DIRECTRENAME.data());
	}
#endif

#ifdef DISABLE_CURSES
	if (m_outputMode == omNCurses)
	{
		LocateOptionSrcPos(OUTPUTMODE.data());
		ConfigError("Invalid value for option \"%s\": program was compiled without curses-support", OUTPUTMODE.data());
	}
#endif

#ifdef DISABLE_TLS
	if (m_secureControl)
	{
		LocateOptionSrcPos(SECURECONTROL.data());
		ConfigError("Invalid value for option \"%s\": program was compiled without TLS/SSL-support", SECURECONTROL.data());
	}

	if (m_certCheck)
	{
		LocateOptionSrcPos(CERTCHECK.data());
		ConfigError("Invalid value for option \"%s\": program was compiled without TLS/SSL-support", CERTCHECK.data());
	}
#endif

#ifndef DISABLE_TLS
#ifndef HAVE_X509_CHECK_HOST
	if (m_certCheck)
	{
		LocateOptionSrcPos(CERTCHECK.data());
		ConfigWarn("TLS certificate verification (option \"%s\") is limited because the program "
			"was compiled with older OpenSSL version not supporting hostname validation (at least OpenSSL 1.0.2d is required)", CERTCHECK.data());
	}
#endif
#endif

	if (m_certCheck && m_certStore.Empty())
	{
		LocateOptionSrcPos(CERTCHECK.data());
		ConfigError("Option \"%s\" requires proper configuration of option \"%s\"", CERTCHECK.data(), CERTSTORE.data());
		m_certCheck = false;
	}

	if (m_rawArticle)
	{
		m_directWrite = false;
	}

	if (m_skipWrite)
	{
		m_directRename = false;
	}

	// if option "ConfigTemplate" is not set, use "WebDir" as default location for template
	// (for compatibility with versions 9 and 10).
	if (m_configTemplate.Empty() && !m_noDiskAccess)
	{
		m_configTemplate.Format("%s%s", *m_webDir, "nzbget.conf");
		if (!FileSystem::FileExists(m_configTemplate))
		{
			m_configTemplate = "";
		}
	}

	if (m_articleCache < 0)
	{
		m_articleCache = 0;
	}
	else if (sizeof(void*) == 4 && m_articleCache > 1900)
	{
		ConfigError("Invalid value for option \"ArticleCache\": %i. Changed to 1900", m_articleCache);
		m_articleCache = 1900;
	}
	else if (sizeof(void*) == 4 && m_parBuffer > 1900)
	{
		ConfigError("Invalid value for option \"ParBuffer\": %i. Changed to 1900", m_parBuffer);
		m_parBuffer = 1900;
	}

	if (sizeof(void*) == 4 && m_parBuffer + m_articleCache > 1900)
	{
		ConfigError("Options \"ArticleCache\" and \"ParBuffer\" in total cannot use more than 1900MB of memory in 32-Bit mode. Changed to 1500 and 400");
		m_articleCache = 1500;
		m_parBuffer = 400;
	}

	if (!m_unpackPassFile.Empty() && !FileSystem::FileExists(m_unpackPassFile))
	{
		ConfigError("Invalid value for option \"UnpackPassFile\": %s. File not found", *m_unpackPassFile);
	}
}

void Options::ConvertOldOptions(OptEntries* optEntries)
{
	MergeOldScriptOption(optEntries, SCANSCRIPT.data(), true);
	MergeOldScriptOption(optEntries, QUEUESCRIPT.data(), true);
	MergeOldScriptOption(optEntries, FEEDSCRIPT.data(), false);
}

void Options::MergeOldScriptOption(OptEntries* optEntries, const char* optname, bool mergeCategories)
{
	OptEntry* optEntry = optEntries->FindOption(optname);
	if (!optEntry || Util::EmptyStr(optEntry->GetValue()))
	{
		return;
	}

	OptEntry* extensionsOpt = optEntries->FindOption(EXTENSIONS.data());
	if (!extensionsOpt)
	{
		optEntries->emplace_back(EXTENSIONS.data(), "");
		extensionsOpt = optEntries->FindOption(EXTENSIONS.data());
	}

	const char* scriptList = optEntry->GetValue();

	Tokenizer tok(scriptList, ",;");
	while (const char* scriptName = tok.Next())
	{
		// merge into global "Extensions"
		if (!HasScript(extensionsOpt->m_value, scriptName))
		{
			if (!extensionsOpt->m_value.Empty())
			{
				extensionsOpt->m_value.Append(",");
			}
			extensionsOpt->m_value.Append(scriptName);
		}

		// merge into categories' "Extensions" (if not empty)
		if (mergeCategories)
		{
			for (OptEntry& opt : optEntries)
			{
				const char* catoptname = opt.GetName();
				if (!strncasecmp(catoptname, "category", 8))
				{
					char* p = (char*)catoptname + 8;
					while (*p >= '0' && *p <= '9') p++;
					if (p && (!strcasecmp(p, ".extensions")))
					{
						if (!opt.m_value.Empty() && !HasScript(opt.m_value, scriptName))
						{
							opt.m_value.Append(",");
							opt.m_value.Append(scriptName);
						}
					}
				}
			}
		}
	}
}

bool Options::HasScript(const char* scriptList, const char* scriptName)
{
	Tokenizer tok(scriptList, ",;");
	while (const char* scriptName2 = tok.Next())
	{
		if (!strcasecmp(scriptName2, scriptName))
		{
			return true;
		}
	}
	return false;
}

FeedInfo::CategorySource Options::ParseCategorySource(const char* value)
{
	if (!value)
	{
		return FeedInfo::CategorySource::NZBFile;
	}

	if (!strncasecmp(value, "auto", 4))
	{
		return FeedInfo::CategorySource::Auto;
	}
	else if (!strncasecmp(value, "nzbfile", 8))
	{
		return FeedInfo::CategorySource::NZBFile;
	}
	else if (!strncasecmp(value, "feedfile", 9))
	{
		return FeedInfo::CategorySource::FeedFile;
	}
	else
	{
		return FeedInfo::CategorySource::NZBFile;
	}
}
