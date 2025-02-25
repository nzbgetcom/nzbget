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

	function filterInput() { }
	function filterClear() { }

	function cleanUpServerStats(serverStats) {
		for (var key in serverStats) {
			if (Object.prototype.hasOwnProperty.call(serverStats, key)) {
				serverStats[key].successArticles = [];
				serverStats[key].failedArticles = [];
			}
		}
	}

	function renderServers(servers) {
		for (const key in servers) {
			if (Object.prototype.hasOwnProperty.call(servers, key)) {
				renderServerDetails(servers[key]);
			}
		}
	}

	function renderServerDetails(serverData) {
		var html = '';

		html += '<div class="server-property"><strong>Name:</strong> ' + serverData.name + '</div>';
		html += '<div class="server-property"><strong>Host:</strong> ' + serverData.host + '</div>';
		html += '<div class="server-property"><strong>Connections:</strong> ' + serverData.connections + '</div>';

		var successSum = serverData.successArticles.reduce((a, b) => a + b, 0);
		var failedSum = serverData.failedArticles.reduce((a, b) => a + b, 0);
		var completion = successSum + failedSum > 0 ? Util.round0(successSum * 100.0 / (successSum +  failedSum)) + '%' : '--';
		if (failedSum > 0 && completion === '100%')
		{
			completion = '99.9%';
		}

        var completionHtml = '<h4>Completion: <span class="label label-info">' + completion + '</h4>';

		html += '<div class="flex-center">';
		html += '<strong>Success Articles:</strong> <span class="label label-success">' + Util.formatNumber(successSum) + '</span>';
		html += '<strong>Failed Articles:</strong> <span class="label label-important">' + Util.formatNumber(failedSum) + '</span>';
		html += '</div>';
		html += completionHtml;
		html += '<hr>';

		$StatisticsTable.append(html);
	}

}(jQuery));
