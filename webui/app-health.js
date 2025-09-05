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

function SectionHealth(options_, sectionName)
{
	var name = sectionName;
	var health = {
		info: [],
		warnings: [],
		errors: []
	}

	var options = {};

	init(options_);

	this.getErrorsCount = function()
	{
		return health.errors.length;
	}

	this.getWarningsCount = function()
	{
		return health.warnings.length;
	}

	this.getInfoCount = function()
	{
		return health.info.length;
	}

	this.getCheck = function(name)
	{
		return options[name];
	}

	this.getName = function()
	{
		return name;
	}

	function init(options_)
	{
		options = options_;
		Object.values(options_).forEach(function (check) {
			if (check.Status === "Info") {
				health.info.push(check);
			}
			else if (check.Status === "Warning") {
				health.warnings.push(check);
			}
			else if (check.Status === "Error") {
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

	this.getInfoCount = function()
	{
		return infoCount;
	}

	this.getWarningsCount = function()
	{
		return warningsCount;
	}

	this.getErrorsCount = function()
	{
		return errorsCount;
	}

	this.getSection = function(name)
	{
		return sections[name];
	}

	this.addSection = function(section) 
	{
		sections[section.getName()] = section;
	}

	this.computeCounters = function()
	{
		infoCount = 0;
		warningsCount = 0;
		errorsCount = 0;

		Object.values(sections).forEach(function(section) {
			infoCount += section.getInfoCount();
			warningsCount += section.getWarningsCount();
			errorsCount += section.getErrorsCount();
		});
	}
}

var AppHealth = (new function($)
{
	'use strict';

	var $appHealthErrorsBadge;
	var $appHealthWarningsBadge;
	var $appHealthInfoBadge;
	var sectionsReport = {};
	var generalReport = {};
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
		update: function(status) 
		{
			sectionsReport = makeHealthReport(status["Health"]["Sections"]);
			generalReport = makeHealthReport(status["Health"]["General"]);
			redrawGlobalBadges(sectionsReport);
		}
	}

	this.init = function()
	{
		$appHealthInfoBadge = $("#AppHealthInfoBadge");
		$appHealthWarningsBadge = $("#AppHealthWarningsBadge");
		$appHealthErrorsBadge = $("#AppHealthErrorsBadge");
		Status.subscribe(statusHandler);
	}

	this.makeBadges = function(section)
	{
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
		wrapper.append($errorsBadge, $warningsBadge, $infoBadge);

		return wrapper;
	}

	this.getSection = function(name) 
	{
		return sectionsReport.getSection(name);
	}

	this.getCheck = function(section, name)
	{
		if (!section)
			return null;

		return section.getCheck(name);
	}

	function redrawGlobalBadges(healthReport)
	{
		var infoCount = healthReport.getInfoCount();
		var warningsCount = healthReport.getWarningsCount();
		var errorsCount = healthReport.getErrorsCount();
		toggleBadgeVisibility($appHealthInfoBadge, infoCount, SEVERITY_STYLE.INFO);
		toggleBadgeVisibility($appHealthWarningsBadge, warningsCount, SEVERITY_STYLE.WARNING);
		toggleBadgeVisibility($appHealthErrorsBadge, errorsCount, SEVERITY_STYLE.ERROR);
	}

	function makeHealthReport(health) {
		var healthReport = new HealthReport();

		Object.entries(health).forEach(function(entry) {
			var name = entry[0];
			var section = entry[1];
			healthReport.addSection(new SectionHealth(section, name.toUpperCase()));
		});

		healthReport.computeCounters();

		return healthReport;
	}

	function toggleBadgeVisibility($badge, checks, severity)
	{
		if (checks > 0)
		{
			$badge.show();
			$badge.addClass("badge-" + severity)
			$badge.text(checks);
		}
		else
		{
			$badge.hide();
		}
	}
	
}(jQuery));
