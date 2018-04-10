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


long fs_size = 0;
long available_size = 0;
char* fs_path;

Node *root;

Node *get_node_by_path(const char *path)
{
	char _path[MAX_PATH_LENGTH];
	strcpy(_path, path);
	Node* node = root;
	if(strcmp(path, "/") == 0)
	{
		return node;
	}
    char* splited_path = strtok(_path,"/");
    Node* tmp_node;
    while(splited_path != NULL)
    {
        int msg = hashmap_get(node->_map, splited_path, (void**)(&tmp_node));
        if(msg == MAP_OK)
        {
            node = tmp_node;
            splited_path = strtok(NULL, "/");
        }
        else
        {
            node = NULL;
            break;
        }
    }
	return node;
}

 int ramdisk_open(const char *path, struct fuse_file_info *fi)
{
    int result = 0;
    Node* node = get_node_by_path(path);
    if (node == NULL)
    {
        result = -ENOENT;
    }
    return result;
}

 int ramdisk_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    int result = SUCCESS;
    Node *node = get_node_by_path(path);
    if (node == NULL){
        return -ENOENT;

    }


    if(node->type != IS_FILE){
        return -EISDIR;
    }

    size_t content_size = node->st->st_size;
    if(offset < content_size)
    {
        if(offset + size > content_size)
        {
            size = content_size - offset;
        }
        memcpy(buf, node->content + offset, size);
    }
    else
    {
        size = 0;
    }

    result = size;
    return result;
}

 int ramdisk_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    if(available_size < size)
    {
        return -ENOSPC;
    }
    int result = size;
    Node *node = get_node_by_path(path);
    if(node != NULL)
    {
        if(node->type == IS_FILE)
        {
        	if(size > 0)
        	{
        		size_t content_size = node->st->st_size;
	        	if(content_size == 0)
	            {
	                node->content = (char *)malloc(sizeof(char) * size);
	                offset = 0;
	                memcpy(node->content + offset, buf, size);
	                node->st->st_size = offset + size;


	                time_t current_time;
	                time(&current_time);
	                node->st->st_mtime = current_time;
	                

	                available_size = available_size - size;

	                result = size;
	            }
	            else if(content_size != 0)
	            {
	                if(offset > content_size)
	                {
	                    offset = content_size;
	                }
	                long new_size = offset + size;
	                char *new_content = (char *)realloc(node->content, sizeof(char) * new_size);
	                node->content = new_content;
                    memcpy(node->content + offset, buf, size);
                    node->st->st_size = new_size;
                    

                    time_t current_time;
                    time(&current_time);
                    node->st->st_mtime = current_time;


                    available_size = available_size + content_size - new_size;
                    
                    result = size;
	            }
	        }
        }
        else
        {
            result = -EISDIR;
        }
    }
    else
    {
        result = -ENOENT;
    }
    return result;
}

 int ramdisk_unlink(const char *path)
{
    int result = 0;
    Node *node = get_node_by_path(path);
    if(node != NULL)
    {
        Node *fs_object_parent_ptr = node->parent_directory;
        size_t old_size = fs_object_parent_ptr->st->st_size;
        long updated_size = old_size;
        int msg = hashmap_remove(fs_object_parent_ptr->_map, node->name);
        if(node->st->st_size != 0)
        {
            available_size = available_size + node->st->st_size;
            updated_size = updated_size - node->st->st_size;
            free(node->content);
        }
        free(node->name);
        free(node->st);
        free(node);

        long size_of_file = sizeof(Node) + sizeof(struct stat);

        updated_size = updated_size - size_of_file;
        if(updated_size < 0)
        	updated_size = 0;
        fs_object_parent_ptr->st->st_size = updated_size;

        available_size = available_size + size_of_file;
    }
    else
    {
        result = -ENOENT;
    }
    return result;
}

 int ramdisk_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    int path_length = strlen(path);
	char _path[path_length];
	strcpy(_path, path);
	char dir_path[path_length];
	char* last_slash = strrchr(_path, '/');
	char* file_name = last_slash + 1;
	*last_slash = 0;
	if(strlen(_path) == 0)
    {
        strcpy(dir_path, "/");
    }
    else
    {
        strcpy(dir_path, _path);
    }
	Node* node = get_node_by_path(dir_path);
    if(node != NULL)
    {
    	Node* tmp_node;
    	int msg = hashmap_get(node->_map, file_name, (void**)(&tmp_node));
        if(msg == MAP_OK)
        {
            return -EEXIST;
        }
    	else if(available_size < 0)
	    {
	        return -ENOSPC;
	    }
        else
        {

        	Node *fs_object_new = (Node *)malloc(sizeof(Node));
	        fs_object_new->st = (struct stat *)malloc(sizeof(struct stat));
	        fs_object_new->name = malloc(FILENAME_SIZE * sizeof(char));
	        strcpy(fs_object_new->name, file_name);

	        long size_of_file = sizeof(Node) + sizeof(struct stat);
            
            fs_object_new->st->st_mode = S_IFREG | mode;
            fs_object_new->st->st_nlink = 1;
            fs_object_new->st->st_size = 0;
            
            time_t current_time;
            time(&current_time);
            fs_object_new->st->st_mtime = current_time;
            fs_object_new->st->st_ctime = current_time;
            
            fs_object_new->parent_directory = node;
            fs_object_new->_map = hashmap_new();
            fs_object_new->content = NULL;
            fs_object_new->type = IS_FILE;
            

            if(node->_map == NULL)
            {
                node->_map = hashmap_new();
            }
            int msg = hashmap_put(node->_map, fs_object_new->name, fs_object_new);

            size_t old_size = node->st->st_size;
            long updated_size = old_size + size_of_file;
            node->st->st_size = updated_size;

            available_size = available_size - size_of_file;
        }
    }
    else
    {
        return -ENOENT;
    }
    return 0;
}

 int ramdisk_mkdir(const char *path, mode_t mode)
{
    int path_length = strlen(path);
	char _path[path_length];
	strcpy(_path, path);
	char dir_path[path_length];
	char* last_slash = strrchr(_path, '/');
	char* dir_name = last_slash + 1;
	*last_slash = 0;
	if(strlen(_path) == 0)
    {
        strcpy(dir_path, "/");
    }
    else
    {
        strcpy(dir_path, _path);
    }
	Node* node = get_node_by_path(dir_path);
    if(node != NULL)
    {
    	Node* tmp_node;
    	int msg = hashmap_get(node->_map, dir_name, (void**)(&tmp_node));
        if(msg == MAP_OK)
        {
            return -EEXIST;
        }
        else if(available_size < 0)
        {
            return -ENOSPC;
        }
        else
        {

            Node *fs_object_new = (Node *)malloc(sizeof(Node));
            fs_object_new->st = (struct stat *)malloc( sizeof(struct stat) );
        	fs_object_new->name = malloc(FILENAME_SIZE * sizeof(char));
        	strcpy(fs_object_new->name, dir_name);

            long size_of_dir = sizeof(Node) + sizeof(struct stat);

            fs_object_new->st->st_nlink = 2;
            fs_object_new->st->st_mode = S_IFDIR | mode;
            fs_object_new->st->st_size = size_of_dir;
            
            time_t current_time;
            time(&current_time);
            fs_object_new->st->st_mtime = current_time;
            fs_object_new->st->st_ctime = current_time;
            
            fs_object_new->parent_directory = node;
            fs_object_new->_map = hashmap_new();
            fs_object_new->type = IS_DIRECTORY;


            if(node->_map == NULL)
            {
                node->_map = hashmap_new();
            }
            int msg = hashmap_put(node->_map, fs_object_new->name, fs_object_new);

            size_t old_size = node->st->st_size;
            long updated_size = old_size + size_of_dir;
            node->st->st_size = updated_size;

            available_size = available_size - size_of_dir;
        }
    }
    else
    {
        return -ENOENT;
    }
    return SUCCESS;
}

 int ramdisk_rmdir(const char *path)
{
    Node *node = get_node_by_path(path);
    if(node != NULL)
    {
       if(node->_map != NULL && hashmap_length(node->_map) > 0)
        {
            return -ENOTEMPTY;
        }
        Node *fs_object_parent_ptr = node->parent_directory;
        int msg = hashmap_remove(fs_object_parent_ptr->_map, node->name);
        fs_object_parent_ptr->st->st_nlink--;
        free(node->name);
        free(node->st);
        hashmap_free(node->_map);
        free(node);

        long size_of_dir = sizeof(Node) + sizeof(struct stat);

        size_t old_size = fs_object_parent_ptr->st->st_size;
        long updated_size = old_size - size_of_dir;
        if(updated_size < 0)
        	updated_size = 0;
        fs_object_parent_ptr->st->st_size = updated_size;

        available_size = available_size + size_of_dir;
    }
    else
    {
        return -ENOENT;
    }
    return SUCCESS;
}

/*
	# Opens the directory at given path
	# We do not need to actually open the directory, just return 0
*/
 int ramdisk_opendir(const char *path, struct fuse_file_info *fi)
{
    int result = SUCCESS;
    Node *node = get_node_by_path(path);
    if (node == NULL)
    {
        result = -ENOENT;
    }
	return result;
}

 int ramdisk_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi)
{
    Node *node = get_node_by_path(path);

    if (node == NULL){
        return -ENOENT;
    }

    filler(buf, ".", NULL, 0);
    filler(buf, "..", NULL, 0);
    int map_size = hashmap_length(node->_map);
    char* keys[map_size];
    int numKeys = hashmap_keys(node->_map, keys);
    int i = 0;
    for(i = 0; i < numKeys; i++)
    {
        filler(buf, keys[i], NULL, 0);
    }

    return SUCCESS;
}

 int ramdisk_getattr(const char *path, struct stat *stbuf)
{
    int result = SUCCESS;
    Node *node = get_node_by_path(path);
    if(node != NULL)
    {
        stbuf->st_nlink = node->st->st_nlink;
        stbuf->st_mode = node->st->st_mode;
        stbuf->st_size = node->st->st_size;
        stbuf->st_mtime = node->st->st_mtime;
        stbuf->st_ctime = node->st->st_ctime;
        
        result = 0;
    }
    else
    {
        result = -ENOENT;
    }
    return result;
}

 int ramdisk_release(const char *path, struct fuse_file_info *fi)
{
    int result = 0;
    Node *node = get_node_by_path(path);
    if (node == NULL)
    {
        result = -ENOENT;
    }
    return result;
}

 int ramdisk_utime(const char *path, struct utimbuf *ubuf)
{
    int result = SUCCESS;
    Node *node = get_node_by_path(path);
    if (node == NULL)
    {
        result = -ENOENT;
    }
    return result;
}

/*
    # Function to handle error occured in echo 
    # while writing data to already existing file 
    # handling error in [echo "new data" > abc.txt] (abc.txt is an existing file)
    # We do not need to implement actual function
    # just returning 0 is sufficient
*/
 int ramdisk_truncate(const char *path, off_t offset)
{
    int result = 0;
    Node *node = get_node_by_path(path);
    if (node == NULL)
    {
        result = -ENOENT;
    }
    return result;
}

static struct fuse_operations ramdisk_operations =
{
    .open = ramdisk_open,
    .release = ramdisk_release,
    .read = ramdisk_read,
    .write = ramdisk_write,
    .create = ramdisk_create,
    .mkdir = ramdisk_mkdir,
    .unlink = ramdisk_unlink,
    .rmdir = ramdisk_rmdir,
    .opendir = ramdisk_opendir,
    .readdir = ramdisk_readdir,
    .getattr = ramdisk_getattr,
    .truncate = ramdisk_truncate,
    .utime = ramdisk_utime
};

void intialize_root()
{
	root = (Node *)malloc(sizeof(Node));
	root->st = (struct stat *)malloc(sizeof(struct stat));
	root->name = malloc(FILENAME_SIZE * sizeof(char));
	strcpy(root->name, "/");

	long size_of_dir = sizeof(Node) + sizeof(struct stat);

    root->st->st_mode = S_IFDIR | 0755;
	root->st->st_nlink = 2;
	root->st->st_size = size_of_dir;
    time_t current_time;
    time(&current_time);
	root->st->st_mtime = current_time;
	root->st->st_ctime = current_time;
	root->parent_directory = NULL;
	root->_map = hashmap_new();
	root->content = NULL;
	root->type = IS_DIRECTORY;

	available_size = available_size - size_of_dir;
}

int main(int argc, char *argv[])
{
	if(argc < 3 || argc > 4)
	{
		exit(0);
	}
	if(argc == 3)
	{
		printf("Starting new filesystem.\n");
	}
	if(argc == 4)
	{
		fs_path = argv[3];
		printf("Loading filesystem from %s\n", fs_path);
		argv[3] = NULL;
		argc--;
	}
	fs_size = atol(argv[2]) * 1024 * 1024;
	available_size = fs_size;
	if(fs_size < 1)
	{
		printf("Filesystem size must be greater than zero.\n");
		exit(0);
	}
	argv[2] = NULL;
	argc--;

	intialize_root();

	return fuse_main(argc, argv, &ramdisk_operations, NULL);
}