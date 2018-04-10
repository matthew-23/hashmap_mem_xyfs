//
// Created by matt on 18-4-10.
//

#ifndef XYFS_XYFS_H
#define XYFS_XYFS_H

#define FILENAME_SIZE 100

#define IS_FILE  0
#define IS_DIRECTORY 1
#define SUCCESS 0
#define MAX_PATH_LENGTH 4096


typedef struct node
{
    char* name;
    int type;
    struct stat* st;
    struct node* parent_directory;
    char* content;
    map_t _map;
}Node;

#endif //XYFS_XYFS_H
