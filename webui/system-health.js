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
		Feeds: "RSS_FEEDS",
		IncomingNzbs: "INCOMING_NZBS",
		DownloadQueue: "DOWNLOAD_QUEUE",
		Connection: "CONNECTION",
		Logging: "LOGGING",
		CheckAndRepair: "CHECK_AND_REPAIR",
		Unpack: "UNPACK",
		Scheduler: "SCHEDULER",
		ExtensionScripts: "EXTENSION_SCRIPTS"
	};

	var SYSTEM_HEALTH_CHECK_OPTION = "SystemHealthCheck";
	var $appHealthBadge;
	var $ConfigTitleStatus;
	var $SystemInfo_Health;
	var $SystemInfoNavBtnBadge;
	var sectionsReport = {};
	var alertsReport = {};
	var healthCheckEnabled = true;
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
	
	this.update = function() {
		healthCheckEnabled = Options.option(SYSTEM_HEALTH_CHECK_OPTION) == "yes";
		if (healthCheckEnabled)
		{
			$appHealthBadge = $("#ConfigTabAppHealthBadge");
			$ConfigTitleStatus = $('#ConfigTitleStatus');
			$SystemInfoNavBtnBadge = $('#SystemInfoNavBtnBadge');
			$SystemInfo_Health = $('#SystemInfo_Health');
			RPC.call('systemhealth', [],
				function (health) {
					alertsReport = makeAlertsReport(health);
					sectionsReport = makeHealthReport(health["Sections"]);
					redrawGlobalBadges(alertsReport, sectionsReport);
					redraw(alertsReport);
				},
				function(err)
				{
					console.error(err);
				}
			);		
		}		
	}

	this.init = function () {
		Options.subscribe(this);
	}
	
	this.isHealthCheckEnabled = function() { return healthCheckEnabled; }

	function redraw(alertsReport) {
		var $container = $("#SystemInfo_Health");
		var $mainWrapper = $("#SystemInfoHealthSection");
		var accordionId = "healthAccordion";
		var sections = [
			{
				id: 'collapseErrors',
				label: 'Errors',
				data: alertsReport.errors,
				cssClass: 'txt-error',
				icon: 'error'
			},
			{
				id: 'collapseWarnings',
				label: 'Warnings',
				data: alertsReport.warnings,
				cssClass: 'txt-warning',
				icon: 'warning'
			},
			{
				id: 'collapseInfo',
				label: 'Info',
				data: alertsReport.info,
				cssClass: 'txt-success',
				icon: 'info'
			}
		];

		$container.empty();

		var hasContent = false;
		var $accordion = $('<div class="accordion" id="' + accordionId + '"></div>');

		sections.forEach(function (section) {
			if (!section.data || section.data.length === 0) {
				return;
			}

			hasContent = true;

			var $group = $('<div class="accordion-group"></div>');
			var $heading = $('<div class="accordion-heading"></div>');
			var $toggle = $('<span class="accordion-toggle" data-toggle="collapse"></span>');
			$toggle.attr('data-parent', '#' + accordionId);
			$toggle.attr('href', '#' + section.id);
			
			$toggle.html(
				'<i class="material-icon ' + section.cssClass + '">' + section.icon + '</i> ' +
				'<span class="' + section.cssClass + '">' + section.label + ' (' + section.data.length + ')</span>'
			);
			
			$heading.append($toggle);
			$group.append($heading);

			var $body = $('<div id="' + section.id + '" class="accordion-body collapse"></div>');
			var $inner = $('<div class="accordion-inner"></div>');
		
			section.data.forEach(function (alert) {
				var $link = $('<a href="#" style="display:block;margin-bottom: 5px;""></a>');
				$link.addClass(section.cssClass);
				
				$link.on('click', function (e) {
					e.preventDefault();
					var $mockBtn = $('<a class="option">' + alert.name + '</a>');
					Config.scrollToOption(e, $mockBtn);
					Config.navigateTo(alert.name);
				});

				$link.html('<span>' + alert.message + '</span>');
				$inner.append($link);
			});

			$body.append($inner);
			$group.append($body);
			$accordion.append($group);
		});

		if (!hasContent) {
			$mainWrapper.hide();
		} else {
			$mainWrapper.show();
			$container.append($accordion);
		}
	}

	this.makeBadges = function (section) {
		if (!section) return null;

		var wrapper = $('<span style="margin-left: 5px;">');
		var $errorsBadge = $('<span class="badge" style="margin-left: 3px;"></span>');
		var errorsCount = section.getErrorsCount();
		toggleBadgeVisibility($errorsBadge, errorsCount, SEVERITY_STYLE.ERROR);
		wrapper.append($errorsBadge);

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
		$SystemInfoNavBtnBadge.hide();

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
		var buckets = {
			"Info": [],
			"Warning": [],
			"Error": []
		};

		function add(severity, name, message) {
			if (buckets[severity]) {
				buckets[severity].push({ name: name, message: message });
			}
		}

		if (health.Alerts) {
			health.Alerts.forEach(function (alert) {
				add(alert.Severity, alert.Source, alert.Message);
			});
		}

		health.Sections.forEach(function (section) {
			section.Issues.forEach(function (issue) {
				add(issue.Severity, SECTION_NAMES[section.Name], issue.Message);
			});

			section.Options.forEach(function (option) {
				add(option.Severity, option.Name, option.Message);
			});

			section.Subsections.forEach(function (sub) {
				sub.Options.forEach(function (option) {
					var fullName = sub.Name + "." + option.Name;
					add(option.Severity, fullName, fullName + ': ' + option.Message);
				});
			});
		});

		return {
			info: buckets["Info"],
			warnings: buckets["Warning"],
			errors: buckets["Error"]
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
			$badge.addClass("txt-" + severity.toLowerCase());
		}
		else {
			$badge.hide();
		}
	}

}(jQuery));
