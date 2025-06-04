/*
 * This file is part of nzbget. See <https://nzbget.com>.
 *
 * Copyright (C) 2012-2019 Andrey Prygunkov <hugbug@users.sourceforge.net>
 * Copyright (C) 2024 Denis <denis@nzbget.com>
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
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * In this module:
 *   1) Loading of program options and post-processing scripts options;
 *   2) Settings tab;
 *   3) Function "Reload".
 */

/*** OPTIONS AND CONFIGS (FROM CONFIG FILES) **************************************/

function Server()
{
	this.id = 0;
	this.host = '';
	this.name = '';
	this.port = 0;
	this.connections = 0;
}

var Options = (new function($)
{
	'use strict';

	// Properties (public)
	this.options;
	this.postParamConfig = [];
	this.configtemplates = [];
	this.categories = [];
	this.restricted = false;

	// State
	var _this = this;
	this.serverTemplateData = null;
	var serverValues;
	var loadComplete;
	var loadConfigError;
	var loadServerTemplateError;
	var shortScriptNames = [];
	var subs = [];
	var onPageRenderedCallback;

	var HIDDEN_SECTIONS = ['DISPLAY (TERMINAL)', 'POSTPROCESSING-PARAMETERS', 'POST-PROCESSING-PARAMETERS', 'POST-PROCESSING PARAMETERS'];
	var POSTPARAM_SECTIONS = ['POSTPROCESSING-PARAMETERS', 'POST-PROCESSING-PARAMETERS', 'POST-PROCESSING PARAMETERS'];

	this.update = function()
	{
		// RPC-function "config" returns CURRENT configuration settings loaded in NZBGet
		RPC.call('config', [], function(_options)
		{
			_this.options = _options;
			initCategories();
			_this.restricted = _this.option('ControlPort') === '***';
			notifyAll(_options);
			RPC.next();
		});

		// load config templates
		RPC.call('configtemplates', [false], function(data)
		{
			_this.configtemplates = data;
			RPC.next();
		});

		// build list of post-processing parameters
		RPC.call('loadextensions', [false], function(data)
		{
			_this.postParamConfig = initPostParamConfig(data);
		});
	}

	this.setOnPageRenderedCallback = function(callback) {
		onPageRenderedCallback = callback;
	}

	this.cleanup = function()
	{
		this.serverTemplateData = null;
		serverValues = null;
	}

	this.option = function(name)
	{
		var opt = findOption(this.options, name);
		return opt ? opt.Value : null;
	}

	this.getServerById = function(id)
	{
		var server = new Server();

		server.id = id;
		var serverId = 'Server' + id;
		var hostOpt = findOption(this.options, serverId + '.Host');
		if (!hostOpt)
			return null;

		server.host = hostOpt.Value;

		var nameOpt = findOption(this.options, serverId + '.Name');
		if (nameOpt)
			server.name = nameOpt.Value;
		
		var portOpt = findOption(this.options, serverId + '.Port');
		if (portOpt)
			server.port = portOpt.Value;

		var activeOpt = findOption(this.options, serverId + '.Active');
		if (activeOpt)
			server.active = activeOpt.Value == 'yes';

		var connectionsOpt = findOption(this.options, serverId + '.Connections');
		if (connectionsOpt)
			server.connections = connectionsOpt.Value;

		return server;
	}

	this.subscribe = function(sub)
	{
		subs.push(sub);
	}

	function notifyAll(options) {
		for (var i = 0; i < subs.length; ++i) {
			subs[i].update(options);
		}
	}

	function initCategories()
	{
		_this.categories = [];
		for (var i=0; i < _this.options.length; i++)
		{
			var option = _this.options[i];
			if ((option.Name.toLowerCase().substring(0, 8) === 'category') && (option.Name.toLowerCase().indexOf('.name') > -1))
			{
				_this.categories.push(option.Value);
			}
		}
	}

	function getOptionName(extConf, rawOption)
	{
		var name = extConf.Name + ':';

		if (rawOption.Prefix)
		{
			name += rawOption.Prefix;
		}

		if (rawOption.Multi)
		{
			name += '1.'
		}
		name += rawOption.Name;

		return name;
	}

	function makeOption(extConf, rawOption)
	{
		var [type, select] = GetTypeAndSelect(rawOption);
		return {
			caption: rawOption.DisplayName,
			name: getOptionName(extConf, rawOption),
			value: String(rawOption.Value),
			defvalue: String(rawOption.Value),
			sectionId: extConf.Name + '_' + rawOption.Section.toUpperCase(),
			select,
			description: arrToStr(rawOption.Description),
			nocontent: false,
			formId: extConf.Name + '_' + rawOption.Name,
			multiid: 1,
			multi: rawOption.Multi,
			prefix: rawOption.Prefix,
			template: rawOption.Multi,
			section: rawOption.Section.toUpperCase(),
			type,
		};
	}

	function makeCommandOption(extConf, rawCommand)
	{
		return {
			caption: rawCommand.DisplayName,
			name: getOptionName(extConf, rawCommand),
			value: null,
			defvalue: rawCommand.Action,
			sectionId: extConf.Name + '_' + rawCommand.Section.toUpperCase(),
			select: [],
			description: arrToStr(rawCommand.Description),
			nocontent: false,
			formId: extConf.Name + '_' + rawCommand.Name,
			commandopts: 'settings',
			multiid: 1,
			template: rawCommand.Multi,
			prefix: rawCommand.Prefix,
			section: rawCommand.Section.toUpperCase(),
			multi: rawCommand.Multi,
			type: 'command',
		};
	}

	function makeSection(extConf, option)
	{
		return {
			name: option.section,
			id: extConf.Name + '_' + option.section,
			options: [option],
			multi: option.multi,
			hidden: false,
			postparam: false,
		};
	}

	/*** LOADING CONFIG ********************************************************************/

	this.loadConfig = function(callbacks)
	{
		loadComplete = callbacks.complete;
		loadConfigError = callbacks.configError;
		loadServerTemplateError = callbacks.serverTemplateError;

		// RPC-function "loadconfig" reads the configuration settings from NZBGet configuration file.
		// that's not neccessary the same settings returned by RPC-function "config". This could be the case,
		// for example, if the settings were modified but NZBGet was not restarted.
		RPC.call('loadconfig', [], serverValuesLoaded, loadConfigError);
	}

	this.complete = function() 
	{
		if (this.serverTemplateData === null) 
		{
			// the loading was cancelled and the data was discarded (via method "cleanup()")
			return;
		}

		var config = [];
		config.values = serverValues;

		readWebSettings(config);

		var serverConfig = readConfigTemplate(this.serverTemplateData[0].Template, undefined, HIDDEN_SECTIONS, '');
		mergeValues(serverConfig.sections, serverValues);
		config.push(serverConfig);

		// read scripts configs
		for (var i = 1; i < this.serverTemplateData.length; i++) 
		{
			var sections = {};
			var scriptConfig = {
				sections: [],
				nameprefix: this.serverTemplateData[i].Name,
			};
			var requirements = this.serverTemplateData[i].Requirements;
			var description = arrToStr(this.serverTemplateData[i].Description) + '\n';
			description = requirements.reduce(function(acc, curr) {
				if (curr)
				{
					return acc += 'NOTE: ' + curr + '\n';
				}
				return acc += '\n';
			}, description);
			scriptConfig['scriptName'] = this.serverTemplateData[i].Name;
			scriptConfig['id'] = Util.makeId(this.serverTemplateData[i].Name);
			scriptConfig['name'] = this.serverTemplateData[i].Name;
			scriptConfig['shortName'] = this.serverTemplateData[i].DisplayName;
			scriptConfig['post'] = this.serverTemplateData[i].PostScript;
			scriptConfig['scan'] = this.serverTemplateData[i].ScanScript;
			scriptConfig['queue'] = this.serverTemplateData[i].QueueScript;
			scriptConfig['scheduler'] = this.serverTemplateData[i].SchedulerScript;
			scriptConfig['defscheduler'] = this.serverTemplateData[i].TaskTime !== '';
			scriptConfig['feed'] = this.serverTemplateData[i].FeedScript;
			scriptConfig['about'] = this.serverTemplateData[i].About;
			scriptConfig['description'] = description;
			scriptConfig['author'] = this.serverTemplateData[i].Author;
			scriptConfig['license'] = this.serverTemplateData[i].License;
			scriptConfig['version'] = this.serverTemplateData[i].Version;

			for (var j = 0; j < this.serverTemplateData[i].Commands.length; j++) 
			{
				var command = makeCommandOption(this.serverTemplateData[i], this.serverTemplateData[i].Commands[j]);
				if (sections[command.section])
				{
					sections[command.section].options.push(command);
				}
				else
				{
					sections[command.section] = makeSection(this.serverTemplateData[i], command);
				}
			}

			for (var j = 0; j < this.serverTemplateData[i].Options.length; j++) 
			{
				var option = makeOption(this.serverTemplateData[i], this.serverTemplateData[i].Options[j]);
				if (sections[option.section])
				{
					sections[option.section].options.push(option);
				}
				else
				{
					sections[option.section] = makeSection(this.serverTemplateData[i], option);
				}
			}

			scriptConfig.sections = Object.values(sections);
			mergeValues(scriptConfig.sections, serverValues);
			config.push(scriptConfig);
		}

		serverValues = null;
		loadComplete(config);
		if (onPageRenderedCallback)
		{
			onPageRenderedCallback();
			onPageRenderedCallback = null;
		}
	}

	function serverValuesLoaded(data)
	{
		serverValues = data;
		RPC.call('configtemplates', [true], serverTemplateLoaded, loadServerTemplateError);
	}

	this.loadExtensions = function()
	{
		RPC.call('loadextensions', [true], extensionsLoaded, function() { Options.complete(); });
	}

	function serverTemplateLoaded(data)
	{
		Options.serverTemplateData = data;
		Options.loadExtensions();
	}

	function extensionsLoaded(data) 
	{
		if (Options.serverTemplateData) 
		{
			Options.serverTemplateData = Options.serverTemplateData.concat(data);
			Options.complete();
			ExtensionManager.setExtensions(data.slice());
		}
	}

	function arrToStr(arr)
	{
		return arr.reduce(function(acc, curr) { return acc += curr + '\n'; }, '');
	}

	function GetTypeAndSelect(option)
	{
		if (typeof option.Value === 'string')
		{
			return ['text', option.Select];
		}
		if (typeof option.Value === 'number')
		{
			if (option.Select.length > 1)
			{
				return ['numeric', [option.Select[0] + '-' + option.Select[1]]];
			}
			return ['numeric', option.Select];
		}
		return ['info', option.Select];
	}

	function readWebSettings(config)
	{
		var webTemplate = '### WEB-INTERFACE ###\n\n';
		var webValues = [];

		for (var optname in UISettings.description)
		{
			var descript = UISettings.description[optname];
			var value = UISettings[optname];
			optname = optname[0].toUpperCase() + optname.substring(1);
			if (value === true) value = 'yes';
			if (value === false) value = 'no';

			descript = descript.replace(/\n/g, '\n# ').replace(/\n# \n/g, '\n#\n');
			webTemplate += '# ' + descript + '\n' + optname + '=' + value + '\n\n';

			webValues.push({Name: optname, Value: value.toString()});
		}

		var webConfig = readConfigTemplate(webTemplate, undefined, '', '');
		mergeValues(webConfig.sections, webValues);
		config.push(webConfig);
	}

	this.reloadConfig = function(_serverValues, _complete)
	{
		loadComplete = _complete;
		serverValues = _serverValues;
		Options.complete();
	}

	/*** PARSE CONFIG AND BUILD INTERNAL STRUCTURES **********************************************/

	function readConfigTemplate(filedata, visiblesections, hiddensections, nameprefix)
	{
		var config = { nameprefix: nameprefix, sections: [], };
		var section = null;
		var description = '';
		var firstdescrline = '';

		var data = filedata.split('\n');
		for (var i=0, len=data.length; i < len; i++)
		{
			var line = data[i].replace(/\r+$/,''); // remove possible trailing CR-characters

			if (line.substring(0, 4) === '### ')
			{
				var section = {};
				section.name = line.substr(4, line.length - 8).trim();
				section.id = Util.makeId(nameprefix + section.name);
				section.options = [];
				description = '';
				section.hidden = !(hiddensections === undefined || (hiddensections.indexOf(section.name) == -1)) ||
					(visiblesections !== undefined && (visiblesections.indexOf(section.name) == -1));
				section.postparam = POSTPARAM_SECTIONS.indexOf(section.name) > -1;
				config.sections.push(section);
			}
			else if (line.substring(0, 2) === '# ' || line === '#')
			{
				if (description !== '')
				{
					description += ' ';
				}
				if (line[2] === ' ' && line[3] !== ' ' && description.substring(description.length-4, 4) != '\n \n ')
				{
					description += '\n';
				}
				description += line.substr(1, 10000).trim();
				var lastchar = description.substr(description.length - 1, 1);
				if (lastchar === '.' && firstdescrline === '')
				{
					firstdescrline = description;
					description = '';
				}
				if ('.;:'.indexOf(lastchar) > -1 || line === '#')
				{
					description += '\n';
				}
			}
			else if (line.indexOf('=') > -1 || line.indexOf('@') > -1)
			{
				if (!section)
				{
					// bad template file; create default section.
					section = {};
					section.name = 'OPTIONS';
					section.id = Util.makeId(nameprefix + section.name);
					section.options = [];
					description = '';
					config.sections.push(section);
				}

				var option = {};
				var enabled = line.substr(0, 1) !== '#';
				var command = line.indexOf('=') === -1 && line.indexOf('@') > -1;
				option.caption = line.substr(enabled ? 0 : 1, line.indexOf(command ? '@' : '=') - (enabled ? 0 : 1)).trim();
				if (command)
				{
					var optpos = option.caption.indexOf('[');
					option.commandopts = optpos > -1 ? option.caption.substring(optpos + 1, option.caption.indexOf(']')).toLowerCase() : 'settings';
					option.caption = optpos > -1 ? option.caption.substring(0, optpos) : option.caption;
				}
				option.name = (nameprefix != '' ? nameprefix : '') + option.caption;
				option.defvalue = line.substr(line.indexOf(command ? '@' : '=') + 1, 1000).trim();
				option.value = null;
				option.sectionId = section.id;
				option.select = [];

				var pstart = firstdescrline.lastIndexOf('(');
				var pend = firstdescrline.lastIndexOf(')');
				if (pstart > -1 && pend > -1 && pend === firstdescrline.length - 2)
				{
					var paramstr = firstdescrline.substr(pstart + 1, pend - pstart - 1);
					var params = paramstr.split(',');
					for (var pj=0; pj < params.length; pj++)
					{
						option.select.push(params[pj].trim());
					}
					firstdescrline = firstdescrline.substr(0, pstart).trim() + '.';
				}

				if (option.name.substr(nameprefix.length, 1000).indexOf('1.') > -1)
				{
					section.multi = true;
					section.multiprefix = option.name.substr(0, option.name.indexOf('1.'));
				}

				if (!section.multi || option.name.indexOf('1.') > -1)
				{
					section.options.push(option);
				}

				if (section.multi)
				{
					option.template = true;
				}

				option.description = firstdescrline + description;
				description = '';
				firstdescrline = '';
			}
			else
			{
				if (section && section.options.length === 0)
				{
					section.description = firstdescrline + description;
				}
				description = '';
				firstdescrline = '';
			}
		}

		return config;
	}

	function findOption(options, name)
	{
		if (!options)
		{
			return null;
		}

		name = name.toLowerCase();

		for (var i=0; i < options.length; i++)
		{
			var option = options[i];
			if ((option.Name && option.Name.toLowerCase() === name) ||
				(option.name && option.name.toLowerCase() === name))
			{
				return option;
			}
		}

		return null;
	}
	this.findOption = findOption;

	function mergeValues(config, values)
	{
		// copy values
		for (var i=0; i < config.length; i++)
		{
			var section = config[i];
			if (section.multi)
			{
				// multi sections (news-servers, scheduler)

				var subexists = true;
				for (var k=1; subexists; k++)
				{
					subexists = false;
					for (var m=0; m < section.options.length; m++)
					{
						var option = section.options[m];
						if (option.name.indexOf('1.') > -1)
						{
							var name = option.name.replace(/1/, k);
							var val = findOption(values, name);
							if (val)
							{
								subexists = true;
								break;
							}
						}
					}
					if (subexists)
					{
						for (var m=0; m < section.options.length; m++)
						{
							var option = section.options[m];
							if (option.template)
							{
								var name = option.name.replace(/1/, k);
								// copy option
								var newoption = $.extend({}, option);
								newoption.name = name;
								newoption.caption = option.caption.replace(/1/, k);
								newoption.template = false;
								newoption.multiid = k;
								newoption.value = null;
								section.options.push(newoption);
								var val = findOption(values, name);
								if (val)
								{
									newoption.value = val.Value;
								}
							}
						}
					}
				}
			}
			else
			{
				// simple sections

				for (var j=0; j < section.options.length; j++)
				{
					var option = section.options[j];
					option.value = null;
					var val = findOption(values, option.name);
					if (val && !option.commandopts)
					{
						option.value = val.Value;
					}
				}
			}
		}
	}
	this.mergeValues = mergeValues;

	function shortScriptName(scriptName)
	{
		var shortName = shortScriptNames[scriptName];
		return shortName ? shortName : scriptName;
	}
	this.shortScriptName = shortScriptName;

	function initPostParamConfig(data) 
		{
		// Create one big post-param section. It consists of one item for every post-processing script
		// and additionally includes all post-param options from post-param section of each script.

		var section = {};
		section.id = 'PP-Parameters';
		section.options = [];
		section.description = '';
		section.about = '';
		section.hidden = false;
		section.postparam = true;

		for (var i = 0; i < data.length; i++) 
		{
			if (data[i].PostScript || data[i].QueueScript) 
			{
				var scriptName = data[i].Name;
				var sectionId = Util.makeId(scriptName + ':');
				var option = {};
				option.name = data[i].Name + ':';
				option.caption = data[i].DisplayName;

				option.defvalue = 'no';
				option.description = '';
				option.about = data[i].About || 'Extension script ' + scriptName + '.';
				option.requirements = [];
				option.value = null;
				option.sectionId = sectionId;
				option.select = ['yes', 'no'];
				section.options.push(option);
			}
		}
		return [section];
	}
}(jQuery));


/*** SETTINGS TAB (UI) *********************************************************/

var Config = (new function($)
{
	'use strict';

	// Controls
	var $ConfigNav;
	var $ConfigData;
	var $ConfigTabBadge;
	var $ConfigTabBadgeEmpty;
	var $ConfigContent;
	var $ConfigInfo;
	var $ConfigTitle;
	var $ConfigTable;
	var $ViewButton;
	var $LeaveConfigDialog;
	var $Body;

	// State
	var config = null;
	var filterText = '';
	var lastSection;
	var reloadTime;
	var updateTabInfo;
	var restored = false;
	var compactMode = false;
	var configSaved = false;
	var leaveTarget;
	this.currentSectionID = '';

	this.init = function(options)
	{
		updateTabInfo = options.updateTabInfo;

		$Body = $('html, body');
		$ConfigNav = $('#ConfigNav');
		$ConfigData = $('#ConfigData');
		$ConfigTabBadge = $('#ConfigTabBadge');
		$ConfigTabBadgeEmpty = $('#ConfigTabBadgeEmpty');
		$ConfigContent = $('#ConfigContent');
		$ConfigInfo = $('#ConfigInfo');
		$ConfigTitle = $('#ConfigTitle');
		$ViewButton = $('#Config_ViewButton');
		$LeaveConfigDialog = $('#LeaveConfigDialog');
		$('#ConfigTable_filter').val('');

		Util.show('#ConfigBackupSafariNote', $.browser.safari);
		$('#ConfigTable_filter').val('');
		compactMode = UISettings.read('$Config_ViewCompact', 'no') == 'yes';
		setViewMode();

		$(window).bind('beforeunload', userLeavesPage);

		$ConfigNav.on('click', 'li > a', navClick);

		$ConfigTable = $('#ConfigTable');
		$ConfigTable.fasttable(
			{
				filterInput: $('#ConfigTable_filter'),
				filterClearButton: $("#ConfigTable_clearfilter"),
				filterInputCallback: filterInput,
				filterClearCallback: filterClear
			});
	}

	this.config = function()
	{
		return config;
	}

	this.show = function()
	{
		removeSaveBanner();
		$('#ConfigSaved').hide();
		$('#ConfigLoadInfo').show();
		$('#ConfigLoadServerTemplateError').hide();
		$('#ConfigLoadError').hide();
		$ConfigContent.hide();
		configSaved = false;
	}

	this.shown = function()
	{
		Options.loadConfig({
			complete: Config.buildPage,
			configError: Config.loadConfigError,
			serverTemplateError: Config.loadServerTemplateError
		});
	}

	this.hide = function()
	{
		Options.cleanup();
		config = null;
		$ConfigNav.children().not('.config-static').remove();
		$ConfigData.children().not('.config-static').remove();
	}

	this.loadConfigError = function(message, resultObj)
	{
		$('#ConfigLoadInfo').hide();
		$('#ConfigLoadError').show();
		if (resultObj && resultObj.error && resultObj.error.message)
		{
			message = resultObj.error.message;
		}
		$('#ConfigLoadErrorText').text(message);
	}

	this.loadServerTemplateError = function()
	{
		$('#ConfigLoadInfo').hide();
		$('#ConfigLoadServerTemplateError').show();
		var optConfigTemplate = Options.option('ConfigTemplate');
		$('#ConfigLoadServerTemplateErrorEmpty').toggle(optConfigTemplate === '');
		$('#ConfigLoadServerTemplateErrorNotFound').toggle(optConfigTemplate !== '');
		$('#ConfigLoadServerTemplateErrorWebDir').text(Options.option('WebDir'));
		$('#ConfigLoadServerTemplateErrorConfigFile').text(Options.option('ConfigFile'));
	}

	function findOptionByName(name)
	{
		name = name.toLowerCase();

		for (var k=0; k < config.length; k++)
		{
			var sections = config[k].sections;
			for (var i=0; i < sections.length; i++)
			{
				var section = sections[i];
				for (var j=0; j < section.options.length; j++)
				{
					var option = section.options[j];
					if (!option.template &&
						((option.Name && option.Name.toLowerCase() === name) ||
						 (option.name && option.name.toLowerCase() === name)))
					{
						return option;
					}
				}
			}
		}
		return null;
	}
	this.findOptionByName = findOptionByName;

	function findOptionById(formId)
	{
		for (var k=0; k < config.length; k++)
		{
			var sections = config[k].sections;
			for (var i=0; i < sections.length; i++)
			{
				var section = sections[i];
				for (var j=0; j < section.options.length; j++)
				{
					var option = section.options[j];
					if (option.formId === formId)
					{
						return option;
					}
				}
			}
		}
		return null;
	}

	function findSectionById(sectionId)
	{
		for (var k=0; k < config.length; k++)
		{
			var sections = config[k].sections;
			for (var i=0; i < sections.length; i++)
			{
				var section = sections[i];
				if (section.id === sectionId)
				{
					return section;
				}
			}
		}
		return null;
	}

	this.processShortcut = function(key)
	{
		switch (key)
		{
			case 'Shift+F': $('#ConfigTable_filter').focus(); return true;
			case 'Shift+C': $('#ConfigTable_clearfilter').click(); return true;
		}
	}

	/*** GENERATE HTML PAGE *****************************************************************/

	function buildOptionsContent(section, extensionsec)
	{
		var html = '';

		var lastmultiid = 1;
		var firstmultioption = true;
		var hasoptions = false;

		for (var i=0, op=0; i < section.options.length; i++)
		{
			if (i > 0 && extensionsec && Options.restricted)
			{
				// in restricted mode don't show any options for extension scripts,
				// option's content is hidden content anyway (***)
				break;
			}

			var option = section.options[i];
			if (!option.template)
			{
				if (section.multi && option.multiid !== lastmultiid)
				{
					// new set in multi section
					html += buildMultiRowEnd(section, lastmultiid, true, true);
					lastmultiid = option.multiid;
					firstmultioption = true;
				}
				if (section.multi && firstmultioption)
				{
					html += buildMultiRowStart(section, option.multiid, option);
					firstmultioption = false;
				}
				html += buildOptionRow(option, section);
				hasoptions = true;
				op++;
			}
		}

		if (section.multi)
		{
			html += buildMultiRowEnd(section, lastmultiid, false, hasoptions);
		}

		return html;
	}
	this.buildOptionsContent = buildOptionsContent;

	function buildMultiSetContent(section, multiid)
	{
		var html = '';
		var firstmultioption = true;
		var hasoptions = false;

		for (var i=0, op=0; i < section.options.length; i++)
		{
			var option = section.options[i];
			if (!option.template && option.multiid === multiid)
			{
				if (firstmultioption)
				{
					html += buildMultiRowStart(section, multiid, option);
					firstmultioption = false;
				}
				html += buildOptionRow(option, section);
				hasoptions = true;
				op++;
			}
		}
		html += buildMultiRowEnd(section, multiid, true, hasoptions);

		return html;
	}

	function buildOptionRow(option, section)
	{
		var value = option.value;
		if (option.value === null)
		{
			value = option.defvalue;
		}

		option.formId = (option.name.indexOf(':') == -1 ? 'S_' : '') + Util.makeId(option.name);

		var caption = option.caption;
		if (section.multi)
		{
			caption = '<span class="config-multicaption">' + caption.substring(0, caption.indexOf('.') + 1) + '</span>' + caption.substring(caption.indexOf('.') + 1);
		}

		var html =
			'<div class="control-group ' + option.sectionId + (section.multi ? ' multiid' + option.multiid + ' multiset' : '') + '">'+
				'<label class="control-label">' +
				'<a class="option-name" href="#" data-optid="' + option.formId + '" '+
				'onclick="Config.scrollToOption(event, this)">' + caption + '</a>' +
				(option.value === null && !section.postparam && !option.commandopts ?
					' <a data-toggle="modal" href="#ConfigNewOptionHelp" class="label label-info">new</a>' : '') + '</label>'+
				'<div class="controls">';

		if (option.nocontent)
		{
			option.type = 'info';
			html +=	'<div class="" id="' + option.formId + '"/>';
		}
		else if (option.select.length > 1)
		{
			option.type = 'switch';
			html +=	'<div class="btn-group btn-switch" id="' + option.formId + '">';

			var valfound = false;
			for (var j=0; j < option.select.length; j++)
			{
				var pvalue = option.select[j];
				if (value && pvalue.toLowerCase() === value.toLowerCase())
				{
					html += '<input type="button" class="btn btn-primary" value="' + Util.textToAttr(pvalue) + '" onclick="Config.switchClick(this)">';
					valfound = true;
				}
				else
				{
					html += '<input type="button" class="btn btn-default" value="' + Util.textToAttr(pvalue) + '" onclick="Config.switchClick(this)">';
				}
			}
			if (!valfound)
			{
				html += '<input type="button" class="btn btn-primary" value="' + Util.textToAttr(value) + '" onclick="Config.switchClick(this)">';
			}

			html +='</div>';

		}
		else if (option.select.length === 1)
		{
			option.type = 'numeric';
			html += '<div class="input-append">'+
				'<input type="text" id="' + option.formId + '" value="' + Util.textToAttr(value) + '" class="editnumeric">'+
				'<span class="add-on">'+ option.select[0] +'</span>'+
				'</div>';
		}
		else if (option.caption.toLowerCase().indexOf('password') > -1 &&
			option.name.toLowerCase() !== '*unpack:password')
		{
			option.type = 'password';
			html += '<div class="password-field input-append">' +
				'<input type="password" id="' + option.formId + '" value="' + Util.textToAttr(value) + '" class="editsmall">'+
				'<span class="add-on">'+
				'<label class="checkbox">'+
				'<input type="checkbox" onclick="Config.togglePassword(this, \'' + option.formId + '\')" /> Show'+
				'</label>'+
				'</span>'+
				'</div>';
		}
		else if (option.caption.toLowerCase().indexOf('username') > -1 ||
			(option.caption.indexOf('IP') > -1 && option.name.toLowerCase() !== 'authorizedip'))
		{
			option.type = 'text';
			html += '<input type="text" id="' + option.formId + '" value="' + Util.textToAttr(value) + '" class="editsmall">';
		}
		else if (option.editor)
		{
			option.type = 'text';
			html += '<table class="editor"><tr><td>';
			html += '<input type="text" id="' + option.formId + '" value="' + Util.textToAttr(value) + '">';
			html += '</td><td>';
			html += '<button type="button" id="' + option.formId + '_Editor" class="btn btn-default" onclick="' + option.editor.click + '($(\'input\', $(this).closest(\'table\')).attr(\'id\'))">' + option.editor.caption + '</button>';
			html += '</td></tr></table>';
		}
		else if (option.commandopts)
		{
			option.type = 'command';
			html += '<button type="button" id="' + option.formId + '" class="btn ' + 
				(option.commandopts.indexOf('danger') > -1 ? 'btn-danger' : 'btn-default') + 
				'" onclick="Config.commandClick(this)">' + value +  '</button>';
		}
		else
		{
			option.type = 'text';
			html += '<input type="text" id="' + option.formId + '" value="' + Util.textToAttr(value) + '" class="editlarge">';
		}
		var about = option['about'] || '';
		if (about)
		{
			about = about.replace('\n', ' ') + '\n';
		}
		var description = about + (option['description'] || '');
		if (description) 
		{
			var htmldescr = description;
			htmldescr = htmldescr.replace(/NOTE: do not forget to uncomment the next line.\n/, '');

			// replace option references
			var exp = /\<([A-Z0-9\.]*)\>/ig;
			htmldescr = htmldescr.replace(exp, '<a class="option" href="#" onclick="Config.scrollToOption(event, this)">$1</a>');

			htmldescr = htmldescr.replace(/&/g, '&amp;');

			// add extra new line after Examples not ended with dot
			htmldescr = htmldescr.replace(/Example:.*/g, function (match) {
				return match + (Util.endsWith(match, '.') ? '' : '\n');
			});

			// replace URLs
			exp = /(https?:\/\/[-A-Z0-9+&@#\/%?=~_|!:,.;]*[-A-Z0-9+&@#\/%=~_|])/ig;
			htmldescr = htmldescr.replace(exp, "<a href='$1'>$1</a>");

			// highlight first line
			htmldescr = htmldescr.replace(/\n/, '</span>\n');
			htmldescr = '<span class="help-option-title">' + htmldescr;

			htmldescr = htmldescr.replace(/\n/g, '<br>');
			htmldescr = htmldescr.replace(/NOTE: /g, '<span class="label label-warning">NOTE:</span> ');
			htmldescr = htmldescr.replace(/INFO: /g, '<span class="label label-info">INFO:</span> ');

			if (htmldescr.indexOf('INFO FOR DEVELOPERS:') > -1)
			{
				htmldescr = htmldescr.replace(/INFO FOR DEVELOPERS:<br>/g, '<input class="btn btn-default btn-mini" value="Show more info for developers" type="button" onclick="Config.showSpoiler(this)"><span class="hide">');
				htmldescr += '</span>';
			}

			if (htmldescr.indexOf('MORE INFO:') > -1)
			{
				htmldescr = htmldescr.replace(/MORE INFO:<br>/g, '<input class="btn btn-default btn-mini" value="Show more info" type="button" onclick="Config.showSpoiler(this)"><span class="hide">');
				htmldescr += '</span>';
			}

			if (section.multi)
			{
				// replace strings like "TaskX.Command" and "Task1.Command"
				htmldescr = htmldescr.replace(new RegExp(section.multiprefix + '[X|1]\.', 'g'), '');
			}

			html += '<p class="help-block">' + htmldescr + '</p>';
		}

		html += '</div>';
		html += '</div>';

		return html;
	}

	function buildMultiRowStart(section, multiid, option)
	{
		var name = option.caption;
		var setname = name.substr(0, name.indexOf('.')) || option.prefix + option.multiid;
		var html = '<div class="config-settitle ' + section.id + ' multiid' + multiid + ' multiset">' + setname + '</div>';
		return html;
	}

	function buildMultiRowEnd(section, multiid, hasmore, hasoptions)
	{
		var option = section.options[0];
		var name = option.caption;
		var setname = name.substr(0, name.indexOf('1')) || option['prefix'] || '';
		var html = '';

		if (hasoptions)
		{
			html += '<div class="' + section.id + ' multiid' + multiid + ' multiset multiset-toolbar">';
			html += '<button type="button" class="btn btn-default config-button config-delete" data-multiid="' + multiid + '" ' +
				'onclick="Config.deleteSet(this, \'' + setname + '\',\'' + section.id + '\')">Delete ' + setname + multiid + '</button>';
			html += ' <button type="button" class="btn btn-default config-button" data-multiid="' + multiid + '" ' +
				'onclick="Config.moveSet(this, \'' + setname + '\',\'' + section.id + '\', \'up\')">Move Up</button>';
			html += ' <button type="button" class="btn btn-default config-button" data-multiid="' + multiid + '" ' +
				'onclick="Config.moveSet(this, \'' + setname + '\',\'' + section.id + '\', \'down\')">Move Down</button>';
			if (setname.toLowerCase() === 'feed')
			{
				html += ' <button type="button" class="btn btn-default config-button" data-multiid="' + multiid + '" ' +
					'onclick="Config.previewFeed(this, \'' + setname + '\',\'' + section.id + '\')">Preview Feed</button>';
			}
			if (setname.toLowerCase() === 'server')
			{
				html += ' <button type="button" class="btn btn-default config-button" data-multiid="' + multiid + '" ' +
					'onclick="Config.testConnection(this, \'' + setname + '\',\'' + section.id + '\')">Test Connection</button>';
				html += ' <button type="button" class="btn btn-default config-button" data-multiid="' + multiid + '" ' +
					'onclick="Config.serverStats(this, \'' + setname + '\',\'' + section.id + '\')">Volume Statistics</button>';
			}
			html += '<hr>';
			html += '</div>';
		}

		if (!hasmore)
		{
			html += '<div class="' + section.id + '">';
			html += '<button type="button" class="btn btn-default config-add ' + section.id + ' multiset" onclick="Config.addSet(\'' + setname + '\',\'' + section.id +
			  '\')">Add ' + (hasoptions ? 'another ' : '') + setname + '</button>';
			html += '</div>';
		}

		return html;
	}

	this.buildPage = function(_config)
	{
		config = _config;

		extendConfig();

		$ConfigNav.children().not('.config-static').remove();
		$ConfigData.children().not('.config-static').remove();

		var haveExtensions = false;

		for (var k=0; k < config.length; k++)
		{
			if (k == 1)
			{
				$ConfigNav.append('<li class="divider"></li>');
			}
			if (k == 2) // Extensions section
			{
				$ConfigNav.append('<li class="divider"></li>');
				$ConfigNav.append('<li><a href="#' + ExtensionManager.id + '">' + 'EXTENSION MANAGER' + '</a></li>');
				haveExtensions = true;
			}
			var conf = config[k];
			var added = false;
			for (var i=0; i < conf.sections.length; i++)
			{
				var section = conf.sections[i];
				if (!section.hidden)
				{
					var html = $('<li><a href="#' + section.id + '">' + section.name + '</a></li>');
					if (haveExtensions)
					{
						html.addClass('list-item--nested');
					}
					$ConfigNav.append(html);
					var content = buildOptionsContent(section, k > 1);
					$ConfigData.append(content);
					added = true;
				}
			}
			if (!added)
			{
				var html = $('<li><a href="#' + conf.id + '">' + conf.name + '</a></li>');
				$ConfigNav.append(html);
			}
		}

		// Show extensions manager even if there are no extensions
		if (!haveExtensions)
		{
			$ConfigNav.append('<li class="divider"></li>');
			$ConfigNav.append('<li><a href="#' + ExtensionManager.id + '">' + 'EXTENSION MANAGER' + '</a></li>');
		}

		notifyChanges();

		$ConfigNav.append('<li class="divider hide ConfigSearch"></li>');
		$ConfigNav.append('<li class="hide ConfigSearch"><a href="#Search">SEARCH RESULTS</a></li>');

		$ConfigNav.toggleClass('long-list', $ConfigNav.children().length > 20);

		Config.showSection('Config-Info', false);

		if (filterText !== '')
		{
			filterInput(filterText);
		}

		$('#ConfigLoadInfo').hide();
		$ConfigContent.show();
	}

	function extendConfig()
	{
		for (var i=2; i < config.length; i++)
		{
			var conf = config[i];

			var firstVisibleSection = null;
			var visibleSections = 0;
			for (var j=0; j < conf.sections.length; j++)
			{
				if (!conf.sections[j].hidden)
				{
					if (!firstVisibleSection)
					{
						firstVisibleSection = conf.sections[j];
					}
					visibleSections++;
				}
			}

			// rename sections
			for (var j=0; j < conf.sections.length; j++)
			{
				var section = conf.sections[j];
				section.name = conf.shortName.toUpperCase() + (visibleSections > 1 ? ' - ' + section.name.toUpperCase() + '' : '');
				section.caption = conf.name.toUpperCase() + (visibleSections > 1 ? ' - ' + section.name.toUpperCase() + '' : '');
			}

			if (!firstVisibleSection)
			{
				// create new section for virtual option "About".
				var section = {};
				section.name = conf.shortName.toUpperCase();
				section.caption = conf.name.toUpperCase();
				section.id = conf.id + '_OPTIONS';
				section.options = [];
				firstVisibleSection = section;
				conf.sections.push(section);
			}

			// create virtual option "About" with scripts description.
			var option = {};
			option.caption = 'About ' + conf.shortName;
			option.name = conf.nameprefix + option.caption;
			option.value = '';
			option.defvalue = '';
			option.sectionId = firstVisibleSection.id;
			option.select = [];
			option.about = conf.about;
			option.nocontent = true;
			option.description = conf.description;
			firstVisibleSection.options.unshift(option);
		}

		// register editors for certain options
		var conf = config[1];
		for (var j=0; j < conf.sections.length; j++)
		{
			var section = conf.sections[j];
			for (var k=0; k < section.options.length; k++)
			{
				var option = section.options[k];
				var optname = option.name.toLowerCase();
				if (optname === 'scriptorder')
				{
					option.editor = { caption: 'Reorder', click: 'Config.editScriptOrder' };
				}
				if (optname === 'extensions')
				{
					option.editor = { caption: 'Choose', click: 'Config.editExtensions' };
				}
				if (optname.indexOf('category') > -1 && optname.indexOf('.extensions') > -1)
				{
					option.editor = { caption: 'Choose', click: 'Config.editCategoryExtensions' };
				}
				if (optname.indexOf('feed') > -1 && optname.indexOf('.extensions') > -1)
				{
					option.editor = { caption: 'Choose', click: 'Config.editFeedExtensions' };
				}
				if (optname.indexOf('task') > -1 && optname.indexOf('.param') > -1)
				{
					option.editor = { caption: 'Choose', click: 'Config.editSchedulerScript' };
				}
				if (optname.indexOf('task') > -1 && optname.indexOf('.command') > -1)
				{
					option.onchange = Config.schedulerCommandChanged;
				}
				if (optname.indexOf('.filter') > -1)
				{
					option.editor = { caption: 'Change', click: 'Config.editFilter' };
				}
			}
		}
	}

	function notifyChanges()
	{
		for (var k=0; k < config.length; k++)
		{
			var sections = config[k].sections;
			for (var i=0; i < sections.length; i++)
			{
				var section = sections[i];
				for (var j=0; j < section.options.length; j++)
				{
					var option = section.options[j];
					if (option.onchange && !option.template)
					{
						option.onchange(option);
					}
				}
			}
		}
	}

	function scrollOptionIntoView(optFormId)
	{
		var option = findOptionById(optFormId);

		// switch to tab and scroll the option into view
		Config.showSection(option.sectionId, false);

		var element = $('#' + option.formId);
		var parent = $('html,body');

		parent[0].scrollIntoView(true);
		var offsetY = 15;
		if ($('body').hasClass('navfixed')) {
			offsetY = 55;
		}
		parent.animate({ scrollTop: parent.scrollTop() + element.offset().top - parent.offset().top - offsetY }, { duration: 'slow', easing: 'swing' });
	}

	this.switchClick = function(control)
	{
		var btn  = $('.btn', $(control).parent());
		btn.removeClass('btn-primary');
		btn.addClass('btn-default');
		$(control).addClass('btn-primary');
		$(control).removeClass('btn-default');

		// not for page Postprocess in download details
		if (config)
		{
			var optFormId = $(control).parent().attr('id');
			var option = findOptionById(optFormId);
			
			if (option.onchange)
			{
				option.onchange(option);
			}

			tlsSwitchHelper(option)
		}
	}

	function tlsSwitchHelper(option)
	{
		var defaultPort = '119';
		var defaultTlsPort = '563';

		var defaultPort2 = '80';
		var defaultTlsPort2 = '443';
		
		var suffixStartIdx = option.formId.indexOf('_Encryption');
		if (suffixStartIdx < 0)
		{
			return;
		}

		var portOptionId = option.formId.substring(0, suffixStartIdx) + '_Port';
		var portOption = findOptionById(portOptionId);
		var useTls = getOptionValue(option) === 'yes';
		var currentPort = getOptionValue(portOption);
		var inputField = $('#' + portOptionId)[0];

		if (useTls)
		{
			if (currentPort === defaultPort)
			{
				inputField.value = defaultTlsPort;
				return;
			}

			if (currentPort === defaultPort2)
			{
				inputField.value = defaultTlsPort2;
			}

			return;
		}

		if (!useTls)
		{
			if (currentPort === defaultTlsPort)
			{
				inputField.value = defaultPort;
				return;
			}

			if (currentPort === defaultTlsPort2)
			{
				inputField.value = defaultPort2;
			}

			return;
		}
	}

	function switchGetValue(control)
	{
		var state = $('.btn-primary', control).val();
		return state;
	}

	function switchSetValue(control, value)
	{
		$('.btn', control).removeClass('btn-primary');
		$('.btn@[value=' + value + ']', control).addClass('btn-primary');
	}

	this.togglePassword = function(control, target)
	{
		var checked = $(control).is(':checked');
		$('#'+target).prop('type', checked ? 'text' : 'password');
	}

	/*** CHANGE/ADD/REMOVE OPTIONS *************************************************************/

	function navClick(event)
	{
		event.preventDefault();
		var sectionId = $(this).attr('href').substr(1);
		Config.showSection(sectionId, true);
	}

	this.showSection = function(sectionId, animateScroll)
	{
		this.currentSectionID = sectionId;
		var link = $('a[href="#' + sectionId + '"]', $ConfigNav);
		$('li', $ConfigNav).removeClass('active');
		link.closest('li').addClass('active');
		$ConfigContent.removeClass('search');
		Util.show($ViewButton, sectionId !== 'Config-Info');

		$ConfigInfo.hide();

		if (sectionId === 'Search')
		{
			search();
			return;
		}

		lastSection = sectionId;

		if (sectionId === 'Config-Info')
		{
			$ConfigInfo.show();
			$ConfigData.children().hide();
			$ConfigTitle.text('INFO');
			return;
		}

		if (sectionId === SystemInfo.id)
		{
			$ConfigData.children().hide();
			$('.config-status', $ConfigData).show();
			SystemInfo.loadSystemInfo();
			$ConfigTitle.text('STATUS');
			return;
		}

		if (sectionId === 'Config-System')
		{
			$ConfigData.children().hide();
			$('.config-system', $ConfigData).show();
			markLastControlGroup();
			$ConfigTitle.text('SYSTEM');
			return;
		}

		if (sectionId === ExtensionManager.id)
		{
			$ConfigData.children().hide();
			markLastControlGroup();
			$ConfigTitle.text('EXTENSION MANAGER');
			ExtensionManager.downloadRemoteExtensions();
			return;
		}

		$ConfigData.children().hide();
		var opts = $('.' + sectionId, $ConfigData);
		opts.show();
		markLastControlGroup();

		var section = findSectionById(sectionId);
		$ConfigTitle.text(section.caption ? section.caption : section.name);

		$Body.animate({ scrollTop: 0 }, { duration: animateScroll ? 'slow' : 0, easing: 'swing' });
	}

	this.deleteSet = function(control, setname, sectionId)
	{
		var multiid = parseInt($(control).attr('data-multiid'));
		$('#ConfigDeleteConfirmDialog_Option').text(setname + multiid);
		ConfirmDialog.showModal('ConfigDeleteConfirmDialog', function()
		{
			deleteOptionSet(setname, multiid, sectionId);
		});
	}

	function deleteOptionSet(setname, multiid, sectionId)
	{
		// remove options from page, using a temporary div for slide effect
		var opts = $('.' + sectionId + '.multiid' + multiid, $ConfigData);
		var div = $('<div></div>');
		opts.first().before(div);
		div.append(opts);
		div.slideUp('normal', function()
		{
			div.remove();
		});

		// remove option set from config
		var section = findSectionById(sectionId);
		for (var j=0; j < section.options.length; j++)
		{
			var option = section.options[j];
			if (!option.template && option.multiid === multiid)
			{
				section.options.splice(j, 1);
				j--;
			}
		}

		// reformat remaining sets (captions, input IDs, etc.)
		reformatSection(section, setname);

		section.modified = true;
	}

	function reformatSection(section, setname)
	{
		var hasOptions = false;
		var lastMultiId = 0;
		for (var j=0; j < section.options.length; j++)
		{
			var option = section.options[j];
			if (!option.template)
			{
				if (option.multiid !== lastMultiId && option.multiid !== lastMultiId + 1)
				{
					reformatSet(section, setname, option.multiid, lastMultiId + 1);
				}
				lastMultiId = option.multiid;
				hasOptions = true;
			}
		}

		// update add-button
		var addButton = $('.config-add.' + section.id, $ConfigData);
		addButton.text('Add ' + (hasOptions ? 'another ' : '') + setname);
	}

	function reformatSet(section, setname, oldMultiId, newMultiId)
	{
		for (var j=0; j < section.options.length; j++)
		{
			var option = section.options[j];
			if (!option.template && option.multiid == oldMultiId)
			{
				// reformat multiid
				var div = $('#' + setname + oldMultiId);
				div.attr('id', setname + newMultiId);

				// update captions
				$('.config-settitle.' + section.id + '.multiid' + oldMultiId, $ConfigData).text(setname + newMultiId);
				$('.' + section.id + '.multiid' + oldMultiId + ' .config-multicaption', $ConfigData).text(setname + newMultiId + '.');
				$('.' + section.id + '.multiid' + oldMultiId + ' .config-delete', $ConfigData).text('Delete ' + setname + newMultiId);

				//update data id
				$('.' + section.id + '.multiid' + oldMultiId + ' .config-button', $ConfigData).attr('data-multiid', newMultiId);

				//update class
				$('.' + section.id + '.multiid' + oldMultiId, $ConfigData).removeClass('multiid' + oldMultiId).addClass('multiid' + newMultiId);

				// update input id
				var oldFormId = option.formId;
				option.formId = option.formId.replace(new RegExp(option.multiid), newMultiId);
				$('#' + oldFormId).attr('id', option.formId);

				// update label data-optid
				$('a[data-optid=' + oldFormId + ']').attr('data-optid', option.formId);

				// update editor id
				$('#' + oldFormId + '_Editor').attr('id', option.formId + '_Editor');

				// update name
				option.name = option.name.replace(new RegExp(option.multiid), newMultiId);

				option.multiid = newMultiId;
			}
		}
	}

	this.addSet = function(setname, sectionId)
	{
		// find section
		var section = findSectionById(sectionId);

		// find max multiid
		var multiid = 0;
		for (var j=0; j < section.options.length; j++)
		{
			var option = section.options[j];
			if (!option.template && option.multiid > multiid)
			{
				multiid = option.multiid;
			}
		}
		multiid++;

		// create new multi set
		var addedOptions = [];
		for (var j=0; j < section.options.length; j++)
		{
			var option = section.options[j];
			if (option.template)
			{
				var name = option.name.replace(/1/, multiid);
				// copy option
				var newoption = $.extend({}, option);
				newoption.name = name;
				newoption.caption = option.caption.replace(/1/, multiid);
				newoption.template = false;
				newoption.multiid = multiid;
				section.options.push(newoption);
				addedOptions.push(newoption);
			}
		}

		section.modified = true;

		// visualize new multi set
		var html = buildMultiSetContent(section, multiid);

		var addButton = $('.config-add.' + section.id, $ConfigData);
		addButton.text('Add another ' + setname);

		// insert before add-button, using a temporary div for slide effect
		var div = $('<div>' + html + '</div>');
		div.hide();
		addButton.parent().before(div);

		for (var j=0; j < addedOptions.length; j++)
		{
			var option = addedOptions[j];
			if (option.onchange)
			{
				option.onchange(option);
			}
		}

		div.slideDown('normal', function()
		{
			var opts = div.children();
			opts.detach();
			div.after(opts);
			div.remove();
		});
	}

	this.moveSet = function(control, setname, sectionId, direction)
	{
		var id1 = parseInt($(control).attr('data-multiid'));
		var id2 = direction === 'down' ? id1 + 1 : id1 - 1;

		// swap options in two sets
		var opts1 = $('.' + sectionId + '.multiid' + (direction === 'down' ? id1 : id2), $ConfigData);
		var opts2 = $('.' + sectionId + '.multiid' + (direction === 'down' ? id2 : id1), $ConfigData);

		if (opts1.length === 0 || opts2.length === 0)
		{
			return;
		}

		opts1.first().before(opts2);

		// reformat remaining sets (captions, input IDs, etc.)
		var section = findSectionById(sectionId);
		reformatSet(section, setname, id2, 10000 + id2);
		reformatSet(section, setname, id1, id2);
		reformatSet(section, setname, 10000 + id2, id1);

		section.modified = true;
	}

	this.viewMode = function()
	{
		compactMode = !compactMode;
		UISettings.write('$Config_ViewCompact', compactMode ? 'yes' : 'no');
		setViewMode();
	}

	function setViewMode()
	{
		if (!compactMode)
		{
			$('#Config_ViewCompact > .material-icon').text('');
		}
		else
		{
			$('#Config_ViewCompact > .material-icon').text('done');
		}
		
		$ConfigContent.toggleClass('hide-help-block', compactMode);
	}

	/*** OPTION SPECIFIC EDITORS *************************************************/

	this.editScriptOrder = function(optFormId)
	{
		var option = findOptionById(optFormId);
		ScriptListDialog.showModal(option, config, null);
	}

	this.editExtensions = function(optFormId)
	{
		var option = findOptionById(optFormId);
		ScriptListDialog.showModal(option, config, ['post', 'scan', 'queue', 'defscheduler']);
	}

	this.editCategoryExtensions = function(optFormId)
	{
		var option = findOptionById(optFormId);
		ScriptListDialog.showModal(option, config, ['post', 'scan', 'queue']);
	}

	this.editFeedExtensions = function(optFormId)
	{
		var option = findOptionById(optFormId);
		ScriptListDialog.showModal(option, config, ['feed']);
	}

	this.editSchedulerScript = function(optFormId)
	{
		var option = findOptionById(optFormId);
		var command = getOptionValue(findOptionById(optFormId.replace(/Param/, 'Command')));
		if (command !== 'Script')
		{
			alert('This button is to choose scheduler scripts when option TaskX.Command is set to "Script".');
			return;
		}
		ScriptListDialog.showModal(option, config, ['scheduler']);
	}

	this.schedulerCommandChanged = function(option)
	{
		var command = getOptionValue(option);
		var btnId = option.formId.replace(/Command/, 'Param_Editor');
		Util.show('#' + btnId, command === 'Script');
	}

	this.commandClick = function(button)
	{
		var optFormId = $(button).attr('id');
		var option = findOptionById(optFormId);
		var script = option.name.substr(0, option.name.indexOf(':'));
		var command = option.name.substr(option.name.indexOf(':') + 1, 1000);
		var changedOptions = prepareSaveRequest(true, false, true);

		function execScript()
		{
			ExecScriptDialog.showModal(script, command, 'SETTINGS', changedOptions);
		}

		if (option.commandopts.indexOf('danger') > -1)
		{
			$('#DangerScriptConfirmDialog_OK').text(option.defvalue);
			$('#DangerScriptConfirmDialog_Command').text(command);
			ConfirmDialog.showModal('DangerScriptConfirmDialog', execScript);
		}
		else
		{
			execScript();
		}
	}

	/*** RSS FEEDS ********************************************************************/

	this.editFilter = function(optFormId)
	{
		var option = findOptionById(optFormId);
		FeedFilterDialog.showModal(
			option.multiid,
			getOptionValue(findOptionByName('Feed' + option.multiid + '.Name')),
			getOptionValue(findOptionByName('Feed' + option.multiid + '.URL')),
			getOptionValue(findOptionByName('Feed' + option.multiid + '.Filter')),
			getOptionValue(findOptionByName('Feed' + option.multiid + '.Backlog')),
			getOptionValue(findOptionByName('Feed' + option.multiid + '.PauseNzb')),
			getOptionValue(findOptionByName('Feed' + option.multiid + '.Category')),
			getOptionValue(findOptionByName('Feed' + option.multiid + '.Priority')),
			getOptionValue(findOptionByName('Feed' + option.multiid + '.Interval')),
			getOptionValue(findOptionByName('Feed' + option.multiid + '.Extensions')),
			function(filter)
				{
					var control = $('#' + option.formId);
					control.val(filter);
				});
	}

	this.previewFeed = function(control, setname, sectionId)
	{
		var multiid = parseInt($(control).attr('data-multiid'));
		FeedDialog.showModal(multiid,
			getOptionValue(findOptionByName('Feed' + multiid + '.Name')),
			getOptionValue(findOptionByName('Feed' + multiid + '.URL')),
			getOptionValue(findOptionByName('Feed' + multiid + '.Filter')),
			getOptionValue(findOptionByName('Feed' + multiid + '.Backlog')),
			getOptionValue(findOptionByName('Feed' + multiid + '.PauseNzb')),
			getOptionValue(findOptionByName('Feed' + multiid + '.Category')),
			getOptionValue(findOptionByName('Feed' + multiid + '.Priority')),
			getOptionValue(findOptionByName('Feed' + multiid + '.Interval')),
			getOptionValue(findOptionByName('Feed' + multiid + '.Extensions')));
	}

	/*** TEST SERVER ********************************************************************/

	var connecting = false;

	this.testConnection = function(control, setname, sectionId)
	{
		if (connecting)
		{
			return;
		}

		connecting = true;
		$('#Notif_Config_TestConnectionProgress').fadeIn(function() {
			var multiid = parseInt($(control).attr('data-multiid'));
			var timeout = Math.min(parseInt(getOptionValue(findOptionByName('ArticleTimeout'))), 10);
			var certVerifLevel = getCertVerifLevel(getOptionValue(findOptionByName('Server' + multiid + '.CertVerification')));
			RPC.call('testserver', [
				getOptionValue(findOptionByName('Server' + multiid + '.Host')),
				parseInt(getOptionValue(findOptionByName('Server' + multiid + '.Port'))),
				getOptionValue(findOptionByName('Server' + multiid + '.Username')),
				getOptionValue(findOptionByName('Server' + multiid + '.Password')),
				getOptionValue(findOptionByName('Server' + multiid + '.Encryption')) === 'yes',
				getOptionValue(findOptionByName('Server' + multiid + '.Cipher')),
				timeout,
				certVerifLevel
				],
				function(errtext) {
					$('#Notif_Config_TestConnectionProgress').fadeOut(function() {
						if (errtext == '')
						{
							PopupNotification.show('#Notif_Config_TestConnectionOK');
						}
						else
						{
							AlertDialog.showModal('Connection test failed', errtext);
						}
					});
					connecting = false;
				},
				function(message, resultObj) {
					$('#Notif_Config_TestConnectionProgress').fadeOut(function() {
						if (resultObj && resultObj.error && resultObj.error.message)
						{
							message = resultObj.error.message;
						}
						AlertDialog.showModal('Connection test failed', message);
						connecting = false;
					});
				});
		});
	}

	/*** DOWNLOADED VOLUMES FOR A SERVER *******************************************************/

	this.serverStats = function(control, setname, sectionId)
	{
		var multiid = parseInt($(control).attr('data-multiid'));

		// Because the settings page isn't saved yet and user can reorder servers
		// we cannot just use the server-id in teh call to "StatDialog".
		// Instead we need to find the server in loaded options and get its ID from there.

		var serverName = getOptionValue(findOptionByName('Server' + multiid + '.Name'));
		var serverHost = getOptionValue(findOptionByName('Server' + multiid + '.Host'));
		var serverPort = getOptionValue(findOptionByName('Server' + multiid + '.Port'));
		var serverId = 0;

		// searching by name
		var opt = undefined;
		for (var i = 1; opt !== null; i++)
		{
			var opt = Options.option('Server' + i + '.Name');
			if (opt === serverName)
			{
				serverId = i;
				break;
			}
		}
		if (serverId === 0)
		{
			// searching by host:port
			var host = undefined;
			for (var i = 1; host !== null; i++)
			{
				host = Options.option('Server' + i + '.Host');
				var port = Options.option('Server' + i + '.Port');
				if (host === serverHost && port === serverPort)
				{
					serverId = i;
					break;
				}
			}
		}

		if (serverId === 0)
		{
			AlertDialog.showModal('Downloaded volumes', 'No statistics available for that server yet.');
			return;
		}

		StatDialog.showModal(serverId);
	}

	/*** SAVE ********************************************************************/

	function getOptionValue(option)
	{
		var control = $('#' + option.formId);
		if (option.type === 'switch')
		{
			return switchGetValue(control);
		}
		else
		{
			return control.val();
		}
	}
	this.getOptionValue = getOptionValue;

	function setOptionValue(option, value)
	{
		var control = $('#' + option.formId);
		if (option.type === 'switch')
		{
			switchSetValue(control, value);
		}
		else
		{
			control.val(value);
		}
	}
	this.setOptionValue = setOptionValue;

	// Checks if there are obsolete or invalid options
	function invalidOptionsExist()
	{
		var hiddenOptions = ['ConfigFile', 'AppBin', 'AppDir', 'Version'];

		for (var i=0; i < Options.options.length; i++)
		{
			var option = Options.options[i];
			var confOpt = findOptionByName(option.Name);
			if (!confOpt && hiddenOptions.indexOf(option.Name) === -1)
			{
				return true;
			}
		}

		return false;
	}

	function prepareSaveRequest(onlyUserChanges, webSettings, onlyChangedOptions)
	{
		var modified = false;
		var request = [];
		for (var k = (webSettings ? 0 : 1); k < (webSettings ? 1 : config.length); k++)
		{
			var sections = config[k].sections;
			for (var i=0; i < sections.length; i++)
			{
				var section = sections[i];
				if (!section.postparam)
				{
					for (var j=0; j < section.options.length; j++)
					{
						var option = section.options[j];
						if (!option.template && !(option.type === 'info') && !option.commandopts)
						{
							var oldValue = option.value;
							var newValue = getOptionValue(option);
							if (section.hidden)
							{
								newValue = oldValue;
							}
							if (newValue != null)
							{
								if (onlyUserChanges)
								{
									var optmodified = oldValue != newValue && oldValue !== null;
								}
								else
								{
									var optmodified = (oldValue != newValue) || (option.value === null);
								}
								modified = modified || optmodified;
								if (optmodified || !onlyChangedOptions)
								{
									var opt = {Name: option.name, Value: newValue};
									request.push(opt);
								}
							}
						}
						modified = modified || section.modified;
					}
				}
			}
		}

		if (!webSettings)
		{
			addSavedSettingsOfDeletedExtensions(request);
		}

		return modified || (!onlyUserChanges && invalidOptionsExist()) || restored ? request : [];
	}

	this.saveChanges = function()
	{
		$LeaveConfigDialog.modal('hide');

		var serverSaveRequest = prepareSaveRequest(false, false);
		var webSaveRequest = prepareSaveRequest(false, true);

		if (serverSaveRequest.length === 0 && webSaveRequest.length === 0)
		{
			PopupNotification.show('#Notif_Config_Unchanged');
			return;
		}

		showSaveBanner();

		Util.show('#ConfigSaved_Reload, #ConfigReload', serverSaveRequest.length > 0);

		if (webSaveRequest.length > 0)
		{
			saveWebSettings(webSaveRequest);
		}

		if (serverSaveRequest.length > 0)
		{
			$('#Notif_Config_Failed_Filename').text(Options.option('ConfigFile'));
			RPC.call('saveconfig', [serverSaveRequest], saveCompleted);
		}
		else
		{
			// only web-settings were changed, refresh page
			document.location.reload(true);
		}
	}

	function addSavedSettingsOfDeletedExtensions(request)
	{
		if (!config.values)
		{
			return;
		}

		config.values.forEach(function(val)
		{
			var extName = val.Name.split(":")[0];
			var alreadyExists = request.map(function(opt) { return opt.Name; }).indexOf(val.Name) != -1;
			var remoteExtExists = ExtensionManager.getRemoteExtensions().map(function(ext) { return ext.name; }).indexOf(extName) != -1;
			if (!alreadyExists && remoteExtExists)
			{
				var extraOpt = { Name: val.Name, Value: val.Value };
				request.push(extraOpt);
			}
		});
	}

	function getCertVerifLevel(verifLevel)
	{
		var level = verifLevel.toLowerCase();
		switch(level)
		{
			case "none": return 0;
			case "minimal": return 1;
			case "strict": return 2;
			default: return 2;
		}
	}

	function showSaveBanner()
	{
		$('#Config_Save').attr('disabled', 'disabled');
	}

	function removeSaveBanner()
	{
		$('#Config_Save').removeAttr('disabled');
	}

	function saveCompleted(result)
	{
		removeSaveBanner();
		if (result)
		{
			$ConfigContent.fadeOut(function() { $('#ConfigSaved').fadeIn(); });
		}
		else
		{
			PopupNotification.show('#Notif_Config_Failed');
		}
		configSaved = true;
	}

	function saveWebSettings(values)
	{
		for (var i=0; i < values.length; i++)
		{
			var option = values[i];
			var optname = option.Name;
			var optvalue = option.Value;
			optname = optname[0].toLowerCase() + optname.substring(1);
			if (optvalue === 'yes') optvalue = true;
			if (optvalue === 'no') optvalue = false;
			UISettings[optname] = optvalue;
		}
		UISettings.save();
	}

	this.canLeaveTab = function(target)
	{
		if (!config || prepareSaveRequest(true).length === 0 || configSaved)
		{
			return true;
		}

		leaveTarget = target;
		$LeaveConfigDialog.modal({backdrop: 'static'});
		return false;
	}

	function userLeavesPage(e)
	{
		if (config && !configSaved && !UISettings.connectionError && prepareSaveRequest(true).length > 0)
		{
			return "Discard changes?";
		}
	}

	this.discardChanges = function()
	{
		configSaved = true;
		$LeaveConfigDialog.modal('hide');
		leaveTarget.click();
	}

	this.scrollToOption = function(event, control)
	{
		event.preventDefault();

		if ($(control).hasClass('option-name') && !$ConfigContent.hasClass('search'))
		{
			// Click on option title scrolls only from Search-page, not from regual pages
			return;
		}

		var optid = $(control).attr('data-optid');
		if (!optid)
		{
			var optname = $(control).text();
			var option = findOptionByName(optname);
			if (option)
			{
				optid = option.formId;
			}
		}
		if (optid)
		{
			scrollOptionIntoView(optid);
		}
	}

	this.showSpoiler = function(control)
	{
		$(control).hide();
		$(control).next().show();
	}

	function filterInput(value)
	{
		filterText = value;
		if (filterText.trim() !== '')
		{
			$('.ConfigSearch').show();
			Config.showSection('Search', true);
		}
		else
		{
			filterClear();
		}
	}

	function filterClear()
	{
		filterText = '';
		Config.showSection(lastSection, true);
		$('.ConfigSearch').hide();
		$ConfigTabBadge.hide();
		$ConfigTabBadgeEmpty.show();
	}

	var searcher = new FastSearcher();

	function search()
	{
		$ConfigTabBadge.show();
		$ConfigTabBadgeEmpty.hide();
		$ConfigContent.addClass('search');

		$ConfigData.children().hide();

		searcher.compile(filterText);

		var total = 0;
		var available = 0;

		for (var k=0; k < config.length; k++)
		{
			var sections = config[k].sections;
			for (var i=0; i < sections.length; i++)
			{
				var section = sections[i];
				if (!section.hidden)
				{
					for (var j=0; j < section.options.length; j++)
					{
						var option = section.options[j];
						if (!option.template)
						{
							total++;
							if (filterOption(option))
							{
								available++;
								var opt = $('#' + option.formId).closest('.control-group');
								opt.show();
							}
						}
					}
				}
			}
		}

		filterStaticPages();

		markLastControlGroup();

		$ConfigTitle.text('SEARCH RESULTS');
		$Body.animate({ scrollTop: 0 }, { duration: 0 });

		updateTabInfo($ConfigTabBadge, { filter: true, available: available, total: total});
	}

	function filterOption(option)
	{
		return searcher.exec({ name: option.caption, description: option.description, value: (option.value === null ? '' : option.value), _search: ['name', 'description', 'value'] });
	}

	function filterStaticPages()
	{
		$ConfigData.children().filter('.config-static').each(function(index, element)
			{
				var name = $('.control-label', element).text();
				var description = $('.controls', element).text();
				var found = searcher.exec({ name: name, description: description, value: '', _search: ['name', 'description'] });
				Util.show(element, found);
			});
	}

	function markLastControlGroup()
	{
		$ConfigData.children().removeClass('last-group');
		$ConfigData.children().filter(':visible').last().addClass('last-group');
	}

	/*** RELOAD ********************************************************************/

	function restart(callback)
	{
		Refresher.pause();
		$('#ConfigReloadInfoNotes').hide();

		$('body').fadeOut(function()
		{
			$('#Navbar, #MainContent').hide();
			$('#ConfigSaved').hide();
			$('body').toggleClass('navfixed', false);
			$('body').show();
			$('#ConfigReloadInfo').fadeIn();
			reloadTime = new Date();
			callback();
		});
	}

	this.reloadConfirm = function()
	{
		ConfirmDialog.showModal('ReloadConfirmDialog', Config.reload);
	}

	this.reload = function()
	{
		$('#ConfigReloadAction').text('Stopping all activities and reloading...');
		restart(function() { RPC.call('reload', [], reloadCheckStatus); });
	}

	function reloadCheckStatus()
	{
		RPC.call('status', [], function(status)
			{
				// OK, checking if it is a restarted instance
				if (status.UpTimeSec >= Status.status.UpTimeSec)
				{
					// the old instance is not restarted yet
					// waiting 0.5 sec. and retrying
					setTimeout(reloadCheckStatus, 500);
					reloadCheckNotes();
				}
				else
				{
					// restarted successfully
					$('#ConfigReloadAction').text('Reloaded successfully. Refreshing the page...');
					// refresh page
					document.location.reload(true);
				}
			},
			function()
			{
				// Failure, waiting 0.5 sec. and retrying
				setTimeout(reloadCheckStatus, 500);
				reloadCheckNotes();
			});
	}

	function reloadCheckNotes()
	{
		// if reload takes more than 30 sec. show additional tips
		if (new Date() - reloadTime > 30000)
		{
			$('#ConfigReloadInfoNotes').show(1000);
		}
	}

	this.applyReloadedValues = function(values)
	{
		Options.reloadConfig(values, Config.buildPage);
		restored = true;
	}

	/*** SHUTDOWN ********************************************************************/

	this.shutdownConfirm = function()
	{
		ConfirmDialog.showModal('ShutdownConfirmDialog', Config.shutdown);
	}

	this.shutdown = function()
	{
		$('#ConfigReloadTitle').text('Shutdown NZBGet');
		$('#ConfigReloadAction').text('Stopping all activities...');
		restart(function() { RPC.call('shutdown', [], shutdownCheckStatus); });
	}

	function shutdownCheckStatus()
	{
		RPC.call('version', [], function(version)
			{
				// the program still runs, waiting 0.5 sec. and retrying
				setTimeout(shutdownCheckStatus, 500);
			},
			function()
			{
				// the program has been stopped
				$('#ConfigReloadTransmit').hide();
				$('#ConfigReloadAction').text('The program has been stopped.');
			});
	}

	/*** UPDATE ********************************************************************/

	this.checkUpdates = function()
	{
		UpdateDialog.showModal();
	}
}(jQuery));


/*** CHOOSE SCRIPT DIALOG *******************************************************/

var ScriptListDialog = (new function($)
{
	'use strict'

	// Controls
	var $ScriptListDialog;
	var $ScriptTable;
	var option;
	var config;
	var kind;
	var scriptList;
	var allScripts;
	var orderChanged;
	var orderMode;

	this.init = function()
	{
		$ScriptListDialog = $('#ScriptListDialog');
		$('#ScriptListDialog_Save').click(save);

		$ScriptTable = $('#ScriptListDialog_ScriptTable');

		$ScriptTable.fasttable(
			{
				pagerContainer: '#ScriptListDialog_ScriptTable_pager',
				infoEmpty: 'No scripts found. If you just changed option "ScriptDir", save settings and reload NZBGet.',
				pageSize: 1000
			});

		$ScriptListDialog.on('hidden', function()
		{
			// cleanup
			$ScriptTable.fasttable('update', []);
		});
	}

	this.showModal = function(_option, _config, _kind)
	{
		option = _option;
		config = _config;
		kind = _kind;
		orderChanged = false;
		orderMode = option.name === 'ScriptOrder';

		if (orderMode)
		{
			$('#ScriptListDialog_Title').text('Reorder extensions');
			$('#ScriptListDialog_Instruction').text('Hover mouse over table elements for reorder buttons to appear.');
		}
		else
		{
			$('#ScriptListDialog_Title').text('Choose extensions');
			$('#ScriptListDialog_Instruction').html('Select extension scripts for option <strong>' + option.name + '</strong>.');
		}

		$ScriptTable.toggleClass('table-hidecheck', orderMode);
		$ScriptTable.toggleClass('table-check table-cancheck', !orderMode);
		$('#ScriptListDialog_OrderInfo').toggleClass('alert alert-info', !orderMode);
		Util.show('#ScriptListDialog_OrderInfo', orderMode, 'inline-block');

		buildScriptList();
		var selectedList = Util.parseCommaList(Config.getOptionValue(option));
		updateTable(selectedList);

		$ScriptListDialog.modal({backdrop: 'static'});
	}

	function updateTable(selectedList)
	{
		var reorderButtons = '<div class="btn-row-order-block">' +
		'<i onclick="ScriptListDialog.move(this, \'top\')" class="material-icon btn-row-order">vertical_align_top</i>' + 
		'<i onclick="ScriptListDialog.move(this, \'up\')" class="material-icon btn-row-order">north</i>' + 
		'<i onclick="ScriptListDialog.move(this, \'down\')" class="material-icon btn-row-order">south</i>' +
		'<i onclick="ScriptListDialog.move(this, \'bottom\')" class="material-icon btn-row-order">vertical_align_bottom</i>' + 
		'</div>';
		var data = [];
		for (var i=0; i < scriptList.length; i++)
		{
			var scriptName = scriptList[i];
			var scriptShortName = Options.shortScriptName(scriptName);

			var fields = ['<div class="check img-check"></div>', '<span data-index="' + i + '">' + scriptShortName + '</span>' + reorderButtons];
			var item =
			{
				id: scriptName,
				fields: fields,
			};
			data.push(item);

			if (!orderMode && selectedList && selectedList.indexOf(scriptName) > -1)
			{
				$ScriptTable.fasttable('checkRow', scriptName, true);
			}
		}
		$ScriptTable.fasttable('update', data);
	}

	function buildScriptList()
	{
		var orderList = Util.parseCommaList(Config.getOptionValue(Config.findOptionByName('ScriptOrder')));

		var availableScripts = [];
		var availableAllScripts = [];
		for (var i=2; i < config.length; i++)
		{
			availableAllScripts.push(config[i].scriptName);
			var accept = !kind;
			if (!accept)
			{
				for (var j=0; j < kind.length; j++)
				{
					accept = accept || config[i][kind[j]];
				}
			}
			if (accept)
			{
				availableScripts.push(config[i].scriptName);
			}
		}
		availableScripts.sort();
		availableAllScripts.sort();

		scriptList = [];
		allScripts = [];

		// first add all scripts from orderList
		for (var i=0; i < orderList.length; i++)
		{
			var scriptName = orderList[i];
			if (availableScripts.indexOf(scriptName) > -1)
			{
				scriptList.push(scriptName);
			}
			if (availableAllScripts.indexOf(scriptName) > -1)
			{
				allScripts.push(scriptName);
			}
		}

		// add all other scripts of this kind from script list
		for (var i=0; i < availableScripts.length; i++)
		{
			var scriptName = availableScripts[i];
			if (scriptList.indexOf(scriptName) === -1)
			{
				scriptList.push(scriptName);
			}
		}

		// add all other scripts of other kinds from script list
		for (var i=0; i < availableAllScripts.length; i++)
		{
			var scriptName = availableAllScripts[i];
			if (allScripts.indexOf(scriptName) === -1)
			{
				allScripts.push(scriptName);
			}
		}

		return scriptList;
	}

	function save(e)
	{
		e.preventDefault();

		if (!orderMode)
		{
			var	 selectedList = '';
			var checkedRows = $ScriptTable.fasttable('checkedRows');

			for (var i=0; i < scriptList.length; i++)
			{
				var scriptName = scriptList[i];
				if (checkedRows[scriptName])
				{
					selectedList += (selectedList == '' ? '' : ', ') + scriptName;
				}
			}

			var control = $('#' + option.formId);
			control.val(selectedList);
		}

		if (orderChanged)
		{
			var scriptOrderOption = Config.findOptionByName('ScriptOrder');
			var control = $('#' + scriptOrderOption.formId);

			// preserving order of scripts of other kinds which were not visible in the dialog
			var orderList = [];
			for (var i=0; i < allScripts.length; i++)
			{
				var scriptName = allScripts[i];
				if (orderList.indexOf(scriptName) === -1)
				{
					if (scriptList.indexOf(scriptName) > -1)
					{
						orderList = orderList.concat(scriptList);
					}
					else
					{
						orderList.push(scriptName);
					}
				}
			}

			control.val(orderList.join(', '));
		}

		$ScriptListDialog.modal('hide');
	}

	this.move = function(control, direction)
	{
		var index = parseInt($('span', $(control).closest('tr')).attr('data-index'));
		if ((index === 0 && (direction === 'up' || direction === 'top')) ||
			(index === scriptList.length-1 && (direction === 'down' || direction === 'bottom')))
		{
			return;
		}

		switch (direction)
		{
			case 'up':
			case 'down':
				{
					var newIndex = direction === 'up' ? index - 1 : index + 1;
					var tmp = scriptList[newIndex];
					scriptList[newIndex] = scriptList[index];
					scriptList[index] = tmp;
					break;
				}
			case 'top':
			case 'bottom':
				{
					var tmp = scriptList[index];
					scriptList.splice(index, 1);
					if (direction === 'top')
					{
						scriptList.unshift(tmp);
					}
					else
					{
						scriptList.push(tmp);
					}
					break;
				}
		}

		if (!orderChanged && !orderMode)
		{
			$('#ScriptListDialog_OrderInfo').fadeIn(500);
		}

		orderChanged = true;
		updateTable();
	}

}(jQuery));


/*** BACKUP/RESTORE SETTINGS *******************************************************/

var ConfigBackupRestore = (new function($)
{
	'use strict'

	// State
	var settings;
	var filename;

	this.init = function(options)
	{
		$('#Config_RestoreInput')[0].addEventListener('change', restoreSelectHandler, false);
	}

	/*** BACKUP ********************************************************************/

	this.backupSettings = function()
	{
		var settings = '';
		for (var i=0; i < Config.config().values.length; i++)
		{
			var option = Config.config().values[i];
			if (option.Value !== null)
			{
				settings += settings==='' ? '' : '\n';
				settings += option.Name + '=' + option.Value;
			}
		}

		var pad = function(arg) { return (arg < 10 ? '0' : '') + arg }
		var dt = new Date();
		var datestr = dt.getFullYear() + pad(dt.getMonth() + 1) + pad(dt.getDate()) + '-' + pad(dt.getHours()) + pad(dt.getMinutes()) + pad(dt.getSeconds());

		var filename = 'nzbget-' + datestr + '.conf';

		if (!Util.saveToLocalFile(settings, "text/plain;charset=utf-8", filename))
		{
			alert('Unfortunately your browser doesn\'t support access to local file system.\n\n'+
				'To backup settings you can manually save file "nzbget.conf" (' +
				Options.option('ConfigFile')+ ').');
		}
	}

	/*** RESTORE ********************************************************************/

	this.restoreSettings = function()
	{
		if (!window.FileReader)
		{
			alert("Unfortunately your browser doesn't support FileReader API.");
			return;
		}

		var testreader = new FileReader();
		if (!testreader.readAsBinaryString && !testreader.readAsDataURL)
		{
			alert("Unfortunately your browser doesn't support neither \"readAsBinaryString\" nor \"readAsDataURL\" functions of FileReader API.");
			return;
		}

		var inp = $('#Config_RestoreInput');

		// Reset file input control (needed for IE10)
		inp.wrap('<form>').closest('form').get(0).reset();
		inp.unwrap();

		inp.click();
	}

	function restoreSelectHandler(event)
	{
		if (!event.target.files)
		{
			alert("Unfortunately your browser doesn't support direct access to local files.");
			return;
		}
		if (event.target.files.length > 0)
		{
			restoreFromFile(event.target.files[0]);
		}
	}

	function restoreFromFile(file)
	{
		var reader = new FileReader();
		reader.onload = function (event)
		{
			if (reader.readAsBinaryString)
			{
				settings = event.target.result;
			}
			else
			{
				var base64str = event.target.result.replace(/^data:[^,]+,/, '');
				settings = atob(base64str);
			}
			filename = file.name;

			if (settings.indexOf('MainDir=') < 0)
			{
				alert('File ' + filename + ' is not a valid NZBGet backup.');
				return;
			}

			RestoreSettingsDialog.showModal(Config.config(), restoreExecute);
		};

		if (reader.readAsBinaryString)
		{
			reader.readAsBinaryString(file);
		}
		else
		{
			reader.readAsDataURL(file);
		}
	}

	function restoreExecute(selectedSections)
	{
		$('#Notif_Config_Restoring').show();
		setTimeout(function() {
			var values = restoreValues(selectedSections);
			Config.applyReloadedValues(values);
			$('#Notif_Config_Restoring').hide();
			$('#SettingsRestoredDialog').modal({backdrop: 'static'});
		}, 50);
	}

	function restoreValues(selectedSections)
	{
		var config = Config.config();
		var values = config.values;

		settings = settings.split('\n');

		for (var i=0; i < settings.length; i++)
		{
			var optstr = settings[i];
			var ind = optstr.indexOf('=');
			var option = { Name: optstr.substr(0, ind).trim(), Value: optstr.substr(ind+1, 100000).trim() };
			settings[i] = option;
		}

		function removeValue(name)
		{
			var name = name.toLowerCase();
			for (var i=0; i < values.length; i++)
			{
				if (values[i].Name.toLowerCase() === name)
				{
					values.splice(i, 1);
					return true;
				}
			}
			return false;
		}

		function addValue(name)
		{
			var name = name.toLowerCase();
			for (var i=0; i < settings.length; i++)
			{
				if (settings[i].Name.toLowerCase() === name)
				{
					values.push(settings[i]);
					return true;
				}
			}
			return false;
		}

		function restoreOption(option)
		{
			if (!option.template && !option.multiid)
			{
				removeValue(option.name);
				addValue(option.name);
			}
			else if (option.template)
			{
				// delete all multi-options
				for (var j=1; ; j++)
				{
					var optname = option.name.replace('1', j);
					if (!removeValue(optname))
					{
						break;
					}
				}
				// add all multi-options
				for (var j=1; ; j++)
				{
					var optname = option.name.replace('1', j);
					if (!addValue(optname))
					{
						break;
					}
				}
			}
		}

		for (var k=0; k < config.length; k++)
		{
			var conf = config[k];
			for (var i=0; i < conf.sections.length; i++)
			{
				var section = conf.sections[i];
				if (!section.hidden && selectedSections[section.id])
				{
					for (var m=0; m < section.options.length; m++)
					{
						restoreOption(section.options[m]);
					}
				}
			}
		}

		return values;
	}
}(jQuery));


/*** RESTORE SETTINGS DIALOG *******************************************************/

var RestoreSettingsDialog = (new function($)
{
	'use strict'

	// Controls
	var $RestoreSettingsDialog;
	var $SectionTable;

	// State
	var config;
	var restoreClick;

	this.init = function()
	{
		$RestoreSettingsDialog = $('#RestoreSettingsDialog');
		$('#RestoreSettingsDialog_Restore').click(restore);

		$SectionTable = $('#RestoreSettingsDialog_SectionTable');

		$SectionTable.fasttable(
			{
				pagerContainer: $('#RestoreSettingsDialog_SectionTable_pager'),
				rowSelect: UISettings.rowSelect,
				infoEmpty: 'No sections found.',
				pageSize: 1000
			});

		$RestoreSettingsDialog.on('hidden', function()
		{
			// cleanup
			$SectionTable.fasttable('update', []);
		});
	}

	this.showModal = function(_config, _restoreClick)
	{
		config = _config;
		restoreClick = _restoreClick;
		updateTable();
		$RestoreSettingsDialog.modal({backdrop: 'static'});
	}

	function updateTable()
	{
		var data = [];
		for (var k=0; k < config.length; k++)
		{
			var conf = config[k];
			for (var i=0; i < conf.sections.length; i++)
			{
				var section = conf.sections[i];
				if (!section.hidden)
				{
					var fields = ['<div class="check img-check"></div>', '<span data-section="' + section.id + '">' + section.name + '</span>'];
					var item =
					{
						id: section.id,
						fields: fields,
					};
					data.push(item);
				}
			}
		}
		$SectionTable.fasttable('update', data);
	}

	function restore(e)
	{
		e.preventDefault();

		var checkedRows = $SectionTable.fasttable('checkedRows');
		var checkedCount = $SectionTable.fasttable('checkedCount');
		if (checkedCount === 0)
		{
			PopupNotification.show('#Notif_Config_RestoreSections');
			return;
		}

		checkedRows = $.extend({}, checkedRows); // clone
		$RestoreSettingsDialog.modal('hide');

		setTimeout(function() { restoreClick(checkedRows); }, 0);
	}

}(jQuery));


/*** UPDATE DIALOG *******************************************************/
var UpdateDialog = (new function($)
{
	'use strict'

	// Controls
	var $UpdateDialog;
	var $UpdateProgressDialog;
	var $UpdateProgressDialog_Log;
	var $UpdateDialog_Close;

	// State
	var VersionInfo;
	var PackageInfo;
	var UpdateInfo;
	var lastUpTimeSec;
	var installing = false;
	var logReceived = false;
	var foreground = false;

	this.init = function()
	{
		$UpdateDialog = $('#UpdateDialog');
		$('#UpdateDialog_InstallStable,#UpdateDialog_InstallTesting').click(install);
		$UpdateProgressDialog = $('#UpdateProgressDialog');
		$UpdateProgressDialog_Log = $('#UpdateProgressDialog_Log');
		$UpdateDialog_Close = $('#UpdateDialog_Close');

		$UpdateDialog.on('hidden', resumeRefresher);
		$UpdateProgressDialog.on('hidden', resumeRefresher);
	}

	function resumeRefresher()
	{
		if (!installing && foreground)
		{
			Refresher.resume();
		}
	}

	this.showModal = function()
	{
		foreground = true;
		this.performCheck();
		$UpdateDialog.modal({backdrop: 'static'});
	}

	this.checkUpdate = function()
	{
		var lastCheck = new Date(parseInt(UISettings.read('LastUpdateCheck', '0')));
		var hoursSinceLastCheck = Math.abs(new Date() - lastCheck) / (60*60*1000);
		if (Options.option('UpdateCheck') !== 'none' && hoursSinceLastCheck > 12)
		{
			UISettings.write('LastUpdateCheck', new Date() / 1);
			this.performCheck();
		}
	}

	this.performCheck = function()
	{
		$('#UpdateDialog_Install').hide();
		$('#UpdateDialog_CheckProgress').show();
		$('#UpdateDialog_CheckFailed').hide();
		$('#UpdateDialog_Versions').hide();
		$('#UpdateDialog_UpdateAvail').hide();
		$('#UpdateDialog_DownloadAvail').hide();
		$('#UpdateDialog_UpdateNotAvail').hide();
		$('#UpdateDialog_InstalledInfo').show();
		$UpdateDialog_Close.text(foreground ? 'Close' : 'Remind Me Later');
		$('#UpdateDialog_VerInstalled').text(Options.option('Version'));

		PackageInfo = {};
		VersionInfo = {};
		UpdateInfo = {};

		installing = false;
		if (foreground)
		{
			Refresher.pause();
		}

		RPC.call('readurl', ['https://nzbget.com/info/nzbget-version.json?nocache=' + new Date().getTime(), 'nzbget version info'], loadedUpstreamInfo, error);
	}

	function error(e)
	{
		$('#UpdateDialog_CheckProgress').hide();
		$('#UpdateDialog_CheckFailed').show();
	}

	function parseJsonP(jsonp)
	{
		var p = jsonp.indexOf('{');
		var obj = JSON.parse(jsonp.substr(p, 10000));
		return obj;
	}

	function loadedUpstreamInfo(data)
	{
		VersionInfo = parseJsonP(data);
		loadPackageInfo();
	}

	function loadPackageInfo()
	{
		$.get('package-info.json', loadedPackageInfo, 'html').fail(loadedAll);
	}

	function loadedPackageInfo(data)
	{
		PackageInfo = parseJsonP(data);
		if (PackageInfo['update-info-link'])
		{
			RPC.call('readurl', [PackageInfo['update-info-link'], 'nzbget update info'], loadedUpdateInfo, loadedAll);
		}
		else if (PackageInfo['update-info-script'])
		{
			RPC.call('checkupdates', [], loadedUpdateInfo, loadedAll);
		}
		else
		{
			loadedAll();
		}
	}

	function loadedUpdateInfo(data)
	{
		UpdateInfo = parseJsonP(data);
		loadedAll();
	}

	function loadedAll()
	{
		var installedVersion = Options.option('Version');

		$('#UpdateDialog_CheckProgress').hide();
		$('#UpdateDialog_Versions').show();
		$('#UpdateDialog_InstalledInfo').show();

		$('#UpdateDialog_CurStable').text(VersionInfo['stable-version'] ? VersionInfo['stable-version'] : 'no data');
		$('#UpdateDialog_CurTesting').text(VersionInfo['testing-version'] ? VersionInfo['testing-version'] : 'no data');

		$('#UpdateDialog_CurNotesStable').attr('href', VersionInfo['stable-release-notes']);
		$('#UpdateDialog_CurNotesTesting').attr('href', VersionInfo['testing-release-notes']);
		$('#UpdateDialog_DownloadStable').attr('href', VersionInfo['stable-download']);
		$('#UpdateDialog_DownloadTesting').attr('href', VersionInfo['testing-download']);
		Util.show('#UpdateDialog_CurNotesStable', VersionInfo['stable-release-notes']);
		Util.show('#UpdateDialog_CurNotesTesting', VersionInfo['testing-release-notes']);


		$('#UpdateDialog_AvailStable').text(UpdateInfo['stable-version'] ? UpdateInfo['stable-version'] : 'not available');
		$('#UpdateDialog_AvailTesting').text(UpdateInfo['testing-version'] ? UpdateInfo['testing-version'] : 'not available');
		
		if (UpdateInfo['stable-version'] === VersionInfo['stable-version'] &&
			UpdateInfo['testing-version'] === VersionInfo['testing-version'])
		{
			$('#UpdateDialog_AvailStableBlock,#UpdateDialog_AvailTestingBlock').hide();
			$('#UpdateDialog_AvailRow .update-row-name').text('');
			$('#UpdateDialog_AvailRow td').css('border-style', 'none');
		}

		$('#UpdateDialog_DownloadRow td').css('border-style', 'none');
		
		$('#UpdateDialog_AvailNotesStable').attr('href', UpdateInfo['stable-package-info']);
		$('#UpdateDialog_AvailNotesTesting').attr('href', UpdateInfo['testing-package-info']);
		Util.show('#UpdateDialog_AvailNotesStableBlock', UpdateInfo['stable-package-info']);
		Util.show('#UpdateDialog_AvailNotesTestingBlock', UpdateInfo['testing-package-info']);


		var installedVer = installedVersion;

		var canInstallStable = UpdateInfo['stable-version'] &&
			(installedVer < UpdateInfo['stable-version']);
		var canInstallTesting = UpdateInfo['testing-version'] &&
			(installedVer < UpdateInfo['testing-version']);
			  
			  
		Util.show('#UpdateDialog_InstallStable', canInstallStable);
		Util.show('#UpdateDialog_InstallTesting', canInstallTesting);

		var canDownloadStable = 
			(installedVer < VersionInfo['stable-version']);
		var canDownloadTesting = 
			(installedVer < VersionInfo['testing-version']);
			  
			  
		Util.show('#UpdateDialog_DownloadStable', canDownloadStable);
		Util.show('#UpdateDialog_DownloadTesting', canDownloadTesting);

		var hasUpdateSource = PackageInfo['update-info-link'] || PackageInfo['update-info-script'];
		var hasUpdateInfo = UpdateInfo['stable-version'] || UpdateInfo['testing-version'];
		var canUpdate = canInstallStable || canInstallTesting;
		var canDownload = canDownloadStable || canDownloadTesting;
		Util.show('#UpdateDialog_UpdateAvail', canUpdate);
		Util.show('#UpdateDialog_UpdateNotAvail', !canUpdate && !canDownload);
		Util.show('#UpdateDialog_CheckFailed', hasUpdateSource && !hasUpdateInfo);
		Util.show('#UpdateDialog_DownloadRow,#UpdateDialog_DownloadAvail', canDownload && !canUpdate);
		$('#UpdateDialog_AvailRow').toggleClass('hide', !hasUpdateInfo);
		var canUpdateStable = Options.option('UpdateCheck') === 'stable' && (canInstallStable || canDownloadStable) && notificationAllowed('stable');
		var canUpdateTesting = Options.option('UpdateCheck') === 'testing' && (canInstallTesting || canDownloadTesting) && notificationAllowed('testing');
		if (canUpdateStable || canUpdateTesting)
		{
			$UpdateDialog.modal({backdrop: 'static'});
		}
	}

	function notificationAllowed(branch)
	{
		// We don't want to update all users on release day.
		// Parameter "update-rate" controls the spreading speed of the release.
		// It contains comma-separated list of percentages of users, which should get
		// notification about new release at a given day after release.

		var rateCurve = UpdateInfo[branch + '-update-rate'] ? UpdateInfo[branch + '-update-rate'] : VersionInfo[branch + '-update-rate'];
		if (!rateCurve)
		{
			return true;
		}

		var rates = rateCurve.split(',');
		var releaseDate = new Date(UpdateInfo[branch + '-date'] ? UpdateInfo[branch + '-date'] : VersionInfo[branch + '-date']);
		var daysSinceRelease = Math.floor((new Date() - releaseDate) / (1000*60*60*24));
		var coverage = rates[Math.min(daysSinceRelease, rates.length-1)];
		var dice = Math.floor(Math.random() * 100);

		return dice <= coverage;
	}

	function install(e)
	{
		e.preventDefault();
		var kind = $(this).attr('data-kind');
		var script = PackageInfo['install-script'];

		if (!script)
		{
			alert('Something is wrong with the package configuration file "package-info.json".');
			return;
		}

		RPC.call('status', [], function(status)
			{
				lastUpTimeSec = status.UpTimeSec;
				RPC.call('startupdate', [kind], updateStarted);
			});
	}

	function updateStarted(started)
	{
		if (!started)
		{
			PopupNotification.show('#Notif_StartUpdate_Failed');
			return;
		}

		installing = true;
		$UpdateDialog.fadeOut(250, function()
			{
				$UpdateProgressDialog_Log.text('');
				$UpdateProgressDialog.fadeIn(250, function()
					{
						$UpdateDialog.modal('hide');
						$UpdateProgressDialog.modal({backdrop: 'static'});
						updateLog();
					});
			});
	}

	function updateLog()
	{
		RPC.call('logupdate', [0, 100], function(data)
			{
				logReceived = logReceived || data.length > 0;
				if (logReceived && data.length === 0)
				{
					terminated();
				}
				else
				{
					updateLogTable(data);
					setTimeout(updateLog, 500);
				}
			}, terminated);
	}

	function terminated()
	{
		// rpc-failure: the program has been terminated. Waiting for new instance.
		setLogContentAndScroll($UpdateProgressDialog_Log.html() + '\n' + 'NZBGet has been terminated. Waiting for restart...');
		setTimeout(checkStatus, 500);
	}

	function setLogContentAndScroll(html)
	{
		var scroll = $UpdateProgressDialog_Log.prop('scrollHeight') - $UpdateProgressDialog_Log.prop('scrollTop') === $UpdateProgressDialog_Log.prop('clientHeight');
		$UpdateProgressDialog_Log.html(html);
		if (scroll)
		{
			$UpdateProgressDialog_Log.scrollTop($UpdateProgressDialog_Log.prop('scrollHeight'));
		}
	}

	function updateLogTable(messages)
	{
		var html = '';
		for (var i=0; i < messages.length; i++)
		{
			var message = messages[i];
			var text = Util.textToHtml(message.Text);
			if (message.Kind === 'ERROR')
			{
				text = '<span class="update-log-error">' + text + '</span>';
			}
			html = html + text + '\n';
		}
		setLogContentAndScroll(html);
	}

	function checkStatus()
	{
		RPC.call('status', [], function(status)
			{
				// OK, checking if it is a restarted instance
				if (status.UpTimeSec >= lastUpTimeSec)
				{
					// the old instance is not restarted yet
					// waiting 0.5 sec. and retrying
					if ($('#UpdateProgressDialog').is(':visible'))
					{
						setTimeout(checkStatus, 500);
					}
				}
				else
				{
					// restarted successfully, refresh page
					setLogContentAndScroll($UpdateProgressDialog_Log.html() + '\n' + 'Successfully started. Refreshing the page...');
					setTimeout(function()
						{
							document.location.reload(true);
						}, 1000);
				}
			},
			function()
			{
				// Failure, waiting 0.5 sec. and retrying
				if ($('#UpdateProgressDialog').is(':visible'))
				{
					setTimeout(checkStatus, 500);
				}
			});
	}

}(jQuery));


/*** EXEC SCRIPT DIALOG *******************************************************/

var ExecScriptDialog = (new function($)
{
	'use strict'

	// Controls
	var $ExecScriptDialog;
	var $ExecScriptDialog_Log;
	var $ExecScriptDialog_Title;
	var $ExecScriptDialog_Status;

	// State
	var visible = false;

	this.init = function()
	{
		$ExecScriptDialog = $('#ExecScriptDialog');
		$ExecScriptDialog_Log = $('#ExecScriptDialog_Log');
		$ExecScriptDialog_Title = $('#ExecScriptDialog_Title');
		$ExecScriptDialog_Status = $('#ExecScriptDialog_Status');
		$ExecScriptDialog.on('hidden', function() { visible = false; });
	}

	this.showModal = function(script, command, context, changedOptions)
	{
		$ExecScriptDialog_Title.text('Executing script ' + script);
		$ExecScriptDialog_Log.text('');
		$ExecScriptDialog_Status.show();
		$ExecScriptDialog.modal({backdrop: 'static'});
		visible = true;

		RPC.call('startscript', [script, command, context, changedOptions],
			function (result)
			{
				if (result)
				{
					updateLog();
				}
				else
				{
					setLogContentAndScroll('<span class="script-log-error">Script start failed</span>');
					$ExecScriptDialog_Status.hide();
				}
			}
		);
	}

	function updateLog()
	{
		RPC.call('logscript', [0, 100], function(data)
			{
				updateLogTable(data);
				if (visible)
				{
					setTimeout(updateLog, 500);
				}
			});
	}

	function setLogContentAndScroll(html)
	{
		var scroll = $ExecScriptDialog_Log.prop('scrollHeight') - $ExecScriptDialog_Log.prop('scrollTop') === $ExecScriptDialog_Log.prop('clientHeight');
		$ExecScriptDialog_Log.html(html);
		if (scroll)
		{
			$ExecScriptDialog_Log.scrollTop($ExecScriptDialog_Log.prop('scrollHeight'));
		}
	}

	function updateLogTable(messages)
	{
		var html = '';
		for (var i=0; i < messages.length; i++)
		{
			var message = messages[i];
			var text = Util.textToHtml(message.Text);
			if (text.substr(0, 7) === 'Script ')
			{
				$ExecScriptDialog_Status.fadeOut(500);
				if (message.Kind === 'INFO')
				{
					text = '<span class="script-log-success">' + text + '</span>';
				}
			}
			if (message.Kind === 'ERROR')
			{
				text = '<span class="script-log-error">' + text + '</span>';
			}
			html = html + text + '\n';
		}
		setLogContentAndScroll(html);
	}

}(jQuery));

/*** EXTENSION MANAGER *******************************************************/

function Extension()
{
	this.id = '';
	this.name = '';
	this.entry = '';
	this.displayName = '', 
	this.version = '';
	this.author = '';
	this.homepage = '';
	this.about = '';
	this.url = '';
	this.testError = '';
	this.nzbgetMinVersion = '';
	this.isActive = false;
	this.installed = false;
	this.outdated = false;
}

var ExtensionManager = (new function($)
{
	'use strict'

	this.id = 'extension-manager';
	this.table = 'ExtensionManagerTable';
	this.tbody = 'ExtensionManagerTBody';
	this.extensionsUrl = 'https://raw.githubusercontent.com/nzbgetcom/nzbget-extensions/main/extensions.json';

	var scriptOrderId = 'ScriptOrder';
	var extensionsId = 'Extensions';

	var defaultSectionName = 'OPTIONS';
	
	var installedExtensions = [];
	var remoteExtensions = [];

	this.getRemoteExtensions = function()
	{
		return remoteExtensions;
	}

	this.setExtensions = function(exts)
	{
		var value = Config.findOptionByName(extensionsId).value;
		var activeExtensions = splitString(value);
		installedExtensions = exts.map(function(ext) 
		{
			var extension = new Extension();
			extension.id = ext.Name + '_' + defaultSectionName;
			extension.entry = ext.Entry;
			extension.displayName = ext.DisplayName;
			extension.version = ext.Version;
			extension.author = ext.Author;
			extension.homepage = ext.Homepage;
			extension.about = ext.About;
			extension.name = ext.Name;
			extension.nzbgetMinVersion = ext.NZBGetMinVersion;
			extension.installed = true;
			extension.isActive = activeExtensions.indexOf(ext.Name) != -1;
			return extension;
		});
		sortExtensions();
	}

	this.downloadRemoteExtensions = function()
	{
		showLoadingBanner();
		showSection();
		hideErrorBanner();
		RPC.call('readurl', [this.extensionsUrl, 'Fetch list of available extensions.'],
			function(data)
			{
				remoteExtensions = JSON.parse(data).map(function(ext) 
				{
					var extension = new Extension();
					extension.id = ext.Name + '_' + defaultSectionName;
					extension.displayName = ext.displayName;
					extension.version = ext.version;
					extension.nzbgetMinVersion = ext['nzbgetMinVersion'] || '';
					extension.author = ext.author;
					extension.homepage = ext.homepage;
					extension.about = ext.about;
					extension.name = ext.name;
					extension.url = ext.url;
					extension.installed = false;
					return extension;
				});
				removeLoadingBanner();
				showHelpBlock();
				hideSection();
				render(getAllExtensions());
			},
			function(error) 
			{
				hideLoadingBanner();
				render(getAllExtensions());
				showErrorBanner("Failed to fetch the list of available extensions", error);
			}
		);
	}

	function render(extensions)
	{
		if (Config.currentSectionID !== ExtensionManager.id)
		{
			return;
		}

		var section = $('.' + ExtensionManager.id);
		var table = $('#' + ExtensionManager.table);
		var tbody = $('#' + ExtensionManager.tbody);
		tbody.empty();

		var firstIntalledIdx = extensions.findIndex(function(ext) { return ext.installed; });
		var lastInstalledIdx = extensions.length - 1;
		extensions.forEach(function(ext, i) {
			var raw = $('<tr></tr>')
				.append(getExtActiveStatusCell(ext))
				.append(getCeneteredTextCell(ext.displayName))
				.append(getTextCell(ext.about))
				.append(getCeneteredTextCell(ext.version))
				.append(getHomepageCell(ext))
				.append(getActionBtnsCell(ext))
				.append(getSortExtensionsBtnsCell(ext, i === firstIntalledIdx, i === lastInstalledIdx));
			tbody.append(raw);
		});
		table.append(tbody);
		section.append(table);
		section.show();
	}

	function getAllExtensions()
	{
		var remote = [];
		for (var i = 0; i < remoteExtensions.length; i++) {
			var extension = remoteExtensions[i];

			if (!checkNzbgetMinRequiredVersion(extension.nzbgetMinVersion))
			{
				continue;
			}

			var idx = installedExtensions.map(function(ext) { return ext.name; }).indexOf(extension.name);
			if (idx == -1)
			{
				remote.push(extension);
			}
			else
			{
				var installedExt = installedExtensions[idx];
				installedExt.outdated = isOutdated(installedExt.version, extension.version);
				installedExt.url = extension.url;
			}
		}

		return remote.concat(installedExtensions);
	}

	function isOutdated(v1, v2)
	{
		return v1.localeCompare(v2, undefined, { numeric: true, sensitivity: 'base' }) < 0;
	}

	function checkNzbgetMinRequiredVersion(extVersion)
	{
		var nzbgetVersion = Options.option('Version');
		return isOutdated(extVersion, nzbgetVersion);
	}

	function downloadExtension(ext)
	{
		hideErrorBanner();
		disableAllBtns(ext, true);

		RPC.call('downloadextension', [ext.url, ext.name], 
			function(_) 
			{
				syncSettingsAndSaveConfig();
			},
			function(error)
			{
				disableAllBtns(ext, false);
				showErrorBanner("Failed to download " + ext.name, error);
			}
		);
	}

	function updateExtension(ext)
	{
		hideErrorBanner();
		disableAllBtns(ext, true);

		RPC.call('updateextension', [ext.url, ext.name], 
			function(_) 
			{
				syncSettingsAndSaveConfig();
			},
			function(error)
			{
				disableAllBtns(ext, false);
				showErrorBanner("Failed to update " + ext.name, error);
			}
		);
	}

	function deleteExtension(ext, deleteWithSettings)
	{
		hideErrorBanner();
		disableAllBtns(ext, true);

		RPC.call('deleteextension', [ext.name], 
			function(_) 
			{
				if (deleteWithSettings)
				{
					deleteSettings(ext.name);
				}
				deleteGlobalSettings(ext.name);

				RPC.call('saveconfig', [Config.config().values], 
				function(_) 
				{
					updatePage();
				}),
				function(error) 
				{
					updatePage();
					showErrorBanner("Failed to save the config file", error);
				}
			},
			function(error) 
			{
				disableAllBtns(ext, false);
				showErrorBanner("Failed to delete " + ext.name, error);
			}
		);
	}

	function update()
	{
		render(getAllExtensions());
	}

	function showSection()
	{
		$('.' + ExtensionManager.id).show();
	}

	function hideSection()
	{
		$('.' + ExtensionManager.id).hide();
	}

	function showDeleteExtensionDropdown(ext)
	{
		var delBtn = $('#DeleteBtn_' + ext.name);
		var dropdown = $('#DeleteExtensionMenu');
		var deleteExtItem = $('#DeleteExtensionItem');
		var deleteExtWithSettingsItem = $('#DeleteExtensionWithSettingsItem');
		deleteExtItem.off('click').on('click', function() { deleteExtension(ext, false); });
		deleteExtWithSettingsItem.off('click').on('click', function() { deleteExtension(ext, true); });
		Frontend.showPopupMenu(dropdown, 'bottom-left',
		{ 
			left: delBtn.offset().left - 30, 
			top: delBtn.offset().top,
			width: delBtn.width() + 30, 
			height: delBtn.outerHeight() - 2 
		});
	}

	function splitString(str)
	{
		if (!str)
			return [];
		return str.split(/[,\s;]+/);
	}

	function updatePage()
	{
		Options.loadConfig({
			complete: function(conf) {
				Options.update();
				Config.buildPage(conf);
				Config.showSection(ExtensionManager.id, false);
			},
			configError: Config.loadConfigError,
			serverTemplateError: Config.loadServerTemplateError,
		});
	}

	function deleteGlobalSettings(extName)
	{
		if (!Config.config() || !Config.config()['values'])
			return;

		var values = Config.config().values;
		values.forEach(function(value, i)
		{
			if (value.Name == extensionsId)
			{
				var newVal = Config.getOptionValue(Config.findOptionByName(extensionsId));
				values[i].Value = deleteExtFromPropVal(newVal, extName);
			}

			if (value.Name == scriptOrderId)
			{
				var newVal = Config.getOptionValue(Config.findOptionByName(scriptOrderId));
				values[i].Value = deleteExtFromPropVal(newVal, extName);
			}
		});
	}

	function syncGlobalSettings()
	{
		if (!Config.config() || !Config.config()['values'])
			return;

		var values = Config.config().values;
		var changed = false;
		values.forEach(function(value, i)
		{
			if (value.Name == extensionsId)
			{
				var newVal = Config.getOptionValue(Config.findOptionByName(extensionsId));
				if (values[i].Value !== newVal)
				{
					values[i].Value = newVal;
					changed = true;
				}
			}

			if (value.Name == scriptOrderId)
			{
				var newVal = Config.getOptionValue(Config.findOptionByName(scriptOrderId));
				if (values[i].Value !== newVal)
				{
					values[i].Value = newVal;
					changed = true;
				}
			}
		});

		return changed;
	}

	function syncSettingsAndSaveConfig()
	{
		var changed = syncGlobalSettings();
		if (changed)
		{
			RPC.call('saveconfig', [Config.config().values], 
			function(_) 
			{
				updatePage();
			}),
			function(error) 
			{
				showErrorBanner("Failed to save the config file", error);
			}
		}
		else
		{
			updatePage();
		}
	}

	function deleteExtFromPropVal(propVal, extName)
	{
		return splitString(propVal).filter(function(name) { return name !== extName; }).join(", ");
	}

	function sortExtensionsByOrder(extensions, order)
	{
		if (!order)
		{
			extensions.sort(function(a, b) { return a.name - b.name; });
			return;
		}

		extensions.sort(function(a, b) {
			var idxA = order.indexOf(a.name);
			var idxB = order.indexOf(b.name);
		  
			if (idxA === -1) 
			{
				return 1;
			}

			if (idxB === -1) 
			{
				return -1;
			}
		  
			return idxA - idxB;
		});
	}

	function showErrorBanner(title, message)
	{
		var banner = $('#ExtensionsErrorAlert');
		$('#ExtensionsErrorAlert-title').text(title);
		$('#ExtensionsErrorAlert-text').html(message);
		banner
			.show()
			.off('click')
			.on('click', function() { banner.hide(); });
	}

	function hideErrorBanner()
	{
		$('#ExtensionsErrorAlert').hide();
	}

	function disableAllBtns(ext, disable)
	{
		disableDownloadBtn(ext, disable);
		disableDeleteBtn(ext, disable);
		disableUpdateBtn(ext, disable);
		disableDeleteBtn(ext, disable);
		disableConfigureBtn(ext, disable);
		disableActivateBtn(ext, disable);
		disableOrderingBtns(ext, disable);
	}

	function disableDeleteBtn(ext, disabled)
	{
		disableBtnToggle('#DeleteBtn_' + ext.name, disabled);
	}

	function disableDownloadBtn(ext, disabled)
	{
		disableBtnToggle('#DownloadBtn_' + ext.name, disabled);
	}

	function disableUpdateBtn(ext, disabled)
	{
		disableBtnToggle('#UpdateBtn_' + ext.name, disabled);
	}

	function disableConfigureBtn(ext, disabled)
	{
		disableBtnToggle('#ConfigureBtn_' + ext.name, disabled);
	}

	function disableActivateBtn(ext, disabled)
	{
		disableBtnToggle('#ActivateBtn_' + ext.name, disabled);
	}

	function disableBtnToggle(id, disabled)
	{
		if (disabled)
		{
			$(id).addClass('btn--disabled');
			return;
		}

		$(id).removeClass('btn--disabled');
	}

	function disableOrderingBtns(ext, disabled)
	{
		if (disabled)
		{
			$('#MvTopBtn_' + ext.name).off('click');
			$('#MvUpBtn_' + ext.name).off('click');
			$('#MvDownBtn_' + ext.name).off('click');
			$('#MvBottomBtn_' + ext.name).off('click');
		}
	}

	function showHelpBlock()
	{
		$('#ExtensionsHelpBlock').show();
	}

	function showLoadingBanner()
	{
		$('#ExtensionsLoadingBanner').show();
	}

	function removeLoadingBanner()
	{
		$('#ExtensionsLoadingBanner').remove();
	}

	function hideLoadingBanner()
	{
		$('#ExtensionsLoadingBanner').hide();
	}

	function deleteSettings(extName)
	{
		if (!Config.config() || !Config.config()['values'])
			return;

		var values = Config.config().values;
		values.forEach(function(value, i) {
		{
			if (value.Name.startsWith(extName + ":"))
			{
				values[i] = null;
			}
		}});
		Config.config().values = values.filter(Boolean);
	}

	function activateExt(ext)
	{
		disableAllBtns(ext, true);
		ext.isActive = !ext.isActive;
		var value = Config.getOptionValue(Config.findOptionByName(extensionsId));
		if (!ext.isActive)
		{
			value = deleteExtFromPropVal(value, ext.name);
			Config.setOptionValue(Config.findOptionByName(extensionsId), value);
			update();
			return;
		}

		if (value)
		{
			value += ", " + ext.name
		}
		else
		{
			value = ext.name;
		}
		Config.setOptionValue(Config.findOptionByName(extensionsId), value);

		RPC.call('testextension', [ext.entry], 
			function(_)
			{
				ext.testError = '';
				disableAllBtns(ext, false);
				update();
			},
			function (_)
			{
				ext.testError = 'Failed to find the corresponding executor for ' + ext.entry + '.\nThe extension may not work';
				disableAllBtns(ext, false);
				update();
			}
		);
	}

	function getActionBtnsCell(ext)
	{
		var cell = $('<td class="extension-manager__td btn-toolbar"></td>');
		if (!ext.installed)
		{
			return cell.append(getDownloadBtn(ext));
		}
		return cell
			.append(getUpdateBtn(ext))
			.append(getConfigureBtn(ext))
			.append(getActivateBtn(ext))
			.append(getDeleteBtn(ext));
	}

	function getDeleteBtn(ext)
	{
		var btn = $('<button type="button" data-toggle="dropdown" class="btn btn-danger dropdown-toggle" id="DeleteBtn_' 
			+ ext.name 
			+ '" title="Delete"><i class="material-icon">delete</i></button>')
			.off('click')
			.on('click', function() { showDeleteExtensionDropdown(ext); });

		return btn;
	}

	function getDownloadBtn(ext)
	{
		var btn = $('<button type="button" class="btn btn-primary btn-group" id="DownloadBtn_' 
			+ ext.name 
			+ '" title="Download"><i class="material-icon">download</i></button>')
			.off('click')
			.on('click', function() { downloadExtension(ext); });

		return btn;
	}

	function getUpdateBtn(ext)
	{
		var btn = $('<button type="button" class="btn btn-info btn-group" id="UpdateBtn_' 
			+ ext.name 
			+ '"><i class="material-icon">update</i></button>');
		if (ext.outdated)
		{
			btn.attr({ title: "Update to new version" });
			btn.off('click').on('click', function() { updateExtension(ext); });
		}
		else
		{
			btn.attr({ disabled: true });
			btn.attr({ title: "Extension up-to-date!" });
			btn.addClass('btn--disabled');
		}

		return btn;
	}

	function getConfigureBtn(ext)
	{
		var btn = $('<button type="button" class="btn btn-default btn-group" id="ConfigureBtn_' 
			+ ext.name 
			+ '" title="Configure"><i class="material-icon">settings</i></button>')
			.off('click')
			.on('click', function() { Config.showSection(ext.id, true); });

		return btn;
	}

	function getActivateBtn(ext)
	{
		var btn = $('<button type="button" class="btn btn-group" id="ActivateBtn_' 
			+ ext.name 
			+ '"></button>')
			.off('click')
			.on('click', function() { activateExt(ext); });
		if (ext.isActive && !ext.testError)
		{	
			btn.append('<i class="material-icon">pause</i>');
			btn.attr({ title: "Deactivate" });
			btn.addClass('btn-warning');
		}
		else if(!ext.isActive && !ext.testError)
		{
			btn.append('<i class="material-icon">play_arrow</i>');
			btn.attr({ title: "Activate for new downloads" });
			btn.addClass('btn-success');
		}
		else
		{
			btn.append('<i class="material-icon">warning</i>');
			btn.attr({ title: ext.testError });
			btn.addClass('btn-warning');
		}
		
		return btn;
	}

	function getEmptyCell()
	{
		return $('<td class="extension-manager__td text-center"><span>-</span></td>');
	}

	function getExtActiveStatusCell(ext)
	{
		if (!ext.installed)
		{
			return getEmptyCell();
		}
		var cell = $('<td class="extension-manager__td"></td>');
		var container = $('<div></div>');
		var circle = $('<div style="margin: auto;">');
		if (ext.isActive)
		{	
			circle.addClass("green-circle");
			circle.attr({ title: "Active" });
		}
		else
		{
			circle.addClass("red-circle");
			circle.attr({ title: "Not active" });
		}
		container.append(circle);
		cell.append(container);
		return cell;
	}

	function getHomepageCell(ext)
	{
		if (ext.homepage)
		{
			var cell = $('<td class="extension-manager__td text-center">');
			cell.append($('<a href="' + ext.homepage + '" target="_blank"><i class="material-icon" title="Homepage">home</i></a>'));
			return cell;
		}
		
		return getEmptyCell();
	}

	function getCeneteredTextCell(text)
	{
		if (text)
		{
			var cell = $('<td class="extension-manager__td text-center">');
			cell.append(text);
			return cell;
		}
		
		return getEmptyCell();
	}

	function getTextCell(text)
	{
		if (text)
		{
			var cell = $('<td class="extension-manager__td">');
			cell.append(text);
			return cell;
		}
		
		return getEmptyCell();
	}

	function getSortExtensionsBtnsCell(ext, isFirst, isLast)
	{
		if (!ext.installed)
		{
			return getEmptyCell();
		}
		var cell = $('<td class="extension-manager__td text-center">');
		var container = $('<div class="btn-row-order-block">');
		var title = "Modify execution order (restart needed)";
		var mvTop = $('<span class="btn-row-order" id="MvTopBtn_' + ext.name +'"><i class="material-icon">vertical_align_top</i></span>')
			.off('click')
			.on('click', function() { moveTop(ext); });
		var mvUp = $('<span class="btn-row-order id="MvUpBtn_' + ext.name +'"><i class="material-icon">north</i></span>')
			.off('click')
			.on('click', function() { moveUp(ext); });
		var mvDown = $('<span class="btn-row-order id="MvDownBtn_' + ext.name +'"><i class="material-icon">south</i></span>')
			.off('click')
			.on('click', function() { moveDown(ext); });
		var mvBottom = $('<span class="btn-row-order id="MvBottomBtn_' + ext.name +'"><i class="material-icon">vertical_align_bottom</i></span>')
			.off('click')
			.on('click', function() { moveBottom(ext); });
		
		mvTop.attr({ title: title});
		mvUp.attr({ title: title});
		mvDown.attr({ title: title});
		mvBottom.attr({ title: title});

		if (isFirst)
		{
			mvTop.addClass('mv-item-arrow-btn--disabled').off('click');
			mvUp.addClass('mv-item-arrow-btn--disabled').off('click');
		}

		if (isLast)
		{
			mvDown.addClass('mv-item-arrow-btn--disabled').off('click');
			mvBottom.addClass('mv-item-arrow-btn--disabled').off('click');
		}

		container.append(mvTop).append(mvUp).append(mvDown).append(mvBottom);
		cell.append(container);
		return cell;
	}

	function getCurrentInstalledExtsOrder()
	{
		var value = Config.getOptionValue(Config.findOptionByName(scriptOrderId));
		if (value)
		{
			var order = splitString(value);
			installedExtensions.forEach(function(ext) {
				if (order.indexOf(ext.name) == -1)
				{
					order.push(ext.name);
				}
			});
			return order;
		}
		var order = installedExtensions.map(function(ext) { return ext.name; });
		return order;
	}

	function moveTop(ext)
	{
		var oldOrder = getCurrentInstalledExtsOrder();
		var newOrder = [ext.name];
		oldOrder.forEach(function(name) {
			if (name !== ext.name)
			{
				newOrder.push(name);
			}
		});
		Config.setOptionValue(Config.findOptionByName(scriptOrderId), newOrder.join(", "));
		sortExtensions();
		update();
	}

	function moveUp(ext)
	{
		var oldOrder = getCurrentInstalledExtsOrder();
		var newOrder = [];
		oldOrder.forEach(function(name, i) {
			if (name === ext.name)
			{
				newOrder[i] = newOrder[i - 1];
				newOrder[i - 1] = ext.name;
			}
			else
			{
				newOrder.push(name);
			}
		});
		Config.setOptionValue(Config.findOptionByName(scriptOrderId), newOrder.join(", "));
		sortExtensions();
		update();
	}

	function moveDown(ext)
	{
		var oldOrder = getCurrentInstalledExtsOrder();
		var newOrder = [];
		var prev = '';
		oldOrder.forEach(function(name, i) {
			if (ext.name === prev)
			{
				newOrder[i - 1] = name;
				prev = '';
				newOrder.push(ext.name);
			}
			else
			{
				prev = name;
				newOrder.push(name);
			}
		});
		Config.setOptionValue(Config.findOptionByName(scriptOrderId), newOrder.join(", "));
		sortExtensions();
		update();
	}

	function moveBottom(ext)
	{
		var oldOrder = getCurrentInstalledExtsOrder();
		var newOrder = [];
		oldOrder.forEach(function(name) {
			if (name !== ext.name)
			{
				newOrder.push(name);
			}
		});
		newOrder.push(ext.name);
		Config.setOptionValue(Config.findOptionByName(scriptOrderId), newOrder.join(", "));
		sortExtensions();
		update();
	}

	function sortExtensions()
	{
		var order = Config.getOptionValue(Config.findOptionByName(scriptOrderId));
		if (order)
		{
			sortExtensionsByOrder(installedExtensions, splitString(order));
		}
	}
}(jQuery))
