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

var DateTime = (function () {
	function DateTime() {
		this.recalculate();
	}

	DateTime.prototype.recalculate = function () {
		var firstWeekDay = new Date();
		firstWeekDay.setUTCDate(firstWeekDay.getUTCDate() - firstWeekDay.getUTCDay());
		var lastWeekDay = new Date();
		lastWeekDay.setUTCDate(lastWeekDay.getUTCDate() + (6 - lastWeekDay.getUTCDay()));
		
		this.currDay = new Date();

		var firstMonthDay = new Date(Date.UTC(this.currDay.getFullYear(), this.currDay.getMonth(), 1));
		var lastMonthDay = new Date(Date.UTC(this.currDay.getFullYear(), this.currDay.getMonth() + 1, 0));

		this.week = this.getDateRange(firstWeekDay, lastWeekDay);
		this.month = this.getDateRange(firstMonthDay, lastMonthDay);
	};

	DateTime.prototype.getCurrentDay = function () {
		return this.currDay;
	};

	DateTime.prototype.getDateRange = function (startDate, endDate) {
		var dates = [];
		var currentDate = new Date(startDate);
	
		while (currentDate.getTime() <= endDate.getTime()) {
			dates.push(new Date(currentDate));
			currentDate.setUTCDate(currentDate.getUTCDate() + 1);
		}
	
		return dates;
	};

	DateTime.prototype.getWeekRange = function () {
		return this.getDateRange(this.week[0], this.week[this.week.length - 1]);
	};

	DateTime.prototype.getMonthRange = function () {
		return this.getDateRange(this.month[0], this.month[this.month.length - 1]);
	};

	DateTime.prototype.getWeek = function () {
		return this.week;
	};

	DateTime.prototype.getMonth = function () {
		return this.month;
	};

	DateTime.prototype.getFirstWeekDate = function () {
		return this.week[0];
	};

	DateTime.prototype.getLastWeekDate = function () {
		return this.week[6];
	};

	DateTime.prototype.getFirstMonthDate = function () {
		return this.month[0];
	};

	DateTime.prototype.getLastMonthDate = function () {
		return this.month[this.month.length - 1];
	};

	DateTime.prototype.getStartDate = function () {
		return this.startDate;
	};

	DateTime.prototype.getEndDate = function () {
		return this.endDate;
	};

	DateTime.prototype.getStartDateString = function () {
		return this.startDateString;
	};

	DateTime.prototype.getEndDateString = function () {
		return this.endDateString;
	};

	DateTime.prototype.formatDateForInput = function (date) {
		var dateCopy = new Date(date);
		if (isNaN(dateCopy)) {
			return "";
		}

		dateCopy.setTime(dateCopy.getTime() - (dateCopy.getTimezoneOffset() * 60000));
		var dateAsString = dateCopy.toISOString();
		return dateAsString.slice(0, 10);
	};

	return DateTime;
})();
