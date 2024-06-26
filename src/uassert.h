/* Copyright (C) 2024 Papavasileiou Dimitris
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
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

#ifndef _UASSERT_H_
#define _UASSERT_H_

void __attribute__((noreturn)) _uassert(
    const char *msg, int line, const char *func);

#define uassert(cond)                                           \
    if (!(cond)) {                                              \
        _uassert(__FILE__ ":%d: %s:: Assertion `" #cond         \
                "` failed.\n", __LINE__, __func__);             \
    }

#endif
