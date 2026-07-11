/*
 * This file is part of nzbget. See <https://nzbget.com>.
 *
 * Copyright (C) 2012-2019 Andrey Prygunkov <hugbug@users.sourceforge.net>
 * Copyright (C) 2024-2026 Denis <denis@nzbget.com>
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

/*
 * In this module:
 *   1) Common utilitiy functions (format time, size, etc);
 *   2) Slideable tab dialog extension;
 *   3) Communication via JSON-RPC.
 */

/*** UTILITY FUNCTIONS *********************************************************/

var Util = (new function($)
{
	'use strict';

	this.emptyObject = function(obj) {
		if (!obj) return true;
		if (Object.keys(obj).length == 0) return true;
		return false;
	}

	this.saveToLocalStorage = function(key, val)
	{
		try 
		{
			localStorage.setItem(key, val);
		} catch (error) 
		{
			console.error(error);
		}
	}

	this.getFromLocalStorage = function(key)
	{
		return localStorage.getItem(key);
	}

	this.removeFromLocalStorage = function(key)
	{
		return localStorage.removeItem(key);
	}

	this.formatTimeHMS = function(sec)
	{
		var hms = '';
		var days = Math.floor(sec / 86400);
		var d = I18n.translate('time_days_short');
		if (days > 0)
		{
			hms = days	+ d + ' ';
		}
		var hours = Math.floor((sec % 86400) / 3600);
		hms = hms + hours + ':';
		var minutes = Math.floor((sec / 60) % 60);
		if (minutes < 10)
		{
			hms = hms + '0';
		}
		hms = hms + minutes + ':';
		var seconds = Math.floor(sec % 60);
		if (seconds < 10)
		{
			hms = hms + '0';
		}
		hms = hms + seconds;
		return hms;
	}

	this.formatTimeLeft = function(sec)
	{
		var days = Math.floor(sec / 86400);
		var hours = Math.floor((sec % 86400) / 3600);
		var minutes = Math.floor((sec / 60) % 60);
		var seconds = Math.floor(sec % 60);

		var d = I18n.translate('time_days_short');
		var h = I18n.translate('time_hours_short');
		var m = I18n.translate('time_minutes_short');
		var s = I18n.translate('time_seconds_short');

		if (days > 10)
		{
			return days + d;
		}
		if (days > 0)
		{
			return days + d + ' ' + hours + h;
		}
		if (hours > 0)
		{
			return hours + h + ' ' + (minutes < 10 ? '0' : '') + minutes + m;
		}
		if (minutes > 0)
		{
			return minutes + m + ' ' + (seconds < 10 ? '0' : '') + seconds + s;
		}

		return seconds + s;
	}

	this.isNumber = function(value)
	{
		return typeof value === 'number';
	}

	this.getDaySinceUnixEpoch = function (date) 
	{
		var utcDate = new Date(Date.UTC(
			date.getFullYear(),
			date.getMonth(),
			date.getDate()
		));

		// Get the number of milliseconds since the Unix epoch (UTC).
		var utcTime = utcDate.getTime();

		// Divide by the number of milliseconds in a day to get the number of days.
		var daysSinceEpoch = Math.floor(utcTime / (1000 * 60 * 60 * 24));

		return daysSinceEpoch;
	}

	this.formatDateTime = function(unixTime)
	{
		if (!Util.isNumber(unixTime) || unixTime < 0)
		{
			return '';
		}

		var dt = new Date(unixTime * 1000);
		return new Intl.DateTimeFormat(I18n.getLocale(), I18n.getTimeFormatOptions()).format(dt);
	}

	this.formatSizeMB = function(sizeMB, sizeLo)
	{
		if (sizeLo !== undefined && sizeMB < 1024)
		{
			sizeMB = sizeLo / 1024.0 / 1024.0;
		}

		if (sizeMB >= 1024 * 1024 * 100)
		{
			return Math.floor(sizeMB / 1024.0 / 1024.0) + ' ' + I18n.translate('unit_tb');
		}
		else if (sizeMB >= 1024 * 1024 * 10)
		{
			return this.round1(sizeMB / 1024.0 / 1024.0) + ' ' + I18n.translate('unit_tb');
		}
		else if (sizeMB >= 1024 * 1000)
		{
			return this.round2(sizeMB / 1024.0 / 1024.0) + ' ' + I18n.translate('unit_tb');
		}
		else if (sizeMB >= 1024 * 100)
		{
			return Math.floor(sizeMB / 1024.0) + ' ' + I18n.translate('unit_gb');
		}
		else if (sizeMB >= 1024 * 10)
		{
			return this.round1(sizeMB / 1024.0) + ' ' + I18n.translate('unit_gb');
		}
		else if (sizeMB >= 1000)
		{
			return this.round2(sizeMB / 1024.0) + ' ' + I18n.translate('unit_gb');
		}
		else if (sizeMB >= 100)
		{
			return Math.floor(sizeMB) + ' ' + I18n.translate('unit_mb');
		}
		else if (sizeMB >= 10)
		{
			return this.round1(sizeMB) + ' ' + I18n.translate('unit_mb');
		}
		else
		{
			return this.round2(sizeMB) + ' ' + I18n.translate('unit_mb');
		}
	}

	this.formatSpeed = function (bytesPerSec) 
	{
		if (!Util.isNumber(bytesPerSec) || bytesPerSec < 0) 
		{
			return '';
		}

		var useBits = (I18n.getSpeedUnit() === 'Mb/s');
		var kBase = useBits ? 1000 : 1024;
		var displayVal = useBits ? bytesPerSec * 8 : bytesPerSec;

		if (displayVal >= 100 * kBase * kBase * kBase) 
		{
			return Util.round0(displayVal / kBase / kBase / kBase) + ' ' + I18n.translate(useBits ? 'unit_gbit_s' : 'unit_gb_s');
		}
		else if (displayVal >= 10 * kBase * kBase * kBase) 
		{
			return Util.round1(displayVal / kBase / kBase / kBase) + ' ' + I18n.translate(useBits ? 'unit_gbit_s' : 'unit_gb_s');
		}
		else if (displayVal >= kBase * kBase * kBase) 
		{
			return Util.round2(displayVal / kBase / kBase / kBase) + ' ' + I18n.translate(useBits ? 'unit_gbit_s' : 'unit_gb_s');
		}
		if (displayVal >= 100 * kBase * kBase) 
		{
			return Util.round0(displayVal / kBase / kBase) + ' ' + I18n.translate(useBits ? 'unit_mbit_s' : 'unit_mb_s');
		}
		else if (displayVal >= 10 * kBase * kBase) 
		{
			return Util.round1(displayVal / kBase / kBase) + ' ' + I18n.translate(useBits ? 'unit_mbit_s' : 'unit_mb_s');
		}
		else if (displayVal >= kBase * 1000) 
		{
			return Util.round2(displayVal / kBase / kBase) + ' ' + I18n.translate(useBits ? 'unit_mbit_s' : 'unit_mb_s');
		}

		return Util.round0(displayVal / kBase) + ' ' + I18n.translate(useBits ? 'unit_kbit_s' : 'unit_kb_s');
	}

	this.formatNetworkSpeed = function(speedMbps) 
	{
		if (!Util.isNumber(speedMbps) || speedMbps < 0)
		{
			return '';
		}

		var useBits = (I18n.getSpeedUnit() === 'Mb/s');
		if (!useBits) speedMbps /= 8;

		if (speedMbps >= 1000)
		{
			return this.round1(speedMbps / 1000.0) + ' ' + I18n.translate(useBits ? 'unit_gbit_s' : 'unit_gb_s');
		}
		else if (speedMbps >= 100)
		{
			return this.round0(speedMbps) + ' ' + I18n.translate(useBits ? 'unit_mbit_s' : 'unit_mb_s');
		}
		else if (speedMbps >= 10)
		{
			return this.round1(speedMbps) + ' ' + I18n.translate(useBits ? 'unit_mbit_s' : 'unit_mb_s');
		}
		else if (speedMbps >= 1)
		{
			return this.round2(speedMbps) + ' ' + I18n.translate(useBits ? 'unit_mbit_s' : 'unit_mb_s');
		}
		else
		{
			return this.round0(speedMbps * 1000.0) + ' ' + I18n.translate(useBits ? 'unit_kbit_s' : 'unit_kb_s');
		}
	}

	this.formatSpeedWithCustomUnit = function (bytesPerSec, unit) 
	{
		var res = this.formatSpeed(bytesPerSec);
		return res.replace('B', unit);
	}

	this.formatAge = function(time)
	{
		if (time <= 0)
		{
			return '--';
		}

		var diff = new Date().getTime() / 1000 - time;
		var d = I18n.translate('time_days_short');
		var h = I18n.translate('time_hours_short');
		var m = I18n.translate('time_minutes_short');

		if (diff > 60*60*24)
		{
			return this.round0(diff / (60*60*24))  + '&nbsp;' + d;
		}
		else if (diff > 60*60)
		{
			return this.round0(diff / (60*60))  + '&nbsp;' + h;
		}
		else if (diff <= 0)
		{
			return '--';
		}
		else
		{
			return this.round0(diff / (60))  + '&nbsp;' + m;
		}
	}

	this.formatNumber = function (num) 
	{
		var formattedNum;

		if (num >= 1000000000) 
		{
			formattedNum = (num / 1000000000).toFixed(0);
			return formattedNum + 'B';
		}
		else if (num >= 1000000) 
		{
			formattedNum = (num / 1000000).toFixed(0);
			return formattedNum + 'M';
		}
		else if (num >= 1000) 
		{
			formattedNum = (num / 1000).toFixed(0);
			return formattedNum + 'K';
		}
		else 
		{
			return num.toFixed(0);
		}
	}

	this.round0 = function(arg)
	{
		return Math.round(arg);
	}

	this.round1 = function(arg)
	{
		return arg.toFixed(1);
	}

	this.round2 = function(arg)
	{
		return arg.toFixed(2);
	}

	this.formatNZBName = function(NZBName)
	{
		return NZBName.replace(/\./g, ' ')
			.replace(/_/g, ' ');
	}

	this.textToHtml = function(str)
	{
		return str.replace(/&/g, '&amp;')
			.replace(/</g, '&lt;')
			.replace(/>/g, '&gt;');
	}

	this.textToAttr = function(str)
	{
		return str.replace(/&/g, '&amp;')
			.replace(/</g, '&lt;')
			.replace(/"/g, '&quot;')
			.replace(/'/g, '&#39;');
	}

	this.setMenuMark = function(menu, data)
	{
		// remove marks from all items
		$('li table tr td:first-child', menu).html('');
		// set mark on selected item
		var mark = $('li[data="mark"]', menu).html();
		$('li[data="' + data + '"] table tr td:first-child', menu).html(mark);
	}

	this.show = function(jqSelector, visible, display)
	{
		if (display)
		{
			$(jqSelector).css({display: visible ? display : 'none'});
		}
		else
		{
			visible ? $(jqSelector).show() : $(jqSelector).hide();
		}
	}

	this.parseBool = function(value)
	{
		return ''+value == 'true';
	}

	this.disableShiftMouseDown = function(event)
	{
		// disable default shift+click behaviour, which is to select a text
		if (event.shiftKey)
		{
			event.stopPropagation();
			event.preventDefault();
		}
	}

	this.centerDialog = function(dialog, center)
	{
		var $elem = $(dialog);
		if (center)
		{
			var top = ($(window).height() - $elem.outerHeight()) * 0.4;
			top = top > 0 ? top : 0;
			$elem.css({ top: top});
		}
		else
		{
			$elem.css({ top: '' });
		}
	}

	this.parseCommaList = function(commaList)
	{
		var valueList = commaList.split(/[,;]+/);
		for (var i=0; i < valueList.length; i++)
		{
			valueList[i] = valueList[i].trim();
			if (valueList[i] === '')
			{
				valueList.splice(i, 1);
				i--;
			}
		}
		return valueList;
	}

	this.saveToLocalFile = function(content, type, filename)
	{
		if (!window.Blob)
		{
			return false;
		}

		var blob = new Blob([content], {type: type});

		if (navigator.msSaveBlob)
		{
			navigator.msSaveBlob(blob, filename);
		}
		else
		{
			var URL = window.URL || window.webkitURL || window;
			var object_url = URL.createObjectURL(blob);

			var save_link = document.createElement('a');
			save_link.href = object_url;
			save_link.download = filename;

			var event = document.createEvent('MouseEvents');
			event.initMouseEvent('click', true, false, window, 0, 0, 0, 0, 0, false, false, false, false, 0, null);
			save_link.dispatchEvent(event);
		}

		return true;
	}

	var keyMap = {
		8:'Backspace', 9:'Tab', 13:'Enter', 27:'Escape', 33:'PgUp', 34:'PgDn',
		35:'End', 36:'Home', 37:'Left', 38:'Up', 39:'Right', 40:'Down', 45:'Insert', 46:'Delete',
		48:'0', 49:'1', 50:'2', 51:'3', 52:'4', 53:'5', 54:'6', 55:'7', 56:'8', 59:'9',
		65:'A', 66:'B', 67:'C', 68:'D', 69:'E', 70:'F', 71:'G', 72:'H', 73:'I', 74:'J', 75:'K',
		76:'L', 77:'M', 78:'N', 79:'O', 80:'P', 81:'Q', 82:'R', 83:'S', 84:'T', 85:'U', 86:'V',
		87:'W', 88:'X', 89:'Y', 90:'Z'};

	this.keyName = function(keyEvent)
	{
		return (keyEvent.metaKey ? 'Meta+' : '') + (keyEvent.ctrlKey ? 'Ctrl+' : '') +
			(keyEvent.altKey ? 'Alt+' : '') + (keyEvent.shiftKey ? 'Shift+' : '') +
			keyMap[keyEvent.keyCode];
	}

	this.isInputControl = function(target)
	{
		return target.tagName == 'INPUT' || target.tagName == 'SELECT' ||
			target.tagName == 'TEXTAREA' || target.isContentEditable;
	}

	this.wantsReturn = function(target)
	{
		return target.tagName == 'TEXTAREA';
	}

	this.endsWith = function(text, substr)
	{
	    return text.substring(text.length - substr.length, text.length) === substr;
	}

	this.makeId = function(text)
	{
		return text.replace(/ |\/|\\|\.|\$|\:|\*/g, '_');
	}

	this.joinInt64 = function(hi, lo)
	{
		return (hi << 32) + lo;
	}

	// "N" articles, or "N (X.X MB)" when the byte-level stream repair also
	// recovered bytes (DupeArticleFallback: article layer counts articles,
	// stream/live layer counts bytes). The 64-bit byte total is combined in
	// MB space the way the rest of the WebUI does (hi*4096 + lo-as-MB); NOT
	// via joinInt64, whose (hi<<32) is a 32-bit no-op in JavaScript and would
	// drop the high word for totals >= 4 GiB.
	this.formatDupeRecovered = function(nzb)
	{
		var articles = nzb.DupeRecoveredArticles || 0;
		var hi = nzb.DupeRecoveredBytesHi || 0;
		var lo = nzb.DupeRecoveredBytesLo || 0;
		if (hi > 0 || lo > 0)
		{
			var sizeMB = hi * 4096 + lo / 1024.0 / 1024.0;
			return articles + ' (' + this.formatSizeMB(sizeMB, lo) + ')';
		}
		return '' + articles;
	}

}(jQuery));


/*** MODAL DIALOG WITH SLIDEABLE TABS *********************************************************/

var TabDialog = (new function($)
{
	'use strict';

	this.extend = function(dialog)
	{
		dialog.restoreTab = restoreTab;
		dialog.switchTab = switchTab;
		dialog.maximize = maximize;
	}

	function maximize(options)
	{
		var bodyPadding = 15;
		var dialog = this;
		var body = $('.modal-body', dialog);
		var footer = $('.modal-footer', dialog);
		var header = $('.modal-header', dialog);
		body.css({top: header.outerHeight(), bottom: footer.outerHeight()});
		if (options.mini)
		{
			var scrollheader = $('.modal-scrollheader', dialog);
			var scroll = $('.modal-inner-scroll', dialog);
			scroll.css('min-height', dialog.height() - header.outerHeight() - footer.outerHeight() - scrollheader.height() - bodyPadding*2);
		}
	}

	function restoreTab()
	{
		var dialog = this;
		var body = $('.modal-body', dialog);
		var footer = $('.modal-footer', dialog);
		dialog.css({margin: '', left: '', top: '', bottom: '', right: '', width: '', height: ''});
		body.css({position: '', height: '', left: '', right: '', top: '', bottom: '', 'max-height': ''});
		footer.css({position: '', left: '', right: '', bottom: ''});
	}

	function switchTab(fromTab, toTab, duration, options)
	{
		var dialog = this;
		var sign = options.back ? -1 : 1;
		var fullscreen = options.fullscreen && !options.back;
		var bodyPadding = 15;
		var dialogMargin = options.mini ? 0 : 15;
		var dialogBorder = 2;
		var toggleClass = options.toggleClass ? options.toggleClass : '';

		var body = $('.modal-body', dialog);
		var footer = $('.modal-footer', dialog);
		var header = $('.modal-header', dialog);

		var oldBodyHeight = body.height();
		var windowWidth = $(window).width();
		var windowHeight = $(window).height();
		var oldTabWidth = fromTab.width();
		var dialogStyleFS, bodyStyleFS, footerStyleFS;

		if (options.fullscreen && options.back)
		{
			// save fullscreen state for later use
			dialogStyleFS = dialog.attr('style');
			bodyStyleFS = body.attr('style');
			footerStyleFS = footer.attr('style');
			// restore non-fullscreen state to calculate proper destination sizes
			dialog.restoreTab();
		}

		fromTab.hide();
		toTab.show();
		dialog.toggleClass(toggleClass);

		// CONTROL POINT: at this point the destination dialog size is active
		// store destination positions and sizes

		var newBodyHeight = fullscreen ? windowHeight - header.outerHeight() - footer.outerHeight() - dialogMargin*2 - bodyPadding*2 : body.height();
		var newTabWidth = fullscreen ? windowWidth - dialogMargin*2 - dialogBorder - bodyPadding*2 : toTab.width();
		var leftPos = toTab.position().left;
		var newDialogPosition = dialog.position();
		var newDialogWidth = dialog.width();
		var newDialogHeight = dialog.height();
		var newDialogMarginLeft = dialog.css('margin-left');
		var newDialogMarginTop = dialog.css('margin-top');

		// restore source dialog size

		if (options.fullscreen && options.back)
		{
			// restore fullscreen state
			dialog.attr('style', dialogStyleFS);
			body.attr('style', bodyStyleFS);
			footer.attr('style', footerStyleFS);
		}

		body.css({position: '', height: oldBodyHeight});
		dialog.css('overflow', 'hidden');
		fromTab.css({position: 'absolute', left: leftPos, width: oldTabWidth, height: oldBodyHeight});
		toTab.css({position: 'absolute', width: newTabWidth, height: oldBodyHeight,
			left: sign * ((options.back ? newTabWidth : oldTabWidth) + bodyPadding*2)});
		fromTab.show();
		dialog.toggleClass(toggleClass);

		// animate dialog to destination position and sizes

		if (options.fullscreen && options.back)
		{
			body.css({position: 'absolute'});
			dialog.animate({
					'margin-left': newDialogMarginLeft,
					'margin-top': newDialogMarginTop,
					left: newDialogPosition.left,
					top: newDialogPosition.top,
					right: newDialogPosition.left + newDialogWidth,
					bottom: newDialogPosition.top + newDialogHeight,
					width: newDialogWidth,
					height: newDialogHeight
				},
				duration);

			body.animate({height: newBodyHeight, 'max-height': newBodyHeight}, duration);
		}
		else if (options.fullscreen)
		{
			dialog.css({height: dialog.height()});
			footer.css({position: 'absolute', left: 0, right: 0, bottom: 0});
			dialog.animate({
					margin: dialogMargin,
					left: '0%', top: '0%', bottom: '0%', right: '0%',
					width: windowWidth - dialogMargin*2,
					height: windowHeight - dialogMargin*2
				},
				duration);

			body.animate({height: newBodyHeight, 'max-height': newBodyHeight}, duration);
		}
		else
		{
			body.animate({height: newBodyHeight}, duration);
			dialog.animate({width: newDialogWidth, 'margin-left': newDialogMarginLeft}, duration);
		}

		fromTab.animate({left: sign * -((options.back ? newTabWidth : oldTabWidth) + bodyPadding*2),
			height: newBodyHeight + bodyPadding}, duration);
		toTab.animate({left: leftPos, height: newBodyHeight + bodyPadding}, duration, function()
			{
				fromTab.hide();
				fromTab.css({position: '', width: '', height: '', left: ''});
				toTab.css({position: '', width: '', height: '', left: ''});
				dialog.css({overflow: '', width: (fullscreen ? 'auto' : ''), height: (fullscreen ? 'auto' : ''), 'margin-left': (fullscreen ? dialogMargin : '')});
				dialog.toggleClass(toggleClass);
				if (fullscreen)
				{
					body.css({position: 'absolute', height: '', left: 0, right: 0,
						top: header.outerHeight(),
						bottom: footer.outerHeight(),
						'max-height': 'inherit'});
				}
				else
				{
					body.css({position: '', height: ''});
				}
				if (options.fullscreen && options.back)
				{
					// restore non-fullscreen state
					dialog.restoreTab();
				}
				if (options.complete)
				{
					options.complete();
				}
			});
	}
}(jQuery));


/*** REMOTE PROCEDURE CALLS VIA JSON-RPC *************************************************/

var RPC = (new function($)
{
	'use strict';

	// Properties
	this.rpcUrl = '';
	this.defaultFailureCallback = undefined;
	this.connectErrorMessage = 'Cannot establish connection';
	this.safeMethods = [];
	this.etags = {};

	this.openRequest = function(method, params, options)
	{
		var xhr = new XMLHttpRequest();

		if (this.safeMethods.indexOf(method) > -1)
		{
			var request = '';
			for (var i = 0; i < params.length; i++)
			{
				request += '&=' + encodeURIComponent(params[i]);
			}
			if (params.length > 0)
			{
				request = '?' + request.substr(1);
			}
			xhr.open('get', this.rpcUrl + '/' + method + request);
			xhr._reportRequest = method + request;
		}
		else
		{
			xhr.open('post', this.rpcUrl);
			var parts = [];
			parts.push('{"nocache":' + new Date().getTime() + ',"method":' + JSON.stringify(method) + ',"params":[');
			for (var i = 0; i < params.length; i++)
			{
				if (i > 0)
				{
					parts.push(',');
				}
				if (typeof params[i] === 'string' && params[i].length > 1024 * 1024)
				{
					parts.push('"');
					parts.push(params[i]);
					parts.push('"');
				}
				else
				{
					parts.push(JSON.stringify(params[i]));
				}
			}
			parts.push(']}');

			var isLarge = false;
			var totalSize = 0;
			for (var i = 0; i < parts.length; i++)
			{
				totalSize += parts[i].length;
				if (totalSize > 1024 * 1024)
				{
					isLarge = true;
				}
			}

			xhr._request = new Blob(parts, {type: 'application/json'});
			if (isLarge)
			{
				xhr._reportRequest = '{"method":"' + method + '", "params": [...large payload...]}';
			}
			else
			{
				xhr._reportRequest = parts.join('');
			}
		}

		if (options && ('timeout' in options))
		{
			xhr.timeout = options['timeout'];
		}

		for (var i = 0; i < (options && ('custom_headers' in options) ? options['custom_headers'].length : 0); i++)
		{
			xhr.setRequestHeader(options['custom_headers'][i].name, options['custom_headers'][i].value);
		}

		return xhr;
	}

	this.call = function(method, params, completed_callback, failure_callback, options)
	{
		var _this = this;
		var xhr = this.openRequest(method, params, options);

		xhr.onreadystatechange = function()
		{
			if (xhr.readyState === 4)
			{
					var res = 'Unknown error';
					var result;
					var cached = false;
					if (xhr.status === 200)
					{
						if (xhr.responseText != '')
						{
							var etag = xhr.getResponseHeader('ETag');
							if (etag)
							{
								cached = RPC.etags[method] == etag;
								RPC.etags[method] = etag;
							}

							if (cached && options && ('prefer_cached' in options) && options['prefer_cached'])
							{
								result = {result: null, error: null};
							}
							else
							{
								try
								{
									result = JSON.parse(xhr.responseText);
								}
								catch (e)
								{
									res = e;
								}
							}

							if (result)
							{
								if (result.error == null)
								{
									res = result.result;
									completed_callback(res, cached);
									return;
								}
								else
								{
									res = result.error.message;
									if (result.error.message != 'Access denied')
									{
										res = res + '<br><br>Request: ' + xhr._reportRequest;
									}
								}
							}
						}
						else
						{
							res = 'No response received.';
						}
					}
					else if (xhr.status === 0)
					{
						res = _this.connectErrorMessage;
					}
					else
					{
						res = 'Invalid Status: ' + xhr.status;
					}

					if (failure_callback)
					{
						failure_callback(res, result);
					}
					else
					{
						_this.defaultFailureCallback(res, result);
					}
			}
		};
		xhr.send(xhr._request);
	}
}(jQuery));
