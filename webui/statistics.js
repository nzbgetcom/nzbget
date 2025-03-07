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
	var mouseOverIndex = -1;

	var optionsHandler = {
		update: function () {
			if (optionsLoaded)
				return;

			optionsLoaded = true;
			RPC.call('servervolumes', [], fistServervolumesLoaded);
		}
	}
	var historyHandler =
	{
		update: function (data) {
			if (historyLen === data.length || (!optionsLoaded || !serverVolumesLoaded))
				return;

			$StatisticsTable.empty();
			serverStats = {};
			historyLen = data.length;

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

			$StatisticsSpinner.hide();
			$StatisticsTable.show();

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
	}

	function renderServers(servers) {
		for (var key in servers) {
			if (Object.prototype.hasOwnProperty.call(servers, key)) {
				renderServer(servers[key]);
			}
		}
	}

	function renderServer(server) {
		var $serverDetailHtml = $(makeServerDetails(server));
		var $statsChartHtml = makeStatsChart(server);

		var $container = $('<div>', { class: 'flex-center' }).css('flex-wrap', 'wrap');
		$serverDetailHtml.css('flex-grow', '1');
		$statsChartHtml.css('flex-grow', '3');

		$container.append($serverDetailHtml, $statsChartHtml);
		$StatisticsTable.append($container, '<hr>');
	}

	function makeServerDetails(serverData) {
		var successSum = serverData.successArticles.reduce((a, b) => a + b, 0);
		var failedSum = serverData.failedArticles.reduce((a, b) => a + b, 0);
		var completion = successSum + failedSum > 0 ? Util.round0(successSum * 100.0 / (successSum + failedSum)) + '%' : '--';
		if (failedSum > 0 && completion === '100%') {
			completion = '99.9%';
		}

		var html = '<table class="statistics__server-details table table-condensed table-bordered table-fixed">';
		html += '<tr><td><h4>Name:</h4></th><td>' + serverData.name + '</td></tr>';
		html += '<tr><td><h4>Host:</h4></td><td>' + serverData.host + '</td></tr>';
		html += '<tr><td><h4>Connections:</h4></td><td>' + serverData.connections + '</td></tr>';
		html += '<tr><td><h4>Success Articles:</h4></td><td>' + Util.formatNumber(successSum) + '</td></tr>';
		html += '<tr><td><h4>Failed Articles:</h4></td><td>' + Util.formatNumber(failedSum) + '</td></tr>';
		html += '<tr><td><h4>Total downloaded:</h4></td><td>' + Util.formatSizeMB(serverData.totalSizeMB) + '</td></tr>';
		html += '<tr><td><h4>Completion:</h4></td><td>' + completion + '</td></tr>';
		html += '</table>';

		return html;
	}

	function makeStatsChart(server) {
		var $container = $('<div>', {
			class: 'statistics',
			id: `${server.id}_VolumesTab`
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
			title: 'Show last 5 minutes',
			text: '5 Minutes'
		}).on('click', function () { chooseRange(server, 'MIN') });

		var $hourButton = $('<button>', {
			class: 'btn btn-default volume-range',
			id: `${server.id}_Volume_HOUR`,
			title: 'Show last hour',
			text: '1 Hour'
		}).on('click', function () { chooseRange(server, 'HOUR') });

		var $dayButton = $('<button>', {
			class: 'btn btn-default volume-range',
			id: `${server.id}_Volume_DAY`,
			title: 'Show last 24 hours',
			text: '24 Hours'
		}).on('click', function () { chooseRange(server, 'DAY') });


		$timeBlockTop.append($minButton, $hourButton, $dayButton);
		$toolbar.append($timeBlockTop);

		var $phoneButtons = $('<div>', { class: 'btn-group phone-only inline' }).append(
			$('<button>', {
				class: 'btn btn-default btn-active volume-range',
				id: `${server.id}_Volume_MIN2`,
				title: 'Show last 5 minutes',
				text: '5 m'
			}).on('click', function () { chooseRange(server, 'MIN') }),
			$('<button>', {
				class: 'btn btn-default volume-range',
				id: `${server.id}_Volume_HOUR2`,
				title: 'Show last hour',
				text: '1 h'
			}).on('click', function () { chooseRange(server, 'HOUR') }),
			$('<button>', {
				class: 'btn btn-default volume-range',
				id: `${server.id}_Volume_DAY2`,
				title: 'Show last 24 hours',
				text: '24 h'
			}).on('click', function () { chooseRange(server, 'DAY') }),
		);

		$toolbar.append($phoneButtons);

		var $tooltip = $('<div>', {
			class: 'statistics__tooltip',
			id: `${server.id}_Tooltip`,
			text: 'Total'
		});

		var $chartBlock = $('<div>', {
			class: 'statistics__chartblock',
			id: `${server.id}_ChartBlock`
		});

		$container.append($toolbar, $tooltip, $chartBlock);

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

	function size64(size) {
		return size.SizeMB < 2000 ? size.SizeLo / 1024.0 / 1024.0 : size.SizeMB;
	}

	function redrawChart(server) {
		if (!server)
			return;

		var serverNo = server.id;
		var curRange = server.chartData.range;

		var lineLabels = [];
		var dataLabels = [];
		var chartDataTB = [];
		var chartDataGB = [];
		var chartDataMB = [];
		var chartDataKB = [];
		var chartDataB = [];
		var curPoint = null;
		var sumMB = 0;
		var sumLo = 0;
		var maxSizeMB = 0;
		var maxSizeLo = 0;

		function addData(bytes, dataLab, lineLab) {
			dataLabels.push(dataLab);
			lineLabels.push(lineLab);

			if (bytes === null) {
				chartDataTB.push(null);
				chartDataGB.push(null);
				chartDataMB.push(null);
				chartDataKB.push(null);
				chartDataB.push(null);
				return;
			}
			chartDataTB.push(bytes.SizeMB / 1024.0 / 1024.0);
			chartDataGB.push(bytes.SizeMB / 1024.0);
			chartDataMB.push(size64(bytes));
			chartDataKB.push(bytes.SizeLo / 1024.0);
			chartDataB.push(bytes.SizeLo);
			if (bytes.SizeMB > maxSizeMB) {
				maxSizeMB = bytes.SizeMB;
			}
			if (bytes.SizeLo > maxSizeLo) {
				maxSizeLo = bytes.SizeLo;
			}
			sumMB += bytes.SizeMB;
			sumLo += bytes.SizeLo;
		}

		function drawFiveMinuteGraph() {
			// the current slot may be not fully filled yet,
			// to make the chart smoother for current slot we use the data from the previous reading
			// and we show the previous slot as current.
			curPoint = servervolumes[serverNo].SecSlot;
			for (var i = 0; i < 60; i++) {
				addData((i == curPoint && prevServervolumes !== null ? prevServervolumes : servervolumes)[serverNo].BytesPerSeconds[i],
					i + 's', i % 10 == 0 || i == 59 ? i : '');
			}
			if (prevServervolumes !== null) {
				curPoint = curPoint > 0 ? curPoint - 1 : 59;
			}
		}

		function drawHourGraph() {
			for (var i = 0; i < 60; i++) {
				addData(servervolumes[serverNo].BytesPerMinutes[i],
					i + 'm', i % 10 == 0 || i == 59 ? i : '');
			}
			curPoint = servervolumes[serverNo].MinSlot;
		}

		function drawDayGraph() {
			for (var i = 0; i < 24; i++) {
				addData(servervolumes[serverNo].BytesPerHours[i],
					i + 'h', i % 3 == 0 || i == 23 ? i : '');
			}
			curPoint = servervolumes[serverNo].HourSlot;
		}

		if (curRange === 'MIN') {
			drawFiveMinuteGraph();
		}
		else if (curRange === 'HOUR') {
			drawHourGraph();
		}
		else if (curRange === 'DAY') {
			drawDayGraph();
		}

		var serieData = maxSizeMB > 1024 * 1024 ? chartDataTB :
			maxSizeMB > 1024 ? chartDataGB :
				maxSizeMB > 1 || maxSizeLo == 0 ? chartDataMB :
					maxSizeLo > 1024 ? chartDataKB : chartDataB;

		var units = maxSizeMB > 1024 * 1024 ? ' TB' :
			maxSizeMB > 1024 ? ' GB' :
				maxSizeMB > 1 || maxSizeLo == 0 ? ' MB' :
					maxSizeLo > 1024 ? ' KB' : ' B';

		var curPointData = [];
		for (var i = 0; i < serieData.length; i++) {
			curPointData.push(i === curPoint ? serieData[i] : null);
		}

		server.chartData = {
			serieData: serieData,
			serieDataMB: chartDataMB,
			serieDataLo: chartDataB,
			sumMB: sumMB,
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
			margins: [10, 15, 20, 60],
			defaultSeries: {
				rounded: 0.5,
				fill: true,
				plotProps: {
					'stroke-width': 3.0
				},
				dot: true,
				dotProps: {
					stroke: '#FFF',
					size: 3.0,
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
				mousearea: {
					type: 'axis',
					onMouseOver: function (env, serie, index, mouseAreaData) {
						chartMouseOver(server, env, serie, index, mouseAreaData);
					},
					onMouseExit: function (env, serie, index, mouseAreaData) {
						chartMouseExit(server, env, serie, index, mouseAreaData);
					},
					onMouseOut: function (env, serie, index, mouseAreaData) {
						chartMouseExit(server, env, serie, index, mouseAreaData);
					},
				},
			}
		});

		simulateMouseEvent(server);
	}

	function chartMouseOver(server, env, serie, index, mouseAreaData) {
		if (mouseOverIndex > -1) {
			var chart = $(`#${server.id}_Chart`);
			if (!chart)
				return;

			var env = chart.data('elycharts_env');
			$.elycharts.mousemanager.onMouseOutArea(env, false, mouseOverIndex, env.mouseAreas[mouseOverIndex]);
		}

		var tooltip = $(`#${server.id}_Tooltip`);
		if (!tooltip)
			return;

		mouseOverIndex = index;
		tooltip.html(server.chartData.dataLabels[index] + ': <span class="stat-size">' +
			Util.formatSizeMB(server.chartData.serieDataMB[index], server.chartData.serieDataLo[index]) + '</span>');
	}

	function chartMouseExit(server, env, serie, index, mouseAreaData) {
		var tooltip = $(`#${server.id}_Tooltip`);
		if (!tooltip)
			return;

		mouseOverIndex = -1;
		var title = server.curRange === 'MIN' ? '5 minutes' :
			server.curRange === 'HOUR' ? '60 minutes' :
				server.curRange === 'DAY' ? '24 hours' : '';

		tooltip.html(title + '<span class="stat-size">' + Util.formatSizeMB(server.chartData.sumMB, server.chartData.sumLo) + '</span>');
	}

	function simulateMouseEvent(server) {
		if (mouseOverIndex > -1) {
			var chart = $(`#${server.id}_Chart`);
			if (!chart)
				return;

			var env = chart.data('elycharts_env');
			$.elycharts.mousemanager.onMouseOverArea(env, false, mouseOverIndex, env.mouseAreas[mouseOverIndex]);
		}
		else {
			chartMouseExit(server)
		}
	}

	function chooseRange(server, range) {
		updateRangeButtons(server, range);
		server.chartData.range = range;
		mouseOverIndex = -1;
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
