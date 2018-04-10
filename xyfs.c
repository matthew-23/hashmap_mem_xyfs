#define FUSE_USE_VERSION 30

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <limits.h>
#include <stdlib.h>
#include "hashmap.h"

#include <fuse.h>

#include "xyfs.h"

Node *root;

Node *get_node_by_path(const char *path) {
    char _path[MAX_PATH_LENGTH];
    strcpy(_path, path);
    Node *node = root;
    if (strcmp(path, "/") == 0) {
        return node;
    }
    char *splited_path = strtok(_path, "/");
    Node *tmp_node;
    while (splited_path != NULL) {
        int msg = hashmap_get(node->_map, splited_path, (void **) (&tmp_node));
        if (msg == MAP_OK) {
            node = tmp_node;
            splited_path = strtok(NULL, "/");
        } else {
            return NULL;
        }
    }
    return node;
}

int xyfs_open(const char *path, struct fuse_file_info *fi) {
    int result = 0;
    Node *node = get_node_by_path(path);
    if (node == NULL) {
        result = -ENOENT;
    }
    return result;
}

int xyfs_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    int result = 0;
    Node *node = get_node_by_path(path);
    if (node == NULL) {
        return -ENOENT;
    }

    if (node->type != FILE_NODE) {
        return -EISDIR;
    }

    size_t content_size = node->st->st_size;
    if (offset < content_size) {
        if (offset + size > content_size) {
            size = content_size - offset;
        }
        memcpy(buf, node->content + offset, size);
    } else {
        size = 0;
    }

    result = size;
    return result;
}

int xyfs_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi) {
    Node *node = get_node_by_path(path);
    if (node == NULL) {
        return -ENOENT;
    }

    if (node->type != FILE_NODE) {
        return -EISDIR;
    }

    size_t content_size = node->st->st_size;
    if (content_size == 0) {
        node->content = (char *) malloc(sizeof(char) * size);
        offset = 0;
        memcpy(node->content + offset, buf, size);
        node->st->st_size = offset + size;
    } else {
        if (offset > content_size) {
            offset = content_size;
        }
        long new_size = offset + size;
        char *new_content = (char *) realloc(node->content, sizeof(char) * new_size);
        node->content = new_content;
        memcpy(node->content + offset, buf, size);
        node->st->st_size = new_size;
    }
    time_t current_time;
    time(&current_time);
    node->st->st_mtime = current_time;

    return size;
}

int xyfs_unlink(const char *path) {
    int result = SUCCESS;
    Node *node = get_node_by_path(path);
    if (node == NULL) {
        return -ENOENT;
    }
    Node *parent_dir = node->parent_dir;
    hashmap_remove(parent_dir->_map, node->name);

    size_t old_size = parent_dir->st->st_size;
    long updated_size = old_size;
    if (node->st->st_size != 0) {
        updated_size = updated_size - node->st->st_size;
        free(node->content);
    }
    free(node->name);
    free(node->st);
    free(node);

    long size_of_file = sizeof(Node) + sizeof(struct stat);
    updated_size = updated_size - size_of_file;
    if (updated_size < 0)
        updated_size = 0;
    parent_dir->st->st_size = updated_size;

    return result;
}

int xyfs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    char _path[MAX_PATH_LENGTH];
    strcpy(_path, path);
    char dir_path[MAX_PATH_LENGTH];

    char *last_slash = strrchr(_path, '/');
    char *file_name = last_slash + 1;
    *last_slash = 0;

    if (strlen(_path) == 0) {
        strcpy(dir_path, "/");
    } else {
        strcpy(dir_path, _path);
    }

    Node *node = get_node_by_path(dir_path);
    if (node == NULL) {
        return -ENOENT;
    }

    Node *tmp_node;
    int msg = hashmap_get(node->_map, file_name, (void **) (&tmp_node));
    if (msg == MAP_OK) {
        return -EEXIST;
    }

    Node *new_node = (Node *) malloc(sizeof(Node));
    new_node->st = (struct stat *) malloc(sizeof(struct stat));
    new_node->name = malloc(MAX_FILENAME_LENGTH * sizeof(char));
    strcpy(new_node->name, file_name);
    new_node->st->st_mode = S_IFREG | mode;
    new_node->st->st_nlink = 1;
    new_node->st->st_size = 0;

    time_t current_time;
    time(&current_time);
    new_node->st->st_mtime = current_time;
    new_node->st->st_ctime = current_time;

    new_node->parent_dir = node;
    new_node->_map = hashmap_new();
    new_node->content = NULL;
    new_node->type = FILE_NODE;

    if (node->_map == NULL) {
        node->_map = hashmap_new();
    }
    hashmap_put(node->_map, new_node->name, new_node);

    size_t old_size = node->st->st_size;
    long size_of_file = sizeof(Node) + sizeof(struct stat);
    long updated_size = old_size + size_of_file;
    node->st->st_size = updated_size;

    return 0;
}

int xyfs_mkdir(const char *path, mode_t mode) {
    char _path[MAX_PATH_LENGTH];
    strcpy(_path, path);
    char dir_path[MAX_PATH_LENGTH];
    char *last_slash = strrchr(_path, '/');
    char *dir_name = last_slash + 1;
    *last_slash = 0;

    if (strlen(_path) == 0) {
        strcpy(dir_path, "/");
    } else {
        strcpy(dir_path, _path);
    }
    Node *node = get_node_by_path(dir_path);
    if (node == NULL) {
        return -ENOENT;
    }
    Node *tmp_node;
    int msg = hashmap_get(node->_map, dir_name, (void **) (&tmp_node));
    if (msg == MAP_OK) {
        return -EEXIST;
    }

    Node *new_node = (Node *) malloc(sizeof(Node));
    new_node->st = (struct stat *) malloc(sizeof(struct stat));
    new_node->name = malloc(MAX_FILENAME_LENGTH * sizeof(char));
    strcpy(new_node->name, dir_name);

    long size_of_dir = sizeof(Node) + sizeof(struct stat);

    new_node->st->st_nlink = 2;
    new_node->st->st_mode = S_IFDIR | mode;
    new_node->st->st_size = size_of_dir;

    time_t current_time;
    time(&current_time);
    new_node->st->st_mtime = current_time;
    new_node->st->st_ctime = current_time;

    new_node->parent_dir = node;
    new_node->_map = hashmap_new();
    new_node->type = DERICTORY_NODE;


    if (node->_map == NULL) {
        node->_map = hashmap_new();
    }
    hashmap_put(node->_map, new_node->name, new_node);

    size_t old_size = node->st->st_size;
    long updated_size = old_size + size_of_dir;
    node->st->st_size = updated_size;

    return SUCCESS;
}

int xyfs_rmdir(const char *path) {
    Node *node = get_node_by_path(path);
    if (node == NULL) {
        return -ENOENT;
    }
    if (node->_map != NULL && hashmap_length(node->_map) > 0) {
        return -ENOTEMPTY;
    }
    Node *parent_dir = node->parent_dir;
    hashmap_remove(parent_dir->_map, node->name);
    parent_dir->st->st_nlink--;
    free(node->name);
    free(node->st);
    hashmap_free(node->_map);
    free(node);

    long size_of_dir = sizeof(Node) + sizeof(struct stat);
    size_t old_size = parent_dir->st->st_size;
    long updated_size = old_size - size_of_dir;

    if (updated_size < 0)
        updated_size = 0;
    parent_dir->st->st_size = updated_size;
    return SUCCESS;
}

int xyfs_opendir(const char *path, struct fuse_file_info *fi) {
    int result = SUCCESS;
    Node *node = get_node_by_path(path);
    if (node == NULL) {
        result = -ENOENT;
    }
    return result;
}

int xyfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi) {
    Node *node = get_node_by_path(path);

    if (node == NULL) {
        return -ENOENT;
    }

    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);
    int map_size = hashmap_length(node->_map);
    char *keys[map_size];
    int numKeys = hashmap_keys(node->_map, keys);
    int i = 0;
    for (i = 0; i < numKeys; i++) {
        filler(buf, keys[i], NULL, 0);
    }

    return SUCCESS;
}

int xyfs_getattr(const char *path, struct stat *stbuf) {
    Node *node = get_node_by_path(path);
    if (node == NULL){
        return -ENOENT;
    }

    stbuf->st_nlink = node->st->st_nlink;
    stbuf->st_mode = node->st->st_mode;
    stbuf->st_size = node->st->st_size;
    stbuf->st_mtime = node->st->st_mtime;
    stbuf->st_ctime = node->st->st_ctime;
    return SUCCESS;
}

int xyfs_release(const char *path, struct fuse_file_info *fi) {
    int result = 0;
    Node *node = get_node_by_path(path);
    if (node == NULL) {
        result = -ENOENT;
    }
    return result;
}

int xyfs_utime(const char *path, struct utimbuf *ubuf) {
    int result = SUCCESS;
    Node *node = get_node_by_path(path);
    if (node == NULL) {
        result = -ENOENT;
    }
    return result;
}

int xyfs_truncate(const char *path, off_t offset) {
    int result = SUCCESS;
    Node *node = get_node_by_path(path);
    if (node == NULL) {
        result = -ENOENT;
    }
    return result;
}

static struct fuse_operations xyfs_operations = {
        .open = xyfs_open,
        .release = xyfs_release,
        .read = xyfs_read,
        .write = xyfs_write,
        .create = xyfs_create,
        .mkdir = xyfs_mkdir,
        .unlink = xyfs_unlink,
        .rmdir = xyfs_rmdir,
        .opendir = xyfs_opendir,
        .readdir = xyfs_readdir,
        .getattr = xyfs_getattr,
        .truncate = xyfs_truncate,
        .utime = xyfs_utime
};

void init_root() {
    root = (Node *) malloc(sizeof(Node));
    root->st = (struct stat *) malloc(sizeof(struct stat));
    root->name = malloc(MAX_FILENAME_LENGTH * sizeof(char));
    strcpy(root->name, "/");

    root->st->st_mode = S_IFDIR | 0755;
    root->st->st_nlink = 2;
    long size_of_dir = sizeof(Node) + sizeof(struct stat);
    root->st->st_size = size_of_dir;
    time_t current_time;
    time(&current_time);
    root->st->st_mtime = current_time;
    root->st->st_ctime = current_time;
    root->parent_dir = NULL;
    root->_map = hashmap_new();
    root->content = NULL;
    root->type = DERICTORY_NODE;

}

int main(int argc, char *argv[]) {
    if (argc == 2) {
        printf("Starting new filesystem.\n");
    }
    init_root();
    return fuse_main(argc, argv, &xyfs_operations, NULL);
}