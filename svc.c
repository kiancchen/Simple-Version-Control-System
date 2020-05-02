#include "svc.h"

#define CHECK 0


int files_copy(struct file **dist, struct file **stage, int n_files) {
    int tracked_file = 0;
    for (int i = 0; i < n_files; ++i) {
        struct file *file = malloc(sizeof(struct file));
        file->file_path = strdup(stage[i]->file_path);
        file->content = strdup(stage[i]->content);
        file->hash = stage[i]->hash;
        file->chg_type = stage[i]->chg_type;
        dist[i] = file;
        if (file->chg_type >= 0){
            tracked_file++;
        }
    }
    return tracked_file;
}

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
//
//int branch_has_file(void *helper, char *file_path) {
//    // if the file_path has been in the stage, return TRUE. Or False.
//    struct helper *help = (struct helper *) helper;
//    struct branch *cur_br = help->cur_branch;
//    struct file **stage = cur_br->stage;
//    int n_files = cur_br->n_files;
//
//    for (int i = 0; i < n_files; ++i) {
//        if (strcmp(stage[i]->file_path, file_path) == 0) {
//            if (stage[i]->chg_type != -1 && stage[i]->chg_type != -2){
//                // the file added but removed
//                return 1;
//            }else{
//                //the file exits new
//                return 2;
//            }
//        }
//    }
//    return 0; // the file never added into the system
//}

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

void init_stage(void *helper) {
    struct helper *help = (struct helper *) helper;
    struct branch *cur_br = help->cur_branch;
    cur_br->stage = malloc(sizeof(struct file *) * 4);
    cur_br->capacity_file = 4;
    cur_br->n_files = 0;
}


void *svc_init(void) {
    struct helper *helper = malloc(sizeof(struct helper));
    // init branches
    helper->branches = malloc(sizeof(struct branch *) * 4);
    helper->capacity_br = 4;
    helper->n_branches = 0;

    // init the master branch
    struct branch *master = malloc(sizeof(struct branch));
    helper->cur_branch = master;
    helper->branches[0] = master;
    helper->n_branches++;
    helper->branches[0]->name = strdup("master");
    master->commits = malloc(sizeof(struct commit *) * 4);
    master->n_commits = 0;
    master->capacity_commit = 4;
    master->head = NULL;
    master->n_detached = 0;

    init_stage(helper);
//    init_change(helper);
    return helper;
}


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

char *calc_cmt_id(void *helper, char *message) {
    if (message == NULL) {
        return NULL;
    }
    struct helper *help = (struct helper *) helper;
    struct branch *cur_br = help->cur_branch;
    // calculate the commit_id
    int id = 0;
    for (unsigned char *c = (unsigned char *)message; *c != '\0'; ++c) {
        id = (id + *c) % 1000;
    }
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
        for (unsigned char *c = (unsigned char *)(file->file_path); *c != '\0'; ++c) {
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
    restore_change(cur_br->stage, cur_br->n_files);

    commit->n_files = cur_br->n_files;
    commit->detached = FALSE;
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

void check_modification(void *helper) {
    struct helper *help = (struct helper *) helper;

    for (int i = 0; i < help->cur_branch->n_files; ++i) {
        struct file *file = help->cur_branch->stage[i];
        if (file->chg_type < 0) {
            return;
        }
        int new_hash = hash_file(helper, file->file_path);
        if (new_hash == -2) {
            if (file->chg_type == 1){
                file->chg_type = -2;
            }else{
                file->chg_type = -1;
            }
            continue;
        }
        if (new_hash != file->hash) {
            file->hash = new_hash;
            file->chg_type = 2;
            long size = file_length(file->file_path);
            // read the file
            char content[size + 1];
            read_file(content, file->file_path, size);
            free(file->content);
            file->content = strdup(content);
        }

    }
}

char *svc_commit(void *helper, char *message) {
    if (CHECK){
        printf("svc_commit with message [%s]\n", message);
    }

    check_modification(helper);
    char *hex = calc_cmt_id(helper, message);
    if (hex == NULL) {
        return NULL;
    }
    add_commit(helper, hex, message);
    return hex;

}

void *get_commit(void *helper, char *commit_id) {
    if (CHECK){
        printf("get_commit with id [%s]\n", commit_id);
    }


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
    if (CHECK){
        printf("get_prev_commit with id[%s]\n", cmt->commit_id);
    }
    *n_prev = cmt->n_parent;
    if (*n_prev == 0){
        return NULL;
    }
    char **parent = malloc(sizeof(char *) * *n_prev);
    for (int i = 0; i < *n_prev; ++i) {
        parent[i] = cmt->parent[i];
    }

    return parent;
}

void print_commit(void *helper, char *commit_id) {
    if (CHECK){
        printf("print_commit with id [%s]\n", commit_id);
    }

    struct commit *commit = (struct commit *) get_commit(helper, commit_id);
    if (commit == NULL) {
        printf("Invalid commit id\n");
        return;
    }
    printf("%s [%s]: %s\n", commit_id, commit->br_name, commit->message);
    for (int i = 0; i < commit->n_files; ++i) {
        struct file *file = commit->files[i];
        if (file->chg_type == 0) {
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
        if (file->chg_type != -1) {
            printf("    [%10d] %s\n", file->hash, file->file_path);
        }
    }

}

int svc_branch(void *helper, char *branch_name) {
    if (CHECK){
        printf("svc_branch with name [%s]\n", branch_name);
    }
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

    // If some changes are not committed
    for (int i = 0; i < help->cur_branch->n_files; ++i) {
        int chg_type = help->cur_branch->stage[i]->chg_type;
        if (chg_type == -1 || chg_type == 1 || chg_type == 2) {
            return -3;
        }
    }

    // If the branches full, expand it
    if (help->n_branches == help->capacity_br) {
        help->capacity_br *= 2;
        help->branches = realloc(help->branches, sizeof(struct branch *) * help->capacity_br);
    }
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
        branch->commits[i]->br_name = strdup(branch_name);
        branch->commits[i]->message = strdup(cur_br->commits[i]->message);
        branch->commits[i]->commit_id = strdup(cur_br->commits[i]->commit_id);
        branch->commits[i]->n_files = cur_br->commits[i]->n_files;
        branch->commits[i]->parent = cur_br->commits[i]->parent;
        branch->commits[i]->n_parent = cur_br->commits[i]->n_parent;
        branch->commits[i]->detached = cur_br->commits[i]->detached;
        branch->commits[i]->files = malloc(sizeof(struct file *) * branch->commits[i]->n_files);
        files_copy(branch->commits[i]->files, cur_br->commits[i]->files, branch->commits[i]->n_files);

    }
    branch->n_detached = 0;
    help->n_branches++;

    return 0;
}

int svc_checkout(void *helper, char *branch_name) {
    if (CHECK){
        printf("svc_checkout with name [%s]\n", branch_name);
    }
    if (branch_name == NULL) {
        return -1;
    }
    struct helper *help = (struct helper *) helper;
    // If some changes are not committed
    for (int i = 0; i < help->cur_branch->n_files; ++i) {
        int chg_type = help->cur_branch->stage[i]->chg_type;
        if (chg_type == -1 || chg_type == 1 || chg_type == 2) {
            return -2;
        }
    }
    // look for the branch
    for (int i = 0; i < help->n_branches; ++i) {
        if (strcmp(help->branches[i]->name, branch_name) == 0) {
            help->cur_branch = help->branches[i];
            return 0;
        }
    }

    return -1;
}

char **list_branches(void *helper, int *n_branches) {
    if (CHECK){
        printf("list_branches\n");
    }
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

int svc_add(void *helper, char *file_name) {
    if (CHECK){
        printf("svc_add with file_path [%s]\n", file_name);
    }
    struct helper *help = (struct helper *) helper;
    struct branch *cur_br = help->cur_branch;
    if (cur_br->n_files > 0) {
    }
    if (helper == NULL || file_name == NULL) {
        return -1;
    }
    for (int i = 0; i < cur_br->n_files; ++i) {
        if (strcmp(cur_br->stage[i]->file_path, file_name) == 0) {
            if (cur_br->stage[i]->chg_type == -1 || cur_br->stage[i]->chg_type == -2){
                // the file added but removed
                cur_br->stage[i] ->chg_type = 1;
                return hash_file(helper, file_name);
            }else{
                //the file exits new
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
    // create a file object
    struct file *file = malloc(sizeof(struct file));
    file->file_path = strdup(file_name);
    file->content = strdup(content);
//    if (CHECK){
//        printf("%s\n", content);
//    }
    int hash = hash_file(helper, file_name);
    file->hash = hash;
    file->chg_type = 1; // change type is addition
    // add the file to stage
    if (cur_br->n_files == cur_br->capacity_file) {
        cur_br->capacity_file *= 2;
        cur_br->stage = realloc(cur_br->stage, cur_br->capacity_file);
    }
    cur_br->stage[cur_br->n_files] = file;
    cur_br->n_files++;
    // store this change
    return hash;
}

int svc_rm(void *helper, char *file_name) {
    if (CHECK){
        printf("svc_rm with file_path [%s]\n", file_name);
    }
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

int svc_reset(void *helper, char *commit_id) {
    if (CHECK){
        printf("svc_reset to id [%s]\n", commit_id);
    }
    if (commit_id == NULL) {
        return -1;
    }
    int index = -1;
    struct helper *help = (struct helper *) helper;
    struct branch *cur_br = help->cur_branch;
    for (int i = 0; i < cur_br->n_files; ++i) {
        struct commit *commit = cur_br->commits[i];
        if (strcmp(commit->commit_id, commit_id) == 0) {
            index = i;
            break;
        }
    }
    if (index == -1) {
        return -2;
    }
    cur_br->head = cur_br->commits[index];
    cur_br->n_files = cur_br->head->n_files;
    for (int i = 0; i < cur_br->n_files; ++i) {
        free(cur_br->stage[i]->content);
        cur_br->stage[i]->content = NULL;
        free(cur_br->stage[i]->file_path);
        cur_br->stage[i]->file_path = NULL;
        free(cur_br->stage[i]);
        cur_br->stage[i] = NULL;
    }
    files_copy(cur_br->stage, cur_br->head->files, cur_br->head->n_files);
    restore_change(cur_br->stage, cur_br->n_files);
    cur_br->capacity_file = cur_br->n_files * 2;
    // restore files
    for (int i = 0; i < cur_br->n_files; ++i) {
        struct file *file = cur_br->stage[i];
        FILE *fp = fopen(file->file_path, "w");
        fputs(file->content, fp);
        fclose(fp);
    }
    // detach commits afterwards
    for (int i = index + 1; i < cur_br->n_commits; ++i) {
        struct commit *commit = cur_br->commits[i];
        commit->detached = TRUE;
        cur_br->n_detached++;
    }

    return 0;
}

char *svc_merge(void *helper, char *branch_name, struct resolution *resolutions, int n_resolutions) {
    if (CHECK){
        printf("svc_merge with branch_name [%s]\n", branch_name);
    }
    struct helper *help = (struct helper *) helper;
    if (branch_name == NULL) {
        printf("Invalid branch message\n");
        return NULL;
    }
    // look for the branch
    struct branch *merged_br = NULL;
    for (int i = 0; i < help->n_branches; ++i) {
        if (strcmp(help->branches[i]->name, branch_name) == 0) {
            merged_br = help->branches[i];
            break;
        }
    }
    if (merged_br == NULL) {
        printf("Branch not found\n");
        return NULL;
    }
    printf("merged_br address: %p/n", merged_br);
    // check if it's the current checked out branch
    if (help->cur_branch == merged_br) {
        printf("Cannot merge a branch with itself\n");
        return NULL;
    }
    struct branch *cur_br = help->cur_branch;
    // check if there's changes uncommitted
    for (int i = 0; i < cur_br->n_files; ++i) {
        struct file *file = cur_br->stage[i];
        if (file->chg_type == -1 || file->chg_type == 1 || file->chg_type == 2) {
            printf("Changes must be committed\n");
            return NULL;
        }
    }
    char message[51] = "Merged branch ";
    strcat(message, branch_name);

    printf("merged_br address: %p/n", merged_br);
    int n_files = cur_br->n_files;
    for (int i = 0; i < merged_br->n_files; ++i) {
        struct file *m_f = merged_br->stage[i];
        int found = FALSE;
        for (int j = 0; j < n_files; ++j) {
            struct file *file = cur_br->stage[i];
            if (strcmp(file->file_path, m_f->file_path) == 0) {
                found = TRUE;
                if (file->hash != m_f->hash) {
                    for (int k = 0; k < n_resolutions; ++k) {
                        // look for the resolution for this conflicting file
                        if (strcmp(resolutions[k].file_name, file->file_path) == 0) {
                            long size = file_length(resolutions[k].file_name);
                            FILE *fp = fopen(resolutions[k].file_name, "r");
                            // read the file
                            char content[size + 1];
                            read_file(content, resolutions[k].file_name, size);
                            fclose(fp);
                            fp = fopen(file->file_path, "w");
                            fputs(content, fp);
                            fclose(fp);
                            free(file->content);
                            file->content = NULL;
                            file->content = strdup(content);
                            file->chg_type = 2;
                            file->hash = hash_file(helper, file->file_path);
                            break;
                        }
                    }
                }
                break;
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
    for (int i = 0; i < help->n_branches; ++i) {
        struct branch *branch = help->branches[i];
        // free commits
        for (int j = 0; j < branch->n_commits; ++j) {
            struct commit *commit = branch->commits[j];
            // free the files
            for (int k = 0; k < commit->n_files; ++k) {
                struct file *file = commit->files[k];
                free(file->file_path);
                file->file_path = NULL;
                free(file->content);
                file->content = NULL;
                free(file);
                commit->files[k] = NULL;
            }
            free(commit->message);
            commit->message = NULL;
            free(commit->files);
            commit->files = NULL;
            free(commit->br_name);
            commit->br_name = NULL;
            free(commit->commit_id);
            commit->commit_id = NULL;
            free(commit->parent);
            commit->parent = NULL;
            free(commit);
            commit = NULL;
        }
        free(branch->commits);
        branch->commits = NULL;

        for (int l = 0; l < branch->n_files; ++l) {
            struct file *file = branch->stage[l];
            free(file->file_path);
            file->file_path = NULL;
            free(file->content);
            file->content = NULL;
            free(file);
            branch->stage[l] = NULL;
        }

        free(branch->stage);
        branch->stage = NULL;
        free(branch->name);
        branch->name = NULL;
        free(branch);
        branch = NULL;
    }

    free(help->branches);
    help->branches = NULL;
    free(help);
    help = NULL;
}