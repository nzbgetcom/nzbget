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

function SectionHealth(alerts_, options_, sectionName) {
	var name = sectionName;
	var alerts = alerts_;
	var health = {
		info: [],
		warnings: [],
		errors: [],
	}

	var options = {};

	init(options_);

	this.getAlerts = function () {
		return alerts;
	}

	this.getErrorsCount = function () {
		return health.errors.length;
	}

	this.getWarningsCount = function () {
		return health.warnings.length;
	}

	this.getInfoCount = function () {
		return health.info.length;
	}

	this.getCheck = function (name) {
		return options[name];
	}

	this.getName = function () {
		return name;
	}

	function init(options_) {
		options = options_;
		Object.values(options_).forEach(function (check) {
			if (check.Severity === "Info") {
				health.info.push(check);
			}
			else if (check.Severity === "Warning") {
				health.warnings.push(check);
			}
			else if (check.Severity === "Error") {
				health.errors.push(check);
			}
		});
	}
}

function HealthReport() {
	var sections = {};
	var infoCount = 0;
	var warningsCount = 0;
	var errorsCount = 0;

	this.getInfoCount = function () {
		return infoCount;
	}

	this.getWarningsCount = function () {
		return warningsCount;
	}

	this.getErrorsCount = function () {
		return errorsCount;
	}

	this.getSection = function (name) {
		return sections[name];
	}

	this.addSection = function (section) {
		sections[section.getName()] = section;
	}

	this.computeCounters = function () {
		infoCount = 0;
		warningsCount = 0;
		errorsCount = 0;

		Object.values(sections).forEach(function (section) {
			infoCount += section.getInfoCount();
			warningsCount += section.getWarningsCount();
			errorsCount += section.getErrorsCount();
		});
	}
}

var SystemHealth = (new function ($) {
	'use strict';

	var SECTION_NAMES = {
		PATHS: "Paths",
		NEWS_SERVERS: "NewsServers",
		SECURITY: "Security",
		CATEGORIES: "Categories",
		FEEDS: "Feeds",
		INCOMING_NZBS: "IncomingNzb",
		DOWNLOAD_QUEUE: "DownloadQueue",
		CONNECTION: "Connection",
		LOGGING: "Logging",
		CHECK_AND_REPAIR: "CheckAndRepair",
		UNPACK: "Unpack",
		SCHEDULER: "Scheduler",
		EXTENSION_SCRIPTS: "ExtensionScripts"
	};

	var $appHealthBadge;
	var $ConfigTitleStatus;
	var $SystemInfo_Health;
	var $SystemInfoNavBtnBadge;
	var sectionsReport = {};
	var alertsReport = {};
	this.SEVERITY = {
		OK: 0,
		INFO: 1,
		WARNING: 2,
		ERROR: 3,
	}
	var SEVERITY_STYLE = {
		INFO: "success",
		WARNING: "warning",
		ERROR: "important",
	}

	var statusHandler =
	{
		update: function (status) {

		}
	}

	this.init = function () {
		$appHealthBadge = $("#ConfigTabAppHealthBadge");
		$ConfigTitleStatus = $('#ConfigTitleStatus');
		$SystemInfoNavBtnBadge = $('#SystemInfoNavBtnBadge');
		$SystemInfo_Health = $('#SystemInfo_Health');
		RPC.call('systemhealth', [],
			function(health)
			{
				alertsReport = makeAlertsReport(health["Alerts"]);
				sectionsReport = makeHealthReport(health["Sections"]);
				redrawGlobalBadges(sectionsReport);
				redraw(health);
			});
	}

	function redraw(health) {
		var alertsErrors = health['Alerts'];
		var status = health['Status'];

		$SystemInfo_Health.empty();
		$ConfigTitleStatus.empty();

		if (status === "Error")
		{
			$ConfigTitleStatus.append('<span class="text-error">ERROR</>');
		}
		else if (status === "Warning")
		{
			$ConfigTitleStatus.append('<span class="text-warning">WARNING</>');
		}
		else if (status === "Info")
		{
			$ConfigTitleStatus.append('<span class="text-success">OK</>');
		}
		else if (status === "Ok")
		{
			$ConfigTitleStatus.append('<span class="text-success">OK</>');
		}

		$SystemInfo_Health.show();
		Object.values(alertsErrors).forEach(function (option) {
			var link = $('<a href="#"></a>')
				.on('click', function (e) {
					e.preventDefault();
					var $btn = $('<a class="option" href="#">' + option.Name + '</a>')
					Config.scrollToOption(e, $btn);
				});
			if (option.Status.Severity == "Error") {
				link.append('<p class="text-error"><i class="option-alert__icon material-icon">error</i><span>' + option.Status.Message + '</span></p>');
				$SystemInfo_Health.append(link);
			}

			else if (option.Status.Severity == "Warning") {
				link.append('<p class="text-warning"><i class="option-alert__icon material-icon">warning</i><span>' + option.Status.Message + '</span></p>');
				$SystemInfo_Health.append(link);
			}
			else if (option.Status.Severity == "Info") {
				link.append('<p class="text-success"><i class="option-alert__icon material-icon">info</i><span>' + option.Status.Message + '</span></p>');
				$SystemInfo_Health.append(link);
			}
		});
	}

	this.makeBadges = function (section) {
		if (!section) return null;

		var wrapper = $('<span style="margin-left: 5px;">');
		// var $infoBadge = $('<span class="badge" style="margin-left: 3px;"></span>');
		var $warningsBadge = $('<span class="badge" style="margin-left: 3px;"></span>');
		var $errorsBadge = $('<span class="badge" style="margin-left: 3px;"></span>');
		// var infoCount = section.getInfoCount();
		var warningsCount = section.getWarningsCount();
		var errorsCount = section.getErrorsCount();
		// toggleBadgeVisibility($infoBadge, infoCount, SEVERITY_STYLE.INFO);
		toggleBadgeVisibility($warningsBadge, warningsCount, SEVERITY_STYLE.WARNING);
		toggleBadgeVisibility($errorsBadge, errorsCount, SEVERITY_STYLE.ERROR);
		wrapper.append($errorsBadge, $warningsBadge);

		return wrapper;
	}

	this.getSection = function (name) {
		return sectionsReport.getSection(name);
	}

	this.getCheck = function (section, name) {
		if (!section)
			return null;

		return section.getCheck(name);
	}

	function redrawGlobalBadges(healthReport) {
		// var infoCount = healthReport.getInfoCount();
		var warningsCount = healthReport.getWarningsCount() + (alertsReport.getWarningsCount ? alertsReport.getWarningsCount() : 0);
		var errorsCount = healthReport.getErrorsCount() + (alertsReport.getErrorsCount ? alertsReport.getErrorsCount() : 0);
		//toggleBadgeVisibility($appHealthInfoBadge, infoCount, SEVERITY_STYLE.INFO);
		if (errorsCount > 0)
		{
			toggleGlobalBadgeVisibility($appHealthBadge, errorsCount, SEVERITY_STYLE.ERROR);
			toggleGlobalBadgeVisibility($SystemInfoNavBtnBadge, errorsCount, SEVERITY_STYLE.ERROR);
			return;
		}

		toggleGlobalBadgeVisibility($appHealthBadge, warningsCount, SEVERITY_STYLE.WARNING);	
		toggleGlobalBadgeVisibility($SystemInfoNavBtnBadge, warningsCount, SEVERITY_STYLE.WARNING);
	}

	function makeAlertsReport(alerts) {
		var infoCount = 0;
		var warningsCount = 0;
		var errorsCount = 0;

		if (Array.isArray(alerts)) {
			alerts.forEach(function (check) {
				if (check.Severity === "Info") infoCount++;
				else if (check.Severity === "Warning") warningsCount++;
				else if (check.Severity === "Error") errorsCount++;
			});
		}

		return {
			getInfoCount: function () { return infoCount; },
			getWarningsCount: function () { return warningsCount; },
			getErrorsCount: function () { return errorsCount; }
		};
	}

	function makeHealthReport(health) {
		var healthReport = new HealthReport();

		health.forEach(function (section) {
			var optionsMap = {};
			var alerts = section.Alerts;
			section.Options.forEach(function (option) {
				optionsMap[option.Name] = option.Status;
			});

			section.Subsections.forEach(function (sub) {
				sub.Options.forEach(function (option) {
					var key = (sub.Name ? (sub.Name + '.') : '') + option.Name;
					optionsMap[key] = option.Status;
				});
			});

			var sectionName = section.Name;
			if (sectionName === SECTION_NAMES.PATHS)
			{
				sectionName = "PATHS";
			}
			else if (sectionName === SECTION_NAMES.NEWS_SERVERS)
			{
				sectionName = "NEWS-SERVERS";
			}
			else if (sectionName === SECTION_NAMES.SECURITY)
			{
				sectionName = "SECURITY";
			}
			else if (sectionName === SECTION_NAMES.CATEGORIES)
			{
				sectionName = "CATEGORIES";
			}
			else if (sectionName === SECTION_NAMES.FEEDS)
			{
				sectionName = "RSS FEEDS";
			}
			else if (sectionName === SECTION_NAMES.INCOMING_NZBS)
			{
				sectionName = "INCOMING NZBS";
			}
			else if (sectionName === SECTION_NAMES.DOWNLOAD_QUEUE)
			{
				sectionName = "DOWNLOAD QUEUE";
			}
			else if (sectionName === SECTION_NAMES.CHECK_AND_REPAIR)
			{
				sectionName = "CHECK AND REPAIR";
			}
			else if (sectionName === SECTION_NAMES.UNPACK)
			{
				sectionName = "UNPACK";
			}
			else if (sectionName === SECTION_NAMES.CONNECTION)
			{
				sectionName = "CONNECTION";
			}
			else if (sectionName === SECTION_NAMES.LOGGING)
			{
				sectionName = "LOGGING";
			}
			else if (sectionName === SECTION_NAMES.SCHEDULER)
			{
				sectionName = "SCHEDULER";
			}
			else if (sectionName === SECTION_NAMES.EXTENSION_SCRIPTS)
			{
				sectionName = "EXTENSION SCRIPTS";
			}
			healthReport.addSection(new SectionHealth(alerts, optionsMap, sectionName));
		});

		healthReport.computeCounters();

		return healthReport;
	}

	function toggleBadgeVisibility($badge, checks, severity) {
		if (checks > 0) {
			$badge.show();
			$badge.addClass("badge-" + severity)
			$badge.text(checks);
		}
		else {
			$badge.hide();
		}
	}

	function toggleGlobalBadgeVisibility($badge, checks, severity) {
		if (checks > 0) {
			$badge.show();
			$badge.addClass("system-health-badge--" + severity.toLowerCase());
		}
		else {
			$badge.hide();
		}
	}

}(jQuery));
