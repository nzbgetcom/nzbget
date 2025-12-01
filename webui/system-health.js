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

function SectionHealth(issues_, options_, sectionName) {
	var name = sectionName;
	var issues = issues_;
	var options = options_;
	var health = {
		info: [],
		warnings: [],
		errors: [],
	}

	init(issues_, options_);

	this.getIssues = function () {
		return issues;
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

	function init(issues_, options_) {
		Object.values(options_).forEach(function (opt) {
			if (opt.Severity === "Info") {
				health.info.push(opt);
			}
			else if (opt.Severity === "Warning") {
				health.warnings.push(opt);
			}
			else if (opt.Severity === "Error") {
				health.errors.push(opt);
			}
		});
		issues_.forEach(function (issue) {
			if (issue.Severity === "Info") {
				health.info.push({name: sectionName, message: issue.Message });
			}
			else if (issue.Severity === "Warning") {
				health.warnings.push({name: sectionName, message: issue.Message });
			}
			else if (issue.Severity === "Error") {
				health.errors.push({name: sectionName, message: issue.Message });
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
		Paths: "PATHS",
		NewsServers: "NEWS-SERVERS",
		Security: "SECURITY",
		Categories: "CATEGORIES",
		Feeds: "FEEDS",
		IncomingNzb: "INCOMING NZBS",
		DownloadQueue: "DOWNLOAD QUEUE",
		Connection: "CONNECTION",
		Logging: "LOGGING",
		CheckAndRepair: "CHECK AND REPAIR",
		Unpack: "UNPACK",
		Scheduler: "SCHEDULER",
		ExtensionScripts: "EXTENSION SCRIPTS"
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

	this.init = function () {
		$appHealthBadge = $("#ConfigTabAppHealthBadge");
		$ConfigTitleStatus = $('#ConfigTitleStatus');
		$SystemInfoNavBtnBadge = $('#SystemInfoNavBtnBadge');
		$SystemInfo_Health = $('#SystemInfo_Health');
		RPC.call('systemhealth', [],
			function (health) {
				console.log(health)
				alertsReport = makeAlertsReport(health);
				sectionsReport = makeHealthReport(health["Sections"]);
				redrawGlobalBadges(alertsReport, sectionsReport);
				redraw(alertsReport);
			});
	}

	function redraw(alertsReport) {
		$SystemInfo_Health.empty();
		$ConfigTitleStatus.empty();

		// if (status === "Error")
		// {
		// 	$ConfigTitleStatus.append('<span class="text-error">ERROR</>');
		// }
		// else if (status === "Warning")
		// {
		// 	$ConfigTitleStatus.append('<span class="text-warning">WARNING</>');
		// }
		// else if (status === "Info")
		// {
		// 	$ConfigTitleStatus.append('<span class="text-success">OK</>');
		// }
		// else if (status === "Ok")
		// {
		// 	$ConfigTitleStatus.append('<span class="text-success">OK</>');
		// }

		$SystemInfo_Health.show();
		alertsReport.errors.forEach(function (alert) {
			var link = $('<a href="#"></a>').on('click', function(e) {
				e.preventDefault();
				Config.navigateTo(alert.name);
			});
			link.append('<p class="text-error"><i class="option-alert__icon material-icon">error</i><span>' + alert.message + '</span></p>');
			$SystemInfo_Health.append(link);
		});

		alertsReport.warnings.forEach(function (alert) {
			var link = $('<a href="#"></a>')
				.on('click', function (e) {
					e.preventDefault();
					var $btn = $('<a class="option" href="#">' + alert.name + '</a>');
					Config.scrollToOption(e, $btn);
				});
			link.append('<p class="text-warning"><i class="option-alert__icon material-icon">warning</i><span>' + alert.message + '</span></p>');
			$SystemInfo_Health.append(link);
		});

		alertsReport.info.forEach(function (alert) {
			var link = $('<a href="#"></a>')
				.on('click', function (e) {
					e.preventDefault();
					var $btn = $('<a class="option" href="#">' + alert.name + '</a>');
					Config.scrollToOption(e, $btn);
				});
			link.append('<p class="text-success"><i class="option-alert__icon material-icon">info</i><span>' + alert.message + '</span></p>');
			$SystemInfo_Health.append(link);
		});
	}

	this.makeBadges = function (section) {
		if (!section) return null;

		var wrapper = $('<span style="margin-left: 5px;">');
		var $infoBadge = $('<span class="badge" style="margin-left: 3px;"></span>');
		var $warningsBadge = $('<span class="badge" style="margin-left: 3px;"></span>');
		var $errorsBadge = $('<span class="badge" style="margin-left: 3px;"></span>');
		var infoCount = section.getInfoCount();
		var warningsCount = section.getWarningsCount();
		var errorsCount = section.getErrorsCount();
		toggleBadgeVisibility($infoBadge, infoCount, SEVERITY_STYLE.INFO);
		toggleBadgeVisibility($warningsBadge, warningsCount, SEVERITY_STYLE.WARNING);
		toggleBadgeVisibility($errorsBadge, errorsCount, SEVERITY_STYLE.ERROR);
		wrapper.append($errorsBadge, $warningsBadge, $warningsBadge);

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

	function redrawGlobalBadges(alertsReport, sectionsReport) {
		var infoCount = sectionsReport.getInfoCount() + (alertsReport.getInfoCount ? alertsReport.getInfoCount() : 0);
		var warningsCount = sectionsReport.getWarningsCount() + (alertsReport.getWarningsCount ? alertsReport.getWarningsCount() : 0);
		var errorsCount = sectionsReport.getErrorsCount() + (alertsReport.getErrorsCount ? alertsReport.getErrorsCount() : 0);
		//toggleBadgeVisibility($appHealthInfoBadge, infoCount, SEVERITY_STYLE.INFO);
		if (errorsCount > 0) {
			toggleGlobalBadgeVisibility($appHealthBadge, errorsCount, SEVERITY_STYLE.ERROR);
			toggleGlobalBadgeVisibility($SystemInfoNavBtnBadge, errorsCount, SEVERITY_STYLE.ERROR);
			return;
		}

		// toggleGlobalBadgeVisibility($appHealthBadge, warningsCount, SEVERITY_STYLE.WARNING);	
		if (warningsCount > 0) {
			toggleGlobalBadgeVisibility($SystemInfoNavBtnBadge, warningsCount, SEVERITY_STYLE.WARNING);
			return;
		}

		if (infoCount > 0) {
			toggleGlobalBadgeVisibility($SystemInfoNavBtnBadge, infoCount, SEVERITY_STYLE.INFO);
			return;
		}
	}

	function makeAlertsReport(health) {
		var alerts = health.Alerts;
		var sections = health.Sections;
		var info = [];
		var warnings = [];
		var errors = [];

		alerts.forEach(function (alert) {
			if (alert.Severity === "Info") info.push({ name: alert.Source, message: alert.Message });
			else if (alert.Severity === "Warning") warnings.push({ name: alert.Source, message: alert.Message });
			else if (alert.Severity === "Error") errors.push({ name: alert.Source, message: alert.Message });
		});

		sections.forEach(function (section) {
			section.Issues.forEach(function (issue) {
				if (issue.Severity === "Info") info.push({ name: SECTION_NAMES[section.Name], message: issue.Message });
				else if (issue.Severity === "Warning") warnings.push({ name: SECTION_NAMES[section.Name], message: issue.Message });
				else if (issue.Severity === "Error") errors.push({ name: SECTION_NAMES[section.Name], message: issue.Message });
			});

			section.Subsections.forEach(function (sub) {
				sub.Options.forEach(function (option) {
					if (option.Severity === "Info") info.push({ name: sub.Name + "." + option.Name, message: option.Message });
					else if (option.Severity === "Warning") warnings.push({ name: sub.Name + "." + option.Name, message: option.Message });
					else if (option.Severity === "Error") errors.push({ name: sub.Name + "." + option.Name, message: option.Message });
				});
			});

			section.Options.forEach(function (option) {
				if (option.Severity === "Info") info.push({ name: option.Name, message: option.Message });
				else if (option.Severity === "Warning") warnings.push({ name: option.Name, message: option.Message });
				else if (option.Severity === "Error") errors.push({ name: option.Name, message: option.Message });
			});
		});

		return {
			info,
			warnings,
			errors,
		};
	}

	function makeHealthReport(sections) {
		var healthReport = new HealthReport();

		sections.forEach(function (section) {
			var optionsMap = {};
			var issues = section.Issues;
			section.Options.forEach(function (option) {
				optionsMap[option.Name] = { Message: option.Message, Severity: option.Severity };
			});

			section.Subsections.forEach(function (sub) {
				sub.Options.forEach(function (option) {
					var key = (sub.Name ? (sub.Name + '.') : '') + option.Name;
					optionsMap[key] = { Message: option.Message, Severity: option.Severity };
				});
			});
			healthReport.addSection(new SectionHealth(issues, optionsMap, SECTION_NAMES[section.Name]));
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
