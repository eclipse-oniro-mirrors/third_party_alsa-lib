/**
 * \file control/namehint.c
 * \brief Give device name hints
 * \author Jaroslav Kysela <perex@suse.cz>
 * \date 2006
 */
/*
 *  Give device name hints  - main file
 *  Copyright (c) 2006 by Jaroslav Kysela <perex@suse.cz>
 *
 *
 *   This library is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU Lesser General Public License as
 *   published by the Free Software Foundation; either version 2.1 of
 *   the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#include "local.h"

#ifndef DOC_HIDDEN
struct hint_list {
	char **list;
	unsigned int count;
	unsigned int allocated;
	snd_ctl_elem_iface_t iface;
	snd_ctl_t *ctl;
	snd_ctl_card_info_t *info;	
	int card;
	int device;
	long device_input;
	long device_output;
	int stream;
	int show_all;
	char *longname;
};
#endif

static int hint_list_add(struct hint_list *list,
			 const char *name,
			 const char *description)
{
	char *x;

	if (list->count == list->allocated) {
		char **n = realloc(list->list, (list->allocated + 10) * sizeof(char *));
		if (n == NULL)
			return -ENOMEM;
		list->allocated += 10;
		list->list = n;
	}
	if (name == NULL) {
		x = NULL;
	} else {
		x = malloc(strlen(name) + (description != NULL ? (strlen(description) + 1) : 0) + 1);
		if (x == NULL)
			return -ENOMEM;
		strcpy(x, name);
		if (description != NULL) {
			strcat(x, "|");
			strcat(x, description);
		}
	}
	list->list[list->count++] = x;
	return 0;
}

static void zero_handler(const char *file ATTRIBUTE_UNUSED,
			 int line ATTRIBUTE_UNUSED,
			 const char *function ATTRIBUTE_UNUSED,
			 int err ATTRIBUTE_UNUSED,
			 const char *fmt ATTRIBUTE_UNUSED, ...)
{
}

static int get_dev_name1(struct hint_list *list, char **res)
{
	*res = NULL;
	if (list->device < 0)
		return 0;
	switch (list->iface) {
	case SND_CTL_ELEM_IFACE_HWDEP:
		{
			snd_hwdep_info_t *info;
			snd_hwdep_info_alloca(&info);
			snd_hwdep_info_set_device(info, list->device);
			if (snd_ctl_hwdep_info(list->ctl, info) < 0)
				return 0;
			*res = strdup(snd_hwdep_info_get_name(info));
			return 0;
		}
	case SND_CTL_ELEM_IFACE_PCM:
		{
			snd_pcm_info_t *info;
			snd_pcm_info_alloca(&info);
			snd_pcm_info_set_device(info, list->device);
			snd_pcm_info_set_stream(info, list->stream ? SND_PCM_STREAM_CAPTURE : SND_PCM_STREAM_PLAYBACK);
			if (snd_ctl_pcm_info(list->ctl, info) < 0)
				return 0;
			switch (snd_pcm_info_get_class(info)) {
			case SND_PCM_CLASS_MODEM:
			case SND_PCM_CLASS_DIGITIZER:
				return -ENODEV;
			default:
				break;
			}
			*res = strdup(snd_pcm_info_get_name(info));
			return 0;
		}
	case SND_CTL_ELEM_IFACE_RAWMIDI:
		{
			snd_rawmidi_info_t *info;
			snd_rawmidi_info_alloca(&info);
			snd_rawmidi_info_set_device(info, list->device);
			snd_rawmidi_info_set_stream(info, list->stream ? SND_RAWMIDI_STREAM_INPUT : SND_RAWMIDI_STREAM_OUTPUT);
			if (snd_ctl_rawmidi_info(list->ctl, info) < 0)
				return 0;
			*res = strdup(snd_rawmidi_info_get_name(info));
			return 0;
		}
	default:
		return 0;
	}
}

static char *get_dev_name(struct hint_list *list)
{
	char *str1, *str2, *res;
	
	list->device = list->device_input >= 0 ? list->device_input : list->device;
	list->stream = 1;
	if (get_dev_name1(list, &str1) < 0)
		return NULL;
	list->device = list->device_output >= 0 ? list->device_input : list->device;
	list->stream = 0;
	if (get_dev_name1(list, &str2) < 0) {
		if (str1)
			free(str1);
		return NULL;
	}
	if (str1 != NULL || str2 != NULL) {
		if (str1 != NULL && str2 != NULL) {
			if (strcmp(str1, str2) == 0) {
				res = malloc(strlen(list->longname) + strlen(str2) + 3);
				if (res != NULL) {
					strcpy(res, list->longname);
					strcat(res, ", ");
					strcat(res, str2);
				}
			} else {
				res = malloc(strlen(list->longname) + strlen(str2) + strlen(str1) + 6);
				if (res != NULL) {
					strcpy(res, list->longname);
					strcat(res, ", ");
					strcat(res, str2);
					strcat(res, " / ");
					strcat(res, str1);
				}
			}
			free(str2);
			free(str1);
			return res;
		} else {
			if (str1 != NULL) {
				str2 = list->iface == SND_CTL_ELEM_IFACE_PCM ? "Capture" : "Input";
			} else {
				str1 = str2;
				str2 = list->iface == SND_CTL_ELEM_IFACE_PCM ? "Playback" : "Output";
			}
			res = malloc(strlen(list->longname) + strlen(str1) + 19);
			if (res == NULL) {
				free(str1);
				return NULL;
			}
			strcpy(res, list->longname);
			strcat(res, ", ");
			strcat(res, str1);
			strcat(res, " {");
			strcat(res, list->iface == SND_CTL_ELEM_IFACE_PCM ? "Capture" : "Input");
			strcat(res, "}");
			free(str1);
			return res;
		}
	} else {
		return strdup(list->longname);
	}
	return NULL;
}

#ifndef DOC_HIDDEN
#define BUF_SIZE 128
#endif

static int try_config(struct hint_list *list,
		      const char *base,
		      const char *name)
{
	snd_lib_error_handler_t eh;
	snd_config_t *res = NULL, *cfg, *n;
	snd_config_iterator_t i, next;
	char *buf, *buf1 = NULL, *buf2;
	const char *str;
	int err = 0, level;
	long dev = list->device;

	list->device_input = -1;
	list->device_output = -1;
	buf = malloc(BUF_SIZE);
	if (buf == NULL)
		return -ENOMEM;
	sprintf(buf, "%s.%s", base, name);
	/* look for redirection */
	if (snd_config_search(snd_config, buf, &cfg) >= 0 &&
	    snd_config_get_string(cfg, &str) >= 0 &&
	    ((strncmp(base, str, strlen(base)) == 0 &&
	     str[strlen(base)] == '.') || strchr(str, '.') == NULL))
	     	goto __skip_add;
	if (list->card >= 0 && list->device >= 0)
		sprintf(buf, "%s:CARD=%s,DEV=%i", name, snd_ctl_card_info_get_id(list->info), list->device);
	else if (list->card >= 0)
		sprintf(buf, "%s:CARD=%s", name, snd_ctl_card_info_get_id(list->info));
	else
		strcpy(buf, name);
	eh = snd_lib_error;
	snd_lib_error_set_handler(&zero_handler);
	err = snd_config_search_definition(snd_config, base, buf, &res);
	snd_lib_error_set_handler(eh);
	if (err < 0)
		goto __skip_add;
	err = -EINVAL;
	if (snd_config_get_type(res) != SND_CONFIG_TYPE_COMPOUND)
		goto __cleanup;
	if (snd_config_search(res, "type", NULL) < 0)
		goto __cleanup;

#if 0	/* for debug purposes */
		{
			snd_output_t *out;
			fprintf(stderr, "********* PCM '%s':\n", buf);
			snd_output_stdio_attach(&out, stderr, 0);
			snd_config_save(res, out);
			snd_output_close(out);
			fprintf(stderr, "\n");
		}
#endif

	cfg = res;
	level = 0;
      __hint:
      	level++;
	if (snd_config_search(cfg, "hint", &cfg) >= 0) {
		if (snd_config_get_type(cfg) != SND_CONFIG_TYPE_COMPOUND) {
			SNDERR("hint (%s) must be a compound", buf);
			err = -EINVAL;
			goto __cleanup;
		}
		if (level == 1 &&
		    snd_config_search(cfg, "show", &n) >= 0 &&
		    snd_config_get_bool(n) <= 0)
		    	goto __skip_add;
		if (buf1 == NULL &&
		    snd_config_search(cfg, "description", &n) >= 0 &&
		    snd_config_get_string(n, &str) >= 0) {
			buf1 = strdup(str);
			if (buf1 == NULL) {
				err = -ENOMEM;
				goto __cleanup;
			}
		}
		if (snd_config_search(cfg, "device", &n) >= 0) {
			if (snd_config_get_integer(n, &dev) < 0) {
				SNDERR("(%s) device must be an integer", buf);
				err = -EINVAL;
				goto __cleanup;
			}
			list->device_input = -1;
			list->device_output = -1;
		}
		if (snd_config_search(cfg, "device_input", &n) >= 0) {
			if (snd_config_get_integer(n, &list->device_input) < 0) {
				SNDERR("(%s) device_input must be an integer", buf);
				err = -EINVAL;
				goto __cleanup;
			}
			list->device_output = -1;
		}
		if (snd_config_search(cfg, "device_output", &n) >= 0) {
			if (snd_config_get_integer(n, &list->device_output) < 0) {
				SNDERR("(%s) device_output must be an integer", buf);
				err = -EINVAL;
				goto __cleanup;
			}
		}
	} else if (level == 1 && !list->show_all)
		goto __skip_add;
	if (snd_config_search(cfg, "slave", &cfg) >= 0 &&
	    snd_config_search(cfg, base, &cfg) >= 0)
	    	goto __hint;
	snd_config_delete(res);
	res = NULL;
	if (strchr(buf, ':') != NULL)
		goto __ok;
	/* find, if all parameters have a default, */
	/* otherwise filter this definition */
	eh = snd_lib_error;
	snd_lib_error_set_handler(&zero_handler);
	err = snd_config_search_alias_hooks(snd_config, base, buf, &res);
	snd_lib_error_set_handler(eh);
	if (err < 0)
		goto __cleanup;
	if (snd_config_search(res, "@args", &cfg) >= 0) {
		snd_config_for_each(i, next, cfg) {
			if (snd_config_search(snd_config_iterator_entry(i),
					      "default", NULL) < 0) {
				err = -EINVAL;
				goto __cleanup;
			}
		}
	}
      __ok:
	err = 0;
      __cleanup:
      	if (err >= 0) {
      		list->device = dev;
 		str = list->card >= 0 ? get_dev_name(list) : NULL;
      		if (str != NULL) {
      			buf2 = realloc((char *)str, (buf1 == NULL ? 0 : strlen(buf1)) + 1 + strlen(str) + 1);
      			if (buf2 != NULL) {
      				if (buf1 != NULL) {
	      				strcat(buf2, "\n");
	      				strcat(buf2, buf1);
					free(buf1);
				}
				buf1 = buf2;
			} else {
				free((char *)str);
			}
      		} else if (list->device >= 0)
      			goto __skip_add;
	      	err = hint_list_add(list, buf, buf1);
	}
      __skip_add:
      	if (res)
	      	snd_config_delete(res);
	if (buf1)
		free(buf1);
      	free(buf);
	return err;
}

#ifndef DOC_HIDDEN
#define IFACE(v, fcn) [SND_CTL_ELEM_IFACE_##v] = (next_devices_t)fcn

typedef int (*next_devices_t)(snd_ctl_t *, int *);

static next_devices_t next_devices[] = {
	IFACE(CARD, NULL),
	IFACE(HWDEP, snd_ctl_hwdep_next_device),
	IFACE(MIXER, NULL),
	IFACE(PCM, snd_ctl_pcm_next_device),
	IFACE(RAWMIDI, snd_ctl_rawmidi_next_device),
	IFACE(TIMER, NULL),
	IFACE(SEQUENCER, NULL)
};
#endif

static int add_card(struct hint_list *list, int card, snd_ctl_elem_iface_t iface)
{
	int err, ok;
	snd_config_t *conf, *n;
	snd_config_iterator_t i, next;
	const char *str, *base;
	char ctl_name[16];
	snd_ctl_card_info_t *info;
	
	snd_ctl_card_info_alloca(&info);
	list->info = info;
	if (iface > SND_CTL_ELEM_IFACE_LAST)
		return -EINVAL;
	base = snd_ctl_iface_conf_name(iface);
	err = snd_config_search(snd_config, base, &conf);
	if (err < 0)
		return err;
	sprintf(ctl_name, "hw:%i", card);
	err = snd_ctl_open(&list->ctl, ctl_name, 0);
	if (err < 0)
		return err;
	err = snd_ctl_card_info(list->ctl, info);
	if (err < 0)
		goto __error;
	snd_config_for_each(i, next, conf) {
		n = snd_config_iterator_entry(i);
		if (snd_config_get_id(n, &str) < 0)
			continue;
		if (next_devices[iface] != NULL) {
			list->card = card;
			list->device = -1;
			err = next_devices[iface](list->ctl, &list->device);
			if (list->device < 0)
				err = -EINVAL;
			ok = 0;
			while (err >= 0 && list->device >= 0) {
				err = try_config(list, base, str);
				if (err < 0)
					break;
				err = next_devices[iface](list->ctl, &list->device);
				ok++;
			}
			if (ok)
				continue;
		} else {
			err = -EINVAL;
		}
		if (err == -EXDEV)
			continue;
		if (err < 0) {
			list->device = -1;
			err = try_config(list, base, str);
		}
		if (err < 0) {
			list->card = -1;
			err = try_config(list, base, str);
		}
		if (err == -ENOMEM)
			goto __error;
	}
	err = 0;
      __error:
      	snd_ctl_close(list->ctl);
	return err;
}

/**
 * \brief Return string list with device name hints.
 * \param card Card number or -1 (means all cards)
 * \param iface Interface identification
 * \param hints Result - array of string with device name hints
 * \result zero if success, otherwise a negative error code
 *
 * Note: The device description is separated with '|' char.
 *
 * User defined hints are gathered from namehint.IFACE tree like:
 *
 * <code>
 * namehint.pcm {<br>
 *   myfile "file:FILE=/tmp/soundwave.raw|Save sound output to /tmp/soundwave.raw"<br>
 *   myplug "plug:front:Do all conversions for front speakers"<br>
 * }
 * </code>
 *
 * Special variables: defaults.namehint.showall specifies if all device
 * definitions are accepted (boolean type).
 */
int snd_device_name_hint(int card, snd_ctl_elem_iface_t iface, char ***hints)
{
	struct hint_list list;
	char ehints[24];
	const char *str;
	snd_config_t *conf;
	snd_config_iterator_t i, next;
	int err;

	if (hints == NULL)
		return -EINVAL;
	err = snd_config_update();
	if (err < 0)
		return err;
	list.list = NULL;
	list.count = list.allocated = 0;
	list.iface = iface;
	list.show_all = 0;
	list.longname = NULL;
	if (snd_config_search(snd_config, "defaults.namehint.showall", &conf) >= 0)
		list.show_all = snd_config_get_bool(conf) > 0;
	if (card >= 0) {
		err = add_card(&list, card, iface);
	} else {
		err = snd_card_next(&card);
		if (err < 0)
			goto __error;
		while (card >= 0) {
			err = snd_card_get_longname(card, &list.longname);
			if (err < 0)
				goto __error;
			err = add_card(&list, card, iface);
			if (err < 0)
				goto __error;
			err = snd_card_next(&card);
			if (err < 0)
				goto __error;
		}
	}
	sprintf(ehints, "namehint.%s", snd_ctl_iface_conf_name(iface));
	err = snd_config_search(snd_config, ehints, &conf);
	if (err >= 0) {
		snd_config_for_each(i, next, conf) {
			if (snd_config_get_string(snd_config_iterator_entry(i),
						  &str) < 0)
				continue;
			err = hint_list_add(&list, str, NULL);
			if (err < 0)
				goto __error;
		}
	}
	err = 0;
      __error:
      	if (err < 0) {
      		snd_device_name_free_hint(list.list);
      		if (list.longname)
	      		free(list.longname);
      		return err;
      	} else {
      		err = hint_list_add(&list, NULL, NULL);
      		if (err < 0)
      			goto __error;
      		*hints = list.list;
      		if (list.longname)
	      		free(list.longname);
	}
      	return 0;
}

/**
 * \brief Free a string list with device name hints.
 * \param hints A string list to free
 * \result zero if success, otherwise a negative error code
 */
int snd_device_name_free_hint(char **hints)
{
	char **h;

	if (hints == NULL)
		return 0;
	h = hints;
	while (*h) {
		free(*h);
		h++;
	}
	free(hints);
	return 0;
}
