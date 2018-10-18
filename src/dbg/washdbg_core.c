/*******************************************************************************
 *
 *
 *    WashingtonDC Dreamcast Emulator
 *    Copyright (C) 2018 snickerbockers
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *
 ******************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "log.h"
#include "dreamcast.h"
#include "io/washdbg_tcp.h"
#include "sh4asm_core/disas.h"

#include "dbg/washdbg_core.h"

#define BUF_LEN 1024

static char in_buf[BUF_LEN];
unsigned in_buf_pos;

struct washdbg_txt_state {
    char const *txt;
    unsigned pos;
};

static void washdbg_process_input(void);
static int washdbg_puts(char const *txt);

static unsigned washdbg_print_buffer(struct washdbg_txt_state *state);

static void washdbg_print_banner(void);

static void washdbg_echo(int argc, char **argv);

static void washdbg_state_echo_process(void);

static int
eval_expression(char const *expr, enum dbg_context_id *ctx_id, unsigned *out);

static unsigned washdbg_print_x(void);

enum washdbg_byte_count {
    WASHDBG_1_BYTE = 1,
    WASHDBG_2_BYTE = 2,
    WASHDBG_4_BYTE = 4,
    WASHDBG_INST   = 5
};

static int
parse_fmt_string(char const *str, enum washdbg_byte_count *byte_count_out,
                 unsigned *count_out);

enum washdbg_state {
    WASHDBG_STATE_BANNER,
    WASHDBG_STATE_PROMPT,
    WASHDBG_STATE_NORMAL,
    WASHDBG_STATE_BAD_INPUT,
    WASHDBG_STATE_CMD_CONTINUE,
    WASHDBG_STATE_RUNNING,
    WASHDBG_STATE_HELP,
    WASHDBG_STATE_CONTEXT_INFO,
    WASHDBG_STATE_PRINT_ERROR,
    WASHDBG_STATE_ECHO,
    WASHDBG_STATE_X,

    // permanently stop accepting commands because we're about to disconnect.
    WASHDBG_STATE_CMD_EXIT
} cur_state;

void washdbg_init(void) {
    washdbg_print_banner();
}

static struct continue_state {
    struct washdbg_txt_state txt;
} continue_state;

void washdbg_do_continue(int argc, char **argv) {
    continue_state.txt.txt = "Continuing execution\n";
    continue_state.txt.pos = 0;

    cur_state = WASHDBG_STATE_CMD_CONTINUE;
}

static bool washdbg_is_continue_cmd(char const *cmd) {
    return strcmp(cmd, "c") == 0 ||
        strcmp(cmd, "continue") == 0;
}

void washdbg_do_exit(int argc, char **argv) {
    LOG_INFO("User requested exit via WashDbg\n");
    dreamcast_kill();
    cur_state = WASHDBG_STATE_CMD_EXIT;
}

static bool washdbg_is_exit_cmd(char const *cmd) {
    return strcmp(cmd, "exit") == 0;
}

void washdbg_input_ch(char ch) {
    if (ch == '\r')
        return;

    // in_buf[1023] will always be \0
    if (in_buf_pos <= (BUF_LEN - 2))
        in_buf[in_buf_pos++] = ch;
}

struct print_banner_state {
    struct washdbg_txt_state txt;
} print_banner_state;

static void washdbg_print_banner(void) {
    // this gets printed to the dev console every time somebody connects to the debugger
    static char const *login_banner =
        "Welcome to WashDbg!\n"
        "WashingtonDC Copyright (C) 2016-2018 snickerbockers\n"
        "This program comes with ABSOLUTELY NO WARRANTY;\n"
        "This is free software, and you are welcome to redistribute it\n"
        "under the terms of the GNU GPL version 3.\n\n";

    print_banner_state.txt.txt = login_banner;
    print_banner_state.txt.pos = 0;

    cur_state = WASHDBG_STATE_BANNER;
}

static struct help_state {
    struct washdbg_txt_state txt;
} help_state;

void washdbg_do_help(int argc, char **argv) {
    static char const *help_msg =
        "WashDbg command list\n"
        "\n"
        "continue - continue execution when suspended.\n"
        "echo     - echo back text\n"
        "exit     - exit the debugger and close WashingtonDC\n"
        "help     - display this message\n"
        "x        - eXamine memory address\n";

    help_state.txt.txt = help_msg;
    help_state.txt.pos = 0;
    cur_state = WASHDBG_STATE_HELP;
}

static bool washdbg_is_help_cmd(char const *cmd) {
    return strcmp(cmd, "help") == 0;
}

struct context_info_state {
    struct washdbg_txt_state txt;
} context_info_state;

/*
 * Display info about the current context before showing a new prompt
 */
void washdbg_print_context_info(void) {
    char const *msg = NULL;
    switch (debug_current_context()) {
    case DEBUG_CONTEXT_SH4:
        msg = "Current debug context is SH4\n";
        break;
    case DEBUG_CONTEXT_ARM7:
        msg = "Current debug context is ARM7\n";
        break;
    default:
        msg = "Current debug context is <unknown/error>\n";
    }
    context_info_state.txt.txt = msg;
    context_info_state.txt.pos = 0;
    cur_state = WASHDBG_STATE_CONTEXT_INFO;
}

struct print_prompt_state {
    struct washdbg_txt_state txt;
} print_prompt_state;

void washdbg_print_prompt(void) {
    static char const *prompt = "(WashDbg): ";

    print_prompt_state.txt.txt = prompt;
    print_prompt_state.txt.pos = 0;
    cur_state = WASHDBG_STATE_PROMPT;
}

static struct bad_input_state {
    struct washdbg_txt_state txt;
    char bad_input_line[BUF_LEN];
} bad_input_state;

static void washdbg_bad_input(char const *bad_cmd) {
    snprintf(bad_input_state.bad_input_line,
             sizeof(bad_input_state.bad_input_line),
             "Unrecognized input \"%s\"\n", bad_cmd);
    bad_input_state.bad_input_line[BUF_LEN - 1] = '\0';

    bad_input_state.txt.txt = bad_input_state.bad_input_line;
    bad_input_state.txt.pos = 0;
    cur_state = WASHDBG_STATE_BAD_INPUT;
}

static struct print_error_state {
    struct washdbg_txt_state txt;
} print_error_state;

static void washdbg_print_error(char const *error) {
    print_error_state.txt.txt = error;
    print_error_state.txt.pos = 0;
    cur_state = WASHDBG_STATE_PRINT_ERROR;
}

static struct echo_state {
    int argc;
    char **argv;
    int cur_arg;
    unsigned cur_arg_pos;
    bool print_space;
} echo_state;

static void washdbg_echo(int argc, char **argv) {
    int arg_no;

    if (argc <= 1) {
        washdbg_print_prompt();
        return;
    }

    echo_state.cur_arg = 1;
    echo_state.cur_arg_pos = 0;
    echo_state.print_space = false;
    echo_state.argc = argc;
    echo_state.argv = (char**)calloc(sizeof(char*), argc);
    if (!echo_state.argv) {
        washdbg_print_error("Failed allocation.\n");
        goto cleanup_args;
    }

    for (arg_no = 0; arg_no < argc; arg_no++) {
        echo_state.argv[arg_no] = strdup(argv[arg_no]);
        if (!echo_state.argv[arg_no]) {
            washdbg_print_error("Failed allocation.\n");
            goto cleanup_args;
        }
    }

    cur_state = WASHDBG_STATE_ECHO;

    return;

cleanup_args:
    for (arg_no = 0; arg_no < argc; arg_no++)
        free(echo_state.argv[arg_no]);
    free(echo_state.argv);
}

static bool washdbg_is_echo_cmd(char const *cmd) {
    return strcmp(cmd, "echo") == 0;
}

#define WASHDBG_X_STATE_STR_LEN 128

static struct x_state {
    char str[WASHDBG_X_STATE_STR_LEN];
    size_t str_pos;

    void *dat;
    unsigned byte_count;
    unsigned count;
    unsigned idx;
    bool disas_mode;
} x_state;

static void washdbg_x_state_disas_emit(char ch) {
    size_t len = strlen(x_state.str);
    if (len >= WASHDBG_X_STATE_STR_LEN - 1)
        return; // no more space
    x_state.str[len] = ch;
}

static void washdbg_x_set_string(void) {
    uint32_t val32;
    uint16_t val16;
    uint8_t val8;

    if (x_state.disas_mode) {
        memset(x_state.str, 0, sizeof(x_state.str));
        val16 = ((uint16_t*)x_state.dat)[x_state.idx];
        disas_inst(val16, washdbg_x_state_disas_emit);
        size_t len = strlen(x_state.str);
        if (len >= WASHDBG_X_STATE_STR_LEN - 1)
            x_state.str[WASHDBG_X_STATE_STR_LEN - 2] = '\n';
        else
            x_state.str[len] = '\n';
    } else {
        switch (x_state.byte_count) {
        case 4:
            val32 = ((uint32_t*)x_state.dat)[x_state.idx];
            snprintf(x_state.str, sizeof(x_state.str), "0x%08x\n", (unsigned)val32);
            break;
        case 2:
            val16 = ((uint16_t*)x_state.dat)[x_state.idx];
            snprintf(x_state.str, sizeof(x_state.str), "0x%04x\n", (unsigned)val16);
            break;
        case 1:
            val8 = ((uint8_t*)x_state.dat)[x_state.idx];
            snprintf(x_state.str, sizeof(x_state.str), "0x%02x\n", (unsigned)val8);
            break;
        default:
            strncpy(x_state.str, "<ERROR>\n", sizeof(x_state.str));
            x_state.str[WASHDBG_X_STATE_STR_LEN - 1] = '\0';
        }
    }
}

static void washdbg_x(int argc, char **argv) {
    unsigned addr;
    enum dbg_context_id ctx;
    char *fmt_str;

    if (argc != 2) {
        washdbg_print_error("only a single argument is supported for the x "
                            "command.\n");
        return;
    }

    fmt_str = strchr(argv[0], '/');
    if (fmt_str) {
        fmt_str++;
        printf("The format string is \"%s\"\n", fmt_str);
    }

    memset(&x_state, 0, sizeof(x_state));

    if (parse_fmt_string(fmt_str, &x_state.byte_count, &x_state.count) < 0) {
        washdbg_print_error("failed to parse x-command format string.\n");
        return;
    }

    if (eval_expression(argv[1], &ctx, &addr) != 0)
        return;

    if (x_state.byte_count == WASHDBG_INST) {
        switch (ctx) {
        case DEBUG_CONTEXT_SH4:
            x_state.byte_count = 2;
            x_state.disas_mode = true;
            break;
        case DEBUG_CONTEXT_ARM7:
            x_state.byte_count = 4;
            x_state.disas_mode = false; // not implemented yet
            break;
        default:
            washdbg_print_error("unknown context ???\n");
        }
    }

    if (x_state.count > 1024 * 32) {
        washdbg_print_error("too much data\n");
        return;
    }

    x_state.dat = calloc(x_state.byte_count, x_state.count);
    if (!x_state.dat) {
        washdbg_print_error("failed allocation\n");
        return;
    }

    // now do the memory lookup here
    if (debug_read_mem(ctx, x_state.dat, addr,
                       x_state.byte_count * x_state.count) < 0) {
        washdbg_print_error("only a single argument is supported for the x "
                            "command.\n");
        return;
    }

    washdbg_x_set_string();
    x_state.idx = 1;

    cur_state = WASHDBG_STATE_X;
}

static unsigned washdbg_print_x(void) {
    char const *start = x_state.str + x_state.str_pos;
    size_t len = strlen(x_state.str);
    unsigned rem_chars = len - x_state.str_pos;
    if (rem_chars) {
        unsigned n_chars = washdbg_puts(start);
        if (n_chars == rem_chars)
            goto reload;
        else
            x_state.str_pos += n_chars;
    } else {
        goto reload;
    }
    return 1;

reload:
    if (x_state.idx == x_state.count)
        return 0;
    washdbg_x_set_string();
    x_state.idx++;
    return 1;
}

static bool washdbg_is_x_cmd(char const *cmd) {
    // TODO: implement formatted versions, like x/w
    return strcmp(cmd, "x") == 0 ||
        (strlen(cmd) && cmd[0] == 'x' && cmd[1] == '/');
}

void washdbg_core_run_once(void) {
    switch (cur_state) {
    case WASHDBG_STATE_BANNER:
        if (washdbg_print_buffer(&print_banner_state.txt) == 0)
            washdbg_print_context_info();
        break;
    case WASHDBG_STATE_PROMPT:
        if (washdbg_print_buffer(&print_prompt_state.txt) == 0)
            cur_state = WASHDBG_STATE_NORMAL;
        break;
    case WASHDBG_STATE_CMD_CONTINUE:
        if (washdbg_print_buffer(&continue_state.txt) == 0) {
            debug_request_continue();
            cur_state = WASHDBG_STATE_RUNNING;
        }
        break;
    case WASHDBG_STATE_NORMAL:
        washdbg_process_input();
        break;
    case WASHDBG_STATE_BAD_INPUT:
        if (washdbg_print_buffer(&bad_input_state.txt) == 0)
            washdbg_print_prompt();
        break;
    case WASHDBG_STATE_HELP:
        if (washdbg_print_buffer(&help_state.txt) == 0)
            washdbg_print_prompt();
        break;
    case WASHDBG_STATE_CONTEXT_INFO:
        if (washdbg_print_buffer(&context_info_state.txt) == 0)
            washdbg_print_prompt();
        break;
    case WASHDBG_STATE_PRINT_ERROR:
        if (washdbg_print_buffer(&print_error_state.txt) == 0)
            washdbg_print_prompt();
        break;
    case WASHDBG_STATE_ECHO:
        washdbg_state_echo_process();
        break;
    case WASHDBG_STATE_X:
        if (washdbg_print_x() == 0) {
            free(x_state.dat);
            x_state.dat = NULL;
            washdbg_print_prompt();
        }
        break;
    default:
        break;
    }
}

// maximum length of a single argument
#define SINGLE_ARG_MAX 128

// maximum number of arguments
#define MAX_ARG_COUNT 256

static void washdbg_process_input(void) {
    static char cur_line[BUF_LEN];
    int argc = 0;
    char **argv = NULL;
    int arg_no;

    char const *newline_ptr = strchr(in_buf, '\n');
    if (newline_ptr) {
        unsigned newline_idx = newline_ptr - in_buf;

        memset(cur_line, 0, sizeof(cur_line));
        memcpy(cur_line, in_buf, newline_idx);

        if (newline_idx < (BUF_LEN - 1)) {
            size_t chars_to_move = BUF_LEN - newline_idx - 1;
            memmove(in_buf, newline_ptr + 1, chars_to_move);
            in_buf_pos = 0;
        }

        // Now separate the current line out into arguments
        char *token = strtok(cur_line, " \t");
        while (token) {
            if (argc + 1 > MAX_ARG_COUNT) {
                washdbg_print_error("too many arguments\n");
                goto cleanup_args;
            }

            // the + 1 is to add in space for the \0
            size_t tok_len = strlen(token) + 1;

            if (tok_len > SINGLE_ARG_MAX) {
                washdbg_print_error("argument exceeded maximum length.\n");
                goto cleanup_args;
            }

            char *new_arg = (char*)malloc(tok_len * sizeof(char));
            if (!new_arg) {
                washdbg_print_error("Failed allocation.\n");
                goto cleanup_args;
            }

            memcpy(new_arg, token, tok_len * sizeof(char));

            char **new_argv = (char**)realloc(argv, sizeof(char*) * (argc + 1));
            if (!new_argv) {
                washdbg_print_error("Failed allocation.\n");
                goto cleanup_args;
            }

            argv = new_argv;
            argv[argc] = new_arg;
            argc++;

            token = strtok(NULL, " \t");
        }

        char const *cmd;
        if (argc)
            cmd = argv[0];
        else
            cmd = "";

        if (strlen(cmd)) {
            if (washdbg_is_continue_cmd(cmd)) {
                washdbg_do_continue(argc, argv);
            } else if (washdbg_is_exit_cmd(cmd)) {
                washdbg_do_exit(argc, argv);
            } else if (washdbg_is_help_cmd(cmd)) {
                washdbg_do_help(argc, argv);
            } else if (washdbg_is_echo_cmd(cmd)) {
                washdbg_echo(argc, argv);
            } else if (washdbg_is_x_cmd(cmd)) {
                washdbg_x(argc, argv);
            } else {
                washdbg_bad_input(cmd);
            }
        } else {
            washdbg_print_prompt();
        }
    }

cleanup_args:
    for (arg_no = 0; arg_no < argc; arg_no++)
        free(argv[arg_no]);
    free(argv);
}

static int washdbg_puts(char const *txt) {
    return washdbg_tcp_puts(txt);
}

static void washdbg_state_echo_process(void) {
    if (echo_state.cur_arg >= echo_state.argc) {
        if (echo_state.print_space) {
            if (washdbg_puts("\n"))
                echo_state.print_space = false;
            else
                return;
        }
        washdbg_print_prompt();
        int arg_no;
        for (arg_no = 0; arg_no < echo_state.argc; arg_no++)
            free(echo_state.argv[arg_no]);
        free(echo_state.argv);
        memset(&echo_state, 0, sizeof(echo_state));
        return;
    }

    for (;;) {
        if (echo_state.print_space == true) {
            if (washdbg_puts(" "))
                echo_state.print_space = false;
            else
                return;
        }

        char *arg = echo_state.argv[echo_state.cur_arg];
        unsigned arg_len = strlen(arg);
        unsigned arg_pos = echo_state.cur_arg_pos;
        unsigned rem_chars = arg_len - arg_pos;

        if (rem_chars) {
            unsigned n_chars = washdbg_puts(arg + arg_pos);
            if (n_chars == rem_chars) {
                echo_state.cur_arg_pos = 0;
                echo_state.cur_arg++;
                echo_state.print_space = true;
                if (echo_state.cur_arg >= echo_state.argc)
                    return;
            } else {
                echo_state.cur_arg_pos += n_chars;
                return;
            }
        }
    }
}

static unsigned washdbg_print_buffer(struct washdbg_txt_state *state) {
    char const *start = state->txt + state->pos;
    unsigned rem_chars = strlen(state->txt) - state->pos;
    if (rem_chars) {
        unsigned n_chars = washdbg_puts(start);
        if (n_chars == rem_chars)
            return 0;
        else
            state->pos += n_chars;
    } else {
        return 0;
    }
    return strlen(state->txt) - state->pos;
}

static bool is_hex_str(char const *str) {
    if (*str == 0)
        return false; // empty string
    while (*str)
        switch (*str++) {
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
        case 'a':
        case 'A':
        case 'b':
        case 'B':
        case 'c':
        case 'C':
        case 'd':
        case 'D':
        case 'e':
        case 'E':
        case 'f':
        case 'F':
            break;
        default:
            return false;
        }
    return true;
}

static bool is_dec_str(char const *str) {
    if (*str == 0)
        return false; // empty string
    while (*str)
        if (!isdigit(*str++))
            return false;
    return true;
}

static unsigned parse_dec_str(char const *str) {
    size_t len = strlen(str);
    if (!len)
        return 0; // error condition; just ignore it for now
    size_t idx = len - 1;
    unsigned total = 0;
    unsigned scale = 1;
    do {
        unsigned weight = str[idx] - '0';
        total += scale * weight;
        scale *= 10;
    } while (idx--);
    return total;
}

static unsigned parse_hex_str(char const *str) {
    size_t len = strlen(str);
    if (!len)
        return 0; // error condition; just ignore it for now
    size_t idx = len - 1;
    unsigned total = 0;
    unsigned scale = 1;
    do {
        /* unsigned weight = str[idx] - '0'; */
        unsigned weight;
        if (str[idx] >= '0' && str[idx] <= '9')
            weight = str[idx] - '0';
        else if (str[idx] >= 'a' && str[idx] <= 'f')
            weight = str[idx] - 'a' + 10;
        else if (str[idx] >= 'A' && str[idx] <= 'F')
            weight = str[idx] - 'A' + 10;
        else
            weight = 0; // error condition; just ignore it for now
        total += scale * weight;
        scale *= 16;
    } while (idx--);
    printf("%s is %u\n", str, total);
    return total;
}

/*
 * expression format:
 * <ctx>:0xhex_val
 * OR
 * <ctx>:dec_val
 * OR
 * <ctx>:$reg_name
 *
 * ctx can be arm7 or sh4.  If it is not provided, it defaults to the current
 * context.  If the command interprets the value as being a pointer, then ctx
 * indicates whether it points to arm7's memory space or sh4's memory space.
 *
 * If the command does not interpret the value as a pointer, then ctx only
 * matters for the $reg_name form.  However, ctx can still be speciified even
 * though it is useless.
 */
static int eval_expression(char const *expr, enum dbg_context_id *ctx_id, unsigned *out) {
    enum dbg_context_id ctx = debug_current_context();

    char *first_colon = strchr(expr, ':');
    if (first_colon != NULL) {
        unsigned n_chars = first_colon - expr;
        if (n_chars == 3 && toupper(expr[0]) == 'S' &&
            toupper(expr[1]) == 'H' && toupper(expr[2]) == '4') {
            ctx = DEBUG_CONTEXT_SH4;
        } else if (n_chars == 4 && toupper(expr[0]) == 'A' &&
                   toupper(expr[1]) == 'R' && toupper(expr[2]) == 'M' &&
                   toupper(expr[3]) == '7') {
            ctx = DEBUG_CONTEXT_ARM7;
        } else {
            // unrecognized context
            washdbg_print_error("Unknown context\n");
            return -1;
        }

        expr = first_colon + 1;
    }

    *ctx_id = ctx;

    if (strlen(expr) == 0) {
        washdbg_print_error("empty expression\n");
        return -1;
    }

    if (expr[0] == '$') {
        // register
        washdbg_print_error("register expressions are not implemented yet\n");
        return -1;
    } else if (expr[0] == '0' && toupper(expr[1]) == 'X' &&
               is_hex_str(expr + 2)) {
        // hex
        *out = parse_hex_str(expr);
        return 0;
    } else if (is_dec_str(expr)) {
        // decimal
        *out = parse_dec_str(expr);
        return 0;
    } else {
        // error
        washdbg_print_error("unknown expression class\n");
        return -1;
    }
}

static int
parse_fmt_string(char const *str, enum washdbg_byte_count *byte_count_out,
                 unsigned *count_out) {
    bool have_count = false;
    bool have_byte_count = false;
    unsigned byte_count = 4;
    unsigned count = 1;

    bool parsing_digits = false;

    char const *digit_start = NULL;

    if (!str)
        goto the_end;

    while (*str || parsing_digits) {
        if (parsing_digits) {
            if (*str < '0' || *str > '9') {
                parsing_digits = false;
                unsigned n_chars = str - digit_start + 1;
                if (n_chars >= 32)
                    return -1;
                char tmp[32];
                strncpy(tmp, digit_start, sizeof(tmp));
                tmp[31] = '\0';
                if (have_count)
                    return -1;
                have_count = true;
                count = atoi(tmp);

                continue;
            }
        } else {
            switch (*str) {
            case 'w':
                if (have_byte_count)
                    return -1;
                byte_count = WASHDBG_4_BYTE;
                have_byte_count = true;
                break;
            case 'h':
                if (have_byte_count)
                    return -1;
                byte_count = WASHDBG_2_BYTE;
                have_byte_count = true;
                break;
            case 'b':
                if (have_byte_count)
                    return -1;
                byte_count = WASHDBG_1_BYTE;
                have_byte_count = true;
                break;
            case 'i':
                if (have_byte_count)
                    return -1;
                byte_count = WASHDBG_INST;
                have_byte_count = true;
                break;
            case '0':
            case '1':
            case '2':
            case '3':
            case '4':
            case '5':
            case '6':
            case '7':
            case '8':
            case '9':
                parsing_digits = true;
                digit_start = str;
                continue;
            default:
                return -1;
            }
        }
        str++;
    }

    /*
     * This limit is arbitrary, you can increase or decrease it as you'd like.
     * I just put this in there to keep things sane.
     */
    if (count >= 2048) {
        washdbg_print_error("too much data\n");
        return -1;
    }

the_end:

    *count_out = count;
    *byte_count_out = byte_count;

    return 0;
}
