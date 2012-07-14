#include <assert.h>
#include <fcntl.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include "libvm.h"


// ---------------------------------------------------------------------------
// Public
// ---------------------------------------------------------------------------

struct state *new(long input_length, const char *input) {
    DEBUG_ASSERT(input);
    long world_w, world_h, world_length;
    struct state *s;
    scan_input(input_length, input, &world_w, &world_h, &world_length);
    if (!(s = malloc(sizeof(struct state) + world_length)))
        PERROR_EXIT("malloc");
    memset(s, 0, sizeof(struct state));
    s->world_w = world_w;
    s->world_h = world_h;
    s->robot_waterproofing = DEFAULT_ROBOT_WATERPROOFING;
    s->condition = C_NONE;
    s->world_length = world_length;
    copy_input(s, input_length, input);
    return s;
}

struct state *new_from_file(const char *path) {
    DEBUG_ASSERT(path);
    int fd;
    struct stat info;
    char *input;
    struct state *s;
    if ((fd = open(path, O_RDONLY)) == -1)
        PERROR_EXIT("open");
    if (fstat(fd, &info) == -1)
        PERROR_EXIT("fstat");
    if ((input = mmap(0, info.st_size, PROT_READ, MAP_SHARED, fd, 0)) == (char *)-1)
        PERROR_EXIT("mmap");
    s = new(info.st_size, input);
    munmap(input, info.st_size);
    close(fd);
    return s;
}

struct state *copy(const struct state *s0) {
    DEBUG_ASSERT(s0);
    struct state *s;
    if (!(s = malloc(sizeof(struct state) + s0->world_length)))
        PERROR_EXIT("malloc");
    memcpy(s, s0, sizeof(struct state) + s0->world_length);
    return s;
}

bool equal(const struct state *s1, const struct state *s2) {
    DEBUG_ASSERT(s1 && s2);
    if (s1->world_length != s2->world_length)
        return false;
    return !memcmp(s1, s2, sizeof(struct state) + s1->world_length);
}


void dump(const struct state *s) {
    DEBUG_ASSERT(s);
    DEBUG_LOG("world_size               = (%ld, %ld)\n", s->world_w, s->world_h);
    DEBUG_LOG("robot_point              = (%ld, %ld)\n", s->robot_x, s->robot_y);
    DEBUG_LOG("lift_point               = (%ld, %ld)\n", s->lift_x, s->lift_y);
    DEBUG_LOG("water_level              = %ld\n", s->water_level);
    DEBUG_LOG("flooding_rate            = %ld\n", s->flooding_rate);
    DEBUG_LOG("robot_waterproofing      = %ld\n", s->robot_waterproofing);
    DEBUG_LOG("used_robot_waterproofing = %ld\n", s->used_robot_waterproofing);
    DEBUG_LOG("lambda_count             = %ld\n", s->lambda_count);
    DEBUG_LOG("collected_lambda_count   = %ld\n", s->collected_lambda_count);
    DEBUG_LOG("move_count               = %ld\n", s->move_count);
    DEBUG_LOG("score                    = %ld\n", s->score);
    DEBUG_LOG("condition                = %c\n", s->condition);
    DEBUG_LOG("world_length             = %ld\n", s->world_length);
    printf("%ld\n%s", s->score, s->world);
}


void get_world_size(const struct state *s, long *out_world_w, long *out_world_h) {
    DEBUG_ASSERT(s && out_world_w && out_world_h);
    *out_world_w = s->world_w;
    *out_world_h = s->world_h;
}

void get_robot_point(const struct state *s, long *out_robot_x, long *out_robot_y) {
    DEBUG_ASSERT(s && out_robot_x && out_robot_y);
    *out_robot_x = s->robot_x;
    *out_robot_y = s->robot_y;
}

void get_lift_point(const struct state *s, long *out_lift_x, long *out_lift_y) {
    DEBUG_ASSERT(s && out_lift_x && out_lift_y);
    *out_lift_x = s->lift_x;
    *out_lift_y = s->lift_y;
}

long get_water_level(const struct state *s) {
    DEBUG_ASSERT(s);
    return s->water_level;
}

long get_flooding_rate(const struct state *s) {
    DEBUG_ASSERT(s);
    return s->flooding_rate;
}

long get_robot_waterproofing(const struct state *s) {
    DEBUG_ASSERT(s);
    return s->robot_waterproofing;
}

long get_used_robot_waterproofing(const struct state *s) {
    DEBUG_ASSERT(s);
    return s->used_robot_waterproofing;
}

long get_lambda_count(const struct state *s) {
    DEBUG_ASSERT(s);
    return s->lambda_count;
}

long get_collected_lambda_count(const struct state *s) {
    DEBUG_ASSERT(s);
    return s->collected_lambda_count;
}

long get_move_count(const struct state *s) {
    DEBUG_ASSERT(s);
    return s->move_count;
}

long get_score(const struct state *s) {
    DEBUG_ASSERT(s);
    return s->score;
}

char get_condition(const struct state *s) {
    DEBUG_ASSERT(s);
    return s->condition;
}

inline char get(const struct state *s, long x, long y) {
    DEBUG_ASSERT(s);
    long i;
    i = unmake_point(s, x, y);
    return s->world[i];
}


struct state *make_one_move(const struct state *s0, char move) {
    DEBUG_ASSERT(s0);
    struct state *s;
    s = copy(s0);
    if (s->condition == C_NONE && is_valid_move(move)) {
        execute_move(s, move);
        if (s->condition == C_NONE) {
            struct state *s1;
            s1 = copy(s);
            update_world(s1, s);
            free(s);
            s = s1;
        }
    }
    return s;
}

struct state *make_moves(const struct state *s0, const char *moves) {
    DEBUG_ASSERT(s0 && moves);
    struct state *s;
    s = copy(s0);
    while (s->condition == C_NONE && is_valid_move(*moves)) {
        execute_move(s, *moves);
        if (s->condition == C_NONE) {
            struct state *s1;
            s1 = copy(s);
            update_world(s1, s);
            free(s);
            s = s1;
        }
        moves++;
    }
    return s;
}


// ---------------------------------------------------------------------------
// Private
// ---------------------------------------------------------------------------

void scan_input(long input_length, const char *input, long *out_world_w, long *out_world_h, long *out_world_length) {
    DEBUG_ASSERT(input && out_world_w && out_world_h);
    long world_w = 0, world_h = 0, w = 0, i;
    for (i = 0; i < input_length; i++) {
        if (input[i] != '\n')
            w++;
        if (i == input_length - 1 || input[i] == '\n') {
            if (w == 0)
                break;
            if (w > world_w)
                world_w = w;
            world_h++;
            w = 0;
        }
    }
    *out_world_w = world_w;
    *out_world_h = world_h;
    *out_world_length = (world_w + 1) * world_h + 1;
}


enum {
    K_NONE,
    K_WATER,
    K_FLOODING,
    K_WATERPROOF,
    K_INVALID
};

#define MAX_TOKEN_SIZE 16

void copy_input_metadata(struct state *s, long input_length, const char *input) {
    DEBUG_ASSERT(s && input);
    char key = K_NONE, token[MAX_TOKEN_SIZE + 1];
    long w = 0, i;
    for (i = 0; i < input_length; i++) {
        if (input[i] != ' ' && input[i] != '\n')
            w++;
        if (i == input_length - 1 || input[i] == ' ' || input[i] == '\n') {
            if (w == 0)
                continue;
            if (w > MAX_TOKEN_SIZE)
                w = MAX_TOKEN_SIZE;
            memcpy(token, input + i - w, w);
            token[w] = 0;
            if (key == K_NONE) {
                if (!strcmp(token, "Water"))
                    key = K_WATER;
                else if (!strcmp(token, "Flooding"))
                    key = K_FLOODING;
                else if (!strcmp(token, "Waterproof"))
                    key = K_WATERPROOF;
                else {
                    key = K_INVALID;
                    DEBUG_LOG("found invalid metadata key '%s'\n", token);
                }
            } else {
                if (key == K_WATER)
                    s->water_level = atoi(token);
                else if (key == K_FLOODING)
                    s->flooding_rate = atoi(token);
                else if (key == K_WATERPROOF)
                    s->robot_waterproofing = atoi(token);
                key = K_NONE;
            }
            w = 0;
        }
    }
}

void copy_input(struct state *s, long input_length, const char *input) {
    DEBUG_ASSERT(s && input);
    long w = 0, h = 0, j = 0, i;
    for (i = 0; i < input_length; i++) {
        if (input[i] != '\n') {
            if (input[i] == O_ROBOT)
                make_point(s, w, h, &s->robot_x, &s->robot_y);
            else if (input[i] == O_LAMBDA)
                s->lambda_count++;
            else if (input[i] == O_CLOSED_LIFT)
                make_point(s, w, h, &s->lift_x, &s->lift_y);
            s->world[j++] = input[i];
            w++;
        }
        if (i == input_length - 1 || input[i] == '\n') {
            if (w == 0)
                break;
            while (w < s->world_w) {
                s->world[j++] = O_EMPTY;
                w++;
            }
            s->world[j++] = '\n';
            h++;
            w = 0;
        }
    }
    i++;
    s->world[j] = 0;
    copy_input_metadata(s, input_length - i, input + i);
}


inline void put(struct state *s, long x, long y, char object) {
    DEBUG_ASSERT(s);
    long i;
    i = unmake_point(s, x, y);
    s->world[i] = object;
}


void move_robot(struct state *s, long x, long y) {
    DEBUG_ASSERT(s);
    DEBUG_ASSERT(get(s, x, y) == O_EMPTY || get(s, x, y) == O_EARTH || get(s, x, y) == O_LAMBDA || get(s, x, y) == O_OPEN_LIFT || (get(s, x, y) == O_ROCK && get(s, x + x - s->robot_x, y) == O_EMPTY));
    put(s, s->robot_x, s->robot_y, O_EMPTY);
    s->robot_x = x;
    s->robot_y = y;
    put(s, x, y, O_ROBOT);
    DEBUG_LOG("robot moved to (%ld, %ld)\n", x, y);
    if (s->used_robot_waterproofing && y > s->water_level) {
        s->used_robot_waterproofing = 0;
        DEBUG_LOG("robot waterproofing restored\n");
    }
}

void collect_lambda(struct state *s) {
    DEBUG_ASSERT(s);
    DEBUG_ASSERT(s->collected_lambda_count < s->lambda_count);
    DEBUG_ASSERT(get(s, s->lift_x, s->lift_y) == O_CLOSED_LIFT);
    s->collected_lambda_count++;
    s->score += 25;
    DEBUG_LOG("robot collected lambda\n");
}

void execute_move(struct state *s, char move) {
    DEBUG_ASSERT(s);
    DEBUG_ASSERT(s->condition == C_NONE);
    DEBUG_ASSERT(is_valid_move(move));
    if (move == M_LEFT || move == M_RIGHT || move == M_UP || move == M_DOWN) {
        long x, y;
        char object;
        x = s->robot_x;
        y = s->robot_y;
        if (move == M_LEFT)
            x = s->robot_x - 1;
        else if (move == M_RIGHT)
            x = s->robot_x + 1;
        else if (move == M_UP)
            y = s->robot_y + 1;
        else if (move == M_DOWN)
            y = s->robot_y - 1;
        object = get(s, x, y);
        if (object == O_EMPTY || object == O_EARTH)
            move_robot(s, x, y);
        else if (object == O_LAMBDA) {
            move_robot(s, x, y);
            collect_lambda(s);
        } else if (object == O_OPEN_LIFT) {
            move_robot(s, x, y);
            s->score += s->collected_lambda_count * 50;
            s->condition = C_WIN;
            DEBUG_LOG("robot won\n");
        } else if (object == O_ROCK && move == M_LEFT && get(s, x - 1, y) == O_EMPTY) {
            move_robot(s, x, y);
            put(s, x - 1, y, O_ROCK);
            DEBUG_LOG("robot pushed rock from (%ld, %ld) to (%ld, %ld)\n", x, y, x - 1, y);
        } else if (object == O_ROCK && move == M_RIGHT && get(s, x + 1, y) == O_EMPTY) {
            move_robot(s, x, y);
            put(s, x + 1, y, O_ROCK);
            DEBUG_LOG("robot pushed rock from (%ld, %ld) to (%ld, %ld\n", x, y, x + 1, y);
        }
        else
            DEBUG_LOG("robot attempted invalid move '%c' from (%ld, %ld) to (%ld, %ld) which is '%c'\n", move, s->robot_x, s->robot_y, x, y, object);
        s->move_count++;
        s->score--;
    } else if (move == M_WAIT) {
        DEBUG_LOG("robot waited\n");
        s->move_count++;
        s->score--;
    } else if (move == M_ABORT) {
        s->score += s->collected_lambda_count * 25;
        s->condition = C_ABORT;
        DEBUG_LOG("robot aborted\n");
    }
}

void update_world(struct state *s, const struct state *s0) {
    DEBUG_ASSERT(s && s0);
    DEBUG_ASSERT(s->condition == C_NONE);
    long x, y;
    for (y = 1; y <= s->world_h; y++) {
        for (x = 1; x <= s->world_w; x++) {
            char object;
            object = get(s0, x, y);
            if (object == O_ROCK && get(s0, x, y - 1) == O_EMPTY) {
                put(s, x, y, O_EMPTY);
                put(s, x, y - 1, O_ROCK);
                if (s0->robot_x == x && s0->robot_y == y - 2) {
                    s->condition = C_LOSE;
                    DEBUG_LOG("robot lost by crushing\n");
                }
            } else if (object == O_ROCK && get(s0, x, y - 1) == O_ROCK && get(s0, x + 1, y) == O_EMPTY && get(s0, x + 1, y - 1) == O_EMPTY) {
                put(s, x, y, O_EMPTY);
                put(s, x + 1, y - 1, O_ROCK);
                if (s0->robot_x == x + 1 && s0->robot_y == y - 2) {
                    s->condition = C_LOSE;
                    DEBUG_LOG("robot lost by crushing\n");
                }
            } else if (object == O_ROCK && get(s0, x, y - 1) == O_ROCK && (get(s0, x + 1, y) != O_EMPTY || get(s0, x + 1, y - 1) != O_EMPTY) && get(s0, x - 1, y) == O_EMPTY && get(s0, x - 1, y - 1) == O_EMPTY) {
                put(s, x, y, O_EMPTY);
                put(s, x - 1, y - 1, O_ROCK);
                if (s0->robot_x == x - 1 && s0->robot_y == y - 2) {
                    s->condition = C_LOSE;
                    DEBUG_LOG("robot lost by crushing\n");
                }
            } else if (object == O_ROCK && get(s0, x, y - 1) == O_LAMBDA && get(s0, x + 1, y) == O_EMPTY && get(s0, x + 1, y - 1) == O_EMPTY) {
                put(s, x, y, O_EMPTY);
                put(s, x + 1, y - 1, O_ROCK);
                if (s0->robot_x == x + 1 && s0->robot_y == y - 2) {
                    s->condition = C_LOSE;
                    DEBUG_LOG("robot lost by crushing\n");
                }
            } else if (object == O_CLOSED_LIFT && s0->collected_lambda_count == s0->lambda_count) {
                put(s, x, y, O_OPEN_LIFT);
                DEBUG_LOG("robot opened lift\n");
            }
        }
    }
    if (s0->robot_y <= s->water_level) {
        DEBUG_LOG("robot is underwater\n");
        s->used_robot_waterproofing++;
        if (s->used_robot_waterproofing > s->robot_waterproofing) {
            s->condition = C_LOSE;
            DEBUG_LOG("robot lost by drowning\n");
        }
    }
    if (s->flooding_rate && !(s->move_count % s->flooding_rate)) {
        s->water_level++;
        DEBUG_LOG("robot increased water level to %ld\n", s->water_level);
    }
}
