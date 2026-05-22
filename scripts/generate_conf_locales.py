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
generate_conf_locales.py - Generate translation entries for NZBGet config options.

Purpose:
  Reads nzbget.conf and extracts comment blocks above each configuration option,
  then outputs JSON translation entries (config_desc_*) for the WebUI i18n system.
  These entries are merged into locales.source.json so that config option
  descriptions appear translated in the WebUI Settings page.

How it works:
  1. Parses nzbget.conf, matching comment blocks (lines starting with #)
     followed by option definitions (OptionName=value).
  2. Builds translation keys like config_desc_server_name or config_desc_maindir
     by normalising option names.
  3. If a merge file is provided (2nd arg), loads existing translations and
     merges them, preserving already-translated strings while adding new keys
     and removing obsolete ones.
  4. Writes the updated file in-place (merge mode) or outputs to stdout (fresh generation).

Usage:
  python3 scripts/generate_conf_locales.py                          # fresh generation
  python3 scripts/generate_conf_locales.py nzbget.conf locales.source.json  # merge mode (writes in-place)
"""

import re
import json
import os
import sys


def main():
    script_dir = os.path.dirname(os.path.realpath(__file__))
    conf_file = os.path.join(os.path.dirname(script_dir), "nzbget.conf")
    merge_file = None
    if len(sys.argv) > 1:
        conf_file = sys.argv[1]
    if len(sys.argv) > 2:
        merge_file = sys.argv[2]

    # Load merge file if provided
    locales = {}
    existing_keys = set()
    if merge_file and os.path.exists(merge_file):
        with open(merge_file, "r", encoding="utf-8") as f:
            locales = json.load(f)
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
                    desc_hint += "If 'News server' sounds like 'Newspaper' in your language, use 'Usenet server'."

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
                with open(merge_file, "r", encoding="utf-8") as f:
                    old_data = json.load(f)
                if k in old_data:
                    old_entry = old_data[k]
            if old_entry and old_entry != locales[k]:
                changed_keys.append(k)

    if merge_file:
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
        json.dump(config_desc_only, sys.stdout, indent=2, ensure_ascii=False)
        print()


if __name__ == "__main__":
    main()
