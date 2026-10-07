#include "mirror.h"
#include "string.h"

#define ATTR_FRAME  USER_COLOR(C_BLACK, C_CYAN)
#define ATTR_TITLE  USER_COLOR(C_BLUE, C_WHITE)
#define ATTR_STATUS USER_COLOR(C_BLUE, C_WHITE)
#define ATTR_TEXT   USER_COLOR(C_BLACK, C_LIGHT_GREY)
#define ATTR_SEL    USER_COLOR(C_WHITE, C_BLACK)
#define ATTR_HILITE USER_COLOR(C_BLACK, C_YELLOW)
#define ATTR_FIELD  USER_COLOR(C_DARK_GREY, C_WHITE)
#define ATTR_BAR    USER_COLOR(C_BLACK, C_LIGHT_GREEN)
#define ATTR_EMPTY  USER_COLOR(C_BLACK, C_DARK_GREY)

#define OUT_X 36
#define LOG_X 3
#define LOG_Y 14
#define LOG_ROWS 6

static mirror_t screen;
static char name[21];
static int progress;
static char logbuf[LOG_ROWS][64] = { [LOG_ROWS - 1] = "mirror demo ready" };

static const char *menu_items[] = {
    "edit a text field",
    "draw a progress bar",
    "about mirror",
    "quit",
};
#define MENU_COUNT 4

static void log_add(const char *s)
{
    for (int i = 0; i < LOG_ROWS - 1; i++)
        strcpy(logbuf[i], logbuf[i + 1]);

    int i = 0;
    for (; s[i] && i < (int)sizeof logbuf[0] - 1; i++)
        logbuf[LOG_ROWS - 1][i] = s[i];
    logbuf[LOG_ROWS - 1][i] = '\0';
}

static void log_draw(void)
{
    for (int i = 0; i < LOG_ROWS; i++)
    {
        mirror_fill(&screen, LOG_X, LOG_Y + i, 74, 1, ' ', ATTR_TEXT);
        mirror_puts(&screen, LOG_X, LOG_Y + i, logbuf[i], ATTR_TEXT);
    }
}

static void clear_output(void)
{
    mirror_fill(&screen, 35, 3, 43, 7, ' ', ATTR_TEXT);
}

static void do_edit(void)
{
    clear_output();
    mirror_puts(&screen, OUT_X, 4, "type and press enter, esc cancels:", ATTR_TEXT);

    if (mirror_input(&screen, OUT_X, 7, 40, name, (int)sizeof name, ATTR_FIELD) == 0)
    {
        mirror_fill(&screen, 35, 4, 43, 1, ' ', ATTR_TEXT);
        mirror_printf(&screen, OUT_X, 4, ATTR_HILITE, "hello, %s!", name);
        mirror_printf(&screen, OUT_X, 5, ATTR_TEXT, "you typed %d characters", strlen(name));
        progress = strlen(name) * 100 / ((int)sizeof name - 1);
        log_add("entered text in the field");
    }
    else
    {
        mirror_fill(&screen, 35, 4, 43, 1, ' ', ATTR_TEXT);
        mirror_puts(&screen, OUT_X, 4, "input cancelled", ATTR_TEXT);
        log_add("text field cancelled");
    }
}

static void do_progress(void)
{
    clear_output();
    mirror_printf(&screen, OUT_X, 4, ATTR_TEXT,
                  "the bar shows how full the field is: %d%%", progress);
    mirror_progress(&screen, OUT_X, 7, 40, progress, ATTR_BAR, ATTR_EMPTY);
    mirror_printf(&screen, OUT_X, 8, ATTR_TEXT, "%d of 20 characters typed", strlen(name));
    log_add("drew a progress bar");
}

static void do_about(void)
{
    clear_output();
    mirror_printf(&screen, OUT_X, 4, ATTR_HILITE, "Mirror v0.1");
    mirror_puts(&screen, OUT_X, 5, "a cell-buffered TUI library for LunariaOS", ATTR_TEXT);
    mirror_puts(&screen, OUT_X, 6, "you draw into an off-screen buffer;", ATTR_TEXT);
    mirror_puts(&screen, OUT_X, 7, "mirror_flush() sends only changed cells", ATTR_TEXT);
    log_add("showed the about text");
}

static void draw_frame(void)
{
    static const char title[] = " Mirror - LunariaOS userspace TUI library";
    static const char tag[] = " mirrordemo ";

    mirror_clear(&screen, ATTR_TEXT);

    mirror_fill(&screen, 0, 0, MIRROR_COLS, 1, ' ', ATTR_TITLE);
    mirror_puts(&screen, 1, 0, title, ATTR_TITLE);
    mirror_puts(&screen, MIRROR_COLS - (int)sizeof tag, 0, tag, ATTR_TITLE);

    mirror_box(&screen, 1, 2, 30, 9, ATTR_FRAME, " menu ");
    mirror_box(&screen, 34, 2, 45, 9, ATTR_FRAME, " output ");
    mirror_box(&screen, 1, 12, 78, 10, ATTR_FRAME, " event log ");

    mirror_status(&screen, 24, " arrows move | enter select | esc quit",
                  "mirror v0.1", ATTR_STATUS);

    clear_output();
    mirror_puts(&screen, OUT_X, 4, "ready - pick a menu entry", ATTR_TEXT);
    log_draw();
}

void _start(void)
{
    int sel = 0;

    mirror_init(&screen, ATTR_TEXT);
    draw_frame();
    mirror_flush(&screen);

    for (;;)
    {
        sel = mirror_menu_select(&screen, 3, 4, 26, menu_items, MENU_COUNT,
                                 sel, ATTR_TEXT, ATTR_SEL);
        if (sel < 0)
            break;
        log_add(menu_items[sel]);

        if (sel == 0)
            do_edit();
        else if (sel == 1)
            do_progress();
        else if (sel == 2)
            do_about();
        else
            break;

        log_draw();
        mirror_flush(&screen);
    }

    mirror_clear(&screen, ATTR_TEXT);
    mirror_flush(&screen);
    sys_exit();
}
