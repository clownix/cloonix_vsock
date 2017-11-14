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
int connect_usock(char *name);
void x11_init(int sock_fd);
void x11_data_rx(int disp_idx, int conn_idx, char *buf, int len);
void x11_connect(int disp_idx, int conn_idx, int sock_fd);
void x11_disconnect(int disp_idx, int conn_idx);
void x11_fdset(fd_set *readfds, int *max);
void x11_fd_isset(fd_set *readfds);
/*--------------------------------------------------------------------------*/


