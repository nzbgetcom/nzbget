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

var Statistics = new (function ($) {
	"use strict";

	var datetime = new DateTime();
	var TEST_SERVER_HOST = "my.newsserver.com";

	function Server() {
		this.DOWNLOAD_SPEED_CHART = 0;
		this.DOWMLOADED_VOLUME_CHART = 1;

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
		this.startDate = datetime.getFirstMonthDate();
		this.endDate = datetime.getLastMonthDate();
		this.lastResetDate = null;
		this.$details = null;
		this.$downloadSpeedChart = null;
		this.$downloadVolumeChart = null;
		this.$spinner = null;
		this.$chartToggleBtn = null;
		this.$speedChartBtn = null;
		this.$volumeChartBtn = null;
		this.activeChart = this.DOWNLOAD_SPEED_CHART;
		this.speedChartData = {
			range: "MIN",
			data: null,
			dataMB: null,
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

		this.getChartData = function () {
			if (this.activeChart === this.DOWNLOAD_SPEED_CHART) {
				return this.speedChartData;
			}
			return this.volumeChartData;
		};

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
		};

		this.showChart = function () {
			if (this.activeChart === this.DOWNLOAD_SPEED_CHART) {
				this.$downloadSpeedChart.show();
				this.$downloadVolumeChart.hide();
			} else {
				this.$downloadSpeedChart.hide();
				this.$downloadVolumeChart.show();
			}
		};

		this.hideChart = function () {
			if (this.activeChart === this.DOWNLOAD_SPEED_CHART) {
				this.$downloadSpeedChart.hide();
			} else {
				this.$downloadVolumeChart.hide();
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
			if (this.activeChart === this.DOWNLOAD_SPEED_CHART) {
				this.activeChart = this.DOWMLOADED_VOLUME_CHART;
				this.$downloadSpeedChart.hide();
				this.$downloadVolumeChart.show();
				this.$speedChartBtn.removeClass("btn-active");
				this.$volumeChartBtn.addClass("btn-active");
			} else {
				this.activeChart = this.DOWNLOAD_SPEED_CHART;
				this.$downloadSpeedChart.hide();
				this.$downloadVolumeChart.show();
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

			RPC.call("servervolumes", [], fistServervolumesLoaded);
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
	};

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

		return amount == 1 && hasTestServer;
	}

	function fistServervolumesLoaded(volumes) {
		if (serverVolumesLoaded) return;
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
		var $container = $("<div>", {
			class: "flex-center"
		})
			.css("flex-wrap", "wrap")
			.css("min-height", "300px");
		server.$details = makeServerDetails(server);
		server.$spinner = makeSpinner(server);
		server.$downloadSpeedChart = makeDownloadSpeedChart(server);
		server.$downloadVolumeChart = makeDownloadVolumeChart(server);
		server.$details.css("flex-grow", "1").css("flex-basis", "400px");
		server.$spinner.css("flex-grow", "3").css("flex-basis", "600px");
		server.$downloadSpeedChart
			.css("flex-grow", "3")
			.css("flex-basis", "600px")
			.css("overflow", "hidden");
		server.$downloadVolumeChart
			.css("flex-grow", "3")
			.css("flex-basis", "600px")
			.css("overflow", "hidden");
		server.$details.hide();
		server.$downloadSpeedChart.hide();
		server.$downloadVolumeChart.hide();
		$container.append(
			server.$details,
			server.$spinner,
			server.$downloadSpeedChart,
			server.$downloadVolumeChart
		);
		$StatisticsTable.append($container, "<hr>");
	}

	function makeResetButton(server) {
		var $btn = $(
			'<button class="btn btn-default" title="Reset counters">'
			+ '<i class="material-icon">refresh</i>Reset'
			+ '</button>'
		);
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
		var html = '<div class="statistics__server-details">';
		html += "<h4>Server Details</h4>";
		html +=
			'<table class="table table-condensed table-bordered table-fixed server-details__table">';
		html += "<tr><th>Name:</th><td>".concat(server.name, "</td></tr>");
		html += "<tr><th>Host:</th><td>".concat(server.host, "</td></tr>");
		html += "<tr><th>Connections:</th><td>".concat(
			server.connections,
			"</td></tr>"
		);
		html += "<tr><th>Active:</th><td>".concat(
			server.active ? "<span class='txt-success'>Yes</span>" : "<span class='txt-important'>No</span>",
			"</td></tr>"
		);
		html += "</table>";
		html += "<h4>Bandwidth Usage</h4>";
		html +=
			'<table class="table table-condensed table-bordered table-fixed server-details__table">';
		html += '<tr><th>Today:</td><td id="'.concat(
			server.id,
			'_TodayDownloaded"></td></tr>'
		);
		html += '<tr><th>This Week:</th><td id="'.concat(
			server.id,
			'_WeekDownloaded"></td></tr>'
		);
		html += '<tr><th>This Month:</th><td id="'.concat(
			server.id,
			'_MonthDownloaded"></td></tr>'
		);
		html += '<tr><th>Total:</td><td id="'.concat(
			server.id,
			'_TotalDownloaded"></td></tr>'
		);
		html += "</table>";
		html += "<h4>Article Statistics</h4>";
		html += '<table class="table table-condensed table-bordered table-fixed">';
		html += '<tr><th>Success/Failed:</th><td id="'.concat(
			server.id,
			'_Articles"></td></tr>'
		);
		html += '<tr><th>Completion:</td><td id="'.concat(
			server.id,
			'_Completion"></td></tr>'
		);
		html += "</table>";
		html += "</div>";

		var $controls = $("<div>", {
			class: "btn-group"
		}).css("padding", "5px 0");

		var $serverConfigBtn = makeConfigServerBtn(server);
		var $resetCountersBtn = makeResetButton(server);
		$controls.append($serverConfigBtn, $resetCountersBtn);

		return $(html).append($controls);
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
					break;

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
		if (!servervolumes[server.id])
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
		);

		$successArticles.addClass("txt-success");
		var $failedArticles = $(
			"<span>".concat(Util.formatNumber(failedSum), "</span>")
		);

		$failedArticles.addClass("txt-important");
		var $articles = $("#".concat(server.id, "_Articles"));

		$articles.empty();
		$articles.append($successArticles, " / ", $failedArticles);

		$("#".concat(server.id, "_TodayDownloaded")).text(
			Util.formatSizeMB(server.todaySizeMB, server.todaySizeLo)
		);
		$("#".concat(server.id, "_WeekDownloaded")).text(
			Util.formatSizeMB(server.weekSizeMB, server.weekSizeLo)
		);
		$("#".concat(server.id, "_MonthDownloaded")).text(
			Util.formatSizeMB(server.monthSizeMB, server.monthSizeLo)
		);
		$("#".concat(server.id, "_TotalDownloaded")).text(
			Util.formatSizeMB(server.totalSizeMB, server.totalSizeLo)
		);
		$("#".concat(server.id, "_Completion")).text(completion);
		server.showDetails();
	}

	function makeSpinner(server) {
		var html = '<div class="statistics__chart-block-spinner">';
		html += '<i class="statistics__chart-block-spinner material-icon spinner"';
		html += 'id="'
			.concat(server.id, "_ChartSpinner-")
			.concat(server.activeChart, '">progress_activity</i>');
		html += "</div>";
		return $(html);
	}

	function makeConfigServerBtn(server) {
		var $btn = $(
			'<button class="btn btn-default" title="News Server configuration">'
			+ '<i class="material-icon">settings</i>Config'
			+ '</button>'
		)
			.attr("data-optid", "S_Server" + server.id + "_Active")
			.on("click", function (ev) {
				$('#ConfigTabLink').click();
				Options.loadConfig({
					complete: function (config) {
						Config.buildPage(config);
						setTimeout(function () {
							$("a[href$='#NEWS-SERVERS']").click();
							Config.scrollToOption(ev, $btn);
						}, 500);
					},
					configError: Config.loadConfigError,
					serverTemplateError: Config.loadServerTemplateError
				});
			});

		return $btn;
	}

	this.navigateToNewsServersConfiguration = function() {
		$('#ConfigTabLink').click();
		Options.loadConfig({
			complete: function (config) {
				Config.buildPage(config);
				setTimeout(function () {
					$("a[href$='#NEWS-SERVERS']").click();
				}, 500);
			},
			configError: Config.loadConfigError,
			serverTemplateError: Config.loadServerTemplateError
		});
	}

	function makeToggleChartBtn(server) {
		var $container = $("<div>", {
			class: "btn-group"
		}).css("padding-bottom", "5px");
		var $speedChartBtn = $("<button>", {
			class: "btn btn-default btn-active",
			title: "Show the Download Speed chart",
			text: "Speed"
		}).on("click", function () {
			server.toggleChart();
			redrawChart(server);
		});
		var $volumeChartBtn = $("<button>", {
			class: "btn btn-default",
			title: "Show the Downloaded Volume chart",
			text: "Volume"
		}).on("click", function () {
			server.toggleChart();
			redrawChart(server);
		});

		server.$speedChartBtn = $speedChartBtn;
		server.$volumeChartBtn = $volumeChartBtn;
		$container.append($speedChartBtn, $volumeChartBtn);
		return $container;
	}

	function makeDownloadSpeedChart(server) {
		var $container = $("<div>", {
			class: "statistics"
		});
		var $title = makeToggleChartBtn(server);
		var $toolbar = $("<div>", {
			class: "btn-toolbar form-inline section-toolbar",
			id: "".concat(server.id, "_Toolbar-").concat(server.DOWNLOAD_SPEED_CHART)
		}).css("margin-bottom", "0");
		var $timeBlockTop = $("<div>", {
			class: "btn-group phone-hide",
			id: ""
				.concat(server.id, "_TimeBlockTop-")
				.concat(server.DOWNLOAD_SPEED_CHART)
		});
		var $minButton = $("<button>", {
			class: "btn btn-default btn-active volume-range",
			id: ""
				.concat(server.id, "_Volume_MIN-")
				.concat(server.DOWNLOAD_SPEED_CHART),
			title: "Show last 60 seconds",
			text: "60 Seconds"
		}).on("click", function () {
			chooseRange(server, "MIN");
		});
		var $5minButton = $("<button>", {
			class: "btn btn-default volume-range",
			id: ""
				.concat(server.id, "_Volume_5MIN-")
				.concat(server.DOWNLOAD_SPEED_CHART),
			title: "Show last 5 minutes",
			text: "5 Minutes"
		}).on("click", function () {
			chooseRange(server, "5MIN");
		});
		var $hourButton = $("<button>", {
			class: "btn btn-default volume-range",
			id: ""
				.concat(server.id, "_Volume_HOUR-")
				.concat(server.DOWNLOAD_SPEED_CHART),
			title: "Show 60 minutes",
			text: "60 Minutes"
		}).on("click", function () {
			chooseRange(server, "HOUR");
		});
		$timeBlockTop.append($minButton, $5minButton, $hourButton);
		$toolbar.append($timeBlockTop);
		var $phoneButtons = $("<div>", {
			class: "btn-group phone-only inline"
		}).append(
			$("<button>", {
				class: "btn btn-default btn-active volume-range",
				id: ""
					.concat(server.id, "_Volume_MIN2-")
					.concat(server.DOWNLOAD_SPEED_CHART),
				title: "Show last 60 seconds",
				text: "60 s"
			}).on("click", function () {
				chooseRange(server, "MIN");
			}),
			$("<button>", {
				class: "btn btn-default volume-range",
				id: ""
					.concat(server.id, "_Volume_5MIN2-")
					.concat(server.DOWNLOAD_SPEED_CHART),
				title: "Show last 5 minutes",
				text: "5 m"
			}).on("click", function () {
				chooseRange(server, "5MIN");
			}),
			$("<button>", {
				class: "btn btn-default volume-range",
				id: ""
					.concat(server.id, "_Volume_HOUR2-")
					.concat(server.DOWNLOAD_SPEED_CHART),
				title: "Show last 60 minutes",
				text: "60 m"
			}).on("click", function () {
				chooseRange(server, "HOUR");
			})
		);
		$toolbar.append($phoneButtons);
		var $tooltip = $("<div>", {
			class: "statistics__tooltip",
			id: "".concat(server.id, "_Tooltip-").concat(server.DOWNLOAD_SPEED_CHART),
			text: "--"
		});
		var $chartBlock = $("<div>", {
			class: "statistics__chartblock"
		});

		var $chart = $("<div>", {
			class: "statistics__chart",
			id: ""
				.concat(server.id, "_Chart-")
				.concat(server.DOWNLOAD_SPEED_CHART)
		});

		$chartBlock.append($chart);
		$container.append($title, $toolbar, $tooltip, $chartBlock);
		return $container;
	}

	function makeDownloadVolumeChart(server) {
		var $container = $("<div>", {
			class: "statistics"
		});
		var $title = makeToggleChartBtn(server);
		var $toolbar = $("<div>", {
			class: "btn-toolbar form-inline section-toolbar",
			id: ""
				.concat(server.id, "_Toolbar-")
				.concat(server.DOWMLOADED_VOLUME_CHART)
		}).css("margin-bottom", "5px");
		var $startDateInput = $("<input>", {
			type: "date"
		});
		var $sep = $("<span> - </span>");
		var $endDateInput = $("<input>", {
			type: "date"
		});
		var startDateStr = datetime.formatDateForInput(server.startDate);
		var endDateStr = datetime.formatDateForInput(server.endDate);
		$startDateInput.val(startDateStr);
		$endDateInput.val(endDateStr);
		$startDateInput.attr("max", endDateStr);
		$endDateInput.attr("min", startDateStr);
		$startDateInput.on("change", function () {
			var startDate = $(this).val();
			server.startDate = new Date(startDate);
			$endDateInput.attr("min", datetime.formatDateForInput(startDate));
			redrawVolumeChart(server);
		});
		$endDateInput.on("change", function () {
			var endDate = $(this).val();
			server.endDate = new Date(endDate);
			$startDateInput.attr("max", datetime.formatDateForInput(endDate));
			redrawVolumeChart(server);
		});
		$toolbar.append($startDateInput, $sep, $endDateInput);
		var $tooltip = $("<div>", {
			class: "statistics__tooltip",
			id: ""
				.concat(server.id, "_Tooltip-")
				.concat(server.DOWMLOADED_VOLUME_CHART),
			text: "--"
		});

		var $chartBlock = $("<div>", {
			class: "statistics__chartblock"
		});

		var $chart = $("<div>", {
			class: "statistics__chart",
			id: ""
				.concat(server.id, "_Chart-")
				.concat(server.DOWMLOADED_VOLUME_CHART)
		});

		$chartBlock.append($chart);
		$container.append($title, $toolbar, $tooltip, $chartBlock);
		return $container;
	}

	function servervolumesLoaded(volumes) {
		var sameDay = servervolumes[0].DaySlot === volumes[0].DaySlot;
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
			var buffer = rerangeCircularBuffer(
				servervolumes[serverNo].BytesPerSeconds,
				servervolumes[serverNo].SecSlot
			);
			for (var i = 0; i < buffer.length; i++) {
				// the current slot may be not fully filled yet,
				// to make the chart smoother for current slot 
				// we show the previous slot as current.
				if (i === 59) {
					addData(buffer[58], i + "s", 1);
				}
				else {
					addData(buffer[i], i + "s", 1);
				}
			}
			curPoint = 59;
		}

		function drawFiveMinuteGraph() {
			var buffer = rerangeCircularBuffer(
				servervolumes[serverNo].BytesPerMinutes,
				servervolumes[serverNo].MinSlot
			);
			for (var i = 55; i < buffer.length; i++) {
				addData(buffer[i], i - 55 + "m", 60);
			}
			curPoint = 4;
		}

		function drawHourGraph() {
			var buffer = rerangeCircularBuffer(
				servervolumes[serverNo].BytesPerMinutes,
				servervolumes[serverNo].MinSlot
			);
			for (var i = 0; i < buffer.length; i++) {
				addData(buffer[i], i + "m", 60);
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

		var serieData =
			maxSizeMb >= 1024 * 1024
				? chartSpeedTb
				: maxSizeMb >= 1024
					? chartSpeedGb
					: maxSizeMb >= 1
						? chartSpeedMb
						: chartSpeedKb;
		var units =
			maxSizeMb >= 1024 * 1024
				? " Tbit/s"
				: maxSizeMb >= 1024
					? " Gbit/s"
					: maxSizeMb >= 1
						? " Mbit/s"
						: " Kbit/s";
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
		server.hideSpinner();

		var $chart = $("#".concat(server.id, "_Chart-").concat(server.activeChart));
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
						chartMouseOver(server, env, serie, index, mouseAreaData);
					},
					onMouseExit: function onMouseExit(env, serie, index, mouseAreaData) {
						chartMouseExit(server, env, serie, index, mouseAreaData);
					},
					onMouseOut: function onMouseOut(env, serie, index, mouseAreaData) {
						chartMouseExit(server, env, serie, index, mouseAreaData);
					}
				})
			);

		simulateMouseEvent(server, chartMouseExit);
	}

	function redrawVolumeChart(server) {
		var serverNo = server.id;
		var startDate = server.startDate;
		var endDate = server.endDate;
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
		var dates = datetime.getDateRange(startDate, endDate);

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
				var slot = Util.getDaySinceUnixEpoch(date);
				if (slot >= firstDay) {
					var idx = slot - firstDay;
					if (idx <= daySlot)
						addData(bytesPerDays[idx], label, "");
					else
						addData({ SizeMB: 0, SizeLo: 0 }, label, "");
				} else {
					addData({ SizeMB: 0, SizeLo: 0 }, label, "");
				}
			}
		}

		drawGraph();

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
			units: units,
			curPoint: curPoint,
			labels: dataLabels,
			sumMB: sumMB,
			sumLo: sumLo
		});

		server.showChart();
		server.hideSpinner();

		var $chart = $("#".concat(server.id, "_Chart-").concat(server.activeChart));
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
						chartMouseOver2(server, env, serie, index, mouseAreaData);
					},
					onMouseExit: function onMouseExit(env, serie, index, mouseAreaData) {
						chartMouseExit2(server, env, serie, index, mouseAreaData);
					},
					onMouseOut: function onMouseOut(env, serie, index, mouseAreaData) {
						chartMouseExit2(server, env, serie, index, mouseAreaData);
					}
				})
			);

		simulateMouseEvent(server, chartMouseExit2);
	}

	function isParentContainerRendered($chart) {
		var containerWidth = $chart.parent().width();
		if (containerWidth > 0)
			return true;
		else
			return false;
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
		if (server.activeChart === server.DOWNLOAD_SPEED_CHART) {
			redrawSpeedChart(server);
		} else {
			redrawVolumeChart(server);
		}
	}

	function chartMouseOver(server, env, serie, index, mouseAreaData) {
		var data = server.getChartData();
		if (data.mouseOverIndex > -1) {
			var chart = $(
				"#".concat(server.id, "_Chart-").concat(server.activeChart)
			);
			if (!chart) return;
			var env = chart.data("elycharts_env");
			if (env.mouseAreas[data.mouseOverIndex])
				$.elycharts.mousemanager.onMouseOutArea(
					env,
					false,
					data.mouseOverIndex,
					env.mouseAreas[data.mouseOverIndex]
				);
		}

		var tooltip = $(
			"#".concat(server.id, "_Tooltip-").concat(server.activeChart)
		);
		if (!tooltip) return;
		var data = server.getChartData();
		var value = data.data[index];
		if (value === undefined || value === null) {
			value = "0.0";
		} else {
			value = value.toFixed(1);
		}
		var title = value + data.units;
		tooltip.html('<span class="stat-size">' + title + "</span>");
		tooltip.html(
			data.labels[index] + ': <span class="stat-size">' + title + "</span>"
		);
	}

	function chartMouseOver2(server, env, serie, index, mouseAreaData) {
		var data = server.getChartData();
		if (data.mouseOverIndex > -1) {
			var chart = $(
				"#".concat(server.id, "_Chart-").concat(server.activeChart)
			);
			if (!chart) return;
			var env = chart.data("elycharts_env");
			if (env.mouseAreas[data.mouseOverIndex])
				$.elycharts.mousemanager.onMouseOutArea(
					env,
					false,
					data.mouseOverIndex,
					env.mouseAreas[data.mouseOverIndex]
				);
		}
		data.mouseOverIndex = index;
		var tooltip = $(
			"#".concat(server.id, "_Tooltip-").concat(server.activeChart)
		);

		if (!tooltip) return;

		var data = server.getChartData();
		tooltip.html(
			data.labels[index] +
			': <span class="stat-size">' +
			Util.formatSizeMB(data.dataMB[index], data.dataLo[index]) +
			"</span>"
		);
	}

	function chartMouseExit(server, env, serie, index, mouseAreaData) {
		var tooltip = $(
			"#".concat(server.id, "_Tooltip-").concat(server.activeChart)
		);
		if (!tooltip) return;
		var data = server.getChartData();
		data.mouseOverIndex = -1;
		var value = data.data[data.curPoint];
		if (value === undefined || value === null) {
			value = "0.0";
		} else {
			value = value.toFixed(1);
		}
		var title = value + data.units;
		tooltip.html('<span class="stat-size">' + title + "</span>");
	}

	function chartMouseExit2(server, env, serie, index, mouseAreaData) {
		var tooltip = $(
			"#".concat(server.id, "_Tooltip-").concat(server.activeChart)
		);
		if (!tooltip) return;
		var data = server.getChartData();
		if (data.sumMB === undefined || data.sumLo === undefined) return;

		data.mouseOverIndex = -1;
		var title = Util.formatSizeMB(data.sumMB, data.sumLo);
		tooltip.html(
			"Selected period:" + " " + '<span class="stat-size">' + title + "</span>"
		);
	}

	function simulateMouseEvent(server, onExitFn) {
		var data = server.getChartData();
		if (data.mouseOverIndex > -1) {
			var chart = $(
				"#".concat(server.id, "_Chart-").concat(server.activeChart)
			);
			if (!chart) return;
			var env = chart.data("elycharts_env");
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
		var id = server.id;
		var chartId = server.activeChart;
		var prevTab = $(
			"#"
				.concat(id, "_Volume_")
				.concat(prevRange, "-")
				.concat(chartId, ", #")
				.concat(id, "_Volume_")
				.concat(prevRange, "2-")
				.concat(chartId)
		);
		var newTab = $(
			"#"
				.concat(id, "_Volume_")
				.concat(range, "-")
				.concat(chartId, ", #")
				.concat(id, "_Volume_")
				.concat(range, "2-")
				.concat(chartId)
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
