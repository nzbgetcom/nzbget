/*
 * This file is part of nzbget. See <https://nzbget.com>.
 *
 * Copyright (C) 2026 Denis <denis@nzbget.com>
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


 var I18n = (new function($)
 {
	'use strict';
 
	var translations = {};
	var baseTranslations = {};
	var currentLocale = 'en';
	var availableLangs = [];
	var speedUnit = 'MB/s';
	var subscribers = [];
	var initialized = false;
	var readyCallbacks = [];

	// Polyfill for Intl.DateTimeFormat for very old browsers
	if (typeof window.Intl === 'undefined' || typeof window.Intl.DateTimeFormat === 'undefined') {
		window.Intl = window.Intl || {};
		window.Intl.DateTimeFormat = function(locale, options) {
			this.options = options || {};
			this.locale = locale || 'en-US';

			this.format = function(date) {
				if (!date || isNaN(date.getTime())) return '';

				var datePart = date.toLocaleDateString(this.locale, {
					year: this.options.year,
					month: this.options.month,
					day: this.options.day
				});

				if (this.options.hour && this.options.minute) {
					var h = date.getHours();
					var m = date.getMinutes();
					var s = date.getSeconds();
					var suffix = '';

					if (this.options.hour12 === true) {
						suffix = h >= 12 ? ' PM' : ' AM';
						h = h % 12;
						h = h ? h : 12; 
					}

					var timeStr = (h < 10 ? '0' : '') + h + ':' + (m < 10 ? '0' : '') + m;
					
					if (this.options.second) {
						timeStr += ':' + (s < 10 ? '0' : '') + s;
					}
					
					return datePart + ', ' + timeStr + suffix;
				}
				
				return datePart;
			};
		};
	}

	this.init = function()
	{
		var self = this;

		// 1. Load English base translations from locales.source.json
		$.ajax({
			url: 'locales.source.json',
			dataType: 'json',
			cache: true
		}).done(function(data)
		{
			if (data)
			{
				// Extract available languages
				if (data['__LOCALES__'])
				{
					var locales = data['__LOCALES__'];
					for (var code in locales)
					{
						if (locales.hasOwnProperty(code))
						{
							availableLangs.push({ code: code, name: locales[code] });
						}
					}
				}

				// Load English messages as base translations
				for (var key in data)
				{
					if (key !== '__LOCALES__' && data.hasOwnProperty(key) && data[key] && data[key].message)
					{
						translations[key] = data[key].message;
					}
				}
				baseTranslations = $.extend({}, translations);
			}

			// 2. Language Detection
			var savedLang = localStorage.getItem('Language');
			if (savedLang && self.isValidLang(savedLang))
			{
				currentLocale = savedLang;
			}
			else
			{
				var browserLang = navigator.language || navigator.userLanguage;
				if (browserLang)
				{
					if (self.isValidLang(browserLang))
					{
						currentLocale = browserLang;
					}
					else
					{
						var shortLang = browserLang.split('-')[0].toLowerCase();
						for (var i = 0; i < availableLangs.length; i++)
						{
							if (availableLangs[i].code.toLowerCase() === shortLang)
							{
								currentLocale = availableLangs[i].code;
								break;
							}
						}
					}
				}
			}

			// 3. Unit Detection
			var savedSpeedUnit = localStorage.getItem('SpeedUnit');
			if (savedSpeedUnit)
			{
				speedUnit = savedSpeedUnit;
			}

			// Mark as initialized - availableLangs is now populated
			initialized = true;
			readyCallbacks.forEach(function(cb) { cb(); });
			readyCallbacks = [];

			// 4. Try to load user locale (if not English)
			if (currentLocale !== 'en')
			{
				$.ajax({
					url: '_locales/' + currentLocale + '/messages.json',
					dataType: 'json',
					cache: true
				}).done(function(data)
				{
					// Merge locale translations
					for (var key in data)
					{
						if (data.hasOwnProperty(key))
						{
							translations[key] = data[key];
						}
					}
				}).fail(function()
				{
					// Locale file not found - continue with English
					console.warn('Locale "' + currentLocale + '" not found, using English');
					currentLocale = 'en';
				}).always(function()
				{
					// Translate page after translations are loaded (or failed)
					self._applyLanguageChange(currentLocale);
				});
			}
			else
			{
				// Translate page after English translations are loaded
				self._applyLanguageChange(currentLocale);
			}
		}).fail(function()
		{
			// locales.source.json failed - critical error
			alert('Error: WebUI cannot work without translations. Please refresh the page or clear your browser cache.');
			console.error('Failed to load locales.source.json');
		});
	};

	this.isValidLang = function(code)
	{
		// English is always valid - it's the base language from locales.source.json
		if (code === 'en') return true;
		for (var i = 0; i < availableLangs.length; i++)
		{
			if (availableLangs[i].code === code) return true;
		}
		return false;
	};
 
	// Simple format function for $1, $2 placeholders
	this.format = function(format, args) {
		if (typeof format !== 'string') format = String(format);
		return format.replace(/\$(\d+)/g, function(match, number) {
			// $1 corresponds to args[0], $2 to args[1], etc.
			var val = args[parseInt(number) - 1];
			return typeof val != 'undefined' ? val : match;
		});
	};

	/**
	 * Extracts the translation text from a translation value.
	 * Translations can be either:
	 *   - A string: "translated text"
	 *   - An object with message property: { message: "translated text", description: "..." }
	 *
	 * @param {string|object} translation - The translation value from the translations object
	 * @returns {string} The translated text string
	 */
	function extractTranslationText(translation)
	{
		if (typeof translation === 'string')
		{
			return translation;
		}
		if (typeof translation === 'object' && translation !== null && translation.message)
		{
			return translation.message;
		}
		return null;
	}

	this.translate = function(key)
	{
		var transValue = translations && translations[key];
		if (!transValue && baseTranslations && baseTranslations[key]) {
			transValue = baseTranslations[key];
		}
		var extractedText = extractTranslationText(transValue);
		var text = extractedText !== null ? extractedText : key;
		
		if (arguments.length > 1) {
			var args = Array.prototype.slice.call(arguments, 1);
			return this.format(text, args);
		}
		
		return text;
	};

	this.defaultValue = function(key, defaultText) {
		if (translations && translations.hasOwnProperty(key)) {
			return this.translate(key);
		}
		if (baseTranslations && baseTranslations.hasOwnProperty(key)) {
			return this.translate(key);
		}
		return defaultText;
	};
	
	this.translatePage = function(context)
	{
		var $context = context ? $(context) : $(document);

		var elements = $context.find('[data-i18n], [data-i18n-title], [data-i18n-placeholder], [data-i18n-value], [data-i18n-html]')
			.add($context.filter('[data-i18n], [data-i18n-title], [data-i18n-placeholder], [data-i18n-value], [data-i18n-html]'));

		elements.each(function()
		{
			var $this = $(this);
			
			// 1. Process HTML Content
			if (this.hasAttribute('data-i18n-html')) {
				var key = this.getAttribute('data-i18n-html');
				var translateArgs = [key];
				var i = 1;
				while (this.hasAttribute('data-i18n-html-arg-' + i)) {
					var arg = this.getAttribute('data-i18n-html-arg-' + i);
					translateArgs.push(arg);
					i++;
				}
				var translated = I18n.translate.apply(I18n, translateArgs);
				if ($this.data('i18n-html-last') !== translated) {
					$this.data('i18n-html-last', translated);
					var oldHtml = $this.html();
					if (oldHtml !== translated) {
						$this.html(translated);
						I18n.translatePage($this.children());
					}
				}
			}

			// 2. Process Text Content
			if (this.hasAttribute('data-i18n')) {
				var key = this.getAttribute('data-i18n');
				var translateArgs = [key];
				var i = 1;
				while (this.hasAttribute('data-i18n-arg-' + i)) {
					translateArgs.push(this.getAttribute('data-i18n-arg-' + i));
					i++;
				}
				var translated = I18n.translate.apply(I18n, translateArgs);
				if ($this.data('i18n-last') !== translated && (translated !== key || $this.text() === "")) {
					$this.data('i18n-last', translated);
					
					$this.text(translated);
				}
			}

			// 3. Process Title Attribute
			if (this.hasAttribute('data-i18n-title')) {
				var key = this.getAttribute('data-i18n-title');
				var translateArgs = [key];
				var i = 1;
				while (this.hasAttribute('data-i18n-title-arg-' + i)) {
					translateArgs.push(this.getAttribute('data-i18n-title-arg-' + i));
					i++;
				}
				var translated = I18n.translate.apply(I18n, translateArgs);
				if ($this.data('i18n-title-last') !== translated && (translated !== key || !$this.attr('title'))) {
					$this.data('i18n-title-last', translated);
					$this.attr('title', translated);
				}
			}

			// 4. Process Placeholder Attribute
			if (this.hasAttribute('data-i18n-placeholder')) {
				var key = this.getAttribute('data-i18n-placeholder');
				var translateArgs = [key];
				var i = 1;
				while (this.hasAttribute('data-i18n-placeholder-arg-' + i)) {
					translateArgs.push(this.getAttribute('data-i18n-placeholder-arg-' + i));
					i++;
				}
				var translated = I18n.translate.apply(I18n, translateArgs);
				if ($this.data('i18n-placeholder-last') !== translated && (translated !== key || !$this.attr('placeholder'))) {
					$this.data('i18n-placeholder-last', translated);
					$this.attr('placeholder', translated);
				}
			}

			// 5. Process Value Attribute
			if (this.hasAttribute('data-i18n-value')) {
				var key = this.getAttribute('data-i18n-value');
				var translateArgs = [key];
				var i = 1;
				while (this.hasAttribute('data-i18n-value-arg-' + i)) {
					translateArgs.push(this.getAttribute('data-i18n-value-arg-' + i));
					i++;
				}
				var translated = I18n.translate.apply(I18n, translateArgs);
				if ($this.data('i18n-value-last') !== translated && (translated !== key || !$this.val())) {
					$this.data('i18n-value-last', translated);
					$this.val(translated);
				}
			}
		});
	};
 
	this.getLocale = function()
	{
		return currentLocale;
	};

	 this.getTimeFormatOptions = function ()
	 {
		return {
			year: 'numeric',
			month: 'short',
			day: 'numeric',
			hour: '2-digit',
			minute: '2-digit',
			second: '2-digit',
			hour12: false
		};	 
	}
 
	function requestExtensionLocale(extName)
	{
		return $.ajax({
			url: 'extensions/' + extName + '/_locales/' + currentLocale + '/messages.json',
			dataType: 'json',
			cache: true
		});
	}

	function mergeExtensionLocale(extName, data)
	{
		var prefix = 'ext_' + extName.toLowerCase() + '_';
		for (var key in data) {
			if (data.hasOwnProperty(key)) {
				translations[prefix + key] = data[key];
			}
		}
	}

	this.loadExtensionTranslations = function(extName)
	{
		if (currentLocale === 'en') return;

		var self = this;
		requestExtensionLocale(extName).done(function(data) {
			mergeExtensionLocale(extName, data);
			if (window.Config)
			{
				Config._translateDescriptions();
			}
			self._applyLanguageChange(currentLocale);
		}).fail(function() {
			console.warn('Extension locale not found: extensions/' + extName + '/_locales/' + currentLocale + '/messages.json, using English fallback');
		});
	};

	this._getExtensionNames = function()
	{
		var extNames = [];
		var exts = ExtensionManager.getInstalledExtensions();
		for (var i = 0; i < exts.length; i++) {
			if (exts[i].name) {
				extNames.push(exts[i].name);
			}
		}
		return extNames;
	};

	this._loadExtensionTranslations = function(extName)
	{
		if (currentLocale === 'en') {
			return $.Deferred().resolve().promise();
		}

		var deferred = $.Deferred();
		requestExtensionLocale(extName).done(function(data) {
			mergeExtensionLocale(extName, data);
			deferred.resolve();
		}).fail(function() {
			deferred.resolve();
		});
		return deferred.promise();
	};

	this.getAvailableLangs = function()
	{
		return availableLangs;
	};

	this.whenReady = function(callback)
	{
		if (initialized)
		{
			callback();
		}
		else
		{
			readyCallbacks.push(callback);
		}
	};
 
	this.subscribe = function(callback)
	{
		subscribers.push(callback);
	};
 
	this.unsubscribe = function(callback)
	{
		subscribers = subscribers.filter(function(s) { return s !== callback; });
	};
 
	this.setLanguage = function(localeCode)
	{
		var self = this;
		if (this.isValidLang(localeCode))
		{
			translations = $.extend({}, baseTranslations);
			// Load new locale strings and apply
			if (localeCode !== 'en')
			{
				$.ajax({
					url: '_locales/' + localeCode + '/messages.json',
					dataType: 'json',
					cache: true
				}).done(function(data) {
					// Success - save language and merge translations
					currentLocale = localeCode;
					localStorage.setItem('Language', localeCode);
					for (var key in data) {
						if (data.hasOwnProperty(key)) {
							translations[key] = data[key];
						}
					}

					var extNames = self._getExtensionNames();
					if (extNames.length > 0)
					{
						var promises = extNames.map(function(extName) {
							return self._loadExtensionTranslations(extName);
						});
						$.when.apply($, promises).always(function() {
							self._applyLanguageChange(localeCode);
						});
					}
					else
					{
						self._applyLanguageChange(localeCode);
					}
				}).fail(function() {
					console.warn('Locale "' + localeCode + '" not found, using English');
					currentLocale = 'en';
					localStorage.setItem('Language', 'en');
					self._applyLanguageChange('en');
				});
			}
			else
			{
				currentLocale = 'en';
				localStorage.setItem('Language', 'en');
				this._applyLanguageChange('en');
			}
		}
	};
	
	this._applyLanguageChange = function(localeCode)
	{
		this.translatePage();
		
		// Notify subscribers
		subscribers.forEach(function(callback) { callback(); });
	};
	
	this.getSpeedUnit = function()
	{
		return speedUnit;
	};

	this.setSpeedUnit = function(unit)
	{
		if (unit === 'MB/s' || unit === 'Mb/s') {
			speedUnit = unit;
			localStorage.setItem('SpeedUnit', speedUnit);
			
			// Update UI dynamically
			$('.unit-btn').removeClass('btn-active');
			$('.unit-btn[data-val="' + speedUnit + '"]').addClass('btn-active');
			
			// Redraw relevant components if available to reflect new units
			if (typeof Status !== 'undefined' && Status.redraw) Status.redraw();
			if (typeof SystemInfo !== 'undefined' && SystemInfo.redraw) SystemInfo.redraw();
			if (typeof Statistics !== 'undefined' && Statistics.redraw) Statistics.redraw();
			if (typeof StatDialog !== 'undefined' && StatDialog.redraw) StatDialog.redraw();
		}
	};

	this.toggleSpeedUnit = function()
	{
		this.setSpeedUnit((speedUnit === 'MB/s') ? 'Mb/s' : 'MB/s');
	};

	// Initialize i18n - translations loaded via AJAX, translatePage called when complete
	this.init();
}(jQuery));
