/*
 * Copyright 2012 - 2013 Michael Drake <tlsa@netsurf-browser.org>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */


#include <stdlib.h>

#include "content/urldb.h"
#include "desktop/browser.h"
#include "desktop/global_history.h"
#include "desktop/treeview.h"
#include "utils/messages.h"
#include "utils/utils.h"
#include "utils/log.h"

#define N_FIELDS 5
#define N_DAYS 28
#define N_SEC_PER_DAY (60 * 60 * 24)

enum global_history_folders {
	GH_TODAY = 0,
	GH_YESTERDAY,
	GH_2_DAYS_AGO,
	GH_3_DAYS_AGO,
	GH_4_DAYS_AGO,
	GH_5_DAYS_AGO,
	GH_6_DAYS_AGO,
	GH_LAST_WEEK,
	GH_2_WEEKS_AGO,
	GH_3_WEEKS_AGO,
	GH_N_FOLDERS
};

struct global_history_folder {
	treeview_node *folder;
	struct treeview_field_data data;
};

struct global_history_ctx {
	treeview *tree;
	struct treeview_field_desc fields[N_FIELDS];
	struct global_history_folder folders[GH_N_FOLDERS];
	time_t today;
	int weekday;
	bool built;
};
struct global_history_ctx gh_ctx;

struct global_history_entry {
	int slot;
	nsurl *url;
	time_t t;
	treeview_node *entry;
	struct global_history_entry *next;
	struct global_history_entry *prev;

	struct treeview_field_data data[N_FIELDS - 1];
};
struct global_history_entry *gh_list[N_DAYS];


/**
 * Find an entry in the global history
 *
 * \param url The URL to find
 * \return Pointer to history entry, or NULL if not found
 */
static struct global_history_entry *global_history_find(nsurl *url)
{
	int i;
	struct global_history_entry *e;

	for (i = 0; i < N_DAYS; i++) {
		e = gh_list[i];

		while (e != NULL) {
			if (nsurl_compare(e->url, url,
					NSURL_COMPLETE) == true) {
				/* Got a match */
				return e;
			}
			e = e->next;
		}

	}

	/* No match found */
	return NULL;
}


/**
 * Initialise the treeview directories
 *
 * \param f		Ident for folder to create
 * \return NSERROR_OK on success, appropriate error otherwise
 */
static nserror global_history_create_dir(enum global_history_folders f)
{
	nserror err;
	time_t t = gh_ctx.today;
	treeview_node *relation = NULL;
	enum treeview_relationship rel = TREE_REL_FIRST_CHILD;
	const char *label;
	int age;
	int i;

	switch (f) {
	case GH_TODAY:
		label = "DateToday";
		age = 0;
		break;
	case GH_YESTERDAY:
		label = "DateYesterday";
		age = 1;
		break;
	case GH_2_DAYS_AGO:
		label = "Date2Days";
		age = 2;
		break;
	case GH_3_DAYS_AGO:
		label = "Date3Days";
		age = 3;
		break;
	case GH_4_DAYS_AGO:
		label = "Date4Days";
		age = 4;
		break;
	case GH_5_DAYS_AGO:
		label = "Date5Days";
		age = 5;
		break;
	case GH_6_DAYS_AGO:
		label = "Date6Days";
		age = 6;
		break;
	case GH_LAST_WEEK:
		label = "Date1Week";
		age = 7;
		break;
	case GH_2_WEEKS_AGO:
		label = "Date2Week";
		age = 14;
		break;
	case GH_3_WEEKS_AGO:
		label = "Date3Week";
		age = 21;
		break;
	default:
		assert(0);
		break;
	}

	t -= age * N_SEC_PER_DAY;

	label = messages_get(label);

	for (i = f - 1; i >= 0; i--) {
		if (gh_ctx.folders[i].folder != NULL) {
			relation = gh_ctx.folders[i].folder;
			rel = TREE_REL_NEXT_SIBLING;
			break;
		}
	}

	gh_ctx.folders[f].data.field = gh_ctx.fields[N_FIELDS - 1].field;
	gh_ctx.folders[f].data.value = label;
	gh_ctx.folders[f].data.value_len = strlen(label);
	err = treeview_create_node_folder(gh_ctx.tree,
			&gh_ctx.folders[f].folder,
			relation, rel,
			&gh_ctx.folders[f].data,
			&gh_ctx.folders[f],
			gh_ctx.built ? TREE_CREATE_NONE :
					TREE_CREATE_SUPPRESS_RESIZE);

	return err;
}


/**
 * Get the treeview folder for history entires in a particular slot
 *
 * \param parent	Updated to parent folder.
 * \param slot		Global history slot of entry we want folder node for
 * \return NSERROR_OK on success, appropriate error otherwise
 */
static inline nserror global_history_get_parent_treeview_node(
		treeview_node **parent, int slot)
{
	int folder_index;
	struct global_history_folder *f;
	nserror err;

	if (slot < 7) {
		folder_index = slot;

	} else if (slot < 14) {
		folder_index = GH_LAST_WEEK;

	} else if (slot < 21) {
		folder_index = GH_2_WEEKS_AGO;

	} else if (slot < N_DAYS) {
		folder_index = GH_3_WEEKS_AGO;

	} else {
		/* Slot value is invalid */
		return NSERROR_BAD_PARAMETER;
	}

	/* Get the folder */
	f = &(gh_ctx.folders[folder_index]);

	if (f->folder == NULL) {
		err = global_history_create_dir(folder_index);
		if (err != NSERROR_OK) {
			*parent = NULL;
			return err;
		}
	}

	/* Return the parent treeview folder */
	*parent = f->folder;
	return NSERROR_OK;
}


/**
 * Set a global history entry's data from the url_data.
 *
 * \param e		Global history entry to set up
 * \param url_data	Data associated with entry's URL
 * \return NSERROR_OK on success, appropriate error otherwise
 */
static nserror global_history_create_treeview_field_data(
		struct global_history_entry *e,
		const struct url_data *data)
{
	const char *title = (data->title != NULL) ? data->title : "<No title>";
	char buffer[16];
	const char *last_visited;
	char *last_visited2;
	int len;

	e->data[0].field = gh_ctx.fields[0].field;
	e->data[0].value = strdup(title);
	e->data[0].value_len = (e->data[0].value != NULL) ? strlen(title) : 0;

	e->data[1].field = gh_ctx.fields[1].field;
	e->data[1].value = nsurl_access(e->url);
	e->data[1].value_len = nsurl_length(e->url);

	last_visited = ctime(&data->last_visit);
	last_visited2 = strdup(last_visited);
	if (last_visited2 != NULL) {
		assert(last_visited2[24] == '\n');
		last_visited2[24] = '\0';
	}

	e->data[2].field = gh_ctx.fields[2].field;
	e->data[2].value = last_visited2;
	e->data[2].value_len = (last_visited2 != NULL) ? 24 : 0;

	len = snprintf(buffer, 16, "%u", data->visits);
	if (len == 16) {
		len--;
		buffer[len] = '\0';
	}

	e->data[3].field = gh_ctx.fields[3].field;
	e->data[3].value = strdup(buffer);
	e->data[3].value_len = len;

	return NSERROR_OK;
}

/**
 * Add a global history entry to the treeview
 *
 * \param e	entry to add to treeview
 * \param slot  global history slot containing entry
 * \return NSERROR_OK on success, or appropriate error otherwise
 *
 * It is assumed that the entry is unique (for its URL) in the global
 * history table
 */
static nserror global_history_entry_insert(struct global_history_entry *e,
		int slot)
{
	nserror err;

	treeview_node *parent;
	err = global_history_get_parent_treeview_node(&parent, slot);
	if (err != NSERROR_OK) {
		return err;
	}

	err = treeview_create_node_entry(gh_ctx.tree, &(e->entry),
			parent, TREE_REL_FIRST_CHILD, e->data, e,
			gh_ctx.built ? TREE_CREATE_NONE :
					TREE_CREATE_SUPPRESS_RESIZE);
	if (err != NSERROR_OK) {
		return err;
	}

	return NSERROR_OK;
}


/**
 * Add an entry to the global history (creates the entry).
 *
 * If the treeview has already been created, the entry will be added to the
 * treeview.  Otherwise, the entry will have to be added to the treeview later.
 *
 * When we first create the global history we create it without the treeview, to
 * simplfy sorting the entries.
 *
 * \param url		URL for entry to add to history
 * \param slot		Global history slot to contain history entry
 * \param data		URL data for the entry
 * \param got_treeview	Whether the treeview has been created already
 * \return NSERROR_OK on success, or appropriate error otherwise
 */
static nserror global_history_add_entry_internal(nsurl *url, int slot,
		const struct url_data *data, bool got_treeview)
{
	nserror err;
	struct global_history_entry *e;

	/* Create new local history entry */
	e = malloc(sizeof(struct global_history_entry));
	if (e == NULL) {
		return false;
	}

	e->slot = slot;
	e->url = nsurl_ref(url);
	e->t = data->last_visit;
	e->entry = NULL;
	e->next = NULL;
	e->prev = NULL;

	err = global_history_create_treeview_field_data(e, data);
	if (err != NSERROR_OK) {
		return err;
	}
	
	if (gh_list[slot] == NULL) {
		/* list empty */
		gh_list[slot] = e;

	} else if (gh_list[slot]->t < e->t) {
		/* Insert at list head */
		e->next = gh_list[slot];
		gh_list[slot]->prev = e;
		gh_list[slot] = e;
	} else {
		struct global_history_entry *prev = gh_list[slot];
		struct global_history_entry *curr = prev->next;
		while (curr != NULL) {
			if (curr->t < e->t) {
				break;
			}
			prev = curr;
			curr = curr->next;
		}

		/* insert after prev */
		e->next = curr;
		e->prev = prev;
		prev->next = e;

		if (curr != NULL)
			curr->prev = e;
	}

	if (got_treeview) {
		err = global_history_entry_insert(e, slot);
		if (err != NSERROR_OK) {
			return err;
		}
	}

	return NSERROR_OK;
}


/**
 * Delete a global history entry
 *
 * This does not delete the treeview node, rather it should only be called from
 * the treeview node delete event message.
 *
 * \param e		Entry to delete
 */
static void global_history_delete_entry_internal(
		struct global_history_entry *e)
{
	assert(e != NULL);
	assert(e->entry == NULL);

	/* Unlink */
	if (gh_list[e->slot] == e) {
		/* e is first entry */
		gh_list[e->slot] = e->next;

		if (e->next != NULL)
			e->next->prev = NULL;

	} else if (e->next == NULL) {
		/* e is last entry */
		e->prev->next = NULL;

	} else {
		/* e has an entry before and after */
		e->prev->next = e->next;
		e->next->prev = e->prev;
	}

	/* Destroy */
	free((void *)e->data[0].value); /* Eww */
	free((void *)e->data[2].value); /* Eww */
	free((void *)e->data[3].value); /* Eww */
	nsurl_unref(e->url);
	free(e);
}

/**
 * Internal routine to actually perform global history addition
 *
 * \param url The URL to add
 * \param data URL data associated with URL
 * \return true (for urldb_iterate_entries)
 */
static bool global_history_add_entry(nsurl *url,
		const struct url_data *data)
{
	int slot;
	struct global_history_entry *e;
	time_t visit_date;
	time_t earliest_date = gh_ctx.today - (N_DAYS - 1) * N_SEC_PER_DAY;
	bool got_treeview = gh_ctx.tree != NULL;

	assert((url != NULL) && (data != NULL));

	visit_date = data->last_visit;

	/* Find day array slot for entry */
	if (visit_date >= gh_ctx.today) {
		slot = 0;
	} else if (visit_date >= earliest_date) {
		slot = (gh_ctx.today - visit_date) / N_SEC_PER_DAY + 1;
	} else {
		/* too old */
		return true;
	}

	if (got_treeview == true) {
		/* The treeview for global history already exists */

		/* See if there's already an entry for this URL */
		e = global_history_find(url);
		if (e != NULL) {
			/* Existing entry.  Delete it. */
			treeview_delete_node(gh_ctx.tree, e->entry);
			return true;
		}
	}

	if (global_history_add_entry_internal(url, slot, data,
			got_treeview) != NSERROR_OK) {
		return false;
	}

	return true;
}

/**
 * Initialise the treeview entry feilds
 *
 * \return true on success, false on memory exhaustion
 */
static nserror global_history_initialise_entry_fields(void)
{
	int i;
	const char *label;

	for (i = 0; i < N_FIELDS; i++)
		gh_ctx.fields[i].field = NULL;

	gh_ctx.fields[0].flags = TREE_FLAG_DEFAULT;
	label = "TreeviewLabelTitle";
	label = messages_get(label);
	if (lwc_intern_string(label, strlen(label),
			&gh_ctx.fields[0].field) !=
			lwc_error_ok) {
		goto error;
	}

	gh_ctx.fields[1].flags = TREE_FLAG_NONE;
	label = "TreeviewLabelURL";
	label = messages_get(label);
	if (lwc_intern_string(label, strlen(label),
			&gh_ctx.fields[1].field) !=
			lwc_error_ok) {
		goto error;
	}

	gh_ctx.fields[2].flags = TREE_FLAG_SHOW_NAME;
	label = "TreeviewLabelLastVisit";
	label = messages_get(label);
	if (lwc_intern_string(label, strlen(label),
			&gh_ctx.fields[2].field) !=
			lwc_error_ok) {
		goto error;
	}

	gh_ctx.fields[3].flags = TREE_FLAG_SHOW_NAME;
	label = "TreeviewLabelVisits";
	label = messages_get(label);
	if (lwc_intern_string(label, strlen(label),
			&gh_ctx.fields[3].field) !=
			lwc_error_ok) {
		goto error;
	}

	gh_ctx.fields[4].flags = TREE_FLAG_DEFAULT;
	label = "TreeviewLabelPeriod";
	label = messages_get(label);
	if (lwc_intern_string(label, strlen(label),
			&gh_ctx.fields[4].field) !=
			lwc_error_ok) {
		return false;
	}

	return NSERROR_OK;

error:
	for (i = 0; i < N_FIELDS; i++)
		if (gh_ctx.fields[i].field != NULL)
			lwc_string_unref(gh_ctx.fields[i].field);

	return NSERROR_UNKNOWN;
}


/**
 * Initialise the time
 *
 * \return true on success, false on memory exhaustion
 */
static nserror global_history_initialise_time(void)
{
	struct tm *full_time;
	time_t t;

	/* get the current time */
	t = time(NULL);
	if (t == -1) {
		LOG(("time info unaviable"));
		return NSERROR_UNKNOWN;
	}

	/* get the time at the start of today */
	full_time = localtime(&t);
	full_time->tm_sec = 0;
	full_time->tm_min = 0;
	full_time->tm_hour = 0;
	t = mktime(full_time);
	if (t == -1) {
		LOG(("mktime failed"));
		return NSERROR_UNKNOWN;
	}

	gh_ctx.today = t;
	gh_ctx.weekday = full_time->tm_wday;

	return NSERROR_OK;
}


/**
 * Initialise the treeview entries
 *
 * \return true on success, false on memory exhaustion
 */
static nserror global_history_init_entries(void)
{
	int i;
	nserror err;

	/* Itterate over all global history data, inserting it into treeview */
	for (i = 0; i < N_DAYS; i++) {
		struct global_history_entry *l = NULL;
		struct global_history_entry *e = gh_list[i];

		/* Insert in reverse order; find last */
		while (e != NULL) {
			l = e;
			e = e->next;
		}

		/* Insert the entries into the treeview */
		while (l != NULL) {
			err = global_history_entry_insert(l, i);
			if (err != NSERROR_OK) {
				return err;
			}
			l = l->prev;
		}
	}

	return NSERROR_OK;
}


static nserror global_history_tree_node_folder_cb(
		struct treeview_node_msg msg, void *data)
{
	return NSERROR_OK;
}
static nserror global_history_tree_node_entry_cb(
		struct treeview_node_msg msg, void *data)
{
	struct global_history_entry *e = (struct global_history_entry *)data;

	switch (msg.msg) {
	case TREE_MSG_NODE_DELETE:
		e->entry = NULL;
		global_history_delete_entry_internal(e);
		break;

	case TREE_MSG_NODE_EDIT:
		break;

	case TREE_MSG_NODE_LAUNCH:
	{
		nserror error;
		struct browser_window *clone = NULL;
		enum browser_window_nav_flags flags =
				BROWSER_WINDOW_VERIFIABLE |
				BROWSER_WINDOW_HISTORY |
				BROWSER_WINDOW_TAB;

		/* TODO: Set clone window, to window that new tab appears in */

		if (msg.data.node_launch.mouse &
				(BROWSER_MOUSE_MOD_1 | BROWSER_MOUSE_MOD_2) ||
				clone == NULL) {
			/* Shift or Ctrl launch, open in new window rather
			 * than tab. */
			flags ^= BROWSER_WINDOW_TAB;
		}

		error = browser_window_create(flags, e->url, NULL, clone, NULL);
		if (error != NSERROR_OK) {
			warn_user(messages_get_errorcode(error), 0);
		}
	}
		break;
	}
	return NSERROR_OK;
}
struct treeview_callback_table tree_cb_t = {
	.folder = global_history_tree_node_folder_cb,
	.entry = global_history_tree_node_entry_cb
};


/* Exported interface, documented in global_history.h */
nserror global_history_init(struct core_window_callback_table *cw_t,
		void *core_window_handle)
{
	nserror err;

	LOG(("Loading global history"));

	/* Init. global history treeview time */
	err = global_history_initialise_time();
	if (err != NSERROR_OK) {
		gh_ctx.tree = NULL;
		return err;
	}

	/* Init. global history treeview entry fields */
	err = global_history_initialise_entry_fields();
	if (err != NSERROR_OK) {
		gh_ctx.tree = NULL;
		return err;
	}

	/* Load the entries */
	urldb_iterate_entries(global_history_add_entry);

	/* Create the global history treeview */
	err = treeview_create(&gh_ctx.tree, &tree_cb_t,
			N_FIELDS, gh_ctx.fields,
			cw_t, core_window_handle,
			TREEVIEW_NO_MOVES | TREEVIEW_DEL_EMPTY_DIRS);
	if (err != NSERROR_OK) {
		gh_ctx.tree = NULL;
		return err;
	}

	/* Ensure there is a folder for today */
	err = global_history_create_dir(GH_TODAY);
	if (err != NSERROR_OK) {
		return err;
	}

	/* Add the history to the treeview */
	err = global_history_init_entries();
	if (err != NSERROR_OK) {
		return err;
	}

	/* History tree is built
	 * We suppress the treeview height callback on entry insertion before
	 * the treeview is built. */
	gh_ctx.built = true;

	/* Expand the "Today" folder node */
	err = treeview_node_expand(gh_ctx.tree,
			gh_ctx.folders[GH_TODAY].folder);
	if (err != NSERROR_OK) {
		return err;
	}

	LOG(("Loaded global history"));

	return NSERROR_OK;
}


/* Exported interface, documented in global_history.h */
nserror global_history_fini(void)
{
	int i;
	nserror err;

	LOG(("Finalising global history"));

	/* Destroy the global history treeview */
	err = treeview_destroy(gh_ctx.tree);

	/* Free global history treeview entry fields */
	for (i = 0; i < N_FIELDS; i++)
		if (gh_ctx.fields[i].field != NULL)
			lwc_string_unref(gh_ctx.fields[i].field);

	LOG(("Finalised global history"));

	return NSERROR_OK;
}


/* Exported interface, documented in global_history.h */
void global_history_redraw(int x, int y, struct rect *clip,
		const struct redraw_context *ctx)
{
	treeview_redraw(gh_ctx.tree, x, y, clip, ctx);
}


/* Exported interface, documented in global_history.h */
void global_history_mouse_action(browser_mouse_state mouse, int x, int y)
{
	treeview_mouse_action(gh_ctx.tree, mouse, x, y);
}


/* Exported interface, documented in global_history.h */
void global_history_keypress(uint32_t key)
{
	treeview_keypress(gh_ctx.tree, key);
}
