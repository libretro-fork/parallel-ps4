/* SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <string.h>

#include <boolean.h>
#include <formats/rjson.h>
#include <formats/rjson_helpers.h>

#include "core/libraries/np/np_handler/np_invitation_json.h"

struct np_invitation_sink {
    char* s;
    size_t len;
    size_t written;
};

static int np_invitation_sink_io(const void* buf, int len, void* user) {
    struct np_invitation_sink* sink = (struct np_invitation_sink*)user;
    if (sink->len && sink->written < sink->len - 1) {
        size_t room = sink->len - 1 - sink->written;
        size_t n = (size_t)len < room ? (size_t)len : room;
        memcpy(sink->s + sink->written, buf, n);
    }
    sink->written += (size_t)len;
    return len;
}

size_t np_invitation_request_json(char* s, size_t len, const char* const* to, size_t count,
                                  const char* message) {
    struct np_invitation_sink sink;
    rjsonwriter_t* writer;
    size_t i;

    sink.s = s;
    sink.len = len;
    sink.written = 0;
    writer = rjsonwriter_open_user(np_invitation_sink_io, &sink);
    if (!writer)
        return 0;
    rjsonwriter_set_options(writer, RJSONWRITER_OPTION_SKIP_WHITESPACE);

    rjsonwriter_add_start_object(writer);
    rjsonwriter_add_string(writer, "to");
    rjsonwriter_add_colon(writer);
    rjsonwriter_add_start_array(writer);
    for (i = 0; i < count; i++) {
        if (i)
            rjsonwriter_add_comma(writer);
        rjsonwriter_add_string(writer, to[i]);
    }
    rjsonwriter_add_end_array(writer);
    if (message && message[0]) {
        rjsonwriter_add_comma(writer);
        rjsonwriter_add_string(writer, "message");
        rjsonwriter_add_colon(writer);
        rjsonwriter_add_string(writer, message);
    }
    rjsonwriter_add_end_object(writer);

    if (!rjsonwriter_free(writer))
        return 0;
    if (len)
        s[sink.written < len ? sink.written : len - 1] = '\0';
    return sink.written;
}
