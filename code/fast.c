#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum value {
    UNASSIGNED = -1,
    FALSE = 0,
    TRUE = 1,
};

struct clause {
    int *lits;
    int size;
    int watch[2];
};

struct watch {
    int clause;
    int next;
};

static int n_vars;
static int n_clauses;
static struct clause *clauses;
static int *assigns;
static int *trail;
static int trail_size;
static int *trail_lim;
static int n_levels;
static int *watch_heads;
static struct watch *watches;
static int n_watches;
static int cap_watches;

static int lit_var(int lit) { return lit < 0 ? -lit : lit; }
static int lit_index(int lit) { return lit > 0 ? 2 * lit : 2 * (-lit) + 1; }
static int lit_value(int lit) {
    int value = assigns[lit_var(lit)];
    if (value == UNASSIGNED) return UNASSIGNED;
    return (lit > 0) == value;
}

static void *xrealloc(void *ptr, size_t size) {
    void *ret = realloc(ptr, size);
    if (!ret) {
        perror("realloc");
        exit(2);
    }
    return ret;
}

static void push_lit(struct clause *clause, int lit) {
    clause->lits = xrealloc(clause->lits, sizeof(int) * (clause->size + 1));
    clause->lits[clause->size++] = lit;
}

static void add_watch(int lit, int clause_i) {
    if (n_watches == cap_watches) {
        cap_watches = cap_watches ? cap_watches * 2 : 1024;
        watches = xrealloc(watches, sizeof(watches[0]) * cap_watches);
    }
    int idx = n_watches++;
    watches[idx].clause = clause_i;
    watches[idx].next = watch_heads[lit_index(lit)];
    watch_heads[lit_index(lit)] = idx;
}

static int enqueue(int lit) {
    int var = lit_var(lit);
    int value = lit > 0;
    if (assigns[var] != UNASSIGNED) return assigns[var] == value;
    assigns[var] = value;
    trail[trail_size++] = lit;
    return 1;
}

static void backtrack(int level) {
    while (n_levels > level) {
        int start = trail_lim[--n_levels];
        while (trail_size > start) {
            assigns[lit_var(trail[--trail_size])] = UNASSIGNED;
        }
    }
}

static int propagate(void) {
    for (int q = 0; q < trail_size; q++) {
        int false_lit = -trail[q];
        int *head = &watch_heads[lit_index(false_lit)];

        for (int *watch_p = head; *watch_p != -1;) {
            int watch_i = *watch_p;
            struct clause *c = &clauses[watches[watch_i].clause];
            int watch_pos = c->lits[c->watch[0]] == false_lit ? 0 : 1;
            int other_pos = watch_pos ^ 1;
            int other_lit = c->lits[c->watch[other_pos]];

            if (lit_value(other_lit) == TRUE) {
                watch_p = &watches[watch_i].next;
                continue;
            }

            int replacement = -1;
            for (int i = 0; i < c->size; i++) {
                if (i == c->watch[0] || i == c->watch[1]) continue;
                if (lit_value(c->lits[i]) != FALSE) {
                    replacement = i;
                    break;
                }
            }

            if (replacement != -1) {
                c->watch[watch_pos] = replacement;
                *watch_p = watches[watch_i].next;
                watches[watch_i].next = watch_heads[lit_index(c->lits[replacement])];
                watch_heads[lit_index(c->lits[replacement])] = watch_i;
                continue;
            }

            if (lit_value(other_lit) == FALSE) return 0;
            if (!enqueue(other_lit)) return 0;
            watch_p = &watches[watch_i].next;
        }
    }
    return 1;
}

static int choose_var(void) {
    for (int v = 1; v <= n_vars; v++) {
        if (assigns[v] == UNASSIGNED) return v;
    }
    return 0;
}

static int solve(void) {
    if (!propagate()) return 0;

    int var = choose_var();
    if (!var) return 1;

    trail_lim[n_levels++] = trail_size;
    if (enqueue(var) && solve()) return 1;

    backtrack(n_levels - 1);
    trail_lim[n_levels++] = trail_size;
    if (enqueue(-var) && solve()) return 1;

    backtrack(n_levels - 1);
    return 0;
}

static void parse(FILE *f) {
    char tok[256];
    int found_header = 0;
    while (fscanf(f, "%255s", tok) == 1) {
        if (tok[0] == 'c') {
            int ch;
            while ((ch = getc(f)) != '\n' && ch != EOF) {}
            continue;
        }
        if (strcmp(tok, "p") == 0) {
            found_header = 1;
            break;
        }
        fprintf(stderr, "unexpected token before DIMACS header: %s\n", tok);
        exit(2);
    }

    if (!found_header) {
        fprintf(stderr, "missing DIMACS header\n");
        exit(2);
    }

    if (fscanf(f, "%255s %d %d", tok, &n_vars, &n_clauses) != 3 ||
        strcmp(tok, "cnf") != 0 || n_vars < 0 || n_clauses < 0) {
        fprintf(stderr, "invalid DIMACS header\n");
        exit(2);
    }

    clauses = calloc(n_clauses, sizeof(clauses[0]));
    assigns = malloc(sizeof(assigns[0]) * (n_vars + 1));
    trail = malloc(sizeof(trail[0]) * (n_vars + 1));
    trail_lim = malloc(sizeof(trail_lim[0]) * (n_vars + 1));
    watch_heads = malloc(sizeof(watch_heads[0]) * (2 * (n_vars + 1) + 2));
    if (!clauses || !assigns || !trail || !trail_lim || !watch_heads) {
        perror("malloc");
        exit(2);
    }
    for (int i = 0; i <= n_vars; i++) assigns[i] = UNASSIGNED;
    for (int i = 0; i < 2 * (n_vars + 1) + 2; i++) watch_heads[i] = -1;

    int clause_i = 0;
    while (fscanf(f, "%255s", tok) == 1) {
        if (tok[0] == 'c') {
            int ch;
            while ((ch = getc(f)) != '\n' && ch != EOF) {}
            continue;
        }

        char *end = NULL;
        long parsed = strtol(tok, &end, 10);
        if (!end || *end != '\0') {
            fprintf(stderr, "invalid literal token: %s\n", tok);
            exit(2);
        }
        if (parsed == 0) {
            clause_i++;
            if (clause_i > n_clauses) {
                fprintf(stderr, "too many clauses in DIMACS input\n");
                exit(2);
            }
            continue;
        }
        if (clause_i >= n_clauses) {
            fprintf(stderr, "literal appears after declared clause count\n");
            exit(2);
        }
        int lit = (int)parsed;
        if (lit_var(lit) > n_vars) {
            fprintf(stderr, "literal out of range: %d\n", lit);
            exit(2);
        }
        push_lit(&clauses[clause_i], lit);
    }

    if (clause_i != n_clauses) {
        fprintf(stderr, "DIMACS header declared %d clauses, found %d\n",
                n_clauses, clause_i);
        exit(2);
    }
}

static int init_formula(void) {
    for (int i = 0; i < n_clauses; i++) {
        struct clause *c = &clauses[i];
        if (c->size == 0) return 0;
        c->watch[0] = 0;
        c->watch[1] = c->size > 1 ? 1 : 0;

        if (c->size == 1) {
            if (!enqueue(c->lits[0])) return 0;
        } else {
            add_watch(c->lits[c->watch[0]], i);
            add_watch(c->lits[c->watch[1]], i);
        }
    }
    return 1;
}

static int print_sat(void) {
    printf("SAT\n");
    for (int v = 1; v <= n_vars; v++) {
        int value = assigns[v] == TRUE ? v : -v;
        printf("%d ", value);
    }
    printf("0\n");
    return 0;
}

int main(int argc, char **argv) {
    FILE *f = stdin;
    if (argc == 2) {
        f = fopen(argv[1], "r");
        if (!f) {
            perror(argv[1]);
            return 2;
        }
    } else if (argc > 2) {
        fprintf(stderr, "usage: %s [file.cnf]\n", argv[0]);
        return 2;
    }

    parse(f);
    if (!init_formula() || !solve()) {
        printf("UNSAT\n");
        return 0;
    }
    return print_sat();
}
