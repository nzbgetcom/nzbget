/*
 * This file is part of nzbget. See <https://nzbget.com>.
 *
 * Copyright (C) 2025 Denis <denis@nzbget.com>
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



// var HistoryItem =
// {
// 	HistoryTime: 0,
// 	FileSizeLo: 0,
// 	FileSizeHi: 0,
// 	FileSizeMB: 0,
// 	TotalArticles: 0,
// 	SuccessArticles: 0,
// 	FailedArticles: 0,
// 	Health: 0,
// 	CriticalHealth: 0,
// 	DownloadedSizeLo: 0,
// 	DownloadedSizeHi: 0,
// 	DownloadedSizeMB: 0,
// 	DownloadTimeSec: 0,
// 	ServerStats: [],
// 	sizemb: 0,
// 	sizegb: 0,
// }


var Statistics = (new function ($) {
	'use strict';

	function ServerStats() {
		this.id = 0;
		this.connections = 0;
		this.host = "";
		this.name = "";
		this.port = "";
		this.successArticles = [];
		this.failedArticles = [];
	}

	var optionsAlreadyLoaded = false;
	var $StatisticsSpinner;
	var $StatisticsTable;
	var serverStats = {};
	var dataLen = 0;
	var curMonth = null;
	var monYear = false;

	var historyHandler =
	{
		update: function (data) {
			if (dataLen == data.length && optionsAlreadyLoaded)
				return;

			dataLen = data.length;
			$StatisticsTable.empty();
			serverStats = {};

			data.forEach(el => {
				el.ServerStats.forEach((stats) => {
					if (serverStats[stats.ServerID]) {
						serverStats[stats.ServerID].successArticles.push(stats.SuccessArticles);
						serverStats[stats.ServerID].failedArticles.push(stats.FailedArticles);
					}
					else {
						var serverStatsNew = new ServerStats();
						serverStatsNew.id = stats.ServerID;
						serverStatsNew.successArticles.push(stats.SuccessArticles);
						serverStatsNew.failedArticles.push(stats.FailedArticles);
						serverStats[serverStatsNew.id] = serverStatsNew;
					}
				});
			});
			if (Options.loaded && !optionsAlreadyLoaded) {
				for (var key in serverStats) {
					if (Object.prototype.hasOwnProperty.call(serverStats, key)) {
						var stats = serverStats[key];
						var server = Options.getServerById(stats.id)
						if (server) {
							stats.connections = server.connections;
							stats.host = server.host;
							stats.name = server.name;
							stats.port = server.port;
						}
					}
				}

				$StatisticsSpinner.hide();
				$StatisticsTable.show();
				optionsAlreadyLoaded = true;
			}
			console.log(serverStats)
			renderServers(serverStats);
		}
	}

	this.init = function () {
		$('#StatisticsTable_filter').val('');
		$StatisticsSpinner = $('#Statistics_Spinner');
		$StatisticsTable = $('#Statistics_Table');
		$StatisticsTable.fasttable(
			{
				filterInput: $('#StatisticsTable_filter'),
				filterClearButton: $("#StatisticsTable_clearfilter"),
				filterInputCallback: filterInput,
				filterClearCallback: filterClear
			});

		History.subscribe(historyHandler);
	}

	this.setPeriod = function () {

	}

	function filterInput() { }
	function filterClear() { }

	function renderServers(servers) {
		for (const key in servers) {
			if (Object.prototype.hasOwnProperty.call(servers, key)) {
				renderServer(servers[key])
			}
		}
	}

	function renderServer(serverStats) {
		var serverDetailHtml = $(makeServerDetails(serverStats));
		var statsChartHtml = $(makeStatsChart(serverStats));
		var container = $('<div class="flex-center">');

		container.css('flex-wrap', 'wrap');
		serverDetailHtml.css('flex-grow', '1');
		statsChartHtml.css('flex-grow', '3');

		container.append(serverDetailHtml);
		container.append(statsChartHtml);

		$StatisticsTable.append(container);
		$StatisticsTable.append('<hr>');
	}

	function makeServerDetails(serverData) {
		var successSum = serverData.successArticles.reduce((a, b) => a + b, 0);
		var failedSum = serverData.failedArticles.reduce((a, b) => a + b, 0);
		var completion = successSum + failedSum > 0 ? Util.round0(successSum * 100.0 / (successSum + failedSum)) + '%' : '--';
		if (failedSum > 0 && completion === '100%') {
			completion = '99.9%';
		}

		var html = '<table class="table table-condensed table-bordered table-fixed">';
		html += '<tr><td><h4>Name:</h4></th><td>' + serverData.name + '</td></tr>';
		html += '<tr><td><h4>Host:</h4></td><td>' + serverData.host + '</td></tr>';
		html += '<tr><td><h4>Connections:</h4></td><td>' + serverData.connections + '</td></tr>';
		html += '<tr><td><h4>Success Articles:</h4></td><td>' + Util.formatNumber(successSum) + '</td></tr>';
		html += '<tr><td><h4>Failed Articles:</h4></td><td>' + Util.formatNumber(failedSum) + '</td></tr>';
		html += '<tr><td><h4>Completion:</h4></td><td>' + completion + '</td></tr>';
		html += '</table>';

		return html;
	}

	function makeStatsChart(serverStats) {
		var html = `<div class="" id="StatDialog_VolumesTab">`;
		html += `<div class="btn-toolbar form-inline section-toolbar" id="StatDialog_Toolbar">`;
		html += `<div class="btn-group phone-hide" id="StatDialog_MonthBlockTop">`;
		html += `<button class="btn btn-default btn-active volume-range" id="StatDialog_Volume_MIN" onclick="${chooseRange(serverStats, 'MIN')}" title="Show last 60 seconds">60 seconds</button>`;
		html += `<button class="btn btn-default volume-range" id="StatDialog_Volume_HOUR" onclick="${chooseRange(serverStats, 'HOUR')}" title="Show last 60 minutes">60 minutes</button>`;
		html += `<button class="btn btn-default volume-range" id="StatDialog_Volume_DAY" onclick="${chooseRange(serverStats, 'DAY')}" title="Show last 24 hours">24 hours</button>`;
		html += `<button class="btn btn-default volume-range" id="StatDialog_Volume_MONTH" onclick="${chooseRange(serverStats, 'MONTH')}" title="Show a month or a whole year">January 2000</button>`;
		html += `<button class="btn btn-default volume-range dropdown-toggle" id="StatDialog_Volume_MONTH3" data-toggle="dropdown"><span class="caret"></span></button>`;
		html += `<ul class="dropdown-menu menu-check pull-right" id="StatDialog_MonthMenu">
					<li class="menu-header">Months</li>
					<li class="volume-month-template hide"><a href="#"></a></li>
					<li class="menu-header" id="StatDialog_MonthMenuYears">Years</li>
					<li class="divider" id="StatDialog_MonthMenuDivider"></li>
					<li><a href="#" onclick="${chooseOtherMonth()}"><i class="material-icon"></i>Older Periods...</a></li>
					<li class="volume-menu-template hide"><a href="#"></a></li>
				</ul>`;
		html += `</div>`;

		html += `<div class="btn-group phone-only inline"><button class="btn btn-default btn-active volume-range" id="StatDialog_Volume_MIN2" onclick="${chooseRange(serverStats, 'MIN')}" title="Show last 60 seconds">60 s</button></div>`;
		html += `<div class="btn-group phone-only inline"><button class="btn btn-default volume-range" id="StatDialog_Volume_HOUR2" onclick="${chooseRange(serverStats, 'HOUR')}" title="Show last 60 minutes">60 m</button></div>`;
		html += `<div class="btn-group phone-only inline"><button class="btn btn-default volume-range" id="StatDialog_Volume_DAY2" onclick="${chooseRange(serverStats, 'DAY')}" title="Show last 24 hours">24 h</button></div>`;
		html += `<div class="btn-group phone-only inline"><button class="btn btn-default volume-range" id="StatDialog_Volume_MONTH2" onclick="${chooseRange(serverStats, 'MONTH')}" title="Show one month">Jan 2000</button></div>`;
		html += `</div>`

		html += `<div id="StatDialog_Tooltip">Total</div>`;
		html += `<div id="StatDialog_ChartBlock"></div>`;

		html += `<hr>`;
		html += `<div id="StatDialog_CountersBlock">`;
		html += `<div class="row-fluid" id="StatDialog_Counters">`;
		html += `<div class="span3" id="StatDialog_Today">Today: <span id="StatDialog_TodaySize" class="stat-size">1.5 GB</span></div>`;
		html += `<div class="span3" id="StatDialog_Month"><span id="StatDialog_MonthTitle">This month:</span> <span id="StatDialog_MonthSize" class="stat-size">200 GB</span></div>`;
		html += `<div class="span3" id="StatDialog_AllTime">All-time: <span id="StatDialog_AllTimeSize" class="stat-size">20.3 TB</span></div>`;
		html += `<div class="span3" id="StatDialog_Custom" title="reset on Fri Apr 04 2014 09:32:24">Custom: <span id="StatDialog_CustomSize" class="stat-size">10.3 TB</span>, <a onclick="StatDialog.resetCounter()"><i class="material-icon">restart_alt</i></a></div>`;
		html += `</div>`;
		html += `</div>`;
		html += `</div>`;

		return html;


		// const container = $('#StatDialog_TimeRangeControls');

		// // Delegated event handler for all volume-range buttons
		// container.on('click', '.volume-range', function () {
		// 	const range = $(this).data('range');
		// 	// Remove active class from all buttons in the container
		// 	container.find('.volume-range').removeClass('btn-active');
		// 	// Add active class to the clicked button
		// 	$(this).addClass('btn-active');
		// 	${ serverStats.name }.chooseRange(range);
		// });

		// // Delegated event handler for the "Older Periods..." link
		// container.on('click', '#chooseOtherMonthLink', function (e) {
		// 	e.preventDefault(); // Prevent the link from navigating
		// 	${ serverStats.name }.chooseOtherMonth();
		// });

		// // Initial setup to handle default active button (if needed)
		// // Example: If you want '60 seconds' to be active on page load
		// container.find('[data-range="MIN"]').addClass('btn-active');

	}

	function chooseRange(serverStats, range) {
		console.log(serverStats, range)
	}

	function chooseOtherMonth(serverStats) {
		console.log(serverStats)
	}

	function setMonth(month)
	{
		curRange = 'MONTH';
		curMonth = month;
		updateRangeButtons();
		updateMonthList();
		redrawChart();
	}

}(jQuery));
