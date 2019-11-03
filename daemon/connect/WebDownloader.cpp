/*
 *  This file is part of nzbget. See <http://nzbget.net>.
 *
 *  Copyright (C) 2012-2019 Andrey Prygunkov <hugbug@users.sourceforge.net>
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
#include "WebDownloader.h"
#include "Log.h"
#include "Options.h"
#include "WorkState.h"
#include "Util.h"
#include "FileSystem.h"
#ifdef HAVE_LIBCURL
#include <curl/curl.h>
#endif

WebDownloader::WebDownloader()
{
	debug("Creating WebDownloader");

	SetLastUpdateTimeNow();
}

void WebDownloader::SetUrl(const char* url)
{
	m_url = WebUtil::UrlEncode(url);
}

void WebDownloader::SetStatus(EStatus status)
{
	m_status = status;
	Notify(nullptr);
}

void WebDownloader::SetLastUpdateTimeNow()
{
	m_lastUpdateTime = Util::CurrentTime();
}

void WebDownloader::Run()
{
	debug("Entering WebDownloader-loop");

	SetStatus(adRunning);

	int remainedDownloadRetries = g_Options->GetUrlRetries() > 0 ? g_Options->GetUrlRetries() : 1;
	int remainedConnectRetries = remainedDownloadRetries > 10 ? remainedDownloadRetries : 10;
	if (!m_retry)
	{
		remainedDownloadRetries = 1;
		remainedConnectRetries = 1;
	}

	EStatus Status = adFailed;

	while (!IsStopped() && remainedDownloadRetries > 0 && remainedConnectRetries > 0)
	{
		SetLastUpdateTimeNow();

		Status = DownloadWithRedirects(5);

		if ((((Status == adFailed) && (remainedDownloadRetries > 1)) ||
			((Status == adConnectError) && (remainedConnectRetries > 1)))
			&& !IsStopped() && !(!m_force && g_WorkState->GetPauseDownload()))
		{
			detail("Waiting %i sec to retry", g_Options->GetUrlInterval());
			int msec = 0;
			while (!IsStopped() && (msec < g_Options->GetUrlInterval() * 1000) &&
				!(!m_force && g_WorkState->GetPauseDownload()))
			{
				Util::Sleep(100);
				msec += 100;
			}
		}

		if (IsStopped() || (!m_force && g_WorkState->GetPauseDownload()))
		{
			Status = adRetry;
			break;
		}

		if (Status == adFinished || Status == adFatalError || Status == adNotFound)
		{
			break;
		}

		if (Status != adConnectError)
		{
			remainedDownloadRetries--;
		}
		else
		{
			remainedConnectRetries--;
		}
	}

	if (Status != adFinished && Status != adRetry)
	{
		Status = adFailed;
	}

	if (Status == adFailed)
	{
		if (IsStopped())
		{
			detail("Download %s cancelled", *m_infoName);
		}
		else
		{
			error("Download %s failed", *m_infoName);
		}
	}

	if (Status == adFinished)
	{
		detail("Download %s completed", *m_infoName);
	}

	SetStatus(Status);

	debug("Exiting WebDownloader-loop");
}

WebDownloader::EStatus WebDownloader::Download()
{
	EStatus Status = adRunning;

#ifdef HAVE_LIBCURL
	static char errorBuffer[CURL_ERROR_SIZE];
	CURLcode code;
	CURL* conn;

	conn = curl_easy_init();

	if (conn == NULL) {
		error("Failed to init CURL connection");
		return adFatalError;
	}

	// From here on in we can't just bail out, the
	// curl control structure needs to be freed.

	code = curl_easy_setopt(conn, CURLOPT_ERRORBUFFER, errorBuffer);
	if (code != CURLE_OK) {
		error("Failed to set CURL error buffer [%d]", code);
		Status = adFatalError;
		goto curl_out;
	}

	code = curl_easy_setopt(conn, CURLOPT_URL, (char *)m_url);
	if (code != CURLE_OK) {
		error("Failed to set URL: %s", errorBuffer);
		Status = adFatalError;
		goto curl_out;
	}

	// Handler functions to capture body and header data
	code = curl_easy_setopt(conn, CURLOPT_WRITEFUNCTION, curl_body_callback);
	if (code != CURLE_OK) {
		error("Failed to set body data handler function: %s", errorBuffer);
		Status = adFatalError;
		goto curl_out;
	}

	code = curl_easy_setopt(conn, CURLOPT_WRITEDATA, this);
	if (code != CURLE_OK) {
		error("Failed to set body data handler data: %s", errorBuffer);
		Status = adFatalError;
		goto curl_out;
	}

	code = curl_easy_setopt(conn, CURLOPT_HEADERFUNCTION, curl_header_callback);
	if (code != CURLE_OK) {
		error("Failed to set header data handler function: %s", errorBuffer);
		Status = adFatalError;
		goto curl_out;
	}

	code = curl_easy_setopt(conn, CURLOPT_HEADERDATA, this);
	if (code != CURLE_OK) {
		error("Failed to set header data handler data: %s", errorBuffer);
		Status = adFatalError;
		goto curl_out;
	}

	// We are explicitly not following redirects
	code = curl_easy_setopt(conn, CURLOPT_FOLLOWLOCATION, 0L);
	if (code != CURLE_OK) {
		error("Failed to disable redirect handling: %s", errorBuffer);
		Status = adFatalError;
		goto curl_out;
	}

	// libcurl does not have a transfer timeout in the classic sense. We
	// are using the slow speed detection, and bail out if
	// transfer speed is below 10 bytes/s for the time
	// specified with UrlTimeout in the config
	code = curl_easy_setopt(conn, CURLOPT_LOW_SPEED_TIME, g_Options->GetUrlTimeout());
	if (code != CURLE_OK) {
		error("Failed to set low speed time: %s", errorBuffer);
		Status = adFatalError;
		goto curl_out;
	}

	code = curl_easy_setopt(conn, CURLOPT_LOW_SPEED_LIMIT, 10L);
	if (code != CURLE_OK) {
		error("Failed to set low speed time: %s", errorBuffer);
		Status = adFatalError;
		goto curl_out;
	}

	// SSL host name verification and cert validation
	code = curl_easy_setopt(conn, CURLOPT_SSL_VERIFYHOST, g_Options->GetCertCheck() ? 2 : 0);
	if (code != CURLE_OK) {
		error("Failed to set SSL host name verification: %s", errorBuffer);
		Status = adFatalError;
		goto curl_out;
	}

	code = curl_easy_setopt(conn, CURLOPT_SSL_VERIFYPEER, g_Options->GetCertCheck() ? 1 : 0);
	if (code != CURLE_OK) {
		error("Failed to set SSL peer verification: %s", errorBuffer);
		Status = adFatalError;
		goto curl_out;
	}

	// SSL Certificate location. This can either be a file or a
	// directory.
	if (FileSystem::DirectoryExists(g_Options->GetCertStore())) {
		code = curl_easy_setopt(conn, CURLOPT_CAPATH, g_Options->GetCertStore());
	} else if (FileSystem::FileExists(g_Options->GetCertStore())) {
		code = curl_easy_setopt(conn, CURLOPT_CAINFO, g_Options->GetCertStore());
	} else {
		// Option not set, pretend all is fine.
		code = CURLE_OK;
	}
	if (code != CURLE_OK) {
		error("Failed to set SSL CA location: %s", errorBuffer);
		Status = adFatalError;
		goto curl_out;
	}

	// User agent
	code = curl_easy_setopt(conn, CURLOPT_USERAGENT, (char *)BString<1024>("nzbget/%s %s", Util::VersionRevision(), curl_version()));
	if (code != CURLE_OK) {
		error("Failed to set user agent: %s", errorBuffer);
		Status = adFatalError;
		goto curl_out;
	}

	// Perform the request
	code = curl_easy_perform(conn);
	if (code != CURLE_OK) {
		// Deal with connect error specifically
		if (code == CURLE_COULDNT_CONNECT) {
			error("Could not connect to host");
			Status = adConnectError;
		} else {
			error("Download error: %s", errorBuffer);
			Status = adFatalError;
		}
		goto curl_out;
	}

	// Check the response code
	long response_code;
	char* location;
	code = curl_easy_getinfo(conn, CURLINFO_RESPONSE_CODE, &response_code);
	if (code != CURLE_OK) {
		error("Could not retrieve response code: %s", errorBuffer);
		Status = adFatalError;
		goto curl_out;
	}

	// Check for redirects
	if ((response_code / 100) == 3) {
		// Redirect. The URL we get from curl is
		// always absolute.
		code = curl_easy_getinfo(conn, CURLINFO_REDIRECT_URL, &location);
		if ((code != CURLE_OK) || !location) {
			error("Could not get redirect url: %s", errorBuffer);
			Status = adFatalError;
			goto curl_out;
		}
		SetUrl(location);
		Status = adRedirect;
		goto curl_out;
	}

	// Not found
	if (response_code == 404) {
		// Not found
		Status = adNotFound;
		goto curl_out;
	}

	// Any other errors
	if (response_code >= 400) {
		info("Error code downloading file: %ld", response_code);
		Status = adFailed;
		goto curl_out;
	}

	Status = adFinished;

curl_out:
	m_outFile.Close();
	curl_easy_cleanup(conn);

#else // HAVE_LIBCURL
	URL url(m_url);

	Status = CreateConnection(&url);
	if (Status != adRunning)
	{
		return Status;
	}

	m_connection->SetTimeout(g_Options->GetUrlTimeout());
	m_connection->SetSuppressErrors(false);

	// connection
	bool connected = m_connection->Connect();
	if (!connected || IsStopped())
	{
		FreeConnection();
		return adConnectError;
	}

	// Okay, we got a Connection. Now start downloading.
	detail("Downloading %s", *m_infoName);

	SendHeaders(&url);

	Status = DownloadHeaders();

	if (Status == adRunning)
	{
		Status = DownloadBody();
	}

	if (IsStopped())
	{
		Status = adFailed;
	}

	FreeConnection();
#endif // HAVE_LIBCURL

	if (Status != adFinished)
	{
		// Download failed, delete broken output file
		FileSystem::DeleteFile(m_outputFilename);
	}

	return Status;
}

WebDownloader::EStatus WebDownloader::DownloadWithRedirects(int maxRedirects)
{
	// do sync download, following redirects
	EStatus status = adRedirect;
	while (status == adRedirect && maxRedirects >= 0)
	{
		maxRedirects--;
		status = Download();
	}

	if (status == adRedirect && maxRedirects < 0)
	{
		warn("Too many redirects for %s", *m_infoName);
		status = adFailed;
	}

	return status;
}

WebDownloader::EStatus WebDownloader::CreateConnection(URL *url)
{
	if (!url->IsValid())
	{
		error("URL is not valid: %s", url->GetAddress());
		return adFatalError;
	}

	int port = url->GetPort();
	if (port == 0 && !strcasecmp(url->GetProtocol(), "http"))
	{
		port = 80;
	}
	if (port == 0 && !strcasecmp(url->GetProtocol(), "https"))
	{
		port = 443;
	}

	if (strcasecmp(url->GetProtocol(), "http") && strcasecmp(url->GetProtocol(), "https"))
	{
		error("Unsupported protocol in URL: %s", url->GetAddress());
		return adFatalError;
	}

#ifdef DISABLE_TLS
	if (!strcasecmp(url->GetProtocol(), "https"))
	{
		error("Program was compiled without TLS/SSL-support. Cannot download using https protocol. URL: %s", url->GetAddress());
		return adFatalError;
	}
#endif

	bool tls = !strcasecmp(url->GetProtocol(), "https");

	m_connection = std::make_unique<Connection>(url->GetHost(), port, tls);

	return adRunning;
}

void WebDownloader::SendHeaders(URL *url)
{
	// retrieve file
	m_connection->WriteLine(BString<1024>("GET %s HTTP/1.0\r\n", url->GetResource()));
	m_connection->WriteLine(BString<1024>("User-Agent: nzbget/%s\r\n", Util::VersionRevision()));

	if ((!strcasecmp(url->GetProtocol(), "http") && (url->GetPort() == 80 || url->GetPort() == 0)) ||
		(!strcasecmp(url->GetProtocol(), "https") && (url->GetPort() == 443 || url->GetPort() == 0)))
	{
		m_connection->WriteLine(BString<1024>("Host: %s\r\n", url->GetHost()));
	}
	else
	{
		m_connection->WriteLine(BString<1024>("Host: %s:%i\r\n", url->GetHost(), url->GetPort()));
	}

	m_connection->WriteLine("Accept: */*\r\n");
#ifndef DISABLE_GZIP
	m_connection->WriteLine("Accept-Encoding: gzip\r\n");
#endif
	m_connection->WriteLine("Connection: close\r\n");
	m_connection->WriteLine("\r\n");
}

WebDownloader::EStatus WebDownloader::DownloadHeaders()
{
	EStatus Status = adRunning;

	m_confirmedLength = false;
	CharBuffer lineBuf(1024*10);
	m_contentLen = -1;
	bool firstLine = true;
	m_gzip = false;
	m_redirecting = false;
	m_redirected = false;

	// Headers
	while (!IsStopped())
	{
		SetLastUpdateTimeNow();

		int len = 0;
		char* line = m_connection->ReadLine(lineBuf, lineBuf.Size(), &len);

		if (firstLine)
		{
			Status = CheckResponse(lineBuf);
			if (Status != adRunning)
			{
				break;
			}
			firstLine = false;
		}

		// Have we encountered a timeout?
		if (!line)
		{
			if (!IsStopped())
			{
				warn("URL %s failed: Unexpected end of file", *m_infoName);
			}
			Status = adFailed;
			break;
		}

		debug("Header: %s", line);

		// detect body of response
		if (*line == '\r' || *line == '\n')
		{
			break;
		}

		Util::TrimRight(line);
		ProcessHeader(line);

		if (m_redirected)
		{
			Status = adRedirect;
			break;
		}
	}

	return Status;
}

WebDownloader::EStatus WebDownloader::DownloadBody()
{
	EStatus Status = adRunning;

	m_outFile.Close();
	bool end = false;
	CharBuffer lineBuf(1024*10);
	int writtenLen = 0;

#ifndef DISABLE_GZIP
	m_gUnzipStream.reset();
	if (m_gzip)
	{
		m_gUnzipStream = std::make_unique<GUnzipStream>(1024*10);
	}
#endif

	// Body
	while (!IsStopped())
	{
		SetLastUpdateTimeNow();

		char* buffer;
		int len;
		m_connection->ReadBuffer(&buffer, &len);
		if (len == 0)
		{
			len = m_connection->TryRecv(lineBuf, lineBuf.Size());
			buffer = lineBuf;
		}

		// Connection closed or timeout?
		if (len <= 0)
		{
			if (len == 0 && m_contentLen == -1 && writtenLen > 0)
			{
				end = true;
				break;
			}

			if (!IsStopped())
			{
				warn("URL %s failed: Unexpected end of file", *m_infoName);
			}
			Status = adFailed;
			break;
		}

		// write to output file
		if (!Write(buffer, len))
		{
			Status = adFatalError;
			break;
		}
		writtenLen += len;

		//detect end of file
		if (writtenLen == m_contentLen || (m_contentLen == -1 && m_gzip && m_confirmedLength))
		{
			end = true;
			break;
		}
	}

#ifndef DISABLE_GZIP
	m_gUnzipStream.reset();
#endif

	m_outFile.Close();

	if (!end && Status == adRunning && !IsStopped())
	{
		warn("URL %s failed: file incomplete", *m_infoName);
		Status = adFailed;
	}

	if (end)
	{
		Status = adFinished;
	}

	return Status;
}

WebDownloader::EStatus WebDownloader::CheckResponse(const char* response)
{
	if (!response)
	{
		if (!IsStopped())
		{
			warn("URL %s: Connection closed by remote host", *m_infoName);
		}
		return adConnectError;
	}

	const char* hTTPResponse = strchr(response, ' ');
	if (strncmp(response, "HTTP", 4) || !hTTPResponse)
	{
		warn("URL %s failed: %s", *m_infoName, response);
		return adFailed;
	}

	hTTPResponse++;

	if (!strncmp(hTTPResponse, "400", 3) || !strncmp(hTTPResponse, "499", 3))
	{
		warn("URL %s failed: %s", *m_infoName, hTTPResponse);
		return adConnectError;
	}
	else if (!strncmp(hTTPResponse, "404", 3))
	{
		warn("URL %s failed: %s", *m_infoName, hTTPResponse);
		return adNotFound;
	}
	else if (!strncmp(hTTPResponse, "301", 3) || !strncmp(hTTPResponse, "302", 3) ||
		 !strncmp(hTTPResponse, "303", 3) || !strncmp(hTTPResponse, "307", 3) ||
		 !strncmp(hTTPResponse, "308", 3))
	{
		m_redirecting = true;
		return adRunning;
	}
	else if (!strncmp(hTTPResponse, "200", 3))
	{
		// OK
		return adRunning;
	}
	else
	{
		// unknown error, no special handling
		warn("URL %s failed: %s", *m_infoName, response);
		return adFailed;
	}
}

void WebDownloader::ProcessHeader(const char* line)
{
#ifdef HAVE_LIBCURL
	// When using libcurl the only important header
	// is Content-Disposition, everything else is handled
	// by libcurl
	if (!strncasecmp(line, "Content-Disposition: ", 21))
	{
		ParseFilename(line);
	}
#else // HAVE_LIBCURL
	if (!strncasecmp(line, "Content-Length: ", 16))
	{
		m_contentLen = atoi(line + 16);
		m_confirmedLength = true;
	}
	else if (!strncasecmp(line, "Content-Encoding: gzip", 22))
	{
		m_gzip = true;
	}
	else if (!strncasecmp(line, "Content-Disposition: ", 21))
	{
		ParseFilename(line);
	}
	else if (m_redirecting && !strncasecmp(line, "Location: ", 10))
	{
		ParseRedirect(line + 10);
		m_redirected = true;
	}
#endif // HAVE_LIBCURL
}

void WebDownloader::ParseFilename(const char* contentDisposition)
{
	// Examples:
	// Content-Disposition: attachment; filename="fname.ext"
	// Content-Disposition: attachement;filename=fname.ext
	// Content-Disposition: attachement;filename=fname.ext;
	const char *p = strstr(contentDisposition, "filename");
	if (!p)
	{
		return;
	}

	p = strchr(p, '=');
	if (!p)
	{
		return;
	}

	p++;

	while (*p == ' ') p++;

	BString<1024> fname = p;

	char *pe = fname + strlen(fname) - 1;
	while ((*pe == ' ' || *pe == '\n' || *pe == '\r' || *pe == ';') && pe > fname) {
		*pe = '\0';
		pe--;
	}

	WebUtil::HttpUnquote(fname);

	m_originalFilename = FileSystem::BaseFileName(fname);

	debug("OriginalFilename: %s", *m_originalFilename);
}

void WebDownloader::ParseRedirect(const char* location)
{
	const char* newLocation = location;
	BString<1024> urlBuf;
	URL newUrl(newLocation);
	if (!newUrl.IsValid())
	{
		// redirect within host

		BString<1024> resource;
		URL oldUrl(m_url);

		if (*location == '/')
		{
			// absolute path within host
			resource = location;
		}
		else
		{
			// relative path within host
			resource = oldUrl.GetResource();

			char* p = strchr(resource, '?');
			if (p)
			{
				*p = '\0';
			}

			p = strrchr(resource, '/');
			if (p)
			{
				p[1] = '\0';
			}

			resource.Append(location);
		}

		if (oldUrl.GetPort() > 0)
		{
			urlBuf.Format("%s://%s:%i%s", oldUrl.GetProtocol(), oldUrl.GetHost(), oldUrl.GetPort(), *resource);
		}
		else
		{
			urlBuf.Format("%s://%s%s", oldUrl.GetProtocol(), oldUrl.GetHost(), *resource);
		}
		newLocation = urlBuf;
	}
	detail("URL %s redirected to %s", *m_url, newLocation);
	SetUrl(newLocation);
}

bool WebDownloader::Write(void* buffer, int len)
{
	if (!m_outFile.Active() && !PrepareFile())
	{
		return false;
	}

#ifndef DISABLE_GZIP
	if (m_gzip)
	{
		m_gUnzipStream->Write(buffer, len);
		const void *outBuf;
		int outLen = 1;
		while (outLen > 0)
		{
			GUnzipStream::EStatus gZStatus = m_gUnzipStream->Read(&outBuf, &outLen);

			if (gZStatus == GUnzipStream::zlError)
			{
				error("URL %s: GUnzip failed", *m_infoName);
				return false;
			}

			if (outLen > 0 && m_outFile.Write(outBuf, outLen) <= 0)
			{
				return false;
			}

			if (gZStatus == GUnzipStream::zlFinished)
			{
				m_confirmedLength = true;
				return true;
			}
		}
		return true;
	}
	else
#endif

	return m_outFile.Write(buffer, len) > 0;
}

bool WebDownloader::PrepareFile()
{
	// prepare file for writing

	const char* filename = m_outputFilename;
	if (!m_outFile.Open(filename, DiskFile::omWrite))
	{
		error("Could not %s file %s", "create", filename);
		return false;
	}
	if (g_Options->GetWriteBuffer() > 0)
	{
		m_outFile.SetWriteBuffer(g_Options->GetWriteBuffer() * 1024);
	}

	return true;
}

void WebDownloader::LogDebugInfo()
{
	info("      Web-Download: status=%i, LastUpdateTime=%s, filename=%s", m_status,
		*Util::FormatTime(m_lastUpdateTime), FileSystem::BaseFileName(m_outputFilename));
}

void WebDownloader::Stop()
{
	debug("Trying to stop WebDownloader");
	Thread::Stop();
	Guard guard(m_connectionMutex);
	if (m_connection)
	{
		m_connection->SetSuppressErrors(true);
		m_connection->Cancel();
	}
	debug("WebDownloader stopped successfully");
}

void WebDownloader::FreeConnection()
{
	if (m_connection)
	{
		debug("Releasing connection");
		Guard guard(m_connectionMutex);
		if (m_connection->GetStatus() == Connection::csCancelled)
		{
			m_connection->Disconnect();
		}
		m_connection.reset();
	}
}

#ifdef HAVE_LIBCURL
// Helper function to handle body download writes. This is
// here so we can call a class member function from C.
size_t WebDownloader::curl_body_callback(char *data, size_t size, size_t nmemb, void *f) {
	// Recreate the class
	return static_cast<WebDownloader*>(f)->CurlBodyCallback(data, size, nmemb);
}

// Helper function to handle header lines. This is here
// so we can call a class member function from C.
size_t WebDownloader::curl_header_callback(char *data, size_t size, size_t nmemb, void *f) {
	// Recreate the class
	return static_cast<WebDownloader*>(f)->CurlHeaderCallback(data, size, nmemb);
}

size_t WebDownloader::CurlBodyCallback(char* data, size_t size, size_t nmemb) {
	info("WebDownloader::CurlBodyCallback(%ld)", (size * nmemb));
	if (!m_outFile.Active() && !PrepareFile())
	{
		return 0;
	}

	return m_outFile.Write(data, (size * nmemb));
}

size_t WebDownloader::CurlHeaderCallback(char* data, size_t size, size_t nmemb) {
	info("WebDownloader::CurlHeaderCallback(%ld)", (size * nmemb));
	// The data that curl gives us is not necessarily NULL terminated. Create a temporary container
	CString header = CString(data, size * nmemb);

	ProcessHeader((char *)header);

	// Curl expects this to signal successful handling
	return size * nmemb;
}
#endif
