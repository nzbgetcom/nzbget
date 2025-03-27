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


var DateHelper = (function () {
	function DateHelper() {
		this.now = new Date();
		this.startDate = new Date(this.now.getFullYear(), this.now.getMonth(), 1);
		this.endDate =  new Date(this.now.getFullYear(), this.now.getMonth() + 1, 0);
		var referenceDateStart = new Date(this.now.getFullYear(), 0, 1);
		this.startDay = Math.floor((this.startDate - referenceDateStart) / (1000 * 60 * 60 * 24));
		var referenceDateEnd = new Date(this.now.getFullYear(), 0, 1);
		this.endDay = Math.floor((this.endDate - referenceDateEnd) / (1000 * 60 * 60 * 24));
		this.startDateString = this.formatDateForInput(this.startDate);
		this.endDateString = this.formatDateForInput(this.endDate);
	}

	DateHelper.prototype.getStartDay = function () {
		return this.startDay;
	};

	DateHelper.prototype.getEndDay = function () {
		return this.endDay;
	};

	DateHelper.prototype.getStartDate = function () {
		return this.startDate;
	};

	DateHelper.prototype.getEndDate = function () {
		return this.endDate;
	};

	DateHelper.prototype.getStartDateString = function () {
		return this.startDateString;
	};

	DateHelper.prototype.getEndDateString = function () {
		return this.endDateString;
	};

	DateHelper.prototype.formatDateForInput = function (date) {
		date.setTime(date.getTime() - (date.getTimezoneOffset() * 60000));
		var dateAsString =  date.toISOString().substr(0, 19);
		return dateAsString.slice(0, 10);
	};

	DateHelper.prototype.setStartDate = function (dateString) {
		this.startDate = new Date(dateString);
		this.startDateString = dateString;
	};

	DateHelper.prototype.setEndDate = function (dateString) {
		this.endDate = new Date(dateString);
		this.endDateString = dateString;
	};

	return DateHelper;
})();

var Statistics = (new function ($) {
	'use strict';

	function Server() {
		this.DOWNLOAD_SPEED_CHART = 0;
		this.DOWMLOADED_VOLUME_CHART = 1;

		this.dateHelper = new DateHelper();
		this.id = 0;
		this.connections = 0;
		this.host = "";
		this.name = "";
		this.port = "";
		this.active = false;
		this.totalSizeMB = 0;
		this.totalSizeLo = 0;
		this.successArticles = [];
		this.failedArticles = [];

		this.$details = null;
		this.$downloadSpeedChart = null;
		this.$downloadVolumeChart = null;
		this.$spinner = null;
		this.$chartToggleBtn = null;
		this.$speedChartBtn = null;
		this.$volumeChartBtn = null;

		this.activeChart = this.DOWNLOAD_SPEED_CHART;
		this.monthListInitialized = false;

		this.speedChartData = {
			range: "MIN",
			data: null,
			dataMB: null,
			curPoint: null,
			units: null,
			labels: null,
			mouseOverIndex: -1,
		};

		this.volumeChartData = {
			range: "MONTH",
			data: null,
			dataMB: null,
			dataLo: null,
			units: null,
			curPoint: null,
			sumMB: null,
			sumLo: null,
			labels: null,
			mouseOverIndex: -1,
		};

		this.getChartData = function () {
			if (this.activeChart === this.DOWNLOAD_SPEED_CHART) {
				return this.speedChartData;
			}

			return this.volumeChartData;
		}

		this.setChartData = function (data) {
			if (this.activeChart === this.DOWNLOAD_SPEED_CHART) {
				var mouseOverIndex = this.speedChartData.mouseOverIndex;
				this.speedChartData = data;
				this.speedChartData.mouseOverIndex = mouseOverIndex;
				return;
			}

			var mouseOverIndex = this.volumeChartData.mouseOverIndex;
			this.volumeChartData = data;
			this.volumeChartData.mouseOverIndex = mouseOverIndex;
		}

		this.showChart = function () {
			if (this.activeChart === this.DOWNLOAD_SPEED_CHART) {
				this.$downloadSpeedChart.show();
				this.$downloadVolumeChart.hide();
			}
			else {
				this.$downloadSpeedChart.hide();
				this.$downloadVolumeChart.show();
			}
		}

		this.showDetails = function() {
			this.$details.show();
		}

		this.hideSpinner = function () {
			this.$spinner.hide();
		}

		this.showSpinner = function () {
			this.$spinner.show();
		}

		this.toggleChart = function () {
			if (this.activeChart === this.DOWNLOAD_SPEED_CHART) {

				this.activeChart = this.DOWMLOADED_VOLUME_CHART;

				this.$downloadSpeedChart.hide();
				this.$downloadVolumeChart.show();

				this.$speedChartBtn.removeClass('btn-active');
				this.$volumeChartBtn.addClass('btn-active');
			}
			else {
				this.activeChart = this.DOWNLOAD_SPEED_CHART;

				this.$downloadSpeedChart.hide();
				this.$downloadVolumeChart.show();

				this.$speedChartBtn.addClass('btn-active');
				this.$volumeChartBtn.removeClass('btn-active');
			}
		}
	}

	var IS_MOBILE = window.innerWidth < 560;

	var $StatisticsSpinner;
	var $StatisticsTable;

	var serverStats = {};
	var historyLen = null;
	var optionsLoaded = false;
	var servervolumes = null;
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

			clearArticles(serverStats);

			data.forEach(el => {
				el.ServerStats.forEach((stats) => {
					if (serverStats[stats.ServerID]) {
						serverStats[stats.ServerID].successArticles.push(stats.SuccessArticles);
						serverStats[stats.ServerID].failedArticles.push(stats.FailedArticles);
					}
				});
			});
			

			updateServersDetails(serverStats);
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

	function clearArticles(serverStats) {
		for (var key in serverStats) {
			if (Object.prototype.hasOwnProperty.call(serverStats, key)) {
				var stats = serverStats[key];
				stats.successArticles = [];
				stats.failedArticles = [];
			}
		}
	}

	function fistServervolumesLoaded(volumes) {
		if (serverVolumesLoaded)
			return;

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
		var $container = $('<div>', { class: 'flex-center' })
			.css('flex-wrap', 'wrap')
			.css('min-height', '300px');

		server.$details = makeServerDetails(server);
		server.$spinner = makeSpinner(server);
		server.$downloadSpeedChart = makeDownloadSpeedChart(server);
		server.$downloadVolumeChart = makeDownloadVolumeChart(server);

		server.$details.css('flex-grow', '1').css("flex-basis", "400px");
		server.$spinner.css('flex-grow', '3').css("flex-basis", "600px");
		server.$downloadSpeedChart.css('flex-grow', '3')
			.css("flex-basis", "600px")
			.css('overflow', 'hidden');
		server.$downloadVolumeChart.css('flex-grow', '3')
			.css("flex-basis", "600px")
			.css('overflow', 'hidden');

		server.$details.hide();
		server.$downloadSpeedChart.hide();
		server.$downloadVolumeChart.hide();

		$container.append(server.$details, server.$spinner, server.$downloadSpeedChart, server.$downloadVolumeChart);
		$StatisticsTable.append($container, '<hr>');
	}

	function makeServerDetails(server) {
		var html = '<table class="statistics__server-details table table-condensed table-bordered table-fixed">';
		html += `<tr><td><h4>Name:</h4></th><td>${server.name}</td></tr>`;
		html += `<tr><td><h4>Host:</h4></td><td>${server.host}</td></tr>`;
		html += `<tr><td><h4>Active:</h4></td><td>${server.active ? "Yes" : "No"}</td></tr>`;
		html += `<tr><td><h4>Connections:</h4></td><td>${server.connections}</td></tr>`;
		html += `<tr><td><h4>Articles Success/Failed:</h4></td><td id="${server.id}_Articles-${server.activeChart}"></td></tr>`;
		html += `<tr><td><h4>Total downloaded:</h4></td><td id="${server.id}_TotalDownloaded-${server.activeChart}"></td></tr>`;
		html += `<tr><td><h4>Completion:</h4></td><td id="${server.id}_Completion-${server.activeChart}"></td></tr>`;
		html += '</table>';

		return $(html);
	}

	function updateServersDetails(servers) {
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

		if (servervolumes[server.id]) {
			server.totalSizeMB = servervolumes[server.id].TotalSizeMB;
			server.totalSizeLo = servervolumes[server.id].TotalSizeLo;
		}

		for (var i = 0; i < len; i++) {
			successSum += server.successArticles[i];
			failedSum += server.failedArticles[i];
		}

		var completion = successSum + failedSum > 0 ? Util.round0(successSum * 100.0 / (successSum + failedSum)) + '%' : '--';
		if (failedSum > 0 && completion === '100%') {
			completion = '99.9%';
		}

		var $successArticles = $(`<span>${Util.formatNumber(successSum)}</span>`);
		$successArticles.addClass('txt-success');
		var $failedArticles = $(`<span>${Util.formatNumber(failedSum)}</span>`);
		$failedArticles.addClass('txt-important');
		var $articles = $(`#${server.id}_Articles-${server.activeChart}`);
		$articles.empty();
		$articles.append($successArticles, " / ", $failedArticles);

		$(`#${server.id}_FailedArticles-${server.activeChart}`).text();
		$(`#${server.id}_TotalDownloaded-${server.activeChart}`).text(Util.formatSizeMB(server.totalSizeMB));
		$(`#${server.id}_Completion-${server.activeChart}`).text(completion);

		server.showDetails();
	}

	function makeSpinner(server) {
		var html = '<div class="statistics__chart-block-spinner">';
		html += `<i class="statistics__chart-block-spinner material-icon spinner"`;
		html += `id="${server.id}_ChartSpinner-${server.activeChart}">progress_activity</i>`;
		html += '</div>';

		return $(html);
	}

	function makeToggleChartBtn(server) {
		var $container = $('<div>', {
			class: 'btn-group',
		})
			.css("padding-bottom", "5px");

		var $speedChartBtn = $('<button>', {
			class: 'btn btn-default btn-active',
			title: 'Show the Download Speed chart',
			text: 'Speed'
		}).on('click', function () {
			server.toggleChart();
			redrawChart(server);
		});


		var $volumeChartBtn = $('<button>', {
			class: 'btn btn-default',
			title: 'Show the Downloaded Volume chart',
			text: 'Volume'
		}).on('click', function () {
			server.toggleChart();
			redrawChart(server);
		});

		server.$speedChartBtn = $speedChartBtn;
		server.$volumeChartBtn = $volumeChartBtn;

		$container.append($speedChartBtn, $volumeChartBtn);

		return $container;
	}

	function makeDownloadSpeedChart(server) {
		var $container = $('<div>', {
			class: 'statistics',
		});

		var $title = makeToggleChartBtn(server);

		var $toolbar = $('<div>', {
			class: 'btn-toolbar form-inline section-toolbar',
			id: `${server.id}_Toolbar-${server.DOWNLOAD_SPEED_CHART}`
		}).css('margin-bottom', '0');

		var $timeBlockTop = $('<div>', {
			class: 'btn-group phone-hide',
			id: `${server.id}_TimeBlockTop-${server.DOWNLOAD_SPEED_CHART}`
		});

		var $minButton = $('<button>', {
			class: 'btn btn-default btn-active volume-range',
			id: `${server.id}_Volume_MIN-${server.DOWNLOAD_SPEED_CHART}`,
			title: 'Show last 60 seconds',
			text: '60 Seconds'
		}).on('click', function () { chooseRange(server, 'MIN') });

		var $5minButton = $('<button>', {
			class: 'btn btn-default volume-range',
			id: `${server.id}_Volume_5MIN-${server.DOWNLOAD_SPEED_CHART}`,
			title: 'Show last 5 minutes',
			text: '5 Minutes'
		}).on('click', function () { chooseRange(server, '5MIN') });

		var $hourButton = $('<button>', {
			class: 'btn btn-default volume-range',
			id: `${server.id}_Volume_HOUR-${server.DOWNLOAD_SPEED_CHART}`,
			title: 'Show 60 minutes',
			text: '60 Minutes'
		}).on('click', function () { chooseRange(server, 'HOUR') });

		$timeBlockTop.append($minButton, $5minButton, $hourButton);
		$toolbar.append($timeBlockTop);

		var $phoneButtons = $('<div>', { class: 'btn-group phone-only inline' }).append(
			$('<button>', {
				class: 'btn btn-default btn-active volume-range',
				id: `${server.id}_Volume_MIN2-${server.DOWNLOAD_SPEED_CHART}`,
				title: 'Show last 60 seconds',
				text: '60 s'
			}).on('click', function () { chooseRange(server, 'MIN') }),
			$('<button>', {
				class: 'btn btn-default volume-range',
				id: `${server.id}_Volume_5MIN2-${server.DOWNLOAD_SPEED_CHART}`,
				title: 'Show last 5 minutes',
				text: '5 m'
			}).on('click', function () { chooseRange(server, '5MIN') }),
			$('<button>', {
				class: 'btn btn-default volume-range',
				id: `${server.id}_Volume_HOUR2-${server.DOWNLOAD_SPEED_CHART}`,
				title: 'Show last 60 minutes',
				text: '60 m'
			}).on('click', function () { chooseRange(server, 'HOUR') }),
		);

		$toolbar.append($phoneButtons);

		var $tooltip = $('<div>', {
			class: 'statistics__tooltip',
			id: `${server.id}_Tooltip-${server.DOWNLOAD_SPEED_CHART}`,
			text: '--'
		});

		var $chart = $('<div>', {
			class: 'statistics__chartblock',
			id: `${server.id}_ChartBlock-${server.DOWNLOAD_SPEED_CHART}`
		});

		$container.append($title, $toolbar, $tooltip, $chart);

		return $container;
	}

	function makeDownloadVolumeChart(server) {
		var $container = $('<div>', {
			class: 'statistics',
		});

		var $title = makeToggleChartBtn(server);

		var $toolbar = $('<div>', {
			class: 'btn-toolbar form-inline section-toolbar',
			id: `${server.id}_Toolbar-${server.DOWMLOADED_VOLUME_CHART}`
		}).css('margin-bottom', '5px');

		var size = IS_MOBILE ? '120px' : '90px';

		var $startDateInput = $('<input>', {
			type: 'date',
			id: $(`${server.id}_StartDate-${server.DOWNLOADED_VOULUME_CHART}`)
		}).css('width', size);

		var $sep = $('<span> - </span>')

		var $endDateInput = $('<input>', {
			type: 'date',
			id: $(`${server.id}_EndDate-${server.DOWNLOADED_VOULUME_CHART}`)
		}).css('width', size);

		$startDateInput.val(server.dateHelper.getStartDateString());
		$endDateInput.val(server.dateHelper.getEndDateString());

		$startDateInput.attr('max', server.dateHelper.getEndDateString());
		$endDateInput.attr('min', server.dateHelper.getStartDateString());

		$startDateInput.on('change', function () {
			const newStartDate = $(this).val();
			server.dateHelper.setStartDate(newStartDate);
			$endDateInput.attr('min', newStartDate);
		});

		$endDateInput.on('change', function () {
			const newEndDate = $(this).val();
			server.dateHelper.setEndDate(newEndDate);
			$startDateInput.attr('max', newEndDate);
		});

		$toolbar.append($startDateInput, $sep, $endDateInput);

		var $tooltip = $('<div>', {
			class: 'statistics__tooltip',
			id: `${server.id}_Tooltip-${server.DOWMLOADED_VOLUME_CHART}`,
			text: '--'
		});

		var $chart = $('<div>', {
			class: 'statistics__chartblock',
			id: `${server.id}_ChartBlock-${server.DOWMLOADED_VOLUME_CHART}`
		});

		$container.append($title, $toolbar, $tooltip, $chart);

		return $container;
	}

	function servervolumesLoaded(volumes) {
		servervolumes = volumes;
		updateServersDetails(serverStats);
		redrawCharts(serverStats);
	}

	function redrawCharts(serverStats) {
		for (var key in serverStats) {
			if (Object.prototype.hasOwnProperty.call(serverStats, key)) {
				var server = serverStats[key];
				server.hideSpinner();
				redrawChart(server);
			}
		}
	}

	function size64(size) {
		return size.SizeMB < 2000 ? size.SizeLo / 1024.0 / 1024.0 : size.SizeMB;
	}

	function redrawSpeedChart(server) {
		var serverData = server.getChartData();
		var serverNo = server.id;
		var curRange = serverData.range;

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

		function addData(data, dataLabel, timeIntervalSeconds) {
			dataLabels.push(dataLabel);

			if (data === null) {
				chartSpeedTb.push(null);
				chartSpeedGb.push(null);
				chartSpeedMb.push(null);
				chartSpeedKb.push(null);
				chartSpeedB.push(null);
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
				addData(buffer[i], i + 's', 1);
			}

			curPoint = 59;
		}

		function drawFiveMinuteGraph() {
			var buffer = rerangeCircularBuffer(
				servervolumes[serverNo].BytesPerMinutes,
				servervolumes[serverNo].MinSlot
			);

			for (var i = 55; i < buffer.length; i++) {
				addData(buffer[i], i - 55 + 'm', 60);
			}

			curPoint = 4;
		}

		function drawHourGraph() {
			var buffer = rerangeCircularBuffer(
				servervolumes[serverNo].BytesPerMinutes,
				servervolumes[serverNo].MinSlot
			);
			for (var i = 0; i < buffer.length; i++) {
				addData(buffer[i], i + 'h', 60);
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
			dataMB: chartSpeedMb,
			curPoint: curPoint,
			units: units,
			range: curRange,
			labels: dataLabels
		});

		server.showChart();

		var chartBlock = $(`#${server.id}_ChartBlock-${server.activeChart}`);
		if (!chartBlock)
			return;

		chartBlock.empty();
		chartBlock.html(`<div class="statistics__chart" id="${server.id}_Chart-${server.activeChart}"></div>`);
		var $chart = $(`#${server.id}_Chart-${server.activeChart}`);
		$chart.chart(makeChart(serieData, curPointData, lineLabels, units,
			{
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
			}
		));

		simulateMouseEvent(server, chartMouseExit);
	}

	function redrawVolumeChart(server) {
		var serverNo = server.id;
		var startDate = server.dateHelper.getStartDate();
		var endDate = server.dateHelper.getEndDate();
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

		var dates = [];
		var currentDate = new Date(startDate);
		while (currentDate <= endDate)
		{
			dates.push(new Date(currentDate));
			currentDate.setDate(currentDate.getDate() + 1);
		}

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

		function drawGraph() {
			var bytesPerDays = servervolumes[serverNo].BytesPerDays;
			var firstDay = servervolumes[serverNo].FirstDay;
			var daySlot = servervolumes[serverNo].DaySlot;
			for (var i = 0; i < dates.length; ++i) {
				var date = dates[i];
				var label = date.toDateString();
				var slot = Math.ceil(date.getTime() / 86400 / 1000);
				if (slot >= firstDay) {
					var idx = slot - firstDay;
					if (idx <= daySlot)
						addData(bytesPerDays[idx], label, '');
					else
						addData({SizeMB: 0, SizeLo: 0 }, label, '');
				}
				else {
					addData({SizeMB: 0, SizeLo: 0 }, label, '');
				}
			}
		}

		drawGraph();

		var serieData = maxSizeMB > 1024 * 1024 ? chartDataTB :
			maxSizeMB > 1024 ? chartDataGB :
				maxSizeMB > 1 || maxSizeLo == 0 ? chartDataMB :
					maxSizeLo > 1024 ? chartDataKB : chartDataB;

		if (serieData.length === 0) 
			return;

		var units = maxSizeMB > 1024 * 1024 ? ' TB' :
			maxSizeMB > 1024 ? ' GB' :
				maxSizeMB > 1 || maxSizeLo == 0 ? ' MB' :
					maxSizeLo > 1024 ? ' KB' : ' B';

		var curPointData = [];
		for (var i = 0; i < serieData.length; i++) {
			curPointData.push(i === curPoint ? serieData[i] : null);
		}

		server.setChartData({
			data: serieData,
			dataMB: chartDataMB,
			dataLo: chartDataB,
			units: units,
			curPoint: curPoint,
			labels: dataLabels,
			sumMB: sumMB,
			sumLo: sumLo,
		});

		server.hideSpinner();
		server.showChart();

		var chartBlock = $(`#${server.id}_ChartBlock-${server.activeChart}`);
		if (!chartBlock)
			return;

		chartBlock.empty();
		chartBlock.html(`<div class="statistics__chart" id="${server.id}_Chart-${server.activeChart}"></div>`);
		var $chart = $(`#${server.id}_Chart-${server.activeChart}`);
		$chart.chart(makeChart(serieData, curPointData, lineLabels, units,
			{
				type: 'axis',
				onMouseOver: function (env, serie, index, mouseAreaData) {
					chartMouseOver2(server, env, serie, index, mouseAreaData);
				},
				onMouseExit: function (env, serie, index, mouseAreaData) {
					chartMouseExit2(server, env, serie, index, mouseAreaData);
				},
				onMouseOut: function (env, serie, index, mouseAreaData) {
					chartMouseExit2(server, env, serie, index, mouseAreaData);
				},
			}
		));

		simulateMouseEvent(server, chartMouseExit2);
	}

	function redrawChart(server) {
		if (!server)
			return;


		if (server.activeChart === server.DOWNLOAD_SPEED_CHART) {
			redrawSpeedChart(server);
		}
		else {
			redrawVolumeChart(server);
		}
	}

	function chartMouseOver(server, env, serie, index, mouseAreaData) {
		var data = server.getChartData();

		if (data.mouseOverIndex > -1) {
			var chart = $(`#${server.id}_Chart-${server.activeChart}`);
			if (!chart)
				return;

			var env = chart.data('elycharts_env');
			if (env.mouseAreas[data.mouseOverIndex])
				$.elycharts.mousemanager.onMouseOutArea(env, false, data.mouseOverIndex, env.mouseAreas[data.mouseOverIndex]);
		}

		var tooltip = $(`#${server.id}_Tooltip-${server.activeChart}`);
		if (!tooltip)
			return;

		var data = server.getChartData();
		var value = data.data[index];
		if (value === undefined || value === null) {
			value = "0.0"
		}
		else {
			value = value.toFixed(1);
		}
		var title = value + data.units;
		tooltip.html('<span class="stat-size">' + title + '</span>');
		tooltip.html(data.labels[index] + ': <span class="stat-size">' + title + '</span>');
	}

	function chartMouseOver2(server, env, serie, index, mouseAreaData)
	{
		var data = server.getChartData();

		if (data.mouseOverIndex > -1) {
			var chart = $(`#${server.id}_Chart-${server.activeChart}`);
			if (!chart)
				return;

			var env = chart.data('elycharts_env');
			if (env.mouseAreas[data.mouseOverIndex])
				$.elycharts.mousemanager.onMouseOutArea(env, false, data.mouseOverIndex, env.mouseAreas[data.mouseOverIndex]);
		}

		data.mouseOverIndex = index;
		var tooltip = $(`#${server.id}_Tooltip-${server.activeChart}`);
		if (!tooltip)
			return;

		var data = server.getChartData();
		tooltip.html(data.labels[index] + ': <span class="stat-size">' +
			Util.formatSizeMB(data.dataMB[index], data.dataLo[index]) + '</span>');
	}

	function chartMouseExit(server, env, serie, index, mouseAreaData) {
		var tooltip = $(`#${server.id}_Tooltip-${server.activeChart}`);
		if (!tooltip)
			return;
		
		var data = server.getChartData();

		data.mouseOverIndex = -1;
		var value = data.data[data.curPoint];
		if (value === undefined || value === null) {
			value = "0.0"
		}
		else {
			value = value.toFixed(1);
		}
		var title = value + data.units;
		tooltip.html('<span class="stat-size">' + title + '</span>');
	}

	function chartMouseExit2(server, env, serie, index, mouseAreaData) {
		var tooltip = $(`#${server.id}_Tooltip-${server.activeChart}`);
		if (!tooltip)
			return;

		var data = server.getChartData();

		data.mouseOverIndex = -1;

		var title = Util.formatSizeMB(data.sumMB, data.sumLo);
		tooltip.html('All-time:' + ' ' + '<span class="stat-size">' + title + '</span>');
	}

	function simulateMouseEvent(server, onExitFn) {
		var data = server.getChartData();

		if (data.mouseOverIndex > -1) {
			var chart = $(`#${server.id}_Chart-${server.activeChart}`);
			if (!chart)
				return;

			var env = chart.data('elycharts_env');
			if (env.mouseAreas[data.mouseOverIndex])
				$.elycharts.mousemanager.onMouseOverArea(env, false, data.mouseOverIndex, env.mouseAreas[data.mouseOverIndex]);
		}
		else {
			onExitFn(server);
		}
	}

	function chooseRange(server, range) {
		var data = server.getChartData();
		updateRangeButtons(server, range, data.range);
		data.range = range;
		data.mouseOverIndex = -1;
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

	function makeChart(serieData, curPointData, lineLabels, units, mousearea) {
		return {
			height: 250,
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
				mousearea: mousearea,
			}
		};
	}
}(jQuery));
