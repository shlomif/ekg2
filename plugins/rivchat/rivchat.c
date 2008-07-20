
/*
 *  (C) Copyright 2007-2008 Jakub Zawadzki <darkjames@darkjames.ath.cx>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License Version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <ekg/debug.h>
#include <ekg/plugins.h>
#include <ekg/protocol.h>
#include <ekg/stuff.h>
#include <ekg/themes.h>
#include <ekg/queries.h>
#include <ekg/vars.h>
#include <ekg/windows.h>
#include <ekg/xmalloc.h>

#define DEFQUITMSG "EKG2 - It's better than sex!"
#define SGQUITMSG(x) session_get(x, "QUIT_MSG")
#define QUITMSG(x) (SGQUITMSG(x)?SGQUITMSG(x):DEFQUITMSG)

#include "rivchat.h"	/* only protocol-stuff */

typedef struct {
	int fd;
	int port;
	char *nick;
	char *topic;

	uint32_t ourid;
	uint8_t seq_nr;
	uint32_t uptime;
} rivchat_private_t;

/* XXX:
 *  - pozwolic podac adres do nasluchu i broadcast-do-wysylania 
 *  - uzywac protocol_message_emit() zeby moc logowac... 
 *  - powiadomic usera, ze po /connect moze sie nic ciekawego nie wydarzyc. */

static int rivchat_theme_init();

PLUGIN_DEFINE(rivchat, PLUGIN_PROTOCOL, rivchat_theme_init);

static QUERY(rivchat_validate_uid) {
	char *uid = *(va_arg(ap, char **));
	int *valid = va_arg(ap, int *);

	if (!uid)
		return 0;

	if (!xstrncmp(uid, "rivchat:", 8) && uid[8]) {
		(*valid)++;
		return -1;
	}
	return 0;
}

static QUERY(rivchat_session_init) {
	char *session = *(va_arg(ap, char**));

	session_t *s = session_find(session);
	rivchat_private_t *j;

	if (!s || s->priv || s->plugin != &rivchat_plugin)
		return 1;

	j = xmalloc(sizeof(rivchat_private_t));
	j->fd = -1;
	j->port = 0;

	s->priv = j;
	return 0;
}

static QUERY(rivchat_session_deinit) {
	char *session = *(va_arg(ap, char**));

	session_t *s = session_find(session);
	rivchat_private_t *j;

	if (!s || !(j = s->priv) || s->plugin != &rivchat_plugin)
		return 1;

	s->priv = NULL;
	xfree(j->nick);
	xfree(j->topic);
	xfree(j);

	return 0;
}

static QUERY(rivchat_print_version) {
	print("generic", "ekg2 plugin for RivChat protocol http://rivchat.prv.pl/");
	return 0;
}

static QUERY(rivchat_topic_header) {
	char **top   = va_arg(ap, char **);
	char **setby = va_arg(ap, char **);
	char **modes = va_arg(ap, char **);

	session_t *sess = window_current->session;
	char *targ = window_current->target;

	if (targ && sess && sess->plugin == &rivchat_plugin && sess->connected && sess->priv)
	{
		rivchat_private_t *j = sess->priv;

		*top = xstrdup(j->topic);

		*setby = *modes = NULL;
		return 5;
	}
	return -3;
}

static char *rivchat_packet_name(int type) {
	switch (type) {
		case RC_MESSAGE:	return "msg";
		case RC_INIT:		return "init";
		case RC_NICKCHANGE:	return "newnick";
		case RC_QUIT:		return "quit";
		case RC_ME:		return "me";
		//  RC_PING, RC_NICKPROTEST
		case RC_TOPIC:		return "topic";
		case RC_NEWTOPIC:	return "newtopic";
		case RC_AWAY:		return "away";
		case RC_REAWAY:		return "reaway";
		case RC_KICK:		return "kick";
		case RC_POP:		return "pop";
		case RC_REPOP:		return "repop";
		case RC_KICKED:		return "kicked";
		case RC_IGNORE:		return "ignore";
		case RC_NOIGNORE:	return "noignore";
		// RC_REPOPIGNORED
		case RC_ECHOMSG:	return "echomsg";
		case RC_PINGAWAY:	return "pingaway";
		// RC_FILEPROPOSE, RC_FILEREQUEST, RC_FILECANCEL
		/* XXX */
	}
	return NULL;
}

static char *rivchat_make_window(unsigned int port) {
	static char buf[50];

	sprintf(buf, "rivchat:%u", port);
	return buf;
}

static char *rivchat_make_formatname(int type, int is_our, int is_priv) {
	static char buf[36];	// zwiekszac.

	const char *typename; 
	const char *outname;
	const char *outpriv;

	if (!(typename = rivchat_packet_name(type)))
		return NULL;

	outname = (is_our)  ? "send" : "recv";
	outpriv = (is_priv) ? "priv" : "ch";

	sprintf(buf, "rivchat_%s_%s_%s", typename, outname, outpriv);
	if (format_exists(buf))
		return buf;

	sprintf(buf, "rivchat_%s_%s", typename, outname);
	if (format_exists(buf))
		return buf;

	sprintf(buf, "rivchat_%s", typename);
	if (format_exists(buf))
		return buf;

	return NULL;
}

static userlist_t *rivchat_find_user(session_t *s, const char *target) {
	rivchat_private_t *j = s->priv;

	if (!xstrcmp(target, rivchat_make_window(j->port)))
		return NULL;	/* main channel */

	return userlist_find(s, target); 
}

extern char *rivchat_cp_to_locale(char *b);	/* misc.c */
extern char *rivchat_locale_to_cp(char *b);	/* misc.c */

static void memncpy(char *dest, const char *src, size_t len) {
	size_t srclen = xstrlen(src);

/* XXX, maybe: memset(dest, 0, len) */
	if (!src)
		return;

	if (len < srclen)
		debug_error("rivchat, memncpy() truncation of data!!!\n");

	if (len > srclen)
		len = srclen;

	memcpy(dest, src, len);
}

static char *rivchat_generate_data(session_t *s) {
/* XXX, this struct in j->info_hdr? */
	static rivchat_info_t hdr;
	rivchat_private_t *j = s->priv;

	memncpy(hdr.host, session_get(s, "hostname"), sizeof(hdr.host));
	memncpy(hdr.user, session_get(s, "username"), sizeof(hdr.user));	/* TODO getpwuid(getuid()); && configuation */

	memncpy(hdr.host, "fob", sizeof(hdr.host));
	memncpy(hdr.user, "fobbar", sizeof(hdr.user));

	memncpy(hdr.os,   "Linux", sizeof(hdr.os));				/* TODO uname() && configuration */
	memncpy(hdr.prog, "ekg2-rivchat", sizeof(hdr.prog));

	/* ekg-rivchat 0.1 */
	hdr.version[0]	= 0;
	hdr.version[1]	= 1;

	hdr.away = 0;
	hdr.master = 0;
	hdr.slowa = -1;			/* ha, it's 0xFFFFFFFF 4 294 967 295 words! */
	hdr.kod = 0;
	hdr.plec = 0;
	hdr.online = j->uptime;
	hdr.filetransfer = 0; 				/* todo */
	hdr.pisze = 0;

	return (char *) &hdr;
}

/*
 * NOTE: previous version of ekg2-rivchat uses another approach:
 * 	- we send text
 *	- we display it [no matter what happened with it after sendto()]
 *
 * now:
 * 	- we send text
 * 	- if we recv it, we check fromid of packet, if it's ok. than ok :) if not, than sorry...
 *
 * It's like orginal rivchat client do.
 */

static int rivchat_send_packet(session_t *s, uint32_t type, userlist_t *user, const char *buf, size_t buflen) {
	rivchat_private_t *j;

	struct sockaddr_in sin;
	rivchat_header_t hdr;
	int errno2;
	int len;

	if (!s || !(j = s->priv)) {
		errno = EFAULT;
		return -1;
	}

	if (buflen > RC_DATASIZE) {
		debug_error("rivchat_send_packet() truncation of data!!!\n");
		buflen = RC_DATASIZE;
	}

	memset(&hdr, 0, sizeof(hdr));
/* XXX, fix byte-order */
	memcpy(hdr.header, rivchat_magic, sizeof(rivchat_magic));
	hdr.size = RC_SIZE;

	hdr.fromid = j->ourid;
	hdr.toid = (user == NULL) ? RC_BROADCAST : 0x00;	/* XXX */
	hdr.type = type;

	memncpy(hdr.nick, j->nick, sizeof(hdr.nick));

	if (buf && buflen)
		memcpy(hdr.data, buf, buflen);

#if 0
	rivchat_user_private_t *user = NULL;

	if (u) user = u->priv;
#endif
	/* RGB colors */
	hdr.colors[0] = 0;
	hdr.colors[1] = 0;
	hdr.colors[2] = 0;

	hdr.seq = j->seq_nr++;		/* XXX */
	hdr.encrypted = 0;
#if 0
	uint8_t gender;				/* 1 - man, 2 - woman */
	uint8_t bold;				/* ? */
#endif
		/* XXX, user ? user->ip : INADDR_ANY */
		/* XXX INADDR_ANY, broadcast addr */
        sin.sin_family = AF_INET;
        sin.sin_port = htons(j->port);
	sin.sin_addr.s_addr = INADDR_BROADCAST;
	sin.sin_addr.s_addr = inet_addr("10.1.0.255");

	len = sendto(j->fd, &hdr, RC_SIZE, 0, (struct sockaddr *) &sin, sizeof(struct sockaddr_in));
	errno2 = errno;

	debug("sendto(%d, %d, %x) == %d\n", j->fd, type, user, len);

	errno = errno2;
	return len;
}

static int rivchat_send_packet_string(session_t *s, uint32_t type, userlist_t *user, const char *str) {
	int ret;
	char *recodedstring = rivchat_locale_to_cp(xstrdup(str));

	ret = rivchat_send_packet(s, type, user, recodedstring, xstrlen(recodedstring));

	xfree(recodedstring);
	return ret;
}

static void rivchat_parse_packet(session_t *s, rivchat_header_t *_hdr, const char *ip) {
	rivchat_private_t *j = s->priv;

/* XXX, fix byte-order */
/* XXX, protect from spoofing, i've got some ideas... */
	int is_our = (_hdr->fromid == j->ourid);
	int is_priv = (_hdr->toid != RC_BROADCAST);
	int type = (_hdr->type);

	int display_activity = EKG_WINACT_NONE;
	char *display_data = NULL;
	char *nick, *uid;

	if (is_priv && _hdr->toid != j->ourid) {
		/* leave-them-alone */
		return;
	}

	nick = rivchat_cp_to_locale(xstrndup(_hdr->nick, sizeof(_hdr->nick)));
	uid = saprintf("rivchat:%s", nick);

	/* XXX, decrypt if needed */

	switch (_hdr->type) {
		case RC_MESSAGE:
		{
			int to_us;
			
			display_data = rivchat_cp_to_locale(xstrndup(_hdr->data, sizeof(_hdr->data)));

			to_us = !!xstrstr(display_data, j->nick);
			display_activity = (is_priv || to_us) ? EKG_WINACT_IMPORTANT : EKG_WINACT_MSG;
			break;
		}

		case RC_INIT:
		{
			rivchat_info_t *hdr2 = (rivchat_info_t *) _hdr->data;

			char *user = rivchat_cp_to_locale(xstrndup(hdr2->user, sizeof(hdr2->user)));
			char *host = rivchat_cp_to_locale(xstrndup(hdr2->host, sizeof(hdr2->host)));

			if (is_our) {	/* we join? */
				window_t *w = window_new(rivchat_make_window(j->port), s, 0);

				window_switch(w->id);
			}

			display_data = saprintf("%s!%s", user, host);
			display_activity = EKG_WINACT_JUNK;

			xfree(user); xfree(host);
			break;
		}

		case RC_QUIT:
		{
			display_data = rivchat_cp_to_locale(xstrndup(_hdr->data, sizeof(_hdr->data)));
			if (!xstrlen(display_data)) {
				xfree(display_data);
				display_data = xstrdup("no reason");
			}
			display_activity = EKG_WINACT_JUNK;
			break;
		}

		case RC_TOPIC:
		case RC_NEWTOPIC: 
		{
			display_activity = EKG_WINACT_MSG;
			display_data = rivchat_cp_to_locale(xstrndup(_hdr->data, sizeof(_hdr->data)));

			if (type == RC_NEWTOPIC) {
				xfree(j->topic);
				j->topic = saprintf("%s (%s)", display_data, nick);
			} else {
				if (j->topic && xstrcmp(j->topic, display_data)) {	/* old-new topic? */
					/* XXX, change type to NEWTOPIC, when somebody mess with topics? */
					xfree(j->topic);
					j->topic = NULL;
				}
				
				if (!j->topic)
					j->topic = xstrdup(display_data);
			}
			break;
		}

		case RC_NICKCHANGE:
		case RC_IGNORE:
		case RC_NOIGNORE:
		case RC_ME:
		{
			display_activity = EKG_WINACT_MSG;
			display_data = rivchat_cp_to_locale(xstrndup(_hdr->data, sizeof(_hdr->data)));
			break;
		}

		default:
		{
			debug_error("rivchat_parse_packet() recv pkt->type: 0x%.4x\n", type);
			/* XXX, dump it */
			return;
		}
	}

	if (display_activity != EKG_WINACT_NONE) {
		char *fname = rivchat_make_formatname(type, is_our, is_priv);

		print_window(rivchat_make_window(j->port), s, display_activity, 1, fname, s->uid, nick, display_data, ip);

		xfree(display_data);
	}

	xfree(nick);
	xfree(uid);
}

static WATCHER_SESSION(rivchat_handle_stream) {
	rivchat_private_t *j;

        struct sockaddr_in oth;
        int oth_size;
	
	unsigned char buf[400];
	rivchat_header_t *hdr = (rivchat_header_t *) buf;
	int len;

	if (type) {
		/* XXX */
		return 0;
	}

	if (!s || !(j = s->priv))
		return -1;

	oth_size = sizeof(struct sockaddr_in);
	len = recvfrom(fd, buf, sizeof(buf), 0, (struct sockaddr *) &oth, &oth_size);

	if (len < 0) {
		/* XXX */
		return -1;
	}

/* XXX, fix byte-order */
	if (len == RC_SIZE && !memcmp(buf, rivchat_magic, sizeof(rivchat_magic)) && hdr->size == RC_SIZE) {
		rivchat_parse_packet(s, hdr, inet_ntoa(oth.sin_addr));
		return 0;
	}

	debug_error("rivchat_handle_stream() ignoring packet: %d [Bytes: %x %x %x %x ...]\n", len, buf[0], buf[1], buf[2], buf[3]);
	/* XXX, dump while packet? */

	return 0;
}

static TIMER_SESSION(rivchat_pingpong) {
	rivchat_private_t *j;

	if (type) {
		return 0;
	}

	if (!s || !(j = s->priv))
		return -1;

	j->uptime++;
#if 0
	list_t l;
	
	debug("[rivchat pingpong session: %s type: %d s: 0x%x s->priv: 0x%x\n", (char *) data, type, s, j);

	for (l = s->userlist; l;) {
// sprawdzic wszystkich userow last_ping_time i jesli mniejszy niz (now - ping_remove) to usun usera.
		userlist_t *u = l->data;
		rivchat_user_private_t *user = u->priv;

		l = l->next;

		if (!u) {
			debug("[RIVCHAT_PING_TIMEOUT] USER %s removed cause of non private data...\n", u->uid);
			userlist_remove(s, u);
			continue;
		}

		if ((user->ping_packet_time && (user->ping_packet_time + RIVCHAT_PING_TIMEOUT < time(NULL))) ||	/* if we have ping_packet_time */
			(user->packet_time + RIVCHAT_PING_TIMEOUT < time(NULL)))				/* otherwise... */
		{
			print("rivchat_user_timeout", session_name(s), u->uid);
			debug("[RIVCHAT_PING_TIMEOUT] USER %s removed cause of timeout. PING: %d LAST:%d NOW: %d\n", 
				u->uid, user->ping_packet_time, user->packet_time, time(NULL));
			xfree(user);
			u->priv = NULL;
			userlist_remove(s, u);
		}
	}
#endif
	rivchat_send_packet(s, RC_PING, NULL, rivchat_generate_data(s), RC_INFOSIZE);
	return 0;
}

static COMMAND(rivchat_command_connect) {
	rivchat_private_t *j = session->priv;
	struct sockaddr_in sin;
	int one = 1;

	const char *newnick;
	
	int port;
	int fd;
	
	port = session_int_get(session, "port");

	if (port < 0 || port > 65535) {
		/* XXX, notify? */
		port = 16127;
	}

	if (!(newnick = session_get(session, "nickname"))) {
		printq("generic_error", "gdzie lecimy ziom ?! [/session nickname]");
		return -1;
	}

	if ((fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1) {
		protocol_disconnected_emit(session, strerror(errno), EKG_DISCONNECT_FAILURE);
		debug_error("rivchat, socket() failed\n");
		return -1;
	}

	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one))) {
		debug_error("rivchat, setsockopt(SO_REUSEADDR) failed\n");
		/* not-fatal */
	}

	if (setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &one, sizeof(one))) {
		protocol_disconnected_emit(session, strerror(errno), EKG_DISCONNECT_FAILURE);
		debug_error("rivchat, setsockopt(SO_BROADCAST) failed\n");
		close(fd);
		return -1;
	}

	sin.sin_port		= htons(port);
	sin.sin_family		= AF_INET;
	sin.sin_addr.s_addr	= INADDR_ANY;

	if (bind(fd, (struct sockaddr *) &sin, sizeof(struct sockaddr_in))) {
		protocol_disconnected_emit(session, strerror(errno), EKG_DISCONNECT_FAILURE);
		debug_error("rivchat, bind() failed\n");
		close(fd);
		return -1;
	}

	debug("bind success @0.0.0.0:%d\n", port);

	/* XXX, if strlen(j->nick) > 30 notify about possible truncation? */

	xfree(j->nick);
	j->nick = xstrdup(newnick);

	j->fd = fd;
	j->port = port;
	j->seq_nr = 0;		/* XXX? */
	j->uptime = 0;		/* XXX? */
	j->ourid = rand();	/* XXX? */

	session_status_set(session, EKG_STATUS_AVAIL);
	protocol_connected_emit(session);

	watch_add_session(session, fd, WATCH_READ, rivchat_handle_stream);
	timer_add_session(session, "rc_pingpong", 10, 1, rivchat_pingpong);

	rivchat_send_packet(session, RC_INIT, NULL, rivchat_generate_data(session), RC_INFOSIZE);
	return 0;
}

static COMMAND(rivchat_command_disconnect) {
	rivchat_private_t *j = session->priv;
	const char *reason;
	
	if (timer_remove_session(session, "reconnect") == 0) {
		printq("auto_reconnect_removed", session_name(session));
		return 0;
	}

	if (!session_connected_get(session)) {
		printq("not_connected", session_name(session));
		return -1;
	}

	reason = params[0]?params[0]:QUITMSG(session);

	/* send quit pkt, XXX rivchat doesn't support reasons.. */
	rivchat_send_packet_string(session, RC_QUIT, NULL, reason);

	watch_remove(&rivchat_plugin, j->fd, WATCH_READ);
	close(j->fd);
	j->fd = -1;

	xfree(j->topic);
	j->topic = NULL;

	protocol_disconnected_emit(session, reason, EKG_DISCONNECT_USER);
	return 0;
}

static COMMAND(rivchat_command_reconnect) {
	if (session->connected)
		rivchat_command_disconnect(name, params, session, target, quiet);

	return rivchat_command_connect(name, params, session, target, quiet);
}

static COMMAND(rivchat_command_inline_msg) {
	if (params[0])
		return rivchat_send_packet_string(session, RC_MESSAGE, rivchat_find_user(session, target), params[0]);

	return -1;
}

static COMMAND(rivchat_command_me) {
	return rivchat_send_packet_string(session, RC_ME, rivchat_find_user(session, target), params[0]);
}

static COMMAND(rivchat_command_nick) {
	rivchat_private_t *j = session->priv;

	xfree(j->nick);
	j->nick	= xstrdup(params[0]);

	return rivchat_send_packet_string(session, RC_NICKCHANGE, NULL, j->nick);
}

static COMMAND(rivchat_command_topic) {
	rivchat_private_t *j = session->priv;

	if (!params[0]) { /* display current topic */
		/* XXX, setby ? */	

		printq("rivchat_topic", rivchat_make_window(j->port), "", j->topic, "0.0.0.0");
		return 0;
	}

	return rivchat_send_packet_string(session, RC_NEWTOPIC, NULL, params[0]);
}

static void rivchat_changed_nick(session_t *s, const char *var) {
	rivchat_private_t *j;
	const char *newnick;

	if (!s || !(j = s->priv))
		return;

	if (!s->connected)
		return;

	if (!(newnick = session_get(s, "nickname")))
		return;		/* ignore */

	if (xstrcmp(newnick, j->nick)) {	/* if nick-really-changed */
		xfree(j->nick);
		j->nick = xstrdup(newnick);
		rivchat_send_packet_string(s, RC_NICKCHANGE, NULL, newnick);
	}
}

static void rivchat_notify_reconnect(session_t *s, const char *var) {
	if (s && s->connected)
		print("config_must_reconnect");
}

static int rivchat_theme_init() {
#ifndef NO_DEFAULT_THEME
/* format of formats ;] 
 *
 * rivchat_pkttype_type_priv 
 *      pkttype  - msg, quit, me, init, ...
 *      type     - recv, send
 *      priv     - ch, priv
 *
 * jak nie znajdzie pelnej formatki to wtedy szuka:
 *   rivchat_pkttype_type
 *
 * a potem:
 *   rivchat_pkttype
 *
 * params: 
 *	%1 - sesja %2 - nick %3 - data. %4 - ip 
 */

	/* te zrobic bardziej irssi-like */
	format_add("rivchat_msg_send_ch",	"%B<%W%2%B>%n %3", 1);
	format_add("rivchat_msg_send_priv",	"%B<%R%2%B>%n %3", 1);
	format_add("rivchat_msg_recv",		"<%2> %3", 1);
	
	/* ok */
	format_add("rivchat_init", 		"%> %C%2%n %B[%c%3@%4%B]%n has joined", 1);
	format_add("rivchat_quit", 		"%> %c%2%n %B[%c%2@%4%B]%n has quit %B[%n%3%B]", 1);

	format_add("rivchat_me",		"%W%e* %2%n %3", 1);

	format_add("rivchat_newnick_send", 	"%> You're now known as %W%3", 1);
	format_add("rivchat_newnick_recv", 	"%> %c%2%n is now known as %C%3", 1);

	format_add("rivchat_newtopic",		"%> %T%2%n changed topic to: %3", 1);
	format_add("rivchat_topic",		"%> Topic: %3", 1);

	/* dziwne */
	format_add("rivchat_ignore_send",	"%) You starts ignoring %3", 1);
	format_add("rivchat_ignore_recv",	"%) %W%2%n starts ignoring %3", 1);

	format_add("rivchat_noignore_send",	"%) You stops ignoring %3", 1);
	format_add("rivchat_noignore_recv",	"%) %W%2%n stops ignoring %3", 1);
#endif
	return 0;
}

static plugins_params_t rivchat_plugin_vars[] = {
	PLUGIN_VAR_ADD("alias", 		VAR_STR, NULL, 0, NULL), 
	PLUGIN_VAR_ADD("auto_connect", 		VAR_BOOL, "0", 0, NULL),
	PLUGIN_VAR_ADD("auto_reconnect", 	VAR_INT, "0", 0, NULL),
	PLUGIN_VAR_ADD("nickname",		VAR_STR, NULL, 0, rivchat_changed_nick),
	PLUGIN_VAR_ADD("port",			VAR_STR, "16127", 0, rivchat_notify_reconnect),
	PLUGIN_VAR_ADD("log_formats", 		VAR_STR, "irssi", 0, NULL),
	PLUGIN_VAR_END()
};

EXPORT int rivchat_plugin_init(int prio) {
	if (sizeof(rivchat_header_t) != RC_SIZE) {
		print("generic_error", "Your arch is currently not supported by rivchat plugin, sorry :(");
		debug_error("rivchat: sizeof(rivchat_header_t) == %x excepted: %x\n", sizeof(rivchat_header_t), RC_SIZE);
		return -1;
	}

	if (sizeof(rivchat_info_t) != RC_INFOSIZE) {
		print("generic_error", "Your arch is currently not supported by rivchat plugin, sorry :(");
		debug_error("rivchat: sizeof(rivchat_info_t) == %x excepted: %x\n", sizeof(rivchat_info_t), RC_DATASIZE);
		return -1;
	}

	/* we assume you're using LE processor :> */
	/* XXX, test BE/LE or use <endian.h> */

	PLUGIN_CHECK_VER("rivchat");

	rivchat_plugin.params = rivchat_plugin_vars;

	plugin_register(&rivchat_plugin, prio);

	query_connect_id(&rivchat_plugin, PROTOCOL_VALIDATE_UID, rivchat_validate_uid, NULL);
	query_connect_id(&rivchat_plugin, SESSION_ADDED, rivchat_session_init, NULL);
	query_connect_id(&rivchat_plugin, SESSION_REMOVED, rivchat_session_deinit, NULL);
	query_connect_id(&rivchat_plugin, PLUGIN_PRINT_VERSION, rivchat_print_version, NULL);

	query_connect_id(&rivchat_plugin, IRC_TOPIC, rivchat_topic_header, NULL);

#if 0
	query_connect(&irc_plugin, ("ui-window-kill"),	irc_window_kill, NULL);
	query_connect(&irc_plugin, ("status-show"),	irc_status_show_handle, NULL);
#endif

#define RIVCHAT_ONLY 		SESSION_MUSTBELONG | SESSION_MUSTHASPRIVATE
#define RIVCHAT_FLAGS		RIVCHAT_ONLY | SESSION_MUSTBECONNECTED

	command_add(&rivchat_plugin, "rivchat:", "?",		rivchat_command_inline_msg, RIVCHAT_ONLY, NULL);

	command_add(&rivchat_plugin, "rivchat:connect", NULL,   rivchat_command_connect,    RIVCHAT_ONLY, NULL);
	command_add(&rivchat_plugin, "rivchat:disconnect", "r",	rivchat_command_disconnect, RIVCHAT_ONLY, NULL);
	command_add(&rivchat_plugin, "rivchat:me", "?",		rivchat_command_me,         RIVCHAT_FLAGS, NULL);
	command_add(&rivchat_plugin, "rivchat:nick", "!",	rivchat_command_nick,       RIVCHAT_FLAGS | COMMAND_ENABLEREQPARAMS, NULL);
	command_add(&rivchat_plugin, "rivchat:topic", "?",	rivchat_command_topic,      RIVCHAT_FLAGS, NULL);
	command_add(&rivchat_plugin, "rivchat:reconnect", "r",	rivchat_command_reconnect,  RIVCHAT_ONLY, NULL);
	return 0;
}

static int rivchat_plugin_destroy() {
	plugin_unregister(&rivchat_plugin);
	return 0;
}
