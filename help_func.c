#include "help_func.h"

// copy files from src to dist
int files_copy(struct file **dist, struct file **src, int n_files) {
    int tracked_file = 0;
    for (int i = 0; i < n_files; ++i) {
        // copy all properties to a new file and assign that to dist
        struct file *file = malloc(sizeof(struct file));
        file->file_path = strdup(src[i]->file_path);
        file->content = strdup(src[i]->content);
        file->hash = src[i]->hash;
        file->chg_type = src[i]->chg_type;
        dist[i] = file;
        if (file->chg_type >= 0) {
            tracked_file++;
        }
    }
    return tracked_file;
}

// make stage to 0 when not removing. 02 when removing
void restore_change(struct file **stage, int n_files) {
    for (int i = 0; i < n_files; ++i) {
        struct file *file = stage[i];
        if (file->chg_type == -1 || file->chg_type == -2) {
            file->chg_type = -2;
        } else {
            file->chg_type = 0;
        }
    }
}

// sort files in ascending alphabetically
void sort_files(void *helper) {
    struct helper *help = (struct helper *) helper;
    struct branch *cur_br = help->cur_branch;
    struct file *tmp;
    struct file **files = cur_br->stage;
    for (int i = 0; i < cur_br->n_files; i++) {
        for (int j = i + 1; j < cur_br->n_files; j++) {
            int diff = strcasecmp(files[i]->file_path, files[j]->file_path);
            if (diff > 0) {
                // swap the file
                tmp = files[i];
                files[i] = files[j];
                files[j] = tmp;
            }
        }
    }
}

// get the length of a file
long file_length(char *file_path) {
    FILE *f = fopen(file_path, "r");
    if (f == NULL) {
        return -1;
    }
    // get the length of the content inside a file
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    fclose(f);
    return size;
}

// get the content of a file
int read_file(char *content, char *file_path, size_t size) {
    FILE *f = fopen(file_path, "rb+");
    if (f == NULL) {
        // if no file exists at the given file_path
        return FALSE;
    }

    // extract the content of the file
    fread(content, size, 1, f);
    fclose(f);
    // make the last char in content to \0
    content[size] = '\0';
    return TRUE;

}

// calculate the commit id
char *calc_cmt_id(void *helper, char *message) {
    if (message == NULL) {
        return NULL;
    }
    struct helper *help = (struct helper *) helper;
    struct branch *cur_br = help->cur_branch;
    // calculate the commit_id
    int id = 0;
    for (unsigned char *c = (unsigned char *) message; *c != '\0'; ++c) {
        id = (id + *c) % 1000;
    }
    // sort files in ascending alphabetically
    sort_files(helper);
    struct file **files = cur_br->stage;
    int n_changes = 0;
    for (int i = 0; i < cur_br->n_files; ++i) {
        struct file *file = files[i];
        if (file->chg_type == 0 || file->chg_type == -2) {
            continue;
        }
        n_changes++;
        if (file->chg_type == 1) {
            id += 376591;
        } else if (file->chg_type == -1) {
            id += 85973;
        } else {
            id += 9573681;
        }
        for (unsigned char *c = (unsigned char *) (file->file_path); *c != '\0'; ++c) {
            id = (id * (*c % 37)) % 15485863 + 1;
        }

    }
    if (n_changes == 0) {
        return NULL;
    }
    // convert to hexadecimal number
    char *hex = malloc(sizeof(char) * 7);
    sprintf(hex, "%06x", id);
    hex[6] = '\0';
    return hex;
}

// make a commit and add to current branch
int add_commit(void *helper, char *id, char *message) {
    struct helper *help = (struct helper *) helper;
    if (id == NULL) {
        return -1;
    }
    struct branch *cur_br = help->cur_branch;

    if (cur_br->n_commits == cur_br->capacity_commit) {
        cur_br->capacity_commit *= 2;
        cur_br->commits = realloc(cur_br->commits, sizeof(struct commit *) * cur_br->capacity_commit);
    }
    // init the new commit
    struct commit *commit = malloc(sizeof(struct commit));
    commit->commit_id = id;
    commit->br_name = strdup(cur_br->name);
    commit->message = strdup(message);
    commit->files = malloc(sizeof(struct file *) * cur_br->n_files);
    commit->tracked_files = files_copy(commit->files, cur_br->stage, cur_br->n_files);
    commit->n_files = cur_br->n_files;
    commit->detached = FALSE;

    // restore changes in the stage
    restore_change(cur_br->stage, cur_br->n_files);
    if (cur_br->head == NULL) {
        commit->n_parent = 0;
        commit->parent = NULL;
    } else {
        commit->n_parent = 1;
        commit->parent = malloc((sizeof(char *)) * 2);
        commit->parent[0] = cur_br->head->commit_id;
    }
    // add the commit to the branch
    cur_br->commits[cur_br->n_commits] = commit;
    cur_br->n_commits++;
    cur_br->head = commit;

    return 1;
}

// check if there're any changes made by user
void check_changes(void *helper, int check_modification) {
    struct helper *help = (struct helper *) helper;

    for (int i = 0; i < help->cur_branch->n_files; ++i) {
        struct file *file = help->cur_branch->stage[i];
        if (file->chg_type < 0) {
            // if the file have been deleted, don't check that
            return;
        }
        int new_hash = hash_file(helper, file->file_path);
        if (new_hash == -2) {
            // if the file is not found
            if (file->chg_type == 1 || file->chg_type == -2) {
                // if the file is newly added or has been deleted
                // treat it as deleted
                file->chg_type = -2;
            } else {
                // if the file it traced before, treat it as removed
                file->chg_type = -1;
            }
            continue;
        }
        if (check_modification && new_hash != file->hash) {
            // if the file is changed
            file->hash = new_hash;
            if (file->chg_type != 1) {
                // if the file is not newly added
                file->chg_type = 2;
            }
            long size = file_length(file->file_path);
            // read the file
            char content[size + 1];
            read_file(content, file->file_path, size);
            free(file->content);
            file->content = strdup(content);
        }
    }
}

// get the pointer to a branch by its name
struct branch *find_branch(void *helper, char *branch_name) {
    struct helper *help = (struct helper *) helper;
    for (int i = 0; i < help->n_branches; ++i) {
        if (strcmp(help->branches[i]->name, branch_name) == 0) {
            return help->branches[i];
            break;
        }
    }
    return NULL;
}

// restore the files
void restore_files(void *helper){
    struct helper *help = (struct helper *) helper;
    for (int j = 0; j < help->cur_branch->n_files; ++j) {
        struct file *file = help->cur_branch->stage[j];
        if (file->chg_type >= 0) {
            FILE *fp = fopen(file->file_path, "w");
            fputs(file->content, fp);
            fclose(fp);
        }
    }
}

// make a copy of a commit
void commit_copy(struct commit *dist, struct commit *src, char *branch_name) {
    dist->br_name = strdup(branch_name);
    dist->message = strdup(src->message);
    dist->commit_id = strdup(src->commit_id);
    dist->n_files = src->n_files;
    dist->tracked_files = src->tracked_files;
    dist->parent = malloc(sizeof(char *) * 2);
    dist->n_parent = src->n_parent;
    for (int j = 0; j < dist->n_parent; ++j) {
        dist->parent[j] = src->parent[j];
    }
    dist->detached = src->detached;
    dist->files = malloc(sizeof(struct file *) * dist->n_files);
    files_copy(dist->files, src->files, dist->n_files);
}

// check if there's uncommitted changes
int check_uncommitted(void *helper){
    struct helper *help = (struct helper *) helper;
    for (int i = 0; i < help->cur_branch->n_files; ++i) {
        int chg_type = help->cur_branch->stage[i]->chg_type;
        if (chg_type == -1 || chg_type == 1 || chg_type == 2) {
            return TRUE;
        }
    }
    return FALSE;
}

void free_files(struct file** files, int n_files){
    for (int i = 0; i < n_files; ++i) {
        struct file *file = files[i];
        free(file->file_path);
        file->file_path = NULL;
        free(file->content);
        file->content = NULL;
        free(file);
        files[i] = NULL;
    }
    free(files);
    files = NULL;
}