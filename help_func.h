#ifndef SVC_HELP_FUNC_H
#define SVC_HELP_FUNC_H

#include "svc.h"

int files_copy(struct file **dist, struct file **src, int n_files);

void restore_change(struct file **stage, int n_files);

void sort_files(void *helper);

long file_length(char *file_path);

int read_file(char *content, char *file_path, size_t size);

char *calc_cmt_id(void *helper, char *message);

int add_commit(void *helper, char *id, char *message);

void check_changes(void *helper, int check_modification);

struct branch *find_branch(void *helper, char *branch_name);

void restore_files(void *helper);

void commit_copy(struct commit *dist, struct commit *src, char *branch_name);

int check_uncommitted(void *helper);

void free_files(struct file** files, int n_files);
#endif //SVC_HELP_FUNC_H
