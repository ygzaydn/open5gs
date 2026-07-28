# Copyright (C) 2019-2026 by Sukchan Lee <acetcom@gmail.com>

# This file is part of Open5GS.

# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

libhsm_sources = files('''
    ogs-hsm.h

    hsm-wire.h
    hsm-tcp.h

    hsm-wire.c
    hsm-tcp.c
    hsm-client.c
'''.split())

libhsm_inc = include_directories('.')

libhsm = library('ogshsm',
    sources : libhsm_sources,
    version : libogslib_version,
    c_args : '-DOGS_HSM_COMPILATION',
    include_directories : [libhsm_inc, libinc],
    dependencies : [libcore_dep],
    install : true)

libhsm_dep = declare_dependency(
    link_with : libhsm,
    include_directories : [libhsm_inc, libinc],
    dependencies : [libcore_dep])
