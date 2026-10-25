/* SPDX-License-Identifier: LGPL-2.1-or-later */
/* PC5: the link for Report on GitHub (pc5_report.h). */
#include "pc5_report.h"

#include <string.h>

const char *pc5_report_file(const Pc5LogIssue *issue)
{
    const char *slash = strrchr(issue->file, '/');
    return slash ? slash + 1 : issue->file;
}

/* Text for a link's query: letters, digits and -._~ as they are, everything else as %XX. */
static int put_encoded(Pc5Buf *out, const char *text)
{
    static const char hex[] = "0123456789ABCDEF";
    for (const unsigned char *p = (const unsigned char *)text; *p; p++) {
        int plain = (*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || *p == '-' ||
                    *p == '.' || *p == '_' || *p == '~';
        char three[3] = { '%', hex[*p >> 4], hex[*p & 15] };
        if (plain ? pc5_buf_put(out, p, 1) : pc5_buf_put(out, three, 3)) return -1;
    }
    return 0;
}

/* The program's file name (Grim Dawn.exe from C:\Games\Grim Dawn\x64\Grim Dawn.exe). */
static const char *program_name(const char *exe)
{
    const char *slash = strrchr(exe, '\\');
    return slash ? slash + 1 : exe;
}

int pc5_report_url(const Pc5LogIssue *issue, const char *game, char *out, size_t size)
{
    Pc5Buf title = { 0 }, body = { 0 }, url = { 0 };
    const char *name = game && game[0] ? game : strcmp(issue->profile, "launcher") ? issue->profile : "PC5";
    char detail[161];
    int failed;

    /* The crash's own line, shortened so the code stays easy to scan. */
    pc5_copy(detail, sizeof(detail), issue->detail);
    for (char *p = detail; *p; p++)
        if (*p == '`') *p = '\'';
    failed = pc5_buf_printf(&title, "%s: %s", name, pc5_log_problem_text(issue->problem)) ||
             pc5_buf_printf(&body, "**What happened**\n(what you did, where it stopped, and whether it happens every time)\n\n\n"
                                   "**Game:** %s", name) ||
             (issue->exe[0] && pc5_buf_printf(&body, " (`%s`)", program_name(issue->exe))) ||
             pc5_buf_printf(&body, "\n**PC5:** %s", PC5_VERSION) ||
             (issue->firmware[0] && pc5_buf_printf(&body, " \xc2\xb7 **Firmware:** %s", issue->firmware)) ||
             pc5_buf_printf(&body, "\n**PC5 saw:** %s\n", pc5_log_problem_text(issue->problem)) ||
             (detail[0] && pc5_buf_printf(&body, "`%s`\n", detail)) ||
             pc5_buf_printf(&body, "\n**Log:** please attach `%s` from `/data/prospero-win/logs/` (drag it into this box).\n",
                            pc5_report_file(issue)) ||
             pc5_buf_terminate(&title) || pc5_buf_terminate(&body) ||
             pc5_buf_puts(&url, PC5_ISSUES_NEW "?title=") || put_encoded(&url, (const char *)title.data) ||
             pc5_buf_puts(&url, "&body=") || put_encoded(&url, (const char *)body.data) || pc5_buf_terminate(&url) ||
             url.len >= size;
    if (!failed) memcpy(out, url.data, url.len + 1);
    pc5_buf_free(&title);
    pc5_buf_free(&body);
    pc5_buf_free(&url);
    return failed ? -1 : 0;
}
