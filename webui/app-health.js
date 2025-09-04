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
	let name = sectionName;
	let health = {
		info: [],
		warnings: [],
		errors: []
	}

	let options = {};

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
	let sections = {};
	let infoCount = 0;
	let warningsCount = 0;
	let errorsCount = 0;

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

const AppHealth = (new function($)
{
	'use strict';

	let $appHealthErrorsBadge;
	let $appHealthWarningsBadge;
	let $appHealthInfoBadge;
	let healthReport = {};
	this.SEVERITY = {
		OK: 0,
		INFO: 1,
		WARNING: 2,
		ERROR: 3,
	}
	const SEVERITY_STYLE = {
		INFO: "success",
		WARNING: "warning",
		ERROR: "important",
	}

	const statusHandler = 
	{
		update: function(status) 
		{
			healthReport = makeHealthReport(status["Health"]);
			redrawGlobalBadges(healthReport);
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

		const wrapper = $('<span style="margin-left: 5px;">');
		const $infoBadge = $('<span class="badge" style="margin-left: 3px;"></span>');
		const $warningsBadge = $('<span class="badge" style="margin-left: 3px;"></span>');
		const $errorsBadge = $('<span class="badge" style="margin-left: 3px;"></span>');
		const infoCount = section.getInfoCount();
		const warningsCount = section.getWarningsCount();
		const errorsCount = section.getErrorsCount();
		toggleBadgeVisibility($infoBadge, infoCount, SEVERITY_STYLE.INFO);
		toggleBadgeVisibility($warningsBadge, warningsCount, SEVERITY_STYLE.WARNING);
		toggleBadgeVisibility($errorsBadge, errorsCount, SEVERITY_STYLE.ERROR);
		wrapper.append($errorsBadge, $warningsBadge, $infoBadge);

		return wrapper;
	}

	this.getSection = function(name) 
	{
		return healthReport.getSection(name);
	}

	this.getCheck = function(section, name)
	{
		if (!section)
			return null;

		return section.getCheck(name);
	}

	function redrawGlobalBadges(healthReport)
	{
		const infoCount = healthReport.getInfoCount();
		const warningsCount = healthReport.getWarningsCount();
		const errorsCount = healthReport.getErrorsCount();
		toggleBadgeVisibility($appHealthInfoBadge, infoCount, SEVERITY_STYLE.INFO);
		toggleBadgeVisibility($appHealthWarningsBadge, warningsCount, SEVERITY_STYLE.WARNING);
		toggleBadgeVisibility($appHealthErrorsBadge, errorsCount, SEVERITY_STYLE.ERROR);
	}

	function makeHealthReport(appHealth) {
		let healthReport = new HealthReport();

		Object.entries(appHealth).forEach(function(entry) {
			const name = entry[0];
			const section = entry[1];
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
