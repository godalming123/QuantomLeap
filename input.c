/*
 * Copyright © 2018-2019 Collabora, Ltd.
 * Copyright © 2018-2019 DAQRI, LLC and its affiliates
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Author: Heinrich Fink <heinrich.fink@daqri.com>
 */

#include <libudev.h>
#include <libinput.h>
#include <stdio.h>
#include <errno.h>
#include "kms-quads.h"

struct input {
	struct udev *udev;
	struct libinput *input;
};

static int open_restricted(const char *path, int flags UNUSED, void* user_data)
{
	struct logind *session = user_data;
#if defined(HAVE_LOGIND)
	assert(session);
	return logind_take_device(session, path);
#else
	int fd = open(path, flags);
	return fd < 0 ? -errno : fd;
#endif
}

static void close_restricted(int fd, void *user_data)
{
	struct logind *session = user_data;
#if defined(HAVE_LOGIND)
	assert(session);
	logind_release_device(session, fd);
#else
	close(fd);
#endif
}

static const struct libinput_interface libinput_impl = {
    .open_restricted = open_restricted,
    .close_restricted = close_restricted
};

struct input* input_create(struct logind *session)
{
	struct input *ret = calloc(1, sizeof(struct input));
	ret->udev = udev_new();
	if (!ret->udev) {
		error("failed to create udev context\n");
		goto err;
	}

	ret->input = libinput_udev_create_context(&libinput_impl, session, ret->udev);
	if (!ret->input) {
	    error("failed to create libinput context\n");
	    goto err_udev;
	}

	if (libinput_udev_assign_seat(ret->input, "seat0") < 0) {
	    error("failed to assign udev seat to libinput instance");
	    goto err_input;
	}

	return ret;
err_input:
	libinput_unref(ret->input);
err_udev:
	udev_unref(ret->udev);
err:
	free(ret);
	return NULL;
}

void input_destroy(struct input *input)
{
	libinput_unref(input->input);
	udev_unref(input->udev);
	free(input);
}

bool input_was_ESC_key_pressed(struct input *input)
{
	assert(input);
	struct libinput_event *event;
	libinput_dispatch(input->input);
	enum libinput_event_type type;
	bool ret = false;

	while ((event = libinput_get_event(input->input)) != NULL) {
		type = libinput_event_get_type(event);
		if (type == LIBINPUT_EVENT_KEYBOARD_KEY) {
			struct libinput_event_keyboard *key_event = libinput_event_get_keyboard_event(event);
			uint32_t key_code = libinput_event_keyboard_get_key(key_event);
			/* 1 == KEY_ESC in input-event-codes.h */
			ret = ret || (key_code == 1);
		}
		libinput_event_destroy(event);
		libinput_dispatch(input->input);
        }
        return ret;
}

