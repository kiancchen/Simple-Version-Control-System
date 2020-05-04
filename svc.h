#ifndef svc_h
#define svc_h
#define TRUE 1
#define FALSE 0

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

typedef struct resolution {
    // NOTE: DO NOT MODIFY THIS STRUCT
    char *file_name;
    char *resolved_file;
} resolution;

struct file {
    char *file_path;
    char *content;
    int hash;
    int chg_type; // change of type:
    // -2 for not tracking; -1 for deletion;
    // 0 for no changes; 1 for addition; 2 for modification;
};

struct commit {
    char *commit_id;
    char *br_name;
    char *message;
    struct file **files;
    int n_files;
    int tracked_files;
    char **parent;
    int n_parent;
    int detached;
};

struct branch {
    char *name;
    struct commit *head;
    // all commits
    struct commit **commits;
    int n_commits;
    int n_detached;
    int capacity_commit;
    // all tracked files
    struct file **stage;
    int n_files;
    int capacity_file;
};

struct helper {
    struct branch *cur_branch;
    struct branch **branches;
    int n_branches;
    int capacity_br;
};

void *svc_init(void);

void cleanup(void *helper);

int hash_file(void *helper, char *file_path);

char *svc_commit(void *helper, char *message);

void *get_commit(void *helper, char *commit_id);

char **get_prev_commits(void *helper, void *commit, int *n_prev);

void print_commit(void *helper, char *commit_id);

int svc_branch(void *helper, char *branch_name);

int svc_checkout(void *helper, char *branch_name);

char **list_branches(void *helper, int *n_branches);

int svc_add(void *helper, char *file_name);

int svc_rm(void *helper, char *file_name);

int svc_reset(void *helper, char *commit_id);

char *svc_merge(void *helper, char *branch_name, resolution *resolutions, int n_resolutions);

#endif

