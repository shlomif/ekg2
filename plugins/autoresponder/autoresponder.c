
/* Simple autoresponder
 * (C) 2006 Michał Górny <peres@peres.int.pl>
 */

#include "ekg2.h"

#include <sys/types.h>

static list_t list_find_str(const list_t first, const char *needle);
static QUERY(autoresponder_message);
static void autoresponder_varchange(const char *varname);
int autoresponder_plugin_init(int prio);
static int autoresponder_plugin_destroy(void);

static char *config_autoresponder_question = NULL;
static char *config_autoresponder_answer = NULL;
static char *config_autoresponder_greeting = NULL;
static char *config_autoresponder_allowed_sessions = NULL;
static int config_autoresponder_match_mode = 1;
static GRegex *autoresponder_answer_regex = NULL;

static list_t autoresponder_allowed_uids;

PLUGIN_DEFINE(autoresponder, PLUGIN_GENERIC, NULL);

static list_t list_find_str(const list_t first, const char *needle)
{
	list_t curr;
	
	for (curr = first; curr && xstrcmp(curr->data, needle); curr = curr->next);

	return curr;
}

static QUERY(autoresponder_message)
{
	char *session	= *(va_arg(ap, char**));
	char *uid	= *(va_arg(ap, char**));
		char **UNUSED(rcpts)	= *(va_arg(ap, char***));
	char *text	= *(va_arg(ap, char**));
		guint32 *UNUSED(format)= *(va_arg(ap, guint32**));
		time_t UNUSED(sent)	= *(va_arg(ap, time_t*));
	int class	= *(va_arg(ap, int*));
		char *UNUSED(seq)	= *(va_arg(ap, char**));
		int UNUSED(dobeep)	= *(va_arg(ap, int*));
		int UNUSED(secure)	= *(va_arg(ap, int*));
	
	session_t *s;
	/* if user didn't give us any answer to his question, we assume that sender should rewrite the question string */
	const char *an = (config_autoresponder_answer && (*config_autoresponder_answer)
			? config_autoresponder_answer : config_autoresponder_question);
	int matchoccured;
	
	if ((class >= EKG_MSGCLASS_SENT) /* at first, skip our messages */
			|| !config_autoresponder_question || !(*config_autoresponder_question) /* are we configured? */
			|| !(s = session_find(session)) /* session really exists? */
			|| !(cssfind(config_autoresponder_allowed_sessions, session, ',', 0) /* check, if session allows autoresponding */
				|| cssfind(config_autoresponder_allowed_sessions, session_alias_get(s), ',', 0)) /* and by alias */
			|| (userlist_find(s, uid)) /* check if it isn't currently on our roster */
			|| (window_find_s(s, uid)) /* or maybe we've already opened query? */
			|| (list_find_str(autoresponder_allowed_uids, uid))) /* search the allowed uids list */
			/* TODO(porridge): || (gg session && uid == "gg:0") - he won't respond, and it may cause connection drops */
		return 0;
	
	switch (config_autoresponder_match_mode) {
		case 0: /* exact match */
			matchoccured = !xstrcmp(text, an);
			break;
		case 2: /* POSIX regex match */
			matchoccured = g_regex_match(autoresponder_answer_regex, text, 0, NULL);
			break;
		case 1: /* substring match */
		default:
			matchoccured = !!(xstrstr(text, an));
	}

	if (matchoccured) {
		/* we've got the answer, hail the user! */
		list_add(&autoresponder_allowed_uids, xstrdup(uid));
		if (config_autoresponder_greeting && (*config_autoresponder_greeting))
			command_exec_format(NULL, s, 1, "/msg %s %s", uid, config_autoresponder_greeting);
	} else {
		command_exec_format(NULL, s, 1, "/msg %s %s", uid, config_autoresponder_question);
	}

	return -1; /* ignore this message */
}

static void autoresponder_varchange(const char *varname)
{
	if (autoresponder_answer_regex) {
		g_regex_unref(autoresponder_answer_regex);
		autoresponder_answer_regex = NULL;
	}

	if (config_autoresponder_match_mode == 2 && config_autoresponder_answer && (*config_autoresponder_answer)) {
		GError *err = NULL;
		if (!((autoresponder_answer_regex = g_regex_new(config_autoresponder_answer,
					G_REGEX_RAW | G_REGEX_NO_AUTO_CAPTURE, 0, &err)))) {
			
			print("regex_error", err->message);
			g_error_free(err);
			config_autoresponder_match_mode = 1;
		}
	}
}

EXPORT int autoresponder_plugin_init(int prio)
{
	PLUGIN_CHECK_VER("autoresponder");

	plugin_register(&autoresponder_plugin, prio);
	
	query_connect(&autoresponder_plugin, "protocol-message", autoresponder_message, NULL);
	variable_add(&autoresponder_plugin, "allowed_sessions", VAR_STR, 1, &config_autoresponder_allowed_sessions, NULL, NULL, NULL);
	variable_add(&autoresponder_plugin, "answer", VAR_STR, 1, &config_autoresponder_answer, autoresponder_varchange, NULL, NULL);
	variable_add(&autoresponder_plugin, "greeting", VAR_STR, 1, &config_autoresponder_greeting, NULL, NULL, NULL);
	variable_add(&autoresponder_plugin, "match_mode", VAR_INT, 1, &config_autoresponder_match_mode, autoresponder_varchange,
			variable_map(3, 0, 0, "exact", 1, 2, "substring", 2, 1, "regex"), NULL);
	variable_add(&autoresponder_plugin, "question", VAR_STR, 1, &config_autoresponder_question, NULL, NULL, NULL);
	
	return 0;
}

static int autoresponder_plugin_destroy(void)
{
	list_destroy(autoresponder_allowed_uids, 1);
	if (autoresponder_answer_regex)
		g_regex_unref(autoresponder_answer_regex);
	plugin_unregister(&autoresponder_plugin);
	
	return 0;
}
