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


var Statistics = (new function ($) {
	'use strict';

	function Server() {
		this.id = 0;
		this.connections = 0;
		this.host = "";
		this.name = "";
		this.port = "";
		this.active = false;
		this.totalSizeMB = 0;
		this.successArticles = [];
		this.failedArticles = [];
		this.chartData = {
			range: "MIN"
		};
	}

	var $StatisticsSpinner;
	var $StatisticsTable;

	var serverStats = {};
	var historyLen = null;
	var optionsLoaded = false;
	var servervolumes = null;
	var prevServervolumes = null;
	var serverVolumesLoaded = false;

	var optionsHandler = {
		update: function () {
			if (optionsLoaded)
				return;

			var id = 1;
			var server = Options.getServerById(id);

			while (server) {
				var newServer = new Server();
				newServer.id = id;
				newServer.connections = server.connections;
				newServer.host = server.host;
				newServer.name = server.name;
				newServer.port = server.port;
				newServer.active = server.active;
				serverStats[newServer.id] = newServer;
				++id;
				server = Options.getServerById(id);
			}

			optionsLoaded = true;
			RPC.call('servervolumes', [], fistServervolumesLoaded);

			renderServers(serverStats);
		}
	}
	var historyHandler =
	{
		update: function (data) {
			if (historyLen === data.length || (!optionsLoaded || !serverVolumesLoaded))
				return;

			historyLen = data.length;

			for (var key in serverStats) {
				if (Object.prototype.hasOwnProperty.call(serverStats, key)) {
					var stats = serverStats[key];
					stats.successArticles = [];
					stats.failedArticles = [];
				}
			}

			data.forEach(el => {
				el.ServerStats.forEach((stats) => {
					if (serverStats[stats.ServerID]) {
						serverStats[stats.ServerID].successArticles.push(stats.SuccessArticles);
						serverStats[stats.ServerID].failedArticles.push(stats.FailedArticles);
					} else {
						var server = new Server();
						server.id = stats.ServerID;
						server.successArticles.push(stats.SuccessArticles);
						server.failedArticles.push(stats.FailedArticles);
						serverStats[server.id] = server;
					}
				});
			});
			for (var key in serverStats) {
				if (Object.prototype.hasOwnProperty.call(serverStats, key)) {
					var stats = serverStats[key];
					var server = Options.getServerById(stats.id)
					if (server) {
						stats.connections = server.connections;
						stats.host = server.host;
						stats.name = server.name;
						stats.port = server.port;
						stats.active = server.active;
					}
				}
			}

			for (var key in serverStats) {
				if (Object.prototype.hasOwnProperty.call(serverStats, key)) {
					var server = serverStats[key];
					if (servervolumes[server.id]) {
						server.totalSizeMB = servervolumes[server.id].TotalSizeMB;
					}
				}
			}

			updateRenderedServers(serverStats);
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

		Options.subscribe(optionsHandler);
		History.subscribe(historyHandler);
	}

	this.update = function () {
		if (optionsLoaded) {
			RPC.call('servervolumes', [], servervolumesLoaded);
		}
	}

	function filterInput() { }
	function filterClear() { }

	function fistServervolumesLoaded(volumes) {
		if (serverVolumesLoaded)
			return;

		prevServervolumes = volumes;
		servervolumes = volumes;
		serverVolumesLoaded = true;

		$StatisticsSpinner.hide();
		$StatisticsTable.show();
	}

	function renderServers(servers) {
		for (var key in servers) {
			if (Object.prototype.hasOwnProperty.call(servers, key)) {
				renderServer(servers[key]);
			}
		}
	}

	function renderServer(server) {
		var $serverDetailHtml = $(makeServerDetailsTemplate(server));
		var $statsChartHtml = $(makeChartTemplate(server));

		var $container = $('<div>', { class: 'flex-center' }).css('flex-wrap', 'wrap');
		$serverDetailHtml.css('flex-grow', '1');
		$statsChartHtml.css('flex-grow', '3');

		$container.append($serverDetailHtml, $statsChartHtml);
		$StatisticsTable.append($container, '<hr>');
	}

	function makeServerDetailsTemplate(server) {
		var html = '<table class="statistics__server-details table table-condensed table-bordered table-fixed">';
		html += `<tr><td><h4>Name:</h4></th><td>${server.name}</td></tr>`;
		html += `<tr><td><h4>Host:</h4></td><td>${server.host}</td></tr>`;
		html += `<tr><td><h4>Active:</h4></td><td>${server.active ? "Yes" : "No"}</td></tr>`;
		html += `<tr><td><h4>Connections:</h4></td><td>${server.connections}</td></tr>`;
		html += `<tr><td><h4>Success Articles:</h4></td><td id="${server.id}_SuccessArticles"></td></tr>`;
		html += `<tr><td><h4>Failed Articles:</h4></td><td id="${server.id}_FailedArticles"></td></tr>`;
		html += `<tr><td><h4>Total downloaded:</h4></td><td id="${server.id}_TotalDownlaoded"></td></tr>`;
		html += `<tr><td><h4>Completion:</h4></td><td id="${server.id}_Completion"></td></tr>`;
		html += '</table>';

		return html;
	}

	function updateRenderedServers(servers) {
		for (var key in servers) {
			if (Object.prototype.hasOwnProperty.call(servers, key)) {
				updateServerDetails(servers[key]);
			}
		}
	}

	function updateServerDetails(server) {
		var successSum = 0;
		var failedSum = 0;
		var len = server.successArticles.length;
		for (var i = 0; i < len; i++) {
			successSum += server.successArticles[i];
			failedSum += server.failedArticles[i];
		}

		var completion = successSum + failedSum > 0 ? Util.round0(successSum * 100.0 / (successSum + failedSum)) + '%' : '--';
		if (failedSum > 0 && completion === '100%') {
			completion = '99.9%';
		}

		$(`#${server.id}_SuccessArticles`).text(Util.formatNumber(successSum));
		$(`#${server.id}_FailedArticles`).text(Util.formatNumber(failedSum));
		$(`#${server.id}_TotalDownlaoded`).text(Util.formatSizeMB(server.totalSizeMB));
		$(`#${server.id}_Completion`).text(completion);
	}

	function makeChartTemplate(server) {
		var $container = $('<div>', {
			class: 'statistics',
		});

		var $toolbar = $('<div>', {
			class: 'btn-toolbar form-inline section-toolbar',
			id: `${server.id}_Toolbar`
		});

		var $timeBlockTop = $('<div>', {
			class: 'btn-group phone-hide',
			id: `${server.id}_TimeBlockTop`
		});

		var $minButton = $('<button>', {
			class: 'btn btn-default btn-active volume-range',
			id: `${server.id}_Volume_MIN`,
			title: 'Show last 60 seconds',
			text: '60 Seconds'
		}).on('click', function () { chooseRange(server, 'MIN') });

		var $5minButton = $('<button>', {
			class: 'btn btn-default volume-range',
			id: `${server.id}_Volume_5MIN`,
			title: 'Show last 5 minutes',
			text: '5 Minutes'
		}).on('click', function () { chooseRange(server, '5MIN') });

		var $hourButton = $('<button>', {
			class: 'btn btn-default volume-range',
			id: `${server.id}_Volume_HOUR`,
			title: 'Show 60 minutes',
			text: '60 Minutes'
		}).on('click', function () { chooseRange(server, 'HOUR') });

		var $dayButton = $('<button>', {
			class: 'btn btn-default volume-range',
			id: `${server.id}_Volume_DAY`,
			title: 'Show last 24 hours',
			text: '24 Hours'
		}).on('click', function () { chooseRange(server, 'DAY') });


		$timeBlockTop.append($minButton, $5minButton, $hourButton, $dayButton);
		$toolbar.append($timeBlockTop);

		var $phoneButtons = $('<div>', { class: 'btn-group phone-only inline' }).append(
			$('<button>', {
				class: 'btn btn-default btn-active volume-range',
				id: `${server.id}_Volume_MIN2`,
				title: 'Show last 60 seconds',
				text: '60 s'
			}).on('click', function () { chooseRange(server, 'MIN') }),
			$('<button>', {
				class: 'btn btn-default volume-range',
				id: `${server.id}_Volume_5MIN2`,
				title: 'Show last 5 minutes',
				text: '5 m'
			}).on('click', function () { chooseRange(server, '5MIN') }),
			$('<button>', {
				class: 'btn btn-default volume-range',
				id: `${server.id}_Volume_HOUR2`,
				title: 'Show last 60 minutes',
				text: '60 m'
			}).on('click', function () { chooseRange(server, 'HOUR') }),
			$('<button>', {
				class: 'btn btn-default volume-range',
				id: `${server.id}_Volume_DAY2`,
				title: 'Show last 24 hours',
				text: '24 h'
			}).on('click', function () { chooseRange(server, 'DAY') }),
		);

		$toolbar.append($phoneButtons);

		var $chartBlock = $('<div>', {
			class: 'statistics__chartblock',
			id: `${server.id}_ChartBlock`
		});

		$container.append($toolbar, $chartBlock);

		return $container;
	}

	function servervolumesLoaded(volumes) {
		prevServervolumes = servervolumes;
		servervolumes = volumes;
		redrawCharts(serverStats);
	}

	function redrawCharts(serverStats) {

		for (var key in serverStats) {
			if (Object.prototype.hasOwnProperty.call(serverStats, key)) {
				var server = serverStats[key];
				redrawChart(server);
			}
		}
	}

	function redrawChart(server) {
		if (!server)
			return;

		var serverNo = server.id;
		var curRange = server.chartData.range;

		var lineLabels = [];
		var dataLabels = [];
		var chartSpeedTb = [];
		var chartSpeedGb = [];
		var chartSpeedMb = [];
		var chartSpeedKb = [];
		var chartSpeedB = [];
		var curPoint = null;
		var sumMb = 0;
		var sumLo = 0;
		var maxSizeMb = 0;
		var maxSizeLo = 0;

		function addData(data, dataLab, lineLab, timeIntervalSeconds) {
			dataLabels.push(dataLab);
			lineLabels.push(lineLab);

			if (data === null) {
				chartDataTB.push(null);
				chartDataGB.push(null);
				chartDataMB.push(null);
				chartDataKB.push(null);
				chartDataB.push(null);
				return;
			}
			var speedMb = (data.SizeMB / timeIntervalSeconds) * 8;
			var speedLoMb = (data.SizeLo / timeIntervalSeconds) * 8;

			chartSpeedTb.push(speedMb / 1024.0 / 1024.0);
			chartSpeedGb.push(speedMb / 1024.0);
			chartSpeedMb.push(speedMb);
			chartSpeedKb.push(speedMb * 1024.0);
			chartSpeedB.push(speedMb * 1024.0 * 1024.0);
			if (speedMb > maxSizeMb) {
				maxSizeMb = speedMb;
			}
			if (speedLoMb > maxSizeLo) {
				maxSizeLo = speedLoMb;
			}
			sumMb += speedMb;
			sumLo += speedLoMb;
		}

		function drawMinuteGraph() {
			// the current slot may be not fully filled yet,
			// to make the chart smoother for current slot we use the data from the previous reading
			// and we show the previous slot as current.
			curPoint = servervolumes[serverNo].SecSlot;
			for (var i = 0; i < 60; i++) {
				addData((i == curPoint && prevServervolumes !== null ? prevServervolumes : servervolumes)[serverNo].BytesPerSeconds[i],
					'', '', 1);
			}
			if (prevServervolumes !== null) {
				curPoint = curPoint > 0 ? curPoint - 1 : 59;
			}
		}

		function drawFiveMinuteGraph() {
			var minSlot = servervolumes[serverNo].MinSlot;
			for (var i = 0; i < 5; i++) {
				var index = minSlot - (minSlot % 5) + i;
				addData(servervolumes[serverNo].BytesPerMinutes[index], '', '', 60);
			}

			curPoint = minSlot % 5;
		}

		function drawHourGraph() {
			for (var i = 0; i < 60; i++) {
				addData(servervolumes[serverNo].BytesPerMinutes[i], '', '', 60);
			}
			curPoint = servervolumes[serverNo].MinSlot;
		}

		function drawDayGraph() {
			for (var i = 0; i < 24; i++) {
				addData(servervolumes[serverNo].BytesPerHours[i], '', '', 3600);
			}
			curPoint = servervolumes[serverNo].HourSlot;
		}

		if (curRange === 'MIN') {
			drawMinuteGraph();
		}
		else if (curRange === '5MIN') {
			drawFiveMinuteGraph();
		}
		else if (curRange === 'HOUR') {
			drawHourGraph();
		}
		else if (curRange === 'DAY') {
			drawDayGraph();
		}

		var serieData = maxSizeMb >= 1024 * 1024 ? chartSpeedTb :
			maxSizeMb >= 1024 ? chartSpeedGb :
				maxSizeMb >= 1 ? chartSpeedMb :
					chartSpeedKb;

		var units = maxSizeMb >= 1024 * 1024 ? ' Tb/s' :
			maxSizeMb >= 1024 ? ' Gb/s' :
				maxSizeMb >= 1 ? ' Mb/s' :
					' Kb/s';

		var curPointData = [];
		for (var i = 0; i < serieData.length; ++i) {
			curPointData.push(i === curPoint ? serieData[i] : null);
		}

		server.chartData = {
			serieData: serieData,
			serieDataMB: chartSpeedMb,
			serieDataLo: chartSpeedB,
			sumMB: sumMb,
			sumLo: sumLo,
			dataLabels: dataLabels,
			range: curRange
		};

		var chartBlock = $(`#${server.id}_ChartBlock`);
		if (!chartBlock)
			return;

		chartBlock.empty();
		chartBlock.html(`<div class="statistics__chart" id="${server.id}_Chart"></div>`);
		$(`#${server.id}_Chart`).chart({
			values: { serie1: serieData, serie2: curPointData },
			labels: lineLabels,
			type: 'line',
			margins: [10, 15, 20, 70],
			defaultSeries: {
				rounded: 0.5,
				fill: true,
				plotProps: {
					'stroke-width': 3.0
				},
				dot: true,
				dotProps: {
					stroke: '#FFF',
					size: 0.0,
					'stroke-width': 1.0,
					fill: '#5AF'
				},
				highlight: {
					scaleSpeed: 0,
					scaleEasing: '>',
					scale: 2.0
				},
				tooltip: {
					active: false,
				},
				color: '#5AF'
			},
			series: {
				serie2: {
					dotProps: {
						stroke: '#F21860',
						fill: '#F21860',
						size: 3.5,
						'stroke-width': 2.5
					},
					highlight: {
						scale: 1.5
					},
				}
			},
			defaultAxis: {
				labels: true,
				labelsProps: {
					'font-size': 13,
					'fill': '#3a87ad'
				},
				labelsDistance: 12
			},
			axis: {
				l: {
					labels: true,
					suffix: units,
				}
			},
			features: {
				grid: {
					draw: [true, false],
					forceBorder: true,
					props: {
						stroke: '#e0e0e0',
						'stroke-width': 1
					},
					ticks: {
						active: [true, false, false],
						size: [6, 0],
						props: {
							stroke: '#e0e0e0'
						}
					}
				},
			}
		});
	}

	function chooseRange(server, range) {
		updateRangeButtons(server, range);
		server.chartData.range = range;
		redrawChart(server);
	}

	function updateRangeButtons(server, range) {
		var id = server.id;
		var prevRange = server.chartData.range;
		var prevTab = $(`#${id}_Volume_${prevRange}, #${id}_Volume_${prevRange}2`);
		var newTab = $(`#${id}_Volume_${range}, #${id}_Volume_${range}2`);
		prevTab.removeClass('btn-active');
		newTab.addClass('btn-active');
	}
}(jQuery));
