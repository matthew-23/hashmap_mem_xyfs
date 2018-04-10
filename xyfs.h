//
// Created by matt on 18-4-10.
//

#ifndef XYFS_XYFS_H
#define XYFS_XYFS_H

#define FILENAME_SIZE 100

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
