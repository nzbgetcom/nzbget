/*
 * This file is part of nzbget. See <https://nzbget.com>.
 *
 * Copyright (C) 2024 Denis <denis@nzbget.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */


var SystemInfo = (new function($)
{
	this.id = "Config-SystemInfo";

	var $SysInfo_OS;
	var $SysInfo_AppVersion;
	var $SysInfo_Uptime;
	var $SysInfo_ConfPath;
	var $SysInfo_CPUModel;
	var $SysInfo_Arch;
	var $SysInfo_IP;
	var $SysInfo_DestDiskSpace;
	var $SysInfo_InterDiskSpace;
	var $SysInfo_DestDiskSpaceContainer;
	var $SysInfo_InterDiskSpaceContainer;
	var $SysInfo_ArticleCache;
	var $SysInfo_WriteBuffer;
	var $SysInfo_ToolsTable;
	var $SysInfo_LibrariesTable;
	var $SysInfo_NewsServersTable;
	var $SysInfo_ErrorAlert;
	var $SysInfo_ErrorAlertText;
	var $SpeedTest_Stats;
	var $SpeedTest_StatsHeader;
	var $SpeedTest_StatsTable;
	var $SystemInfo_Spinner;
	var $SystemInfo_MainContent;

	var nzbFileTestPrefix = 'NZBGet Speed Test ';
	var testNZBUrl = 'https://nzbget.com/nzb/';
	var testNZBFiles = [
		'100MB',
		'500MB',
		'1GB',
		'5GB',
		'10GB',
	];
	var testNZBListId = 'dropdown_test_nzb_list_';
	var lastTestStatsId = 'last_test_stats_';
	var serverTestSpinnerId = 'server_test_spinner_';
	var lastTestStatsBtns = {};
	var spinners = {};
	var allStats = [];
	var downloads = [];
	var statusHandler = 
	{
		update: function(status) 
		{
			$SysInfo_Uptime.text(Util.formatTimeHMS(status['UpTimeSec']));

			var destDirOpt = Options.findOption(Options.options, 'DestDir');
			var interDirOpt = Options.findOption(Options.options, 'InterDir');

			

			if (destDirOpt && interDirOpt)
			{
				var destDirPath = destDirOpt.Value;
				var interDistPath = interDirOpt.Value ? interDirOpt.Value : destDirPath;
				renderDiskSpace(+status['FreeDiskSpaceMB'], +status['TotalDiskSpaceMB'], destDirPath);
				renderInterDiskSpace(+status['FreeInterDiskSpaceMB'], +status['TotalInterDiskSpaceMB'], interDistPath);
			}
		}
	}

	var statsHandler = 
	{
		update: function(data) 
		{
			allStats = getFinishedTestStats(data);
			renderLastTestBtns(allStats);
		}
	}

	var downloadsHandler = 
	{
		update: function(data) 
		{
			downloads = getTestStats(data);
			renderSpinners(downloads);
		}
	}

	this.init = function()
	{
		$SysInfo_OS = $('#SysInfo_OS');
		$SysInfo_AppVersion = $('#SysInfo_AppVersion');
		$SysInfo_Uptime = $('#SysInfo_Uptime');
		$SysInfo_ConfPath = $('#SysInfo_ConfPath');
		$SysInfo_CPUModel = $('#SysInfo_CPUModel');
		$SysInfo_Arch = $('#SysInfo_Arch');
		$SysInfo_IP = $('#SysInfo_IP');
		$SysInfo_DestDiskSpace = $('#SysInfo_DestDiskSpace');
		$SysInfo_InterDiskSpace = $('#SysInfo_InterDiskSpace');
		$SysInfo_InterDiskSpaceContainer = $('#SysInfo_InterDiskSpaceContainer');
		$SysInfo_DestDiskSpaceContainer = $('#SysInfo_DestDiskSpaceContainer');
		$SysInfo_ArticleCache = $('#SysInfo_ArticleCache');
		$SysInfo_WriteBuffer = $('#SysInfo_WriteBuffer');
		$SysInfo_ToolsTable = $('#SysInfo_ToolsTable');
		$SysInfo_LibrariesTable = $('#SysInfo_LibrariesTable');
		$SysInfo_NewsServersTable = $('#SysInfo_NewsServersTable');
		$SysInfo_ErrorAlert = $('#SystemInfoErrorAlert');
		$SysInfo_ErrorAlertText = $('#SystemInfoAlert-text');
		$SpeedTest_Stats = $('#SpeedTest_Stats');
		$SpeedTest_StatsHeader = $('#SpeedTest_StatsHeader');
		$SpeedTest_StatsTable = $('#SpeedTest_StatsTable tbody');
		$SystemInfo_Spinner = $('#SystemInfo_Spinner');
		$SystemInfo_MainContent = $('#SystemInfo_MainContent');

		Status.subscribe(statusHandler);
		History.subscribe(statsHandler);
		Downloads.subscribe(downloadsHandler);
	}

	this.loadSystemInfo = function()
	{
		cleanUp();
		hideError();
		hideMainContent();
		showSpinner();

		RPC.call('sysinfo', [], 
			function (sysInfo)
			{
				hideSpinner();
				showMainContent();
				render(sysInfo);
				renderLastTestBtns(allStats);
				renderSpinners(downloads);
			},
			errorHandler
		);
	}

	function pathsOnSameDisk(path1, path2) 
	{
		path1 = path1.replace(/\\/g, '/');
		path2 = path2.replace(/\\/g, '/');
	  
		var drive1 = path1.match(/^[a-zA-Z]:\//i) ? path1.match(/^[a-zA-Z]:\//i)[0] : '/';
		var drive2 = path2.match(/^[a-zA-Z]:\//i) ? path2.match(/^[a-zA-Z]:\//i)[0] : '/';

		return drive1 === drive2;
	}

	function hideSpinner()
	{
		$SystemInfo_Spinner.hide();
	}

	function showSpinner()
	{
		$SystemInfo_Spinner.show();
	}

	function hideMainContent()
	{
		$SystemInfo_MainContent.hide();
	}

	function showMainContent()
	{
		$SystemInfo_MainContent.show();
	}

	function getFinishedTestStats(allStats)
	{
		return getTestStats(allStats).filter(function(stats)
		{
			return stats.Status.startsWith('SUCCESS');
		});
	}

	function getTestStats(allStats)
	{
		return allStats.filter(function(stats) 
		{ 
			return stats.NZBFilename
				&& stats.ServerStats
				&& stats.NZBFilename.startsWith(testNZBUrl + nzbFileTestPrefix)
				&& stats.ServerStats.length; 
		});
	}

	function cleanUp()
	{
		$SysInfo_ToolsTable.empty();
		$SysInfo_LibrariesTable.empty();
		$SysInfo_NewsServersTable.empty();
	}

	function errorHandler(err)
	{
		$SysInfo_ErrorAlertText.text(err);
		$SysInfo_ErrorAlertText.show();
		$SysInfo_ErrorAlert.show();
	}

	function hideError()
	{
		$SysInfo_ErrorAlertText.hide();
	}

	function render(sysInfo)
	{
		if (sysInfo['OS'].Name)
		{
			$SysInfo_OS.text(sysInfo['OS'].Name + ' ' + sysInfo['OS'].Version);
		}
		else
		{
			$SysInfo_OS.text('N/A');
		}
		
		$SysInfo_CPUModel.text(sysInfo['CPU'].Model || 'Unknown');
		$SysInfo_Arch.text(sysInfo['CPU'].Arch || 'Unknown');
		$SysInfo_ConfPath.text(Options.option('ConfigFile'));
		$SysInfo_ArticleCache.text(Util.formatSizeMB(+Options.option('ArticleCache')));
		$SysInfo_WriteBuffer.text(Util.formatSizeMB(+Options.option('WriteBuffer')));

		renderIP(sysInfo['Network']);
		renderAppVersion(Options.option('Version'));
		renderTools(sysInfo['Tools']);
		renderLibraries(sysInfo['Libraries']);
		renderNewsServers(Status.getStatus()['NewsServers']);
	}

	function renderSpinners(downloads)
	{
		for (var key in spinners) {
			if (Object.hasOwnProperty.call(spinners, key)) {
				spinners[key].hide();
			}
		}
		downloads.forEach(function(stats)
		{
			var id = serverTestSpinnerId + stats['ServerStats'][0]['ServerID'];
			if (spinners[id])
			{
				spinners[id].show();
			}
		});
	}

	function renderLastTestBtns(stats)
	{
		var alreadyRendered = {};
		stats.forEach(function(stats)
		{
			var id = lastTestStatsId + stats['ServerStats'][0]['ServerID'];
			if (lastTestStatsBtns[id] && !alreadyRendered[id])
			{
				alreadyRendered[id] = true;
				lastTestStatsBtns[id].text(getSpeed(stats));
				lastTestStatsBtns[id].show();
				lastTestStatsBtns[id]
					.off('click')
					.on('click', function(e)
					{
						e.preventDefault();
						showStatsTable(stats);
					});
			}
		});
	}

	function renderDiskSpace(free, total, path)
	{
		$SysInfo_DestDiskSpace.text(formatDiskInfo(free, total));
		$SysInfo_DestDiskSpaceContainer.attr('title', path);
	}

	function renderInterDiskSpace(free, total, path)
	{
		$SysInfo_InterDiskSpace.text(formatDiskInfo(free, total));
		$SysInfo_InterDiskSpaceContainer.attr('title', path);
	}

	function formatDiskInfo(free, total)
	{
		var percents = total !== 0 ? (free / total * 100).toFixed(1) + '%' : '0.0%';
		return Util.formatSizeMB(free) + ' (' + percents + ') / ' + Util.formatSizeMB(total);
	}

	function renderIP(network)
	{
		var privateIP = network.PrivateIP ? network.PrivateIP : 'N/A';
		var publicIP = network.PublicIP ? network.PublicIP : 'N/A';
		$SysInfo_IP.text(privateIP + ' / ' + publicIP);
	}

	function renderAppVersion(version)
	{
		$SysInfo_AppVersion.text(version);
		var updateBtn = $('<button type="button" class="btn btn-default">Updates</>');
		updateBtn.css('margin-left', '5px');
		updateBtn.on('click', Config.checkUpdates);
		$SysInfo_AppVersion.append(updateBtn);
	}

	function renderTools(tools)
	{
		tools.forEach(function(tool)
			{
				var tr = $('<tr>');
				var tdName = $('<td>');
				var tdVersion = $('<td>');
				var tdPath = $('<td>');
				tdName.text(tool.Name);
				tdVersion.text(tool.Version ? tool.Version : 'N/A');
				tdPath.text(tool.Path ? tool.Path : 'Not found');
				tr.append(tdName);
				tr.append(tdVersion);
				tr.append(tdPath);
				$SysInfo_ToolsTable.append(tr);
			}
		);
	}

	function renderLibraries(libs)
	{
		libs.forEach(function(lib)
			{
				var tr = $('<tr>');
				var tdName = $('<td>');
				var tdVersion = $('<td>');
				tdName.text(lib.Name);
				tdVersion.text(lib.Version);
				tr.append(tdName);
				tr.append(tdVersion);
				$SysInfo_LibrariesTable.append(tr);
			}
		);
	}

	function renderNewsServers(newsServers)
	{
		newsServers.forEach(function(newsServer)
			{
				var server = Options.getServerById(newsServer.ID);
				var tr = $('<tr>');
				var tdName = $('<td>');
				var tdActive = $('<td>');
				var tdTests = $('<td>');
				tdTests.css('display', 'flex');
				tdTests.css('flex-wrap', 'wrap');
				tdTests.css('align-items', 'center');
				tdTests.css('gap', '3px');
				tdTests.css('overflow', 'visible');

				var testConnectionBtn = makeTestConnectionBtn(server.id);
				var testServerSpeedBtn = makeTestServerSpeedBtn(server.id);
				var lastTestStatsBtn = makeLastTestStatsBtnPlaceholder(lastTestStatsId + server.id);
				var spinner = makeTestServerSpinnerPlaceholder(serverTestSpinnerId + newsServer.ID);

				lastTestStatsBtns[lastTestStatsId + server.id] = lastTestStatsBtn;
				spinners[serverTestSpinnerId + newsServer.ID] = spinner;

				testServerSpeedBtn.on('click', function(e)
				{
					e.preventDefault();
					var listOfTestNZB = makeListOfTestNZB(server.id);
					var alreadyExists = $('#' + testNZBListId + server.id);
					if (alreadyExists.length > 0)
					{
						alreadyExists.replaceWith(listOfTestNZB);
					}
					else
					{
						testServerSpeedBtn.append(listOfTestNZB);
					}
				});

				tdName.text(server.host + ':' + server.port + '(' + server.connections + ')');
				tdName.attr({ title: server.name });

				if (newsServer.Active) 
				{
					tdActive.text('Yes');
					tdActive.css('color', '#468847');
				}
				else 
				{
					tdActive.text('No');
					tdActive.css('color', '#da4f49');
				}

				tdTests.append(testConnectionBtn);
				tdTests.append(testServerSpeedBtn);
				tdTests.append(lastTestStatsBtn);
				tdTests.append(spinner);
				tr.append(tdName);
				tr.append(tdActive);
				tr.append(tdTests);
				$SysInfo_NewsServersTable.append(tr);
			}
		);
	}

	function makeLastTestStatsBtnPlaceholder(id)
	{
		var btn = $('<button id="' 
			+ id + 
			'type="button" class="btn btn-default" data-toggle="modal" data-target="#SpeedTest_Stats" title="Statistics">' 
			+ '</>'
		);
		btn.css('display', 'none');
		return btn;
	}

	function makeTestServerSpinnerPlaceholder(id)
	{
		var spinner = $('<i id="' + id + '" class="material-icon spinner">progress_activity</>');
		spinner.css('display', 'none');
		return spinner;
	}

	function makeTestConnectionBtn(serverid)
	{
		var testConnectionBtn = $('<button type="button" class="btn btn-default">Connection</>');

		testConnectionBtn.attr({ 'data-multiid': serverid });
		testConnectionBtn.on('click', function () 
			{
				Config.testConnection(this, "Server", serverid);
			}
		);

		return testConnectionBtn;
	}

	function makeTestServerSpeedBtn()
	{
		var container = $('<div>');
		container.css('position', 'relative');

		var caret = $('<span class="caret"></>');
		caret.css('margin-left', '5px');
		
		var testBtn = $('<button class="btn btn-default dropdown-toggle" data-toggle="dropdown">Speed</>');

		testBtn.append(caret);
		container.append(testBtn);

		return container;
	}

	function makeListOfTestNZB(serverid)
	{
		var list = $('<ul class="test-server-dropdwon-menu dropdown-menu" id="' + testNZBListId + serverid + '"></ul>');
		testNZBFiles.forEach(function(name)
		{
			var fullName = nzbFileTestPrefix + name + '.nzb';
			var li = $('<li></>');
			li.css('display', 'flex');
			li.css('align-items', 'center');
			li.css('padding', '1px 0');
			var btn = $('<a href="#">' + name + '</a>').on('click', function(e) 
				{
					e.preventDefault();
					testServerSpeed(fullName, serverid);
				}
			);

			btn.css('display', 'block');
			btn.css('width', '100%');
			li.append(btn);

			var stats = getStats(fullName, serverid);
			if (stats)
			{
				var statsBtn = makeStatisticsBtn(stats)
				li.append(statsBtn);
			}
			
			list.append(li);
		});

		return list;
	}

	function getStats(nzbName, serverid)
	{
		return allStats.find(function(h) { 
			return h.NZBFilename === testNZBUrl + nzbName && h.ServerStats[0]['ServerID'] == serverid; 
		});
	}

	function makeStatisticsBtn(stats)
	{
		var statsBtn = $('<button '
			+ 'type="button" class="btn btn-default" data-toggle="modal" data-target="#SpeedTest_Stats" title="Statistics">' 
			+ getSpeed(stats)
			+ '</>'
		);
		statsBtn.css('font-size', '12px');
		statsBtn.css('padding', '3px');
		statsBtn.css('position', 'absolute');
		statsBtn.css('right', '5px');

		statsBtn.on('click', function(e) 
		{ 
			e.preventDefault();
			showStatsTable(stats)
		});

		return statsBtn;
	}

	function testServerSpeed(nzbFileName, serverId)
	{
		RPC.call('testserverspeed', [
				testNZBUrl + nzbFileName, 
				serverId
			], 
			function() { }, 
			errorHandler,
		);
	}

	function showStatsTable(stats)
	{
		$($SpeedTest_Stats).show();
		$($SpeedTest_StatsHeader).text(stats.NZBName);
		var table = makeStatistics(stats);
		$($SpeedTest_StatsTable).html(table);
	}

	function makeStatistics(stats)
	{
		var downloaded = Util.formatSizeMB(stats.DownloadedSizeMB, stats.DownloadedSizeLo);
		var speed = getSpeed(stats);
		var date = Util.formatDateTime(stats.HistoryTime + UISettings.timeZoneCorrection * 60 * 60);
		var table = '';
		table += '<tr><td>Download speed</td><td class="text-center">' + speed + '</td></tr>';
		table += '<tr><td>Downloaded size</td><td class="text-center">' + downloaded + '</td></tr>';
		table += '<tr><td>Download time</td><td class="text-center">' + Util.formatTimeHMS(stats.DownloadTimeSec) + '</td></tr>';
		table += '<tr><td>Date</td><td class="text-center">' + date + '</td></tr>';

		return table;
	}

	function getSpeed(stats)
	{
		var bytes = stats.DownloadedSizeMB > 1024 ? stats.DownloadedSizeMB * 1024.0 * 1024.0 : stats.DownloadedSizeLo;
		var speed = stats.DownloadTimeSec > 0 ? Util.formatSpeed(bytes / stats.DownloadTimeSec) : '--';

		return speed;
	}

}(jQuery))
