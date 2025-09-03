#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#define INITIAL_LINES 100
#define INITIAL_COLS  1024

typedef struct {
    char *text;
    int length;
} Line;

static void disable_flow_control(void) {
    struct termios tio;
    if (tcgetattr(STDIN_FILENO, &tio) == 0) {
        tio.c_iflag &= ~(IXON | IXOFF | IXANY);
        tcsetattr(STDIN_FILENO, TCSANOW, &tio);
    }
}

char *my_strdup(const char *s) {
    size_t len = strlen(s) + 1;
    char *copy = malloc(len);
    if (copy) memcpy(copy, s, len);
    return copy;
}

void save_file(Line *lines, int total_lines, const char *filename) {
    FILE *fp = fopen(filename, "w");
    if (!fp) return;
    for (int i = 0; i < total_lines; i++) {
        fprintf(fp, "%s\n", lines[i].text);
    }
    fclose(fp);
}

int prompt_filename(char *filename, size_t size) {
    echo();
    curs_set(1);
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    (void)max_x;
    move(max_y-1, 0);
    clrtoeol();
    printw("Save as: ");
    refresh();
    wgetnstr(stdscr, filename, size-1);
    noecho();
    curs_set(0);
    return strlen(filename) > 0;
}

int main(int argc, char *argv[]) {
    initscr();

    raw();
    noecho();
    keypad(stdscr, TRUE);
    set_escdelay(25);
    disable_flow_control();

    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);
    (void)max_x;

    int lines_capacity = INITIAL_LINES;
    int total_lines = 0;
    Line *lines = malloc(lines_capacity * sizeof(Line));

    char filename[256] = "new.txt";

    if (argc > 1) {
        strncpy(filename, argv[1], sizeof(filename)-1);
        FILE *fp = fopen(argv[1], "r");
        if (fp) {
            char buf[INITIAL_COLS];
            while (fgets(buf, sizeof(buf), fp)) {
                buf[strcspn(buf, "\n")] = 0;
                if (total_lines >= lines_capacity) {
                    lines_capacity *= 2;
                    lines = realloc(lines, lines_capacity * sizeof(Line));
                }
                lines[total_lines].text = my_strdup(buf);
                lines[total_lines].length = (int)strlen(buf);
                total_lines++;
            }
            fclose(fp);
        }
    }

    if (total_lines == 0) {
        total_lines = 1;
        lines[0].text = calloc(INITIAL_COLS, sizeof(char));
        lines[0].length = 0;
    }

    int cur_x = 0, cur_y = 0;
    int ch;
    int top_line = 0;

    char *clipboard = NULL;

    for (;;) {
        clear();
        for (int i = top_line; i < total_lines && i < top_line + max_y - 2; i++) {
            mvprintw(i - top_line, 0, "%s", lines[i].text);
        }
        mvprintw(max_y-2, 0,
                 "File: %s | ^S save | ^O save as | ^Q quit | ^K cut | ^U paste | ESC quit",
                 filename);
        move(cur_y - top_line, cur_x);
        refresh();

        ch = getch();

        if (ch == 17 || ch == 27) {
            break;
        } else if (ch == 19) {
            save_file(lines, total_lines, filename);
        } else if (ch == 15) {
            char newname[256];
            newname[0] = '\0';
            if (prompt_filename(newname, sizeof(newname))) {
                strncpy(filename, newname, sizeof(filename)-1);
                save_file(lines, total_lines, filename);
            }
        } else if (ch == 11) {
            free(clipboard);
            clipboard = my_strdup(lines[cur_y].text);
            free(lines[cur_y].text);
            memmove(&lines[cur_y], &lines[cur_y+1],
                    sizeof(Line) * (total_lines - cur_y - 1));
            total_lines--;
            if (cur_y >= total_lines) cur_y = total_lines - 1;
            if (cur_y < 0) cur_y = 0;
            cur_x = 0;
        } else if (ch == 21) {
            if (clipboard) {
                if (total_lines >= lines_capacity) {
                    lines_capacity *= 2;
                    lines = realloc(lines, lines_capacity * sizeof(Line));
                }
                memmove(&lines[cur_y+2], &lines[cur_y+1],
                        sizeof(Line) * (total_lines - cur_y - 1));
                lines[cur_y+1].text = my_strdup(clipboard);
                lines[cur_y+1].length = (int)strlen(clipboard);
                total_lines++;
            }
        } else {
            switch (ch) {
                case KEY_UP:
                    if (cur_y > 0) cur_y--;
                    if (cur_y < top_line) top_line--;
                    if (cur_x > lines[cur_y].length) cur_x = lines[cur_y].length;
                    break;
                case KEY_DOWN:
                    if (cur_y < total_lines - 1) cur_y++;
                    if (cur_y >= top_line + max_y - 2) top_line++;
                    if (cur_x > lines[cur_y].length) cur_x = lines[cur_y].length;
                    break;
                case KEY_LEFT:
                    if (cur_x > 0) cur_x--;
                    break;
                case KEY_RIGHT:
                    if (cur_x < lines[cur_y].length) cur_x++;
                    break;
                case KEY_BACKSPACE:
                case 127:
                case 8:
                    if (cur_x > 0) {
                        memmove(&lines[cur_y].text[cur_x-1],
                                &lines[cur_y].text[cur_x],
                                (size_t)(lines[cur_y].length - cur_x + 1));
                        lines[cur_y].length--;
                        cur_x--;
                    } else if (cur_y > 0) {
                        int prev_len = lines[cur_y-1].length;
                        lines[cur_y-1].text = realloc(lines[cur_y-1].text,
                                                      (size_t)prev_len + lines[cur_y].length + 1);
                        strcat(lines[cur_y-1].text, lines[cur_y].text);

                        free(lines[cur_y].text);
                        memmove(&lines[cur_y], &lines[cur_y+1],
                                sizeof(Line) * (total_lines - cur_y - 1));
                        total_lines--;
                        cur_y--;
                        cur_x = prev_len;
                    }
                    break;
                case '\n': {
                    if (total_lines >= lines_capacity) {
                        lines_capacity *= 2;
                        lines = realloc(lines, lines_capacity * sizeof(Line));
                    }
                    memmove(&lines[cur_y+2], &lines[cur_y+1],
                            sizeof(Line) * (total_lines - cur_y - 1));

                    lines[cur_y+1].text = calloc(INITIAL_COLS, sizeof(char));

                    if (cur_x < lines[cur_y].length) {
                        strcpy(lines[cur_y+1].text, &lines[cur_y].text[cur_x]);
                        lines[cur_y+1].length = (int)strlen(lines[cur_y+1].text);

                        lines[cur_y].text[cur_x] = '\0';
                        lines[cur_y].length = (int)strlen(lines[cur_y].text);
                    } else {
                        lines[cur_y+1].text[0] = '\0';
                        lines[cur_y+1].length = 0;
                    }

                    total_lines++;
                    cur_y++;
                    cur_x = 0;
                    break;
                }
                default:
                    lines[cur_y].text = realloc(lines[cur_y].text,
                                                (size_t)lines[cur_y].length + 2);
                    memmove(&lines[cur_y].text[cur_x+1],
                            &lines[cur_y].text[cur_x],
                            (size_t)(lines[cur_y].length - cur_x + 1));
                    lines[cur_y].text[cur_x] = (char)ch;
                    lines[cur_y].length++;
                    cur_x++;
                    break;
            }
        }
    }

    for (int i = 0; i < total_lines; i++) free(lines[i].text);
    free(lines);
    free(clipboard);

    endwin();
    return 0;
}
