/*****************************************************************************/
/*    Copyright (C) 2017-2018 cloonix@cloonix.net License AGPL-3             */
/*                                                                           */
/*  This program is free software: you can redistribute it and/or modify     */
/*  it under the terms of the GNU Affero General Public License as           */
/*  published by the Free Software Foundation, either version 3 of the       */
/*  License, or (at your option) any later version.                          */
/*                                                                           */
/*  This program is distributed in the hope that it will be useful,          */
/*  but WITHOUT ANY WARRANTY; without even the implied warranty of           */
/*  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            */
/*  GNU Affero General Public License for more details.a                     */
/*                                                                           */
/*  You should have received a copy of the GNU Affero General Public License */
/*  along with this program.  If not, see <http://www.gnu.org/licenses/>.    */
/*                                                                           */
/*****************************************************************************/
#define UNIX_X11_SOCKET_PREFIX "/tmp/.X11-unix/X%d"
int connect_usock(char *name);
void x11_fwd_init(int sock_fd);
int x11_recv_data(int x11_fd, char *buf, int len);
int x11_action(int x11_fd, int sock_fd);
void x11_fwd_connect(int comm_idx);
void x11_fdset(fd_set *readfds);
void x11_fd_isset(fd_set *readfds);
/*--------------------------------------------------------------------------*/


