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
#define UNIX_X11_SOCKET_PREFIX "/tmp/.X11-unix/X42"
#define UNIX_X11_DISPLAY "DISPLAY=unix:42.0"
int open_listen_usock(char *name);
int x11_get_max_fd(int max);
void x11_fdset(fd_set *readfds);
void x11_fd_isset(fd_set *readfds);
int x11_recv_data(int x11_fd, char *buf, int len);
void x11_fwd_connect_ack(int conn_idx, char *txt);
void x11_fwd_init_msg(int sock_fd);
void x11_init_pool(void);
/*--------------------------------------------------------------------------*/


