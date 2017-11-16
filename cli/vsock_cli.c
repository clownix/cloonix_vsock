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
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <pty.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <linux/vm_sockets.h>
#include <linux/sockios.h>
#include <netinet/in.h>
#include "mdl.h"
#include "scp_cli.h"
#include "x11_fwd.h"

static struct termios g_orig_term;
static struct termios g_cur_term;
static int g_win_chg_write_fd;
static int g_x11_ok;
static char g_sock_path[MAX_PATH_LEN];
static int g_is_snd;
static int g_is_rcv;



/****************************************************************************/
static void restore_term(void)
{
  tcsetattr(0, TCSADRAIN, &g_orig_term);
  printf("\033[?25h\r\n");
  fflush(stdout);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void cli_exit(int sig)
{
  KOUT("%d", sig);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void win_size_chg(int sig)
{
  char buf[1];
  buf[0] = 'w';
  write(g_win_chg_write_fd, buf, 1);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void send_msg_type_open_pty(int s, char *cmd)
{
  t_msg msg;
  if (cmd)
    {
    msg.type = msg_type_open_cmd;
    msg.len = sprintf(msg.buf, "%s", cmd) + 1;
    }
  else
    {
    msg.type = msg_type_open_bash;
    msg.len = 0;
    }
  if (mdl_queue_write_msg(s, &msg))
    KERR("%d", msg.len);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void send_msg_type_win_size(int s)
{
  t_msg msg;
  msg.type = msg_type_win_size;
  msg.len = sizeof(struct winsize);
  ioctl(0, TIOCGWINSZ, msg.buf);
  if (mdl_queue_write_msg(s, &msg))
    KERR("%d", msg.len);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void config_sigs(void)
{
  if (signal(SIGPIPE, cli_exit))
    KERR("%d", errno);
  if (signal(SIGHUP, cli_exit))
    KERR("%d", errno);
  if (signal(SIGTERM, cli_exit))
    KERR("%d", errno);
  if (signal(SIGINT, cli_exit))
    KERR("%d", errno);
  if (signal(SIGQUIT, cli_exit))
    KERR("%d", errno);
  if (signal(SIGWINCH, win_size_chg))
    KERR("%d", errno);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void config_term(void)
{
  if (atexit(restore_term))
    KOUT("%d", errno);
  g_cur_term = g_orig_term;
  g_cur_term.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL);
  g_cur_term.c_iflag &= ~(IXON|IXOFF);
  g_cur_term.c_oflag &= ~(OPOST);
  g_cur_term.c_lflag &= ~(ECHO|ECHONL|ICANON|ISIG|IEXTEN);
  g_cur_term.c_cflag &= ~(CSIZE|PARENB);
  g_cur_term.c_cflag |= CS8;
  g_cur_term.c_cc[VMIN] = 1;
  g_cur_term.c_cc[VTIME] = 0;
  tcsetattr(0, TCSADRAIN, &g_cur_term);
}
/*--------------------------------------------------------------------------*/

/*****************************************************************************/
static int connect_isock(char *ipascii, int ip, int port)
{
  int s, result = -1;
  struct sockaddr_in sockin;
  memset(&sockin, 0, sizeof(sockin));
  s = socket (AF_INET,SOCK_STREAM,0);
  if (s >= 0)
    {
    sockin.sin_family = AF_INET;
    sockin.sin_port = htons(port);
    sockin.sin_addr.s_addr = htonl(ip);
    if (connect(s, (struct sockaddr*)&sockin, sizeof(sockin)) == 0)
      result = s;
    else
      KOUT("Connect error ip: %s  port: %d\n", ipascii, port);
    }
  else
    KOUT("socket AF_INET SOCK_STREAM create error");
  return result;
}
/*---------------------------------------------------------------------------*/

/****************************************************************************/
int connect_usock(char *name)
{
  int s, result = -1;
  struct sockaddr_un sockun;
  memset(&sockun, 0, sizeof(sockun));
  s = socket(PF_UNIX, SOCK_STREAM, 0);
  if (s >= 0)
    {
    sockun.sun_family = AF_UNIX;
    strcpy(sockun.sun_path, name);
    if (connect(s, (struct sockaddr*)&sockun, sizeof(sockun)) == 0)
      result = s;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int connect_vsock(int cid, int port)
{
  int s, result = -1;
  struct sockaddr_vm vsock;
  memset(&vsock, 0, sizeof(vsock));
  s = socket(AF_VSOCK, SOCK_STREAM, 0);
  if (s >= 0)
    {
    vsock.svm_family = AF_VSOCK,
    vsock.svm_cid = cid;
    vsock.svm_port = port;
    if (connect(s, (struct sockaddr*)&vsock, sizeof(vsock)) == 0)
      result = s;
    else
      KOUT("Connect error cid: %d port: %d\n", cid, port);
    }
  else
    KOUT("socket AF_VSOCK SOCK_STREAM create error");
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void action_win_chg(int sock_fd, int win_chg_read_fd)
{
  int len;
  char buf[16];
  len = read(win_chg_read_fd, buf, sizeof(buf));
  send_msg_type_win_size(sock_fd);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void rx_err_cb (void *ptr, char *err)
{
  (void) ptr;
  KOUT("%s", err);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int rx_msg_cb(void *ptr, int sock_fd, t_msg *msg)
{
  (void) ptr;
  int len, type, disp_idx, conn_idx;
  type = msg->type & 0xFFFF;
  disp_idx = (msg->type >> 24) & 0x00FF;
  conn_idx = (msg->type >> 16) & 0x00FF;

  switch(type)
    {
    case msg_type_data_cli:
      if (mdl_queue_write_raw(1, msg->buf, msg->len))
        KERR("%d", msg->len);
      break;
    case msg_type_end_cli:
      exit(msg->buf[0]);
      break;
    case msg_type_x11_init:
      if (!strcmp(msg->buf, "OK"))
        g_x11_ok = 1;
      else
        KERR("%s", msg->buf); 
      break;
    case msg_type_x11_data:
      x11_data_rx(disp_idx, conn_idx, msg->buf, msg->len);
      break;
    case msg_type_x11_connect:
      x11_connect(disp_idx, conn_idx, sock_fd);
      break;
    case msg_type_x11_disconnect:
      x11_disconnect(disp_idx, conn_idx);
      break;
    default:
      KOUT("%d", msg->type & 0xFFFF);
    }
  return 0;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void action_input_rx(int sock_fd)
{
  int len;
  t_msg msg;
  if (!mdl_queue_write_saturated(sock_fd))
    {
    len = read(0, msg.buf, sizeof(msg.buf));
    if (len <= 0)
      KOUT(" ");
    msg.type = msg_type_data_pty;
    msg.len = len;
    if (mdl_queue_write_msg(sock_fd, &msg))
      KERR("%d", msg.len);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void select_loop_pty(int sock_fd, int win_chg_read_fd, int tmax)
{
  fd_set readfds;
  fd_set writefds;
  int n, max = tmax;
  FD_ZERO(&readfds);
  FD_ZERO(&writefds);
  if ((g_x11_ok) &&
      (!mdl_queue_write_saturated(sock_fd)))
    x11_fdset(&readfds, &max);
  FD_SET(win_chg_read_fd, &readfds);
  if (!mdl_queue_write_saturated(1))
    FD_SET(sock_fd, &readfds);
  if (mdl_queue_write_not_empty(1))
    FD_SET(1, &writefds);
  if (!mdl_queue_write_saturated(sock_fd))
    {
    FD_SET(0, &readfds);
    if (mdl_queue_write_not_empty(sock_fd))
      FD_SET(sock_fd, &writefds);
    }
  else
    FD_SET(sock_fd, &writefds);
  n = select(max + 1, &readfds, &writefds, NULL, NULL);
  if (n <= 0)
    {
    if ((errno != EINTR) && (errno != EAGAIN))
      KOUT(" ");
    }
  else
    {
    if (FD_ISSET(1, &writefds))
      mdl_write(1);
    if (FD_ISSET(sock_fd, &writefds))
      mdl_write(sock_fd);
    if (FD_ISSET(win_chg_read_fd, &readfds))
      action_win_chg(sock_fd, win_chg_read_fd);
    if (FD_ISSET(0, &readfds))
      action_input_rx(sock_fd);
    if (FD_ISSET(sock_fd, &readfds))
      mdl_read(NULL, sock_fd, rx_msg_cb, rx_err_cb);
    if (g_x11_ok)
      x11_fd_isset(&readfds);
    }
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void loop_cli(int sock_fd, char *cmd, char *src, char *dst)
{
  int pipe_fd[2];
  int max = sock_fd;
  if (mdl_open(sock_fd)) 
    KOUT("Error too big fd\n");
  if ((g_is_snd == 0) && (g_is_rcv == 0))
    {
    if (pipe(pipe_fd) < 0)
      printf("Error pipe\n");
    else if (tcgetattr(0, &g_orig_term) < 0)
      printf("Error No pty\n");
    else
      {
      if (pipe_fd[0] > max)
        max = pipe_fd[0];
      mdl_open(1); 
      g_win_chg_write_fd = pipe_fd[1];
      config_term();
      config_sigs();
      send_msg_type_open_pty(sock_fd, cmd);
      send_msg_type_win_size(sock_fd);
      x11_init(sock_fd);
      for(;;)
        select_loop_pty(sock_fd, pipe_fd[0], max);
      }
    }
  else
    scp_loop(g_is_snd, sock_fd, src, dst);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void usage(char *name)
{
  printf("\n%s <vsock_cid> <vsock_port>", name);
  printf("\n%s <vsock_cid> <vsock_port> -cmd \"<cmd + params>\"", name);
  printf("\n%s <vsock_cid> <vsock_port> -snd <loc_src> <rem_dst_dir>", name);
  printf("\n%s <vsock_cid> <vsock_port> -rcv <rem_src> <loc_dst_dir>\n", name);
  printf("\n\t or for tests:\n");
  printf("\n%s -i <ip> <port>", name);
  printf("\n%s -i <ip> <port> -cmd \"<cmd + params>\"", name);
  printf("\n%s -i <ip> <port> -snd <loc_src> <rem_dst_dir>", name);
  printf("\n%s -i <ip> <port> -rcv <rem_src> <loc_dst_dir>\n", name);
  printf("\n%s -u <unix_sock>", name);
  printf("\n%s -u <unix_sock> -cmd \"<cmd + params>\"", name);
  printf("\n%s -u <unix_sock> -snd <loc_src> <rem_dst_dir>", name);
  printf("\n%s -u <unix_sock> -rcv <rem_src> <loc_dst_dir>\n", name);
  printf("\n\n");
  exit(1);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void main_usock(char *unix_sock_path,
                       char *cmd, char *src, char *dst)
{
  int sock_fd;
  sock_fd = connect_usock(unix_sock_path);
  if (sock_fd < 0)
    KOUT(" %s: %s\n", unix_sock_path, strerror(errno));
  if (cmd && (strlen(cmd) >= MAX_MSG_LEN))
    KOUT("%d %s", strlen(cmd), cmd);
  loop_cli(sock_fd, cmd, src, dst);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void main_vsock(char *cid, char *port,
                       char *cmd, char *src, char *dst)
{
  int sock_fd, vsock_cid, vsock_port;
  vsock_cid = mdl_parse_val(cid);
  vsock_port = mdl_parse_val(port);
  sock_fd = connect_vsock(vsock_cid, vsock_port);
  if (cmd && (strlen(cmd) >= MAX_MSG_LEN))
    KOUT("%d %s", strlen(cmd), cmd);
  loop_cli(sock_fd, cmd, src, dst);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static void main_isock(char *ip, char *port,
                       char *cmd, char *src, char *dst)
{
  int sock_fd, ip_num, port_num;
  if (ip_string_to_int(&ip_num, ip))
    KOUT("Bad ip: %s", ip);
  port_num = mdl_parse_val(port);
  sock_fd = connect_isock(ip, ip_num, port_num);
  if (cmd && (strlen(cmd) >= MAX_MSG_LEN))
    KOUT("%d %s", strlen(cmd), cmd);
  loop_cli(sock_fd, cmd, src, dst);
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
static int get_params(int ac, char **av, char **cmd, char **src, char **dst)
{
  int result = -1;
  *cmd = NULL; *src = NULL; *dst = NULL;
  if (ac == 0)
    result = 0;
  if ((ac == 2) && !strcmp(av[0], "-cmd"))
    {
    *cmd = av[1];
    result = 0;
    }
  else if ((ac == 3) && !strcmp(av[0], "-snd"))
    {
    g_is_snd = 1;
    *src = av[1];
    *dst = av[2];
    result = 0;
    }
  else if ((ac == 3) && !strcmp(av[0], "-rcv"))
    {
    g_is_rcv = 1;
    *src = av[1];
    *dst = av[2];
    result = 0;
    }
  return result;
}
/*--------------------------------------------------------------------------*/

/****************************************************************************/
int main(int argc, char **argv)
{
  char unix_sock_path[MAX_PATH_LEN];
  char *cmd, *src, *dst;
  g_is_snd = 0;
  g_is_rcv = 0;
  g_x11_ok = 0;
  if (argc < 3)
    usage(argv[0]);
  if (!strcmp(argv[1], "-u"))
    {
    if (get_params(argc-3, &(argv[3]), &cmd, &src, &dst))
      usage(argv[0]);
    main_usock(argv[2], cmd, src, dst);
    }
  else if (!strcmp(argv[1], "-i"))
    {
    if (get_params(argc-4, &(argv[4]), &cmd, &src, &dst))
      usage(argv[0]);
    main_isock(argv[2], argv[3], cmd, src, dst);
    }
  else
    {
    if (get_params(argc-3, &(argv[3]), &cmd, &src, &dst))
      usage(argv[0]);
    main_vsock(argv[1], argv[2], cmd, src, dst);
    }
  return 0;
}
/*--------------------------------------------------------------------------*/
