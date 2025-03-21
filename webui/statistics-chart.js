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


var StatisticsChart = (new function ($) {
	'use strict';


	function size64(size) {
		return size.SizeMB < 2000 ? size.SizeLo / 1024.0 / 1024.0 : size.SizeMB;
	}

	function redrawChart(server) {
		if (!server)
			return;

		var serverData = server.getChartData();
		var serverNo = server.id;
		var curRange = serverData.range;

		var lineLabels = [];
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

		function addData(data, timeIntervalSeconds) {
			if (data === null) {
				chartDataTB.push(null);
				chartDataGB.push(null);
				chartDataMB.push(null);
				chartDataKB.push(null);
				chartDataB.push(null);
				return;
			}
			var sizeMB = size64(data);
			var speedMb = (sizeMB / timeIntervalSeconds) * 8;
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
		function rerangeCircularBuffer(buffer, currentSlot) {
			const rearranged = [];

			for (var i = currentSlot + 1; i < buffer.length; i++) {
				rearranged.push(buffer[i]);
			}

			for (var i = 0; i <= currentSlot; i++) {
				rearranged.push(buffer[i]);
			}

			return rearranged;
		}

		function drawMinuteGraph() {
			var buffer = rerangeCircularBuffer(
				servervolumes[serverNo].BytesPerSeconds,
				servervolumes[serverNo].SecSlot
			);
			for (var i = 0; i < buffer.length; i++) {
				addData(buffer[i], 1);
			}

			curPoint = 59;
		}

		function drawFiveMinuteGraph() {
			var buffer = rerangeCircularBuffer(
				servervolumes[serverNo].BytesPerMinutes,
				servervolumes[serverNo].MinSlot
			);

			for (var i = 55; i < buffer.length; i++) {
				addData(buffer[i], 60);
			}

			curPoint = 4;
		}

		function drawHourGraph() {
			var buffer = rerangeCircularBuffer(
				servervolumes[serverNo].BytesPerMinutes,
				servervolumes[serverNo].MinSlot
			);
			for (var i = 0; i < buffer.length; i++) {
				addData(buffer[i], 60);
			}
			curPoint = 59;
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
		else if (curRange === 'MONTH') {
			drawMinuteGraph();
		}

		var serieData = maxSizeMb >= 1024 * 1024 ? chartSpeedTb :
			maxSizeMb >= 1024 ? chartSpeedGb :
				maxSizeMb >= 1 ? chartSpeedMb : chartSpeedKb;

		var units = maxSizeMb >= 1024 * 1024 ? ' Tbit/s' :
			maxSizeMb >= 1024 ? ' Gbit/s' :
				maxSizeMb >= 1 ? ' Mbit/s' : ' Kbit/s';

		var curPointData = [];
		for (var i = 0; i < serieData.length; ++i) {
			curPointData.push(i === curPoint ? serieData[i] : null);
		}

		server.setChartData({
			data: serieData,
			currPoint: serieData[curPoint],
			range: curRange,
			units: units,
		});

		server.hideSpinner();
		server.showChart();

		var chartBlock = $(`#${server.id}_ChartBlock-${server.activeChart}`);
		if (!chartBlock)
			return;

		chartBlock.empty();
		chartBlock.html(`<div class="statistics__chart" id="${server.id}_Chart-${server.activeChart}"></div>`);
		var $chart = $(`#${server.id}_Chart-${server.activeChart}`);
		$chart.chart({
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
				// mousearea: {
				// 	type: 'axis',
				// 	onMouseOver: function (env, serie, index, mouseAreaData) {
				// 		chartMouseOver(server, env, serie, index, mouseAreaData);
				// 	},
				// 	onMouseExit: function (env, serie, index, mouseAreaData) {
				// 		chartMouseExit(server, env, serie, index, mouseAreaData);
				// 	},
				// 	onMouseOut: function (env, serie, index, mouseAreaData) {
				// 		chartMouseExit(server, env, serie, index, mouseAreaData);
				// 	},
				// },
			},
		});

		//simulateMouseEvent(server);
	}

	function chartMouseOver(server, env, serie, index, mouseAreaData) {
		if (index === undefined)
			return;

		if (mouseOverIndex > -1) {
			var chart = $(`#${server.id}_Chart-${server.activeChart}`);
			if (!chart)
				return;

			var env = chart.data('elycharts_env');
			$.elycharts.mousemanager.onMouseOutArea(env, false, mouseOverIndex, env.mouseAreas[mouseOverIndex]);
		}

		var tooltip = $(`#${server.id}_Tooltip-${server.activeChart}`);
		if (!tooltip)
			return;

		mouseOverIndex = index;
		var data = server.getChartData();
		var title = data.units + " " + data.data[index].toFixed(1);
		tooltip.html('<span class="stat-size">' + title + '</span>');
	}

	function chartMouseExit(server, env, serie, index, mouseAreaData) {
		if (index === undefined)
			return;

		var tooltip = $(`#${server.id}_Tooltip-${server.activeChart}`);
		if (!tooltip)
			return;

		mouseOverIndex = -1;
		var data = server.getChartData();
		var title = data.units + " " + data.data[index].toFixed(1);
		tooltip.html('<span class="stat-size">' + title + '</span>');
	}

	function simulateMouseEvent(server) {
		if (mouseOverIndex > -1) {
			var chart = $(`#${server.id}_Chart-${server.activeChart}`);
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
		updateRangeButtons(server, range, server.getChartData().range);
		server.getChartData().range = range;
		mouseOverIndex = -1;
		redrawChart(server);
	}

	function updateRangeButtons(server, range, prevRange) {
		var id = server.id;
		var chartId = server.activeChart;
		var prevTab = $(`#${id}_Volume_${prevRange}-${chartId}, #${id}_Volume_${prevRange}2-${chartId}`);
		var newTab = $(`#${id}_Volume_${range}-${chartId}, #${id}_Volume_${range}2-${chartId}`);
		prevTab.removeClass('btn-active');
		newTab.addClass('btn-active');
	}
}(jQuery));
