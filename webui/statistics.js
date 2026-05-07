/*
 * This file is part of nzbget. See <https://nzbget.com>.
 *
 * Copyright (C) 2025-2026 Denis <denis@nzbget.com>
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

var Statistics = new (function ($) {
	"use strict";

	var datetime = new DateTime();
	var TEST_SERVER_HOST = "my.newsserver.com";

	function Server() {
		this.SPEED_CHART = 0;
		this.VOLUME_CHART = 1;

		this.id = 0;
		this.connections = 0;
		this.host = "";
		this.name = "";
		this.port = "";
		this.active = false;
		this.weekSizeMB = 0;
		this.weekSizeLo = 0;
		this.monthSizeMB = 0;
		this.monthSizeLo = 0;
		this.totalSizeMB = 0;
		this.totalSizeLo = 0;
		this.successArticles = [];
		this.failedArticles = [];
		this.startCustomDate = datetime.getFirstMonthDate();
		this.endCustomDate = datetime.getLastMonthDate();
		this.lastResetDate = null;
		this.$details = null;
		this.$speedChart = null;
		this.$volumeChart = null;
		this.$spinner = null;
		this.$chartToggleBtn = null;
		this.$speedChartBtn = null;
		this.$volumeChartBtn = null;
		this.activeChart = this.SPEED_CHART;

		this.speedChartData = {
			range: "MIN",
			data: null,
			dataMB: null,
			dataBits: null,
			curPoint: null,
			units: null,
			labels: null,
			mouseOverIndex: -1
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
			mouseOverIndex: -1
		};

		this.makeId = function(suffix) {
			return "Server_" + this.id + "_" + suffix;
		}

		this.getChartData = function () {
			if (this.activeChart === this.SPEED_CHART) {
				return this.speedChartData;
			}
			return this.volumeChartData;
		};

		this.setChartData = function (data) {
			if (this.activeChart === this.SPEED_CHART) {
				var mouseOverIndex = this.speedChartData.mouseOverIndex;
				this.speedChartData = data;
				this.speedChartData.mouseOverIndex = mouseOverIndex;
				return;
			}
			var mouseOverIndex = this.volumeChartData.mouseOverIndex;
			this.volumeChartData = data;
			this.volumeChartData.mouseOverIndex = mouseOverIndex;
		};

		this.showChart = function () {
			if (this.activeChart === this.SPEED_CHART) {
				this.$speedChart.show();
				this.$volumeChart.hide();
			} else {
				this.$speedChart.hide();
				this.$volumeChart.show();
			}
		};

		this.hideChart = function () {
			if (this.activeChart === this.SPEED_CHART) {
				this.$speedChart.hide();
			} else {
				this.$volumeChart.hide();
			}
		};

		this.showDetails = function () {
			this.$details.show();
		};

		this.hideSpinner = function () {
			this.$spinner.hide();
		};

		this.showSpinner = function () {
			this.$spinner.show();
		};

		this.toggleChart = function () {
			if (this.activeChart === this.SPEED_CHART) {
				this.activeChart = this.VOLUME_CHART;
				this.$speedChart.hide();
				this.$volumeChart.show();
				this.$speedChartBtn.removeClass("btn-active");
				this.$volumeChartBtn.addClass("btn-active");
			} else {
				this.activeChart = this.SPEED_CHART;
				this.$speedChart.hide();
				this.$volumeChart.show();
				this.$speedChartBtn.addClass("btn-active");
				this.$volumeChartBtn.removeClass("btn-active");
			}
		};
	}

	var $StatisticsSpinner;
	var $StatisticsTable;
	var $NoStatisticsAvailable;
	var serverStats = {};
	var options = null;
	var servervolumes = null;
	var serverVolumesLoaded = false;
	var optionsHandler = {
		update: function update(options_) {
			if (options || !options_) return;
			options = options_;

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

			if (noServers(serverStats) || testServerOnly(serverStats)) {
				$StatisticsSpinner.hide();
				$NoStatisticsAvailable.show();
				return;
			}

			RPC.call("servervolumes", [], firstServervolumesLoaded);
			renderServers(serverStats);
		}
	};

	this.init = function () {
		$("#StatisticsTable_filter").val("");
		$StatisticsSpinner = $("#Statistics_Spinner");
		$StatisticsTable = $("#Statistics_Table");
		$NoStatisticsAvailable = $("#Statistics_NoStatisticsAvailable");
		$StatisticsTable.fasttable({
			filterInput: $("#StatisticsTable_filter"),
			filterClearButton: $("#StatisticsTable_clearfilter"),
			filterInputCallback: filterInput,
			filterClearCallback: filterClear
		});

		Options.subscribe(optionsHandler);
		I18n.subscribe(function() { Statistics.redraw(); Statistics.update(); });
	};

	this.redraw = function () {
		if ($StatisticsTable && $StatisticsTable.is(":visible")) {
			I18n.translatePage($StatisticsTable);
			for (var key in serverStats) {
				if (Object.prototype.hasOwnProperty.call(serverStats, key)) {
					redrawChart(serverStats[key]);
				}
			}
		}
	}

	this.update = function () {
		if (serverVolumesLoaded) {
			RPC.call("servervolumes", [], servervolumesLoaded);
		}
	};

	function filterInput() { }
	function filterClear() { }

	function noServers(servers) {
		return Util.emptyObject(servers);
	}

	function testServerOnly(servers) {
		var amount = 0;
		var hasTestServer = false;
		for (const key in servers) {
			if (Object.prototype.hasOwnProperty.call(servers, key)) {
				++amount;
				if (servers[key].host === TEST_SERVER_HOST) {
					hasTestServer = true;
				}
			}
		}

		return amount === 1 && hasTestServer;
	}

	function firstServervolumesLoaded(volumes) {
		if (serverVolumesLoaded || !volumes || !volumes.length) return;
		serverVolumesLoaded = true;
		$StatisticsSpinner.hide();
		$StatisticsTable.show();
		servervolumesLoaded(volumes);
	}

	function renderServers(servers) {
		for (var key in servers) {
			if (Object.prototype.hasOwnProperty.call(servers, key)) {
				renderServer(servers[key]);
			}
		}
	}

	function renderServer(server) {
		var $container = $(".statistics__server-container-template").children().clone();
		server.$details = makeServerDetails(server);
		server.$spinner = makeSpinner(server);
		server.$speedChart = makeSpeedChart(server);
		server.$volumeChart = makeVolumeChart(server);

		server.$details.css("flex-grow", "1").css("flex-basis", "400px").hide();
		server.$spinner.css("flex-grow", "3").css("flex-basis", "600px");
		server.$speedChart
			.css("flex-grow", "3")
			.css("flex-basis", "600px")
			.css("overflow", "hidden")
			.hide();
		server.$volumeChart
			.css("flex-grow", "3")
			.css("flex-basis", "600px")
			.css("overflow", "hidden")
			.hide();

		$container.append(
			server.$details,
			server.$spinner,
			server.$speedChart,
			server.$volumeChart
		);
		$StatisticsTable.append($container, "<hr>");
	}

	function makeResetButton(server) {
		var $btn = $(
			'<button class="btn btn-default" data-i18n-title="btn_reset_counters_title">'
			+ '<i class="material-icon">refresh</i>'
			+ '<span data-i18n="btn_reset_counters"></span>'
			+ '</button>'
		);
		I18n.translatePage($btn);
		$btn
			.on("click", function () {
				$("#ServerStatResetConfirmDialog_Server").text(server.name);
				$("#ServerStatResetConfirmDialog_Time").text(server.lastResetDate);
				ConfirmDialog.showModal("ServerStatResetConfirmDialog", function () {
					RPC.call(
						"resetservervolume",
						[server.id === 0 ? -1 : server.id, ""],
						function () {
							PopupNotification.show("#Notif_StatReset");
							Refresher.update();
						}
					);
				});
			});
		return $btn;
	}

	function makeServerDetails(server) {
		var $details = $(".statistics__server-details-template").children().clone();
		I18n.translatePage($details);

		$details.find(".server-details__name").text(server.name);
		$details.find(".server-details__host").text(server.host);
		$details.find(".server-details__connections").text(server.connections);
		$details.find(".server-details__active").html(
			server.active ? "<span class='txt-success' data-i18n='label_yes_cap'></span>" : "<span class='txt-important' data-i18n='label_no_cap'></span>"
		);
		I18n.translatePage($details.find(".server-details__active"));

		var $controls = $("<div>", {
			class: "btn-group"
		}).css("padding", "5px 0");

		var $serverConfigBtn = makeConfigServerBtn(server);
		var $resetCountersBtn = makeResetButton(server);
		$controls.append($serverConfigBtn, $resetCountersBtn);

		return $details.append($controls);
	}

	function updateServersDetails(servers) {
		for (var key in servers) {
			if (Object.prototype.hasOwnProperty.call(servers, key)) {
				updateServerDetails(servers[key]);
			}
		}
	}

	function calculateSizePerDays(dates, bytesPerDays, firstDay, daySlot) {
		var sizeMB = 0;
		var sizeLo = 0;
		for (var i = 0; i < dates.length; ++i) {
			var date = dates[i];
			var slot = Util.getDaySinceUnixEpoch(date);
			if (slot >= firstDay) {
				var idx = slot - firstDay;
				if (!bytesPerDays[idx] || idx > daySlot)
					continue;

				sizeMB += bytesPerDays[idx].SizeMB;
				sizeLo += bytesPerDays[idx].SizeLo;
			}
		}
		return {
			sizeMB: sizeMB,
			sizeLo: sizeLo
		};
	}

	function updateServerDetails(server) {
		if (!servervolumes || !servervolumes[server.id])
			return;

		var serverVolume = servervolumes[server.id];
		var daySlot = serverVolume.DaySlot;
		var week = datetime.getWeek();
		var month = datetime.getMonth();
		var weekSizes = calculateSizePerDays(
			week,
			serverVolume.BytesPerDays,
			serverVolume.FirstDay,
			daySlot
		);
		server.weekSizeMB = weekSizes.sizeMB;
		server.weekSizeLo = weekSizes.sizeLo;
		var monthSizes = calculateSizePerDays(
			month,
			serverVolume.BytesPerDays,
			serverVolume.FirstDay,
			daySlot
		);
		server.monthSizeMB = monthSizes.sizeMB;
		server.monthSizeLo = monthSizes.sizeLo;
		var year = datetime.getYear();
		var yearSizes = calculateSizePerDays(
			year,
			serverVolume.BytesPerDays,
			serverVolume.FirstDay,
			daySlot
		);
		server.yearSizeMB = yearSizes.sizeMB;
		server.yearSizeLo = yearSizes.sizeLo;
		server.todaySizeMB = serverVolume.BytesPerDays[daySlot].SizeMB;
		server.todaySizeLo = serverVolume.BytesPerDays[daySlot].SizeLo;
		server.totalSizeMB = serverVolume.TotalSizeMB;
		server.totalSizeLo = serverVolume.TotalSizeLo;
		server.lastResetDate = Util.formatDateTime(serverVolume.CountersResetTime);

		var successSum = 0;
		var failedSum = 0;
		serverVolume.ArticlesPerDays.forEach(function (articles) {
			successSum += articles.Success;
			failedSum += articles.Failed;
		});

		var completion =
			successSum + failedSum > 0
				? Util.round0((successSum * 100.0) / (successSum + failedSum)) + "%"
				: "--";

		if (failedSum > 0 && completion === "100%") {
			completion = "99.9%";
		}

		var $successArticles = $(
			"<span>".concat(Util.formatNumber(successSum), "</span>")
		).addClass("txt-success");

		var $failedArticles = $(
			"<span>".concat(Util.formatNumber(failedSum), "</span>")
		).addClass("txt-important");

		server.$details.find(".server-details__articles")
			.empty()
			.append($successArticles, " / ", $failedArticles);

		server.$details.find(".server-details__today").text(
			Util.formatSizeMB(server.todaySizeMB, server.todaySizeLo)
		);
		server.$details.find(".server-details__week").text(
			Util.formatSizeMB(server.weekSizeMB, server.weekSizeLo)
		);
		server.$details.find(".server-details__month").text(
			Util.formatSizeMB(server.monthSizeMB, server.monthSizeLo)
		);
		server.$details.find(".server-details__year").text(
			Util.formatSizeMB(server.yearSizeMB, server.yearSizeLo)
		);
		server.$details.find(".server-details__total").text(
			Util.formatSizeMB(server.totalSizeMB, server.totalSizeLo)
		);
		server.$details.find(".server-details__completion").text(completion);
		server.showDetails();
	}

	function makeSpinner(server) {
		var $spinner = $(".statistics__chart-spinner-template").children().clone();
		$spinner.find("i").attr("id", server.id + "_ChartSpinner-" + server.activeChart);
		return $spinner;
	}

	function makeConfigServerBtn(server) {
		var $btn = $(
			'<button class="btn btn-default" data-i18n-title="btn_server_config_title">'
			+ '<i class="material-icon">settings</i>'
			+ '<span data-i18n="btn_server_config"></span>'
			+ '</button>'
		);
		I18n.translatePage($btn);

		$btn
			.attr("data-optid", "S_Server" + server.id + "_Active")
			.on("click", function (ev) {
				Options.setOnPageRenderedCallback(function() {
					$("a[href$='#NEWS-SERVERS']").click();
					Config.scrollToOption(ev, $btn);
				});
				$('#ConfigTabLink').click();
			});

		return $btn;
	}

	this.navigateToNewsServersConfiguration = function() {
		Options.setOnPageRenderedCallback(function() {
			$("a[href$='#NEWS-SERVERS']").click();
		});
		$('#ConfigTabLink').click();
	}

	function makeToggleChartBtn(server) {
		var $container = $(".statistics__chart-toggle-template").children().clone();
		I18n.translatePage($container);

		var $speedChartBtn = $container.find(".chart-toggle-speed");
		var $volumeChartBtn = $container.find(".chart-toggle-data");

		$speedChartBtn.on("click", function () {
			server.toggleChart();
			redrawChart(server);
		});
		$volumeChartBtn.on("click", function () {
			server.toggleChart();
			redrawChart(server);
		});

		server.$speedChartBtn = $speedChartBtn;
		server.$volumeChartBtn = $volumeChartBtn;
		return $container;
	}

	function makeSpeedChart(server) {
		var $container = $(".statistics__speed-chart-template").children().clone();
		I18n.translatePage($container);

		var $title = makeToggleChartBtn(server);
		$container.prepend($title);

		$container.find(".speed-range-min").attr("id", server.makeId(server.SPEED_CHART + "_MIN"))
			.on("click", function () { chooseRange(server, "MIN"); });
		$container.find(".speed-range-5min").attr("id", server.makeId(server.SPEED_CHART + "_5MIN"))
			.on("click", function () { chooseRange(server, "5MIN"); });
		$container.find(".speed-range-hour").attr("id", server.makeId(server.SPEED_CHART + "_HOUR"))
			.on("click", function () { chooseRange(server, "HOUR"); });

		$container.find(".speed-range-min2").attr("id", server.makeId(server.SPEED_CHART + "_MIN2"))
			.on("click", function () { chooseRange(server, "MIN"); });
		$container.find(".speed-range-5min2").attr("id", server.makeId(server.SPEED_CHART + "_5MIN2"))
			.on("click", function () { chooseRange(server, "5MIN"); });
		$container.find(".speed-range-hour2").attr("id", server.makeId(server.SPEED_CHART + "_HOUR2"))
			.on("click", function () { chooseRange(server, "HOUR"); });

		$container.find(".statistics__tooltip").attr("id", server.makeId(server.SPEED_CHART + "_TOOLTIP"));
		$container.find(".statistics__tooltip-unit").attr("id", server.makeId(server.SPEED_CHART + "_TOOLTIP_UNIT"));
		$container.find(".statistics__chart").attr("id", server.makeId(server.SPEED_CHART + "_CHART"));

		return $container;
	}

	function makeVolumeChart(server) {
		var $container = $(".statistics__volume-chart-template").children().clone();
		I18n.translatePage($container);

		var $title = makeToggleChartBtn(server);
		$container.prepend($title);

		$container.find(".volume-range-day").attr("id", server.makeId(server.VOLUME_CHART + "_DAY"))
			.on("click", function () { chooseRange(server, "DAY"); redrawVolumeChart(server); });
		$container.find(".volume-range-week").attr("id", server.makeId(server.VOLUME_CHART + "_WEEK"))
			.on("click", function () { chooseRange(server, "WEEK"); redrawVolumeChart(server); });
		$container.find(".volume-range-month").attr("id", server.makeId(server.VOLUME_CHART + "_MONTH"))
			.on("click", function () { chooseRange(server, "MONTH"); redrawVolumeChart(server); });

		$container.find(".volume-range-day2").attr("id", server.makeId(server.VOLUME_CHART + "_DAY2"))
			.on("click", function () { chooseRange(server, "DAY"); redrawVolumeChart(server); });
		$container.find(".volume-range-week2").attr("id", server.makeId(server.VOLUME_CHART + "_WEEK2"))
			.on("click", function () { chooseRange(server, "WEEK"); redrawVolumeChart(server); });
		$container.find(".volume-range-month2").attr("id", server.makeId(server.VOLUME_CHART + "_MONTH2"))
			.on("click", function () { chooseRange(server, "MONTH"); redrawVolumeChart(server); });

		var $startDateInput = $container.find(".statistics__start-date");
		var $endDateInput = $container.find(".statistics__end-date");
		var startDateStr = datetime.formatDateForInput(server.startCustomDate);
		var endDateStr = datetime.formatDateForInput(server.endCustomDate);

		$startDateInput.val(startDateStr).attr("max", endDateStr).on("change", function () {
			var startDate = $(this).val();
			chooseRange(server, "CUSTOM");
			server.startCustomDate = new Date(startDate);
			$endDateInput.attr("min", datetime.formatDateForInput(startDate));
			redrawVolumeChart(server);
		});
		$endDateInput.val(endDateStr).attr("min", startDateStr).on("change", function () {
			var endDate = $(this).val();
			chooseRange(server, "CUSTOM");
			server.endCustomDate = new Date(endDate);
			$startDateInput.attr("max", datetime.formatDateForInput(endDate));
			redrawVolumeChart(server);
		});

		$container.find(".statistics__tooltip").attr("id", server.makeId(server.VOLUME_CHART + "_TOOLTIP"));
		$container.find(".statistics__chart").attr("id", server.makeId(server.VOLUME_CHART + "_CHART"));

		return $container;
	}

	function servervolumesLoaded(volumes) {
		if (!volumes || !volumes.length) return;
		var sameDay = servervolumes && servervolumes.length > 0 && servervolumes[0] && volumes[0] &&
			servervolumes[0].DaySlot === volumes[0].DaySlot;
		servervolumes = volumes;

		if (!sameDay) {
			datetime.recalculate();
		}

		updateServersDetails(serverStats);
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

	function redrawSpeedChart(server) {
		var serverNo = server.id;
		if (!servervolumes || !servervolumes[serverNo]) return;
		var serverData = server.getChartData();
		var curRange = serverData.range;
		var lineLabels = [];
		var dataLabels = [];
		var chartSpeedTB = [];
		var chartSpeedGB = [];
		var chartSpeedMB = [];
		var chartSpeedKB = [];
		var chartSpeedB = [];
		var chartSpeedBits = [];
		var curPoint = null;
		var sumMb = 0;
		var sumLo = 0;
		var maxSizeMB = 0;
		var maxSizeLo = 0;

		function addData(data, dataLabel, timeIntervalSeconds) {
			dataLabels.push(dataLabel);
			var sizeMB = size64(data);
			var speedMB = (sizeMB / timeIntervalSeconds);
			var speedLoMB = (data.SizeLo / timeIntervalSeconds);
			chartSpeedTB.push(speedMB / 1024.0 / 1024.0);
			chartSpeedGB.push(speedMB / 1024.0);
			chartSpeedMB.push(speedMB);
			chartSpeedKB.push(speedMB * 1024.0);
			chartSpeedB.push(speedMB * 1024.0 * 1024.0);
			chartSpeedBits.push(speedMB * 1024.0 * 1024.0 * 8);
			if (speedMB > maxSizeMB) {
				maxSizeMB = speedMB;
			}
			if (speedLoMB > maxSizeLo) {
				maxSizeLo = speedLoMB;
			}
			sumMb += speedMB;
			sumLo += speedLoMB;
		}

		function rerangeCircularBuffer(buffer, currentSlot) {
			var rearranged = [];
			for (var i = currentSlot + 1; i < buffer.length; i++) {
				rearranged.push(buffer[i]);
			}
			for (var i = 0; i <= currentSlot; i++) {
				rearranged.push(buffer[i]);
			}
			return rearranged;
		}

		function drawMinuteGraph() {
			if (!servervolumes[serverNo].BytesPerSeconds) return;
			var buffer = rerangeCircularBuffer(
				servervolumes[serverNo].BytesPerSeconds,
				servervolumes[serverNo].SecSlot
			);
			for (var i = 0; i < buffer.length; i++) {
				// the current slot may be not fully filled yet,
				// to make the chart smoother for current slot 
				// we show the previous slot as current.
				var s = I18n.translate('time_seconds_short');
				if (i === 59) {
					addData(buffer[58], i + s, 1);
				}
				else {
					addData(buffer[i], i + s, 1);
				}
			}
			curPoint = 59;
		}

		function drawFiveMinuteGraph() {
			if (!servervolumes[serverNo].BytesPerMinutes) return;
			var buffer = rerangeCircularBuffer(
				servervolumes[serverNo].BytesPerMinutes,
				servervolumes[serverNo].MinSlot
			);
			var m = I18n.translate('time_minutes_short');
			for (var i = 55; i < buffer.length; i++) {
				addData(buffer[i], (i - 55) + m, 60);
			}
			curPoint = 4;
		}

		function drawHourGraph() {
			if (!servervolumes[serverNo].BytesPerMinutes) return;
			var buffer = rerangeCircularBuffer(
				servervolumes[serverNo].BytesPerMinutes,
				servervolumes[serverNo].MinSlot
			);
			var m = I18n.translate('time_minutes_short');
			for (var i = 0; i < buffer.length; i++) {
				addData(buffer[i], i + m, 60);
			}
			curPoint = 59;
		}

		if (curRange === "MIN") {
			drawMinuteGraph();
		} else if (curRange === "5MIN") {
			drawFiveMinuteGraph();
		} else if (curRange === "HOUR") {
			drawHourGraph();
		}

		if (server.activeChart !== server.SPEED_CHART) {
			return;
		}

		var useBits = (I18n.getSpeedUnit() === 'Mb/s');
		var unitSuffix = useBits ? 'b/s' : 'B/s';
		var kBase = useBits ? 1000 : 1024;
		var effectiveMaxSize = useBits ? (maxSizeMB * 1024 * 1024 * 8) / (kBase * kBase) : maxSizeMB;

		var serieData =
			effectiveMaxSize >= kBase * kBase
				? (useBits ? chartSpeedBits.map(function(b){return b / (kBase*kBase*kBase*kBase);}) : chartSpeedTB)
				: effectiveMaxSize >= kBase
					? (useBits ? chartSpeedBits.map(function(b){return b / (kBase*kBase*kBase);}) : chartSpeedGB)
					: effectiveMaxSize >= 1
						? (useBits ? chartSpeedBits.map(function(b){return b / (kBase*kBase);}) : chartSpeedMB)
						: (useBits ? chartSpeedBits.map(function(b){return b / kBase;}) : chartSpeedKB);
		var units =
			effectiveMaxSize >= kBase * kBase
				? " T" + unitSuffix
				: effectiveMaxSize >= kBase
					? " G" + unitSuffix
					: effectiveMaxSize >= 1
						? " M" + unitSuffix
						: " K" + unitSuffix;
		var curPointData = [];

		for (var i = 0; i < serieData.length; ++i) {
			curPointData.push(i === curPoint ? serieData[i] : null);
		}

		server.setChartData({
			data: serieData,
			dataMB: chartSpeedMB,
			dataBits: chartSpeedBits,
			curPoint: curPoint,
			units: units,
			range: curRange,
			labels: dataLabels
		});

		server.showChart();
		server.hideSpinner();

		var $chart = $("#" + server.makeId(server.SPEED_CHART + "_CHART"));
		if (!$chart) return;
		if (!isParentContainerRendered($chart)) {
			server.hideChart();
			server.showSpinner();
			return;
		}

		$chart
			.chart('clear')
			.width(getChartWidth($chart))
			.chart(
				makeChart(serieData, curPointData, lineLabels, units, {
					type: "axis",
					onMouseOver: function onMouseOver(env, serie, index, mouseAreaData) {
						speedChartMouseOver(server, env, serie, index, mouseAreaData);
					},
					onMouseExit: function onMouseExit(env, serie, index, mouseAreaData) {
						speedChartMouseExit(server, env, serie, index, mouseAreaData);
					},
					onMouseOut: function onMouseOut(env, serie, index, mouseAreaData) {
						speedChartMouseExit(server, env, serie, index, mouseAreaData);
					}
				})
			);

		simulateMouseEvent(server, speedChartMouseExit);
	}

	function redrawVolumeChart(server) {
		var serverNo = server.id;
		if (!servervolumes || !servervolumes[serverNo]) return;
		var serverData = server.getChartData();
		var startCustomDate = server.startCustomDate;
		var endCustomDate = server.endCustomDate;
		var curRange = serverData.range;
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

		function drawGraph(dates) {
			if (!servervolumes[serverNo].BytesPerDays) return;
			var bytesPerDays = servervolumes[serverNo].BytesPerDays;
			var firstDay = servervolumes[serverNo].FirstDay;
			var daySlot = servervolumes[serverNo].DaySlot;
			var currDaySlot = Util.getDaySinceUnixEpoch(datetime.getCurrentDay());
			for (var i = 0; i < dates.length; ++i) {
				var date = dates[i];
				var label = new Intl.DateTimeFormat(I18n.getLocale(), I18n.getTimeFormatOptions()).format(date);
				var slot = Util.getDaySinceUnixEpoch(date);
				if (slot >= firstDay) {
					var idx = slot - firstDay;
					if (idx <= daySlot) {
						addData(bytesPerDays[idx], label, "");
					}
					else {
						addData({ SizeMB: 0, SizeLo: 0 }, label, "");
					}
				} else {
					addData({ SizeMB: 0, SizeLo: 0 }, label, "");
				}

				if (currDaySlot == slot) {
					curPoint = i;
				}
			}
		}

		function drawDayGraph() {
			if (!servervolumes[serverNo].BytesPerHours) return;
			var h = I18n.translate('time_hours_short');
			var bytesPerHours = servervolumes[serverNo].BytesPerHours;
			for (var i = 0; i < 24; ++i) {
				addData(bytesPerHours[i], i + h, "");
			}
			curPoint = servervolumes[serverNo].HourSlot;
		}

		if (curRange == "DAY") {
			drawDayGraph();
		}
		else if (curRange == "WEEK") {
			var dates = datetime.getWeekRange();
			drawGraph(dates);

		}
		else if (curRange == "MONTH") {
			var dates = datetime.getMonthRange();
			drawGraph(dates);
		}
		else if (curRange == "CUSTOM") {
			var dates = datetime.getDateRange(startCustomDate, endCustomDate);
			drawGraph(dates);
		}
		else {
			return;
		}

		if (server.activeChart !== server.VOLUME_CHART) {
			return;
		}

		var serieData =
			maxSizeMB > 1024 * 1024
				? chartDataTB
				: maxSizeMB > 1024
					? chartDataGB
					: maxSizeMB > 1 || maxSizeLo == 0
						? chartDataMB
						: maxSizeLo > 1024
							? chartDataKB
							: chartDataB;

		if (serieData.length === 0) return;

		var units =
			maxSizeMB > 1024 * 1024
				? " TB"
				: maxSizeMB > 1024
					? " GB"
					: maxSizeMB > 1 || maxSizeLo == 0
						? " MB"
						: maxSizeLo > 1024
							? " KB"
							: " B";

		var curPointData = [];

		for (var i = 0; i < serieData.length; i++) {
			curPointData.push(i === curPoint ? serieData[i] : null);
		}

		server.setChartData({
			data: serieData,
			dataMB: chartDataMB,
			dataLo: chartDataB,
			range: curRange,
			units: units,
			curPoint: curPoint,
			labels: dataLabels,
			sumMB: sumMB,
			sumLo: sumLo
		});

		server.showChart();
		server.hideSpinner();

		var $chart = $("#" + server.makeId(server.VOLUME_CHART + "_CHART"));
		if (!$chart) return;
		if (!isParentContainerRendered($chart)) {
			server.hideChart();
			server.showSpinner();
			return;
		}

		$chart
			.chart('clear')
			.width(getChartWidth($chart))
			.chart(
				makeChart(serieData, curPointData, lineLabels, units, {
					type: "axis",
					onMouseOver: function onMouseOver(env, serie, index, mouseAreaData) {
						volumeChartMouseOver(server, env, serie, index, mouseAreaData);
					},
					onMouseExit: function onMouseExit(env, serie, index, mouseAreaData) {
						volumeChartMouseExit(server, env, serie, index, mouseAreaData);
					},
					onMouseOut: function onMouseOut(env, serie, index, mouseAreaData) {
						volumeChartMouseExit(server, env, serie, index, mouseAreaData);
					}
				})
			);

		simulateMouseEvent(server, volumeChartMouseExit);
	}

	function isParentContainerRendered($chart) {
		return $chart.parent().width() > 0;
	}

	function getChartWidth($chart) {
		var chartWidth = $chart.width();
		var parentWidth = $chart.parent().width();
		if (parentWidth !== 0 && chartWidth !== parentWidth) {
			return parentWidth;
		}

		return chartWidth;
	}

	function redrawChart(server) {
		if (!server) return;
		if (server.activeChart === server.SPEED_CHART) {
			redrawSpeedChart(server);
		} else {
			redrawVolumeChart(server);
		}
	}

	function speedChartMouseOver(server, env, serie, index, mouseAreaData) {
		var data = server.getChartData();
		if (!data || !data.dataMB || !data.labels) return;

		if (data.mouseOverIndex > -1) {
			var $chart = $("#" + server.makeId(server.SPEED_CHART + "_CHART"));
			if (!$chart) return;
			var env = $chart.data("elycharts_env");
			if (!env) return;

			if (env.mouseAreas[data.mouseOverIndex])
				$.elycharts.mousemanager.onMouseOutArea(
					env,
					false,
					data.mouseOverIndex,
					env.mouseAreas[data.mouseOverIndex]
				);
		}

		var $tooltip = $("#" + server.makeId(server.SPEED_CHART + "_TOOLTIP"));
		var $tooltipUnit = $("#" + server.makeId(server.SPEED_CHART + "_TOOLTIP_UNIT"));
		if (!$tooltip || !$tooltipUnit) return;

		var valueMB = data.dataMB[index];
		var label = data.labels[index];
		var title = '';
		if (valueMB === undefined || valueMB === null) {
			var useBits = (I18n.getSpeedUnit() === 'Mb/s');
			title = useBits ? "0.0 Mb/s" : "0.0 MB/s";
		} else {
			title = Util.formatSpeed(valueMB * 1024 * 1024);
		}
		$tooltipUnit.html(
			"<span>" + label + " " + "</span>" +
			'<span class="stat-size">' + title + '</span>',
		);
	}

	function volumeChartMouseOver(server, env, serie, index, mouseAreaData) {
		var data = server.getChartData();
		if (!data || !data.labels || !data.dataMB || !data.dataLo) return;

		if (data.mouseOverIndex > -1) {
			var $chart = $("#" + server.makeId(server.VOLUME_CHART + "_CHART"));
			if (!$chart) return;
			var env = $chart.data("elycharts_env");
			if (!env) return;

			if (env.mouseAreas[data.mouseOverIndex])
				$.elycharts.mousemanager.onMouseOutArea(
					env,
					false,
					data.mouseOverIndex,
					env.mouseAreas[data.mouseOverIndex]
				);
		}
		data.mouseOverIndex = index;
		var $tooltip = $("#" + server.makeId(server.VOLUME_CHART + "_TOOLTIP"));

		if (!$tooltip) return;

		var data = server.getChartData();
		$tooltip.html(
			data.labels[index] +
			': <span class="stat-size">' +
			Util.formatSizeMB(data.dataMB[index], data.dataLo[index]) +
			"</span>"
		);
	}

	function speedChartMouseExit(server, env, serie, index, mouseAreaData) {
		var $tooltip = $("#" + server.makeId(server.SPEED_CHART + "_TOOLTIP"));
		var $tooltipUnit = $("#" + server.makeId(server.SPEED_CHART + "_TOOLTIP_UNIT"));
		if (!$tooltip || !$tooltipUnit) return;

		var data = server.getChartData();
		if (!data || !data.dataBits || data.curPoint === null) return;

		var valueBytes = data.dataBits[data.curPoint] || 0;
		if (!(I18n.getSpeedUnit() === 'Mb/s')) {
			valueBytes = (data.dataMB[data.curPoint] || 0) * 1024 * 1024;
		} else {
			// formatSpeed multiplies by 8 internally if Mb/s is selected, so we must divide the stored bits by 8 to get raw bytes for it
			valueBytes = valueBytes / 8;
		}
		
		var title = Util.formatSpeed(valueBytes);
		
		$tooltipUnit.html('<span class="stat-size">' + title + '</span>');
	}

	function volumeChartMouseExit(server, env, serie, index, mouseAreaData) {
		var $tooltip = $("#" + server.makeId(server.VOLUME_CHART + "_TOOLTIP"));
		if (!$tooltip) return;
		var data = server.getChartData();
		if (data.sumMB == null || data.sumLo == null || (data.sumMB === 0 && data.sumLo === 0)) return;

		data.mouseOverIndex = -1;
		var title = Util.formatSizeMB(data.sumMB, data.sumLo);
		var selectedPeriodTxt = I18n.translate('label_selected_period');
		$tooltip.html(
			selectedPeriodTxt + " " + '<span class="stat-size">' + title + "</span>"
		);
	}

	function simulateMouseEvent(server, onExitFn) {
		var data = server.getChartData();
		if (data.mouseOverIndex > -1) {
			var $chart = $("#" + server.makeId(server.activeChart + "_CHART"));
			if (!$chart) return;
			var env = $chart.data("elycharts_env");
			if (!env) return;

			if (env.mouseAreas[data.mouseOverIndex])
				$.elycharts.mousemanager.onMouseOverArea(
					env,
					false,
					data.mouseOverIndex,
					env.mouseAreas[data.mouseOverIndex]
				);
		} else {
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
		var chartId = server.activeChart;
		var prevTab = $(
			"#" + server.makeId(chartId + "_" + prevRange) + "," +
			"#" + server.makeId(chartId + "_" + prevRange) + "2"
		);
		var newTab = $(
			"#" + server.makeId(chartId + "_" + range) + "," +
			"#" + server.makeId(chartId + "_" + range) + "2"
		);
		prevTab.removeClass("btn-active");
		newTab.addClass("btn-active");
	}

	function makeChart(serieData, curPointData, lineLabels, units, mousearea) {
		return {
			height: 250,
			values: {
				serie1: serieData,
				serie2: curPointData
			},
			labels: lineLabels,
			type: "line",
			margins: [10, 15, 20, 70],
			defaultSeries: {
				rounded: 0.0,
				fill: true,
				plotProps: {
					"stroke-width": 3.0
				},
				dot: true,
				dotProps: {
					stroke: "#FFF",
					size: 3.0,
					"stroke-width": 1.0,
					fill: "#5AF"
				},
				highlight: {
					scaleSpeed: 0,
					scaleEasing: ">",
					scale: 2.0
				},
				tooltip: {
					active: false
				},
				color: "#5AF"
			},
			series: {
				serie2: {
					dotProps: {
						stroke: "#F21860",
						fill: "#F21860",
						size: 3.5,
						"stroke-width": 2.5
					},
					highlight: {
						scale: 1.5
					}
				}
			},
			defaultAxis: {
				labels: true,
				labelsProps: {
					"font-size": 13,
					fill: "#3a87ad"
				},
				labelsDistance: 12
			},
			axis: {
				l: {
					labels: true,
					suffix: units
				}
			},
			features: {
				grid: {
					draw: [true, false],
					forceBorder: true,
					props: {
						stroke: "#e0e0e0",
						"stroke-width": 1
					},
					ticks: {
						active: [true, false, false],
						size: [6, 0],
						props: {
							stroke: "#e0e0e0"
						}
					}
				},
				mousearea: mousearea
			}
		};
	}
})(jQuery);
