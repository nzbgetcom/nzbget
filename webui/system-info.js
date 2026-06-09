/*
 * This file is part of nzbget. See <https://nzbget.com>.
 *
 * Copyright (C) 2024-2026 Denis <denis@nzbget.com>
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

var SPINNER = '<i class="material-icon spinner">progress_activity</i>';
var TEST_BTN_DEFAULT_TEXT = 'Run test';
var NETWORK_SPEED_TEST_RUNNING = false;

function DiskSpeedTestsForm()
{
	var _512MiB = 1024 * 1024 * 512;
	var $timeout;
	var $maxSize;
	var $writeBufferInput;
	var $diskSpeedTestInputLabel;
	var $diskSpeedTestInput;
	var $diskSpeedTestBtn;
	var $diskSpeedTestErrorTxt;

	this.init = function(writeBuffer, dirPath, label, lsKey)
	{
		$timeout = $('#SysInfo_DiskSpeedTestTimeout');
		$maxSize = $('#SysInfo_DiskSpeedTestMaxSize');
		$writeBufferInput = $('#SysInfo_DiskSpeedTestWriteBufferInput');
		$diskSpeedTestInputLabel  = $('#SysInfo_DiskSpeedTestInputLabel');
		$diskSpeedTestInput  = $('#SysInfo_DiskSpeedTestInput');
		$diskSpeedTestBtn = $('#SysInfo_DiskSpeedTestBtn');
		$diskSpeedTestErrorTxt = $('#SysInfo_DiskSpeedTestErrorTxt');

		disableBtnToggle(false);

		var defaultTestText = I18n.translate('sysinfo_run_test');
		var saved = Util.getFromLocalStorage(lsKey);
		var isNumeric = saved !== null && saved !== '' && !isNaN(saved);
		if (isNumeric) {
			$diskSpeedTestBtn.text(Util.formatSpeed(Number(saved))).removeAttr('data-i18n');
		} else {
			$diskSpeedTestBtn.attr('data-i18n', 'sysinfo_run_test').removeData('i18n-last');
			I18n.translatePage($diskSpeedTestBtn);
		}

		$writeBufferInput.val(writeBuffer);
		$diskSpeedTestInputLabel.text(label);
		$diskSpeedTestInput.val(dirPath);

		$diskSpeedTestBtn
			.off('click')
			.on('click', function()
		{	
			var writeBufferVal = + $writeBufferInput.val();
			var path = $diskSpeedTestInput.val();
			var maxSize = + $maxSize.val();
			var timeout = + $timeout.val();
			runDiskSpeedTest(path, writeBufferVal, maxSize, timeout, lsKey);
		});

		$writeBufferInput
			.off('change paste keyup')
			.on('change paste keyup', function(event)
		{
			if (isValidWriteBuffer(event.target.value))
				disableBtnToggle(false);
			else
				disableBtnToggle(true);
		});

		$diskSpeedTestInput
			.off('change paste keyup')
			.on('change paste keyup', function(event)
		{
			if (isValidPath(event.target.value))
				disableBtnToggle(false);
			else
				disableBtnToggle(true);
		});
	}

	function runDiskSpeedTest(path, writeBufferSize, maxFileSize, timeout, lsKey)
	{
		$diskSpeedTestErrorTxt.empty();
		$diskSpeedTestBtn.html(SPINNER);
		disableBtnToggle(true);

		RPC.call('testdiskspeed', [path, writeBufferSize, maxFileSize, timeout], 
			function(rawRes) 
			{
				var bytesPerSec = (rawRes.SizeMB * 1024.0 * 1024.0) / (rawRes.DurationMS / 1000.0);
				Util.saveToLocalStorage(lsKey, bytesPerSec);
				$diskSpeedTestBtn.html(Util.formatSpeed(bytesPerSec)).removeAttr('data-i18n');
				disableBtnToggle(false);
			}, 
			function(res) 
			{
				var saved = Util.getFromLocalStorage(lsKey);
				var isNumeric = saved !== null && saved !== '' && !isNaN(saved);
				if (isNumeric) {
					$diskSpeedTestBtn.html(Util.formatSpeed(Number(saved))).removeAttr('data-i18n');
				} else {
					$diskSpeedTestBtn.attr('data-i18n', 'sysinfo_run_test').removeData('i18n-last');
					I18n.translatePage($diskSpeedTestBtn);
				}
				disableBtnToggle(false);

				var errTxt = res.split('<br>')[0];
				$diskSpeedTestErrorTxt.html(errTxt);
			},
		);
	}

	function disableBtnToggle(disable)
	{
		if (disable)
		{
			$diskSpeedTestBtn.addClass('btn--disabled');
		}
		else
		{
			$diskSpeedTestBtn.removeClass('btn--disabled');
		}
	}

	function isValidWriteBuffer(input)
	{
		var val = input.trim();
		if (!val)
			return false;

		var num = Number(val);

		return !isNaN(num) && num >= 0 && num < _512MiB;
	}

	function isValidPath(input)
	{
		var path = input.trim();

		return path !== '';
	}
}

var SystemInfo = (new function($)
{
	this.id = "Config-SystemInfo";

	var $Container;
	var $SysInfo_OS;
	var $SysInfo_AppVersion;
	var $SysInfo_Uptime;
	var $SysInfo_ConfPath;
	var $SysInfo_CPUModel;
	var $SysInfo_Arch;
	var $SysInfo_IP;
	var $SysInfo_DestDiskSpace;
	var $SysInfo_InterDiskSpace;
	var $SysInfo_DestDirDiskTestBtn;
	var $SysInfo_InterDirDiskTestBtn;
	var $SysInfo_NetworkSpeedTestBtn;
	var $SysInfo_NetworkSpeedTestErrorTxt;
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
	var $DiskSpeedTest_Modal;
	var $SystemInfo_Spinner;
	var $SystemInfo_MainContent;
	var $SpeedTest_Stats;
	var $SpeedTest_StatsHeader;
	var $SpeedTest_StatsSpeed;
	var $SpeedTest_StatsSize;
	var $SpeedTest_StatsTime;
	var $SpeedTest_StatsDate;

	var sysInfoLoading = false;
	var lastSysInfo = null;

	var nzbFileTestPrefix = 'NZBGet Speed Test ';
	var testNZBUrl = 'https://nzbget.com/nzb/';
	function getTestNZBFiles() {
		return [
			{ name: '100MB', label: '100' + I18n.translate("unit_mb") },
			{ name: '500MB', label: '500' + I18n.translate("unit_mb") },
			{ name: '1GB', label: '1' + I18n.translate("unit_gb") },
			{ name: '5GB', label: '5' + I18n.translate("unit_gb") },
			{ name: '10GB', label: '10' + I18n.translate("unit_gb") },
		];
	}
	var testNZBListId = 'dropdown_test_nzb_list_';
	var lastTestStatsId = 'last_test_stats_';
	var serverTestSpinnerId = 'server_test_spinner_';

	var DEST_DIR_LS_KEY = 'DestDirSpeedResults';
	var INTER_DIR_LS_KEY = 'InterDirSpeedResults';
	var NETWORK_SPEED_TEST_LS_KEY = 'NetworkSpeedResults';
	var NETWORK_SPEED_TEST_DATE_LS_KEY = 'NetworkSpeedDate';

	var lastTestStatsBtns = {};
	var spinners = {};
	var allStats = [];
	var downloads = [];
	var statusHandler = 
	{
		update: function(status) 
		{
			$SysInfo_Uptime.text(Util.formatTimeHMS(status['UpTimeSec']));

			var writeBufferKB = Options.option('WriteBuffer');
			var destDir = Options.option('DestDir');
			var interDir = Options.option('InterDir') || destDir;

			renderDestDiskSpace(writeBufferKB, +status['FreeDiskSpaceMB'], +status['TotalDiskSpaceMB'], destDir);
			renderInterDiskSpace(writeBufferKB, +status['FreeInterDiskSpaceMB'], +status['TotalInterDiskSpaceMB'], interDir);
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
		$Container = $('.config__main');
		$SysInfo_OS = $('#SysInfo_OS');
		$SysInfo_AppVersion = $('#SysInfo_AppVersion');
		$SysInfo_Uptime = $('#SysInfo_Uptime');
		$SysInfo_ConfPath = $('#SysInfo_ConfPath');
		$SysInfo_CPUModel = $('#SysInfo_CPUModel');
		$SysInfo_Arch = $('#SysInfo_Arch');
		$SysInfo_IP = $('#SysInfo_IP');
		$SysInfo_DestDiskSpace = $('#SysInfo_DestDiskSpace');
		$SysInfo_InterDiskSpace = $('#SysInfo_InterDiskSpace');
		$SysInfo_DestDirDiskTestBtn = $('#SysInfo_DestDirDiskTestBtn');
		$SysInfo_InterDirDiskTestBtn = $('#SysInfo_InterDirDiskTestBtn');
		$SysInfo_NetworkSpeedTestBtn = $('#SysInfo_NetworkSpeedTestBtn');
		$SysInfo_NetworkSpeedTestErrorTxt = $('#SysInfo_NetworkSpeedTestErrorTxt');
		$SysInfo_InterDiskSpaceContainer = $('#SysInfo_InterDiskSpaceContainer');
		$SysInfo_DestDiskSpaceContainer = $('#SysInfo_DestDiskSpaceContainer');
		$SysInfo_ArticleCache = $('#SysInfo_ArticleCache');
		$SysInfo_WriteBuffer = $('#SysInfo_WriteBuffer');
		$SysInfo_ToolsTable = $('#SysInfo_ToolsTable');
		$SysInfo_LibrariesTable = $('#SysInfo_LibrariesTable');
		$SysInfo_NewsServersTable = $('#SysInfo_NewsServersTable');
		$SysInfo_ErrorAlert = $('#SystemInfoErrorAlert');
		$SysInfo_ErrorAlertText = $('#SystemInfo_alertText');
		$SpeedTest_Stats = $('#SpeedTest_Stats');
		$SpeedTest_StatsHeader = $('#SpeedTest_StatsHeader');
		$SpeedTest_StatsSpeed = $('#SpeedTest_StatsSpeed');
		$SpeedTest_StatsSize = $('#SpeedTest_StatsSize');
		$SpeedTest_StatsTime = $('#SpeedTest_StatsTime');
		$SpeedTest_StatsDate = $('#SpeedTest_StatsDate');
		$DiskSpeedTest_Modal = $('#DiskSpeedTest_Modal');
		$NetworkSpeedTest_Modal = $('#NetworkSpeedTest_Modal');
		$SystemInfo_Spinner = $('#SystemInfo_Spinner');
		$SystemInfo_MainContent = $('#SystemInfo_MainContent');

		Status.subscribe(statusHandler);
		History.subscribe(statsHandler);
		Downloads.subscribe(downloadsHandler);
	}

	this.redraw = function()
	{
		if (!$Container) return;

		// Update disk speed buttons
		[
			{ btn: $SysInfo_DestDirDiskTestBtn, key: DEST_DIR_LS_KEY },
			{ btn: $SysInfo_InterDirDiskTestBtn, key: INTER_DIR_LS_KEY }
		].forEach(function(item) {
			var saved = Util.getFromLocalStorage(item.key);
			var isNumeric = saved !== null && saved !== '' && !isNaN(saved);
			if (isNumeric) {
				item.btn.text(Util.formatSpeed(Number(saved))).removeAttr('data-i18n');
			} else {
				item.btn.attr('data-i18n', 'sysinfo_run_test');
				I18n.translatePage(item.btn);
			}
		});

		// Update network speed button
		var savedNet = Util.getFromLocalStorage(NETWORK_SPEED_TEST_LS_KEY);
		if (savedNet && !isNaN(savedNet) && !NETWORK_SPEED_TEST_RUNNING) {
			$SysInfo_NetworkSpeedTestBtn.text(Util.formatNetworkSpeed(Number(savedNet)));
		}
		
		var savedDate = Number(Util.getFromLocalStorage(NETWORK_SPEED_TEST_DATE_LS_KEY));
		if (savedDate) {
			renderNetworkSpeedTestBtnTitle(savedDate);
		}

		// Refresh news servers table to update speed buttons and Yes/No labels
		if (Status.getStatus() && Status.getStatus()['NewsServers']) {
			$SysInfo_NewsServersTable.empty();
			renderNewsServers(Status.getStatus()['NewsServers']);
		}

		if (lastSysInfo) {
			$SysInfo_ToolsTable.empty();
			$SysInfo_LibrariesTable.empty();
			renderTools(lastSysInfo['Tools']);
			renderLibraries(lastSysInfo['Libraries']);
		}
		
		// Update App version (includes updates button with data-i18n)
		renderAppVersion(Options.option('Version'));
	};

	this.loadSystemInfo = function()
	{
		if (sysInfoLoading) return;
		sysInfoLoading = true;

		hideError();
		hideMainContent();
		showSpinner();

		RPC.call('sysinfo', [], 
			function (sysInfo)
			{
				lastSysInfo = sysInfo;
				cleanUp();
				hideSpinner();
				showMainContent();
				render(sysInfo);
				renderLastTestBtns(allStats);
				renderSpinners(downloads);
				sysInfoLoading = false;
			},
			function(err)
			{
				hideSpinner();
				errorHandler(err);
				sysInfoLoading = false;
			}
		);
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
		if (!allStats || !Array.isArray(allStats)) return [];
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
		lastTestStatsBtns = {};
		spinners = {};
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
			$SysInfo_OS.text(I18n.translate('label_na'));
		}
		
		$SysInfo_CPUModel.text(sysInfo['CPU'].Model || I18n.translate('label_unknown'));
		$SysInfo_Arch.text(sysInfo['CPU'].Arch || I18n.translate('label_unknown'));
		$SysInfo_ConfPath.text(Options.option('ConfigFile'));
		$SysInfo_ArticleCache.text(Util.formatSizeMB(+Options.option('ArticleCache')));

		renderWriteBuffer(+Options.option('WriteBuffer'));
		renderIP(sysInfo['Network']);
		renderAppVersion(Options.option('Version'));
		renderTools(sysInfo['Tools']);
		renderLibraries(sysInfo['Libraries']);
		if (Status.getStatus() && Status.getStatus()['NewsServers']) {
			renderNewsServers(Status.getStatus()['NewsServers']);
		}

		scrollToTop();
	}

	function scrollToTop() {
		$Container.animate({ scrollTop: 0 }, 'fast');
	}

	function renderWriteBuffer(writeBufferKB)
	{
		var writeBufferMB = writeBufferKB / 1024;
		$SysInfo_WriteBuffer.text(Util.formatSizeMB(writeBufferMB));
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
				lastTestStatsBtns[id].empty();
				alreadyRendered[id] = true;
				lastTestStatsBtns[id].append(makeSpeed(stats));
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

	function renderDestDiskSpace(writeBufferKB, free, total, dirPath)
	{
		$SysInfo_DestDiskSpace.text(formatDiskInfo(free, total));
		$SysInfo_DestDiskSpaceContainer.attr('title', dirPath);
		$SysInfo_DestDirDiskTestBtn.off('click').on('click', function()
		{
			showDiskSpeedModal(writeBufferKB, dirPath, 'DestDir', DEST_DIR_LS_KEY);
		});

		var savedResults = Util.getFromLocalStorage(DEST_DIR_LS_KEY);
		var isNumeric = savedResults !== null && savedResults !== '' && !isNaN(savedResults);
		if (isNumeric) {
			$SysInfo_DestDirDiskTestBtn.text(Util.formatSpeed(Number(savedResults))).removeAttr('data-i18n');
		} else {
			$SysInfo_DestDirDiskTestBtn.attr('data-i18n', 'sysinfo_run_test').removeData('i18n-last');
			I18n.translatePage($SysInfo_DestDirDiskTestBtn);
		}
	}

	function renderInterDiskSpace(writeBufferKB, free, total, dirPath)
	{
		$SysInfo_InterDiskSpace.text(formatDiskInfo(free, total));
		$SysInfo_InterDiskSpaceContainer.attr('title', dirPath);
		$SysInfo_InterDirDiskTestBtn.off('click').on('click', function()
		{
			showDiskSpeedModal(writeBufferKB, dirPath, 'InterDir', INTER_DIR_LS_KEY);
		});

		var savedResults = Util.getFromLocalStorage(INTER_DIR_LS_KEY);
		var isNumeric = savedResults !== null && savedResults !== '' && !isNaN(savedResults);
		if (isNumeric) {
			$SysInfo_InterDirDiskTestBtn.text(Util.formatSpeed(Number(savedResults))).removeAttr('data-i18n');
		} else {
			$SysInfo_InterDirDiskTestBtn.attr('data-i18n', 'sysinfo_run_test').removeData('i18n-last');
			I18n.translatePage($SysInfo_InterDirDiskTestBtn);
		}
	}

	function formatDiskInfo(free, total)
	{
		if (free === 0 || total === 0)
		{
			return I18n.translate('label_na');
		}

		var percents = (free / total * 100).toFixed(1) + '%';
		return Util.formatSizeMB(free) + ' (' + percents + ') / ' + Util.formatSizeMB(total);
	}

	function renderIP(network)
	{
		var privateIP = network.PrivateIP ? network.PrivateIP : I18n.translate('label_na');
		var publicIP = network.PublicIP ? network.PublicIP : I18n.translate('label_na');
		$SysInfo_IP.text(privateIP + ' / ' + publicIP);

		renderNetworkSpeedTestBtn();
	}

	function renderNetworkSpeedTestBtn()
	{
		var savedResults = Number(Util.getFromLocalStorage(NETWORK_SPEED_TEST_LS_KEY));
		if (savedResults && !NETWORK_SPEED_TEST_RUNNING)
		{
			renderNetworkSpeedTestResults(savedResults);
		}
		else if (savedResults && NETWORK_SPEED_TEST_RUNNING)
		{
			$SysInfo_NetworkSpeedTestBtn.html(SPINNER);
		}

		var savedDate = Number(Util.getFromLocalStorage(NETWORK_SPEED_TEST_DATE_LS_KEY));
		if (savedDate)
		{
			renderNetworkSpeedTestBtnTitle(savedDate);
		}

		$SysInfo_NetworkSpeedTestBtn.off('click').on('click', function()
		{
			$SysInfo_NetworkSpeedTestBtn.html(SPINNER);
			$SysInfo_NetworkSpeedTestBtn.addClass('btn--disabled');
			$SysInfo_NetworkSpeedTestErrorTxt.empty();
			NETWORK_SPEED_TEST_RUNNING = true;

			RPC.call('testnetworkspeed', [], 
				function(rawRes) 
				{
					var date = Date.now();
					Util.saveToLocalStorage(NETWORK_SPEED_TEST_LS_KEY, rawRes.SpeedMbps);
					Util.saveToLocalStorage(NETWORK_SPEED_TEST_DATE_LS_KEY, date);
					$SysInfo_NetworkSpeedTestBtn
						.html(Util.formatNetworkSpeed(rawRes.SpeedMbps))
						.removeClass('btn--disabled');
					renderNetworkSpeedTestBtnTitle(date);
					NETWORK_SPEED_TEST_RUNNING = false;
				}, 
				function(res) 
				{
					$SysInfo_NetworkSpeedTestBtn
						.attr('data-i18n', 'sysinfo_run_test')
						.text('')
						.removeClass('btn--disabled');
					I18n.translatePage($SysInfo_NetworkSpeedTestBtn);
					removeNetworkSpeedTestBtnTitle();
					var errTxt = res.split('<br>')[0];
					$SysInfo_NetworkSpeedTestErrorTxt.html(errTxt);
					NETWORK_SPEED_TEST_RUNNING = false;
				},
			);
		});
	}

	function renderNetworkSpeedTestBtnTitle(date)
	{
		var formatted = Util.formatDateTime(date / 1000);
		if (formatted)
		{
			var datePrefix = I18n.translate('sysinfo_stat_date');
			$SysInfo_NetworkSpeedTestBtn.attr('title', datePrefix + ': ' + formatted);
		}
	}

	function removeNetworkSpeedTestBtnTitle()
	{
		$SysInfo_NetworkSpeedTestBtn.removeAttr('title');
	}

	function renderNetworkSpeedTestResults(results)
	{
		var formatted = Util.formatNetworkSpeed(results);
		if (formatted)
		{
			$SysInfo_NetworkSpeedTestBtn.text(formatted);
		}
	}

	function renderAppVersion(version)
	{
		$SysInfo_AppVersion.text(version);
		var updateBtn = $('<button type="button" class="btn btn-default" data-i18n="btn_updates"></button>');
		updateBtn.css('margin-left', '5px');
		updateBtn.on('click', Config.checkUpdates);
		$SysInfo_AppVersion.append(updateBtn);
		I18n.translatePage($SysInfo_AppVersion);
	}

	function renderTools(tools)
	{
		tools.forEach(function(tool)
			{
				var tr = $('<tr>');
				var tdName = $('<td>');
				var tdVersion = $('<td>');
				var tdPath = $('<td>');
				tdPath.addClass('flex-center');
				tdName.text(tool.Name);
				tdVersion.text(tool.Version ? tool.Version : I18n.translate('label_na'));
				tdPath.text(tool.Path ? tool.Path : I18n.translate('label_not_found'));
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
				var divName = $('<div>');
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

				divName.text(server.host + ':' + server.port + '(' + server.connections + ')');
				divName.attr({ title: server.name });
				divName
					.addClass('overflow-auto')
					.addClass('no-wrap')
					.css('line-height', '30px')
				tdName.append(divName);

				if (newsServer.Active) 
				{
					tdActive.text(I18n.translate('label_yes_cap'));
					tdActive.css('color', '#468847');
				}
				else 
				{
					tdActive.text(I18n.translate('label_no_cap'));
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
				I18n.translatePage(tr);
			}
		);
	}

	function makeLastTestStatsBtnPlaceholder(id)
	{
		var btn = $('<button id="' 
			+ id + 
			'type="button" class="btn btn-default" data-toggle="modal" data-target="#SpeedTest_Stats">' 
			+ '</>'
		);
		btn.css('display', 'none');
		return btn;
	}

	function makeTestServerSpinnerPlaceholder(id)
	{
		var spinner = $('<i id="' + id + '" class="material-icon spinner">progress_activity</i>');
		spinner.css('display', 'none');
		return spinner;
	}

	function makeTestConnectionBtn(serverid)
	{
		var testConnectionBtn = $('<button type="button" class="btn btn-default" data-i18n="btn_connection"></button>');

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
		
		var testBtn = $('<button class="btn btn-default dropdown-toggle" data-toggle="dropdown"><span data-i18n="sysinfo_speed"></span></button>');

		testBtn.append(caret);
		container.append(testBtn);

		return container;
	}

	function makeListOfTestNZB(serverid)
	{
		var list = $('<ul class="test-server-dropdwon-menu dropdown-menu" id="' + testNZBListId + serverid + '"></ul>');
		getTestNZBFiles().forEach(function(item)
		{
			var fullName = nzbFileTestPrefix + item.name + '.nzb';
			var li = $('<li></>');
			li.css('display', 'flex');
			li.css('align-items', 'center');
			li.css('padding', '1px 0');
			var btn = $('<a href="#">' + item.label + '</a>').on('click', function(e) 
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
		var speedEl = makeSpeed(stats);
		var statsBtn = $('<button '
			+ 'type="button" class="btn btn-default"'
			+ 'data-toggle="modal" data-target="#SpeedTest_Stats">' 
			+ '</>'
		);

		statsBtn.append(speedEl);

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

	function showDiskSpeedModal(writeBuffer, dirPath, label, lsKey)
	{
		$DiskSpeedTest_Modal.show();
		(new DiskSpeedTestsForm()).init(writeBuffer, dirPath, label, lsKey);
	}

	function showStatsTable(stats)
	{
		$SpeedTest_Stats.show();
		$SpeedTest_StatsHeader.text(stats.NZBName);
		fillStatsTable(stats);
	}

	function fillStatsTable(stats)
	{
		var speed = makeSpeed(stats);
		var size = Util.formatSizeMB(stats.DownloadedSizeMB, stats.DownloadedSizeLo);
		var timeHMS = Util.formatTimeHMS(stats.DownloadTimeSec);
		var date = Util.formatDateTime(stats.HistoryTime + UISettings.timeZoneCorrection * 60 * 60);

		$SpeedTest_StatsSpeed.empty();
		$SpeedTest_StatsSpeed.append(speed);
		$SpeedTest_StatsSize.text(size);
		$SpeedTest_StatsTime.text(timeHMS);
		$SpeedTest_StatsDate.text(date);
	}

	function makeSpeed(stats)
	{
		var bytes = stats.DownloadedSizeMB > 1024 ? stats.DownloadedSizeMB * 1024.0 * 1024.0 : stats.DownloadedSizeLo;
		var bytesPerSec = bytes / stats.DownloadTimeSec;
		var speedBytes = stats.DownloadTimeSec > 0 ? Util.formatSpeed(bytesPerSec) : '--';
		var speedContainer = $('<span></span>');
		
		speedContainer.text(speedBytes);
		return speedContainer;
	}

}(jQuery))
