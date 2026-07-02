# Copyright (C) 2026 CharOfString <markus_verify@126.com>
#
# This file is part of gxde-dock.
#
# gxde-dock is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# gxde-dock is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with gxde-dock.  If not, see <https://www.gnu.org/licenses/>.

"""
I18N module.
"""
import os

_C_LOCALES = {"", "C", "POSIX", "C.UTF-8", "C.utf8"}

def ui_locale_names():
    """Return UI locales."""
    res = []
    language = os.environ.get("LANGUAGE", "")
    if language:
        res.extend(language.split(":"))

    if not res or all(cur in _C_LOCALES for cur in res):
        res = [os.environ.get("LC_MESSAGES", "")]
    if all(cur in _C_LOCALES for cur in res):
        res = [os.environ.get("LANG", "")]

    fin_ret = []
    for cur in res:
        if cur in _C_LOCALES:
            continue
        locale_name = cur.split(".", 1)[0]
        if locale_name and locale_name not in fin_ret:
            fin_ret.append(locale_name)
    return fin_ret


def locale_fallbacks(locale_name):
    """Country to lang"""
    base, modifier_separator, modifier = locale_name.partition("@")
    language, country_separator, country = base.partition("_")
    candidates = [locale_name]
    if country_separator:
        candidates.append(f"{language}_{country}")
    if modifier_separator:
        candidates.append(f"{language}@{modifier}")
    candidates.append(language)
    return list(dict.fromkeys(candidate for candidate in candidates
        if candidate))


def ui_locale_fallbacks():
    result = []
    for locale_name in ui_locale_names():
        for candidate in locale_fallbacks(locale_name):
            if candidate not in result:
                result.append(candidate)
    return result
