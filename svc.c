#include "svc.h"
#include "help_func.h"

// init the helper
void *svc_init(void) {
    struct helper *helper = malloc(sizeof(struct helper));
    // init branches
    helper->branches = malloc(sizeof(struct branch *) * 8);
    helper->capacity_br = 8;
    helper->n_branches = 0;

    // init the master branch
    struct branch *master = malloc(sizeof(struct branch));
    helper->cur_branch = master;
    helper->branches[0] = master;
    helper->n_branches++;
    helper->branches[0]->name = strdup("master");
    master->commits = malloc(sizeof(struct commit *) * 8);
    master->n_commits = 0;
    master->capacity_commit = 8;
    master->head = NULL;
    master->n_detached = 0;

    // init the stage of master
    master->stage = malloc(sizeof(struct file *) * 8);
    master->capacity_file = 8;
    master->n_files = 0;

    return helper;
}

// calculate the hash value
int hash_file(void *helper, char *file_path) {
    helper = NULL;
    if (file_path == NULL) {
        return -1;
    }

    FILE *fp = fopen(file_path, "rb+");
    // if the file not found
    if (!fp) {
        return -2;
    }
    fclose(fp);

    int hash = 0;
    for (char *c = file_path; *c != '\0'; c++) {
        unsigned char *uc = (unsigned char *) c;
        int a = *uc;
        hash = (hash + a) % 1000;
    }

    while (!feof(fp)) {
        int c = fgetc(fp);
        hash = (hash + c) % 2000000000;
    }
    hash += 1; // add 1 which is subtracted due to EOF (-1)

    return hash;
}

//  commit to current branch
char *svc_commit(void *helper, char *message) {
    check_changes(helper, TRUE);

    char *hex = calc_cmt_id(helper, message);
    if (hex == NULL) {
        return NULL;
    }
    add_commit(helper, hex, message);

    return hex;
}

// get the pointer to a commit by its id
void *get_commit(void *helper, char *commit_id) {
    if (commit_id == NULL) {
        return NULL;
    }
    struct helper *help = (struct helper *) helper;
    for (int i = 0; i < help->n_branches; ++i) {
        struct branch *br = help->branches[i];
        for (int j = 0; j < br->n_commits; ++j) {
            struct commit *cmt = br->commits[j];
            if (strcmp(cmt->commit_id, commit_id) == 0) {
                return cmt;
            }
        }
    }

    return NULL;
}

// get the previous commit
char **get_prev_commits(void *helper, void *commit, int *n_prev) {
    helper = NULL;
    if (n_prev == NULL) {
        return NULL;
    }
    if (commit == NULL) {
        *n_prev = 0;
        return NULL;
    }
    struct commit *cmt = (struct commit *) commit;

    *n_prev = cmt->n_parent;
    if (*n_prev == 0) {
        return NULL;
    }
    char **parent = malloc(sizeof(char *) * *n_prev);
    for (int i = 0; i < *n_prev; ++i) {
        parent[i] = cmt->parent[i];
    }
    return parent;
}

void print_commit(void *helper, char *commit_id) {

    struct commit *commit = (struct commit *) get_commit(helper, commit_id);
    if (commit == NULL) {
        printf("Invalid commit id\n");
        return;
    }

    printf("%s [%s]: %s\n", commit_id, commit->br_name, commit->message);
    for (int i = 0; i < commit->n_files; ++i) {
        struct file *file = commit->files[i];
        if (file->chg_type == 0 || file->chg_type == -2) {
            continue;
        }
        char sign;
        if (file->chg_type == -1) {
            sign = '-';
        } else if (file->chg_type == 1) {
            sign = '+';
        } else {
            sign = '/';
        }
        printf("    %c %s\n", sign, file->file_path);
    }
    printf("\n    Tracked files (%d):\n", commit->tracked_files);
    for (int i = 0; i < commit->n_files; ++i) {
        struct file *file = commit->files[i];
        if (file->chg_type >= 0) {
            printf("    [%10d] %s\n", file->hash, file->file_path);
        }
    }
}

// make a new branch
int svc_branch(void *helper, char *branch_name) {

    if (branch_name == NULL) {
        return -1;
    }
    // if the branch_name is invalid
    for (char *c = branch_name; *c != '\0'; c++) {
        if (!(isalnum(*c) || *c == '-' || *c == '_' || *c == '/')) {
            return -1;
        }
    }
    struct helper *help = (struct helper *) helper;
    // if the branch with this name already exits
    for (int i = 0; i < help->n_branches; ++i) {
        if (strcmp(help->branches[i]->name, branch_name) == 0) {
            return -2;
        }
    }

    // If some changes are uncommitted
    if (check_uncommitted(helper)){
        return -3;
    }

    // If the branches full, expand it
    if (help->n_branches == help->capacity_br) {
        help->capacity_br *= 2;
        help->branches = realloc(help->branches, sizeof(struct branch *) * help->capacity_br);
    }
    // add the new branch to the SVC
    struct branch *cur_br = help->cur_branch;
    struct branch *branch = malloc(sizeof(struct branch));
    help->branches[help->n_branches] = branch;
    branch->name = strdup(branch_name);
    branch->head = cur_br->head;
    // copy files
    branch->n_files = cur_br->n_files;
    branch->capacity_file = cur_br->capacity_file;
    branch->stage = malloc(sizeof(struct file *) * branch->capacity_file);
    files_copy(branch->stage, cur_br->stage, cur_br->n_files);
    restore_change(cur_br->stage, cur_br->n_files);
    // copy commits
    branch->n_commits = cur_br->n_commits;
    branch->capacity_commit = cur_br->capacity_commit;
    branch->commits = malloc(sizeof(struct commit *) * branch->capacity_commit);
    for (int i = 0; i < branch->n_commits; ++i) {
        branch->commits[i] = malloc(sizeof(struct commit));
        commit_copy(branch->commits[i], cur_br->commits[i], branch_name);
    }
    branch->n_detached = 0;
    help->n_branches++;

    return 0;
}

// change the head to the specific branch
int svc_checkout(void *helper, char *branch_name) {
    if (branch_name == NULL) {
        return -1;
    }
    struct helper *help = (struct helper *) helper;

    // If some changes are not committed
    if (check_uncommitted(helper)) {
        return -2;
    }

    struct branch* target = find_branch(helper, branch_name);
    if (target == NULL){
        // if the branch does not exit
        return -1;
    }
    help->cur_branch = target;
    restore_files(helper);
    return 0; //successfully

}

char **list_branches(void *helper, int *n_branches) {
    if (n_branches == NULL) {
        return NULL;
    }
    struct helper *help = (struct helper *) helper;
    *n_branches = help->n_branches;
    char **br_names = malloc(sizeof(char *) * *n_branches);
    for (int i = 0; i < *n_branches; ++i) {
        printf("%s\n", help->branches[i]->name);
        br_names[i] = help->branches[i]->name;
    }
    return br_names;
}

// add the file to stage in current branch
int svc_add(void *helper, char *file_name) {
    struct helper *help = (struct helper *) helper;
    struct branch *cur_br = help->cur_branch;

    if (helper == NULL || file_name == NULL) {
        return -1;
    }
    for (int i = 0; i < cur_br->n_files; ++i) {
        if (strcmp(cur_br->stage[i]->file_path, file_name) == 0) {
            if (cur_br->stage[i]->chg_type == -1 || cur_br->stage[i]->chg_type == -2) {
                // the file added but removed
                cur_br->stage[i]->chg_type = 1;
                return hash_file(helper, file_name);
            } else {
                //the file exits now
                return -2;
            }
        }
    }
    // if file never added to the system

    // get the length of the file
    long size = file_length(file_name);
    // if file not found
    if (size == -1) {
        return -3;
    }
    // read the file
    char content[size + 1];
    read_file(content, file_name, size);
    content[size] = '\0';

    // create a file object
    struct file *file = malloc(sizeof(struct file));
    file->file_path = strdup(file_name);
    file->content = strdup(content);

    int hash = hash_file(helper, file_name);
    file->hash = hash;
    file->chg_type = 1; // change type is addition
    // add the file to stage
    if (cur_br->n_files >= cur_br->capacity_file) {
        cur_br->capacity_file *= 2;
        cur_br->stage = realloc(cur_br->stage, cur_br->capacity_file);
    }
    cur_br->stage[cur_br->n_files] = file;
    cur_br->n_files++;

    // store this change
    return hash;
}

// remove the file from the stage of current branch
int svc_rm(void *helper, char *file_name) {
    if (file_name == NULL) {
        return -1;
    }

    struct helper *help = (struct helper *) helper;
    struct branch *cur_br = help->cur_branch;
    for (int i = 0; i < cur_br->n_files; ++i) {
        if (strcmp(cur_br->stage[i]->file_path, file_name) == 0) {
            if (cur_br->stage[i]->chg_type < 0) {
                return -2;
            }
            cur_br->stage[i]->chg_type = -1;
            return cur_br->stage[i]->hash;
        }
    }

    return -2;
}

// restore all changes to a specific commit
int svc_reset(void *helper, char *commit_id) {
    if (commit_id == NULL) {
        return -1;
    }
    struct helper *help = (struct helper *) helper;
    struct branch *cur_br = help->cur_branch;
    // look for the commit
    int index = -1;
    for (int i = 0; i < cur_br->n_commits; ++i) {
        struct commit *commit = cur_br->commits[i];
        if (strcmp(commit->commit_id, commit_id) == 0) {
            index = i;
            break;
        }
    }
    if (index == -1) {
        return -2;
    }

    // free the current stage and copy from the source
    free_files(cur_br->stage, cur_br->n_files);
    cur_br->head = cur_br->commits[index];
    cur_br->n_files = cur_br->head->n_files;
    cur_br->stage = malloc(sizeof(struct file*) * cur_br->n_files);
    files_copy(cur_br->stage, cur_br->head->files, cur_br->head->n_files);
    restore_change(cur_br->stage, cur_br->n_files);
    cur_br->capacity_file = 8;

    // restore files
    restore_files(helper);

    // detach commits afterwards
    for (int i = index + 1; i < cur_br->n_commits; ++i) {
        struct commit *commit = cur_br->commits[i];
        commit->detached = TRUE;
        cur_br->n_detached++;
    }
    return 0;
}

char *svc_merge(void *helper, char *branch_name, struct resolution *resolutions, int n_resolutions) {
    struct helper *help = (struct helper *) helper;
    if (branch_name == NULL) {
        printf("Invalid branch name\n");
        return NULL;
    }
    // look for the branch
    struct branch *merged_br = find_branch(helper, branch_name);
    if (merged_br == NULL) {
        printf("Branch not found\n");
        return NULL;
    }

    // check if it's the current checked out branch
    if (help->cur_branch == merged_br) {
        printf("Cannot merge a branch with itself\n");
        return NULL;
    }
    struct branch *cur_br = help->cur_branch;

    // check if there's changes uncommitted
    check_changes(helper, FALSE);
    if (check_uncommitted(helper)) {
        printf("Changes must be committed\n");
        return NULL;
    }
    // make the merge message
    char message[51] = "Merged branch ";
    strcat(message, branch_name);

    int n_files = cur_br->n_files;
    for (int i = 0; i < merged_br->n_files; ++i) {
        struct file *m_f = merged_br->stage[i];
        if (m_f->chg_type < 0) {
            // if the file does not exit, go to the next
            continue;
        }
        int found = FALSE; // found the file with same name
        for (int j = 0; j < n_files; ++j) {
            struct file *file = cur_br->stage[j];
            if (strcmp(file->file_path, m_f->file_path) == 0) {
                found = TRUE;
                int identical = FALSE; // files have same hash value
                int hasRes = FALSE; // files have a resolution
                if (file->hash == m_f->hash) {
                    identical = TRUE;
                }

                for (int k = 0; k < n_resolutions; ++k) {
                    if (resolutions[k].resolved_file == NULL || resolutions[k].file_name == NULL) {
                        continue;
                    }
                    // look for the resolution for this conflicting file
                    if (strcmp(resolutions[k].file_name, file->file_path) == 0) {
                        if (resolutions[k].resolved_file == NULL) {
                            break;
                        }
                        hasRes = TRUE;
                        if (hash_file(helper, resolutions[k].resolved_file) == file->hash) {
                            // if the resolved file is identical to the file
                            // do not make changes
                            break;
                        }

                        long size = file_length(resolutions[k].resolved_file);
                        // read the file
                        char content[size + 1];
                        read_file(content, resolutions[k].resolved_file, size);
                        content[size] = '\0';
                        // write the resolution to the file
                        FILE *fp = fopen(file->file_path, "w");
                        fputs(content, fp);
                        fclose(fp);
                        // modify the file
                        free(file->content);
                        file->content = NULL;
                        file->content = strdup(content);
                        file->chg_type = 2;
                        file->hash = hash_file(helper, file->file_path);
                        break;
                    }
                }
                // if conflicting files do not have resolutions, delete them
                if (!identical && !hasRes) {
                    file->chg_type = -2;
                }
            }
        }
        if (!found) {
            svc_add(helper, m_f->file_path);
        }
    }

    char *cmt_id = svc_commit(helper, message);
    cur_br->commits[cur_br->n_commits - 1]->n_parent = 2;
    cur_br->commits[cur_br->n_commits - 1]->parent[1] = merged_br->head->commit_id;

    printf("Merge successful\n");
    return cmt_id;

}

void cleanup(void *helper) {
    struct helper *help = (struct helper *) helper;
    // free branches
    for (int i = 0; i < help->n_branches; ++i) {
        struct branch *branch = help->branches[i];
        // free commits
        for (int j = 0; j < branch->n_commits; ++j) {
            struct commit *commit = branch->commits[j];
            // free the files
            free_files(commit->files, commit->n_files);
            // free the commit
            free(commit->message);
            commit->message = NULL;
            free(commit->br_name);
            commit->br_name = NULL;
            free(commit->commit_id);
            commit->commit_id = NULL;
            free(commit->parent);
            branch->commits[j] = NULL;
            free(commit);
            commit = NULL;
        }
        free(branch->commits);
        branch->commits = NULL;

        // free the stage
        free_files(branch->stage, branch->n_files);
        // free the branch
        free(branch->name);
        branch->name = NULL;
        free(branch);
        branch = NULL;
    }
    // free the system
    free(help->branches);
    help->branches = NULL;
    free(help);
    help = NULL;
}