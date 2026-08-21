#!/usr/bin/env python3

#  This file is part of nzbget. See <https://nzbget.com>.
#
#  Copyright (C) 2026 Denis <denis@nzbget.com>
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program. If not, see <https://www.gnu.org/licenses/>.

"""
generate_locales.py - Generate translation entries for NZBGet config options or extensions.

Purpose:
  Reads either nzbget.conf or an extension's manifest.json and generates or updates
  locales.source.json with translatable strings for the WebUI i18n system.

Usage:
  python3 scripts/generate_locales.py <input_file> [output_file] [--fresh]

Examples:
  # Core config options:
  python3 scripts/generate_locales.py nzbget.conf webui/locales.source.json

  # Extension options:
  python3 scripts/generate_locales.py path/to/manifest.json
"""

import json
import os
import re
import sys

# --- Shared Helpers ---

def join_description(desc):
    if isinstance(desc, list):
        return "\n".join(desc)
    return desc or ""

# --- Extension Locales Generation Logic ---

def translator_note(opt_name, desc_text, ext_name=None):
    safe_name = ext_name or "extension"
    parts = [f"Help text for the {opt_name} option in {safe_name}."]

    if re.search(r"<[^>]+>", desc_text):
        parts.append(
            "DO NOT translate option names enclosed in angle brackets"
            " (e.g., <MinSize>, <TvCategories>)."
        )

    if re.search(r'"[^"]+"', desc_text):
        parts.append(
            "DO NOT translate technical or example values enclosed in quotes."
        )

    if re.search(r"%[a-zA-Z0-9_.^,]+", desc_text):
        parts.append(
            "DO NOT translate format specifiers beginning with %"
            " (e.g., %s, %0e, %sn, %^dn)."
        )

    if re.search(r"\b(Example=)\S", desc_text):
        parts.append("DO NOT translate the example value after 'Example='.")

    if re.search(r"\b(https?://|ftp://)\S+", desc_text):
        parts.append("DO NOT translate URLs.")

    keywords = re.findall(
        r"\b(NOTE|WARNING|INFO|INFO FOR DEVELOPERS|MORE INFO):", desc_text
    )
    if keywords:
        unique = sorted(set(k.upper() for k in keywords))
        parts.append(
            f"DO NOT translate the exact uppercase label"
            f" ({', '.join(unique)}) as they are used to render UI badges."
        )

    if re.search(r"\b(yes|no)\b", desc_text, re.IGNORECASE):
        parts.append("DO NOT translate the fixed values 'yes' and 'no'.")

    return " ".join(parts)


def extract_from_manifest(manifest):
    entries = {}

    # Note: We intentionally do not extract "displayName" for the extension,
    # options, or commands. Translating names/labels is generally unnecessary,
    # and if the translation key is missing, the WebUI's fallback mechanism
    # automatically displays the English displayName from manifest.json.
    ext_name = manifest.get("displayName") or manifest.get("name", "")

    about_text = manifest.get("about", "")
    if about_text:
        do_not_translate = ""
        if ext_name and re.search(rf"\b{re.escape(ext_name)}\b", about_text):
            do_not_translate = f" DO NOT translate '{ext_name}'."
        if re.search(r"\bNZBGet\b", about_text):
            do_not_translate += " DO NOT translate 'NZBGet'."
        entries["about"] = {
            "message": about_text,
            "description": f"Short description of the {ext_name} extension shown in settings." + do_not_translate,
        }

    desc = manifest.get("description", [])
    desc_text = join_description(desc)
    if desc_text:
        do_not_translate = ""
        if ext_name and re.search(rf"\b{re.escape(ext_name)}\b", desc_text):
            do_not_translate = f" DO NOT translate '{ext_name}'."
        if re.search(r"\bNZBGet\b", desc_text):
            do_not_translate += " DO NOT translate 'NZBGet'."
        entries["description"] = {
            "message": desc_text,
            "description": "Full description of the extension shown in settings." + do_not_translate,
        }

    requirements = manifest.get("requirements", [])
    if requirements:
        entries["requirements"] = {
            "message": "\n".join(requirements),
            "description": "System requirements (each requirement is a separate string)."
            + " DO NOT translate technical version strings (e.g., Python 3.8.x).",
        }

    for opt in manifest.get("options", []):
        opt_name = opt.get("name", "")
        if not opt_name:
            continue
        opt_key = opt_name.lower()

        opt_desc = opt.get("description", [])
        if opt_desc:
            entries[f"{opt_key}_desc"] = {
                "message": join_description(opt_desc),
                "description": translator_note(opt_name, join_description(opt_desc), ext_name),
            }

    for cmd in manifest.get("commands", []):
        cmd_name = cmd.get("name", "")
        if not cmd_name:
            continue
        cmd_key = cmd_name.lower()

        cmd_desc = cmd.get("description", [])
        if cmd_desc:
            entries[f"{cmd_key}_desc"] = {
                "message": join_description(cmd_desc),
                "description": translator_note(cmd_name, join_description(cmd_desc), ext_name),
            }

    return entries


def merge_extension_entries(new_entries, existing_entries):
    result = {}
    changed = []
    added = []
    removed = []

    for key in existing_entries:
        if key.startswith("_"):
            result[key] = existing_entries[key]

    for key, new_val in new_entries.items():
        if key in existing_entries:
            old_val = existing_entries[key]
            old_message = old_val.get("message", "") if isinstance(old_val, dict) else ""
            old_description = old_val.get("description", "") if isinstance(old_val, dict) else ""

            if old_message != new_val["message"]:
                changed.append(key)

            result[key] = {
                "message": new_val["message"],
                "description": old_description or new_val["description"]
            }
        else:
            added.append(key)
            result[key] = new_val

    for key in existing_entries:
        if key.startswith("_"):
            continue
        if key not in new_entries:
            removed.append(key)

    if added:
        print(f"  Added: {', '.join(sorted(added))}", file=sys.stderr)
    if changed:
        print(f"  Changed: {', '.join(sorted(changed))}", file=sys.stderr)
    if removed:
        print(f"  Removed: {', '.join(sorted(removed))}", file=sys.stderr)
    if not added and not changed and not removed:
        print("  Up to date", file=sys.stderr)

    return result


def handle_extension(manifest_path, locales_path, fresh_mode):
    try:
        with open(manifest_path, "r", encoding="utf-8") as f:
            manifest = json.load(f)
    except json.JSONDecodeError as e:
        print(f"Error: {manifest_path} is not a valid JSON file: {e}", file=sys.stderr)
        sys.exit(1)

    new_entries = extract_from_manifest(manifest)

    if not locales_path:
        ext_dir = os.path.dirname(manifest_path)
        locales_path = os.path.join(ext_dir, "locales.source.json")

    if not fresh_mode and os.path.exists(locales_path):
        try:
            with open(locales_path, "r", encoding="utf-8") as f:
                existing = json.load(f)
        except json.JSONDecodeError as e:
            print(f"Error: {locales_path} is not a valid JSON file: {e}", file=sys.stderr)
            sys.exit(1)
        print(f"Merging into {locales_path}", file=sys.stderr)
        entries = merge_extension_entries(new_entries, existing)
    else:
        if fresh_mode:
            print(f"Fresh generation: {locales_path}", file=sys.stderr)
        else:
            print(f"Generating: {locales_path}", file=sys.stderr)
        entries = new_entries

    with open(locales_path, "w", encoding="utf-8") as f:
        json.dump(entries, f, indent=4, ensure_ascii=False)
        f.write("\n")

    print(f"  Written {len(entries)} entries", file=sys.stderr)


# --- Core Config Locales Generation Logic ---

def handle_config(conf_file, merge_file, fresh_mode):
    # Load merge file if provided and not in fresh mode
    locales = {}
    existing_keys = set()
    if merge_file and os.path.exists(merge_file) and not fresh_mode:
        try:
            with open(merge_file, "r", encoding="utf-8") as f:
                locales = json.load(f)
        except json.JSONDecodeError as e:
            print(f"Error: {merge_file} is not a valid JSON file: {e}", file=sys.stderr)
            sys.exit(1)
        existing_keys = {k for k in locales if k.startswith("config_desc_")}

    # Parse nzbget.conf
    with open(conf_file, "r", encoding="utf-8") as f:
        lines = f.readlines()

    keys_to_delete = set()
    current_comments = []

    cat_re = re.compile(r"^###\s+(.+?)\s+###$")
    opt_re = re.compile(r"^#?([a-zA-Z0-9_.]+)=(.*)$")
    commented_opt_re = re.compile(r"^#([a-zA-Z0-9_.]+)=.*$")

    for line in lines:
        line_stripped = line.strip()
        if cat_re.match(line_stripped):
            current_comments = []
            continue

        is_commented_opt = commented_opt_re.match(line_stripped)
        if is_commented_opt:
            opt_name = is_commented_opt.group(1)
            is_option = True
        else:
            opt_match = opt_re.match(line_stripped)
            if opt_match:
                opt_name = opt_match.group(1)
                is_option = True
            else:
                is_option = False

        if is_option:
            parts = opt_name.split(".")
            if len(parts) >= 2:
                base_name = re.sub(r"[0-9]+$", "", parts[0])
                opt_key = (base_name + "_" + "_".join(parts[1:])).lower()
            else:
                opt_key = opt_name.replace(".", "_").lower()

            if current_comments:
                first_line = current_comments[0]
                pstart = first_line.rfind("(")
                pend = first_line.rfind(")")
                if (
                    pstart > -1
                    and pend > -1
                    and pend == len(first_line) - 2
                    and first_line.endswith(".")
                ):
                    current_comments[0] = first_line[:pstart].strip() + "."

            temp_comments = []
            for i, comment in enumerate(current_comments):
                if comment == "\x00":
                    if temp_comments and temp_comments[-1] == "\n":
                        temp_comments[-1] = "\n\n"
                    else:
                        temp_comments.append("\n\n")
                else:
                    temp_comments.append(comment)
                    if (
                        i < len(current_comments) - 1
                        and current_comments[i + 1] != "\x00"
                    ):
                        temp_comments.append("\n")

            desc_message = "".join(temp_comments).strip()

            if desc_message:
                desc_hint = f"Description for the option {opt_name}"
                if re.search(r"<[^>]+>", desc_message):
                    desc_hint += " DO NOT translate option names enclosed in angle brackets (e.g., <OptionName>)."
                if re.search(r'"[^"]+"', desc_message):
                    desc_hint += (
                        " DO NOT translate technical values enclosed in quotes."
                    )
                if re.search(
                    r"(NOTE:|WARNING:|INFO:|INFO FOR DEVELOPERS:|MORE INFO:)",
                    desc_message,
                ):
                    desc_hint += " DO NOT translate the exact uppercase keywords (NOTE:, WARNING:, INFO:, INFO FOR DEVELOPERS:, MORE INFO:) as they are used to render UI badges."
                if re.search(
                    r"news[ -]servers?",
                    desc_message,
                    re.IGNORECASE,
                ):
                    desc_hint += " If 'News server' sounds like 'Newspaper' in your language, use 'Usenet server'."

                value_names = re.findall(
                    r"^\s*([A-Za-z]\w*)\s+-\s", desc_message, re.MULTILINE
                )
                if value_names:
                    unique = sorted(set(value_names))
                    desc_hint += (
                        " DO NOT translate configuration option values"
                        f" ({', '.join(unique)}) as they are fixed configuration values."
                    )

                key = f"config_desc_{opt_key}"
                locales[key] = {"message": desc_message, "description": desc_hint}
                keys_to_delete.add(key)

            current_comments = []
            continue

        if line_stripped.startswith("#"):
            if line_stripped.startswith("####"):
                continue
            comment_text = line_stripped[1:]
            if comment_text.startswith(" "):
                comment_text = comment_text[1:]
            if comment_text == "":
                current_comments.append("\x00")
            else:
                current_comments.append(comment_text)
            continue

        if not line_stripped:
            current_comments = []

    # Remove obsolete config_desc keys
    obsolete_keys = [
        k
        for k in locales.keys()
        if k.startswith("config_desc_") and k not in keys_to_delete
    ]
    for k in obsolete_keys:
        del locales[k]

    # Compute changes
    new_keys = keys_to_delete - existing_keys
    removed_keys = set(obsolete_keys)
    changed_keys = []
    for k in keys_to_delete:
        if k in existing_keys:
            old_entry = None
            if merge_file:
                try:
                    with open(merge_file, "r", encoding="utf-8") as f:
                        old_data = json.load(f)
                    if k in old_data:
                        old_entry = old_data[k]
                except Exception:
                    pass
            if old_entry and old_entry != locales[k]:
                changed_keys.append(k)

    if merge_file and os.path.exists(merge_file):
        # Merge mode: surgically replace only the config_desc section
        with open(merge_file, "r", encoding="utf-8") as f:
            original_lines = f.readlines()

        first_desc_line = None
        last_desc_line = None
        for i, line in enumerate(original_lines):
            if '"config_desc_' in line:
                if first_desc_line is None:
                    first_desc_line = i
                last_desc_line = i

        if first_desc_line is not None and last_desc_line is not None:
            section_end = last_desc_line
            brace_depth = 0
            for i in range(last_desc_line, len(original_lines)):
                brace_depth += original_lines[i].count("{") - original_lines[i].count(
                    "}"
                )
                if brace_depth <= 0 and "}" in original_lines[i]:
                    section_end = i
                    break
        else:
            section_end = None

        new_entries = {k: locales[k] for k in locales if k.startswith("config_desc_")}
        if new_entries:
            new_json = json.dumps(new_entries, indent=2, ensure_ascii=False)
            inner_lines = new_json.split("\n")[1:-1]
            if inner_lines:
                inner_lines[-1] = inner_lines[-1].rstrip() + ","
        else:
            inner_lines = []

        if section_end is not None:
            before_lines = original_lines[:first_desc_line]
            after_lines = original_lines[section_end + 1 :]
        else:
            insert_pos = len(original_lines) - 1
            for i in range(len(original_lines) - 1, -1, -1):
                if original_lines[i].strip() == "}":
                    insert_pos = i
                    break
            before_lines = original_lines[:insert_pos]
            after_lines = original_lines[insert_pos:]

        if before_lines:
            last_before = before_lines[-1].rstrip()
            if not last_before.endswith(","):
                before_lines[-1] = last_before + ",\n"

        output_lines = (
            before_lines + [line + "\n" for line in inner_lines] + after_lines
        )

        with open(merge_file, "w", encoding="utf-8") as f:
            f.writelines(output_lines)

        if removed_keys:
            print(f"Removed: {', '.join(sorted(removed_keys))}")
        if new_keys:
            print(f"Added: {', '.join(sorted(new_keys))}")
        if changed_keys:
            print(f"Changed: {', '.join(sorted(changed_keys))}")
        if not removed_keys and not new_keys and not changed_keys:
            print("Up to date")
    else:
        # Fresh generation: output only config_desc entries
        config_desc_only = {
            k: v for k, v in locales.items() if k.startswith("config_desc_")
        }
        if merge_file:
            with open(merge_file, "w", encoding="utf-8") as f:
                json.dump(config_desc_only, f, indent=2, ensure_ascii=False)
                f.write("\n")
            print(f"Generated new file: {merge_file}")
        else:
            json.dump(config_desc_only, sys.stdout, indent=2, ensure_ascii=False)
            print()


# --- Main Entry Point ---

def main():
    fresh_mode = "--fresh" in sys.argv
    args = [arg for arg in sys.argv if arg != "--fresh"]

    if len(args) < 2:
        print(f"Usage: {sys.argv[0]} <input_file> [output_file] [--fresh]", file=sys.stderr)
        print("  <input_file> can be either nzbget.conf or manifest.json", file=sys.stderr)
        sys.exit(1)

    input_file = args[1]
    output_file = args[2] if len(args) > 2 else None

    if not os.path.exists(input_file):
        print(f"Error: {input_file} not found", file=sys.stderr)
        sys.exit(1)

    # Detect input type
    base_name = os.path.basename(input_file).lower()
    if base_name == "manifest.json" or input_file.endswith(".json"):
        handle_extension(input_file, output_file, fresh_mode)
    elif base_name == "nzbget.conf" or input_file.endswith(".conf"):
        handle_config(input_file, output_file, fresh_mode)
    else:
        print(f"Error: Unrecognized input file type for {input_file}.", file=sys.stderr)
        print("Please provide either a .conf file (e.g., nzbget.conf) or a .json file (e.g., manifest.json).", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()