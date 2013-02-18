/*
 
 Header goes here 

*/

#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <inttypes.h>

#include "camtool.h"

#include "common.h"

#define TODO_HARDCODED_DIR_NAME "."
#define TODO_HARDCODED_VERSION 0x12120402 

#define WEBUI_DATABLOB_INITIAL_SIZE (1024 * 1024) // 1 MB

int webui_data_blob_clean (webui_data_blob* blob);
int webui_data_blob_init  (webui_data_blob* blob, size_t offset);


int webui_append_fentry (const webui_fentry* fentry, webui_data_blob* blob);
int webui_append_dentry (const webui_dentry* fentry, webui_data_blob* blob);

int get_file_content (const char* path, const size_t size, char* content);
int traverse_target_dir (const char* dir_name, webui_data_blob* blob);

int webui_create_file (const webui_data_blob* blob, FILE* fd);

int main() {
  webui_data_blob* blob;
  // Initialize the blob 
  webui_data_blob_init(blob, OFFSET_FIRST_FILE);

  // Record the version (has to be done before checksum calculation) 
  int32_t version = TODO_HARDCODED_VERSION; 
  memmove (&blob->data[OFFSET_VERSION], &version, sizeof(version));

  // Traverse the target directory and pack
  traverse_target_dir (TODO_HARDCODED_DIR_NAME, blob);
  //size_t n = fwrite(blob->data, 1, blob->size, stdout);
  
  // Wrtie the result to a file 
  FILE* fd = fopen ("webui.bin", "w");
  webui_create_file (blob, fd); 
  fclose(fd);  

  webui_data_blob_clean(blob);
  return 0;
}

/* Internal functions */
int webui_create_file (const webui_data_blob* blob, FILE* fd) {

  // Write the header first 
  webui_file_header h;
  
  h.magic = WEBUI_MAGIC;
  h.checksum = calc_checksum_blob (blob, OFFSET_VERSION);  
  h.version = TODO_HARDCODED_VERSION;
  h.size = blob->size;
  
  fwrite (&h, 1, sizeof(h), fd);

  // Write the blob 
  fwrite (&blob->data[OFFSET_FIRST_FILE], 1, blob->size, fd); 

  return 0;
}

/// \brief Appends a file name to the path  
char* append_file_name (char* path, const char* fname) {
  const size_t path_len = strlen (path),
               fname_len = strlen (fname);

  assert ((path_len + fname_len) < MAX_PATH_LEN);

  // Make sure we do not prepend an empty path with /
  if (path[0] != '\0')
    strcat (path, "/");  

  strcat (path, fname);
  return path;
} 

/// \brief Given a directory name and the file name creates a path to a file   
char* create_path (char* path, const char*dirname, const char* fname) {
  const size_t dirname_len = strlen (dirname),
               fname_len = strlen (fname); 
  
  assert ((dirname_len + fname_len) < MAX_PATH_LEN);

  path[0] = '\0';
  append_file_name(path, dirname);
  append_file_name(path, fname);
  return path; 
}

/// \brief Traverses the target directory and populates the memory blob with the Web UI data
int traverse_target_dir (const char* dir_name, webui_data_blob* blob) { 
  struct dirent* dir_entry;
  int err_code = 0;
  char full_path[MAX_PATH_LEN] = {'\0'};

  // Open the directory 
  DIR* dir = opendir (dir_name);

  if ( dir == NULL ) {
    // TODO: verbose error message 
    // Proper error handling
    perror ("opendir failed ");
    return -1;
  }
  
  // Iterate over entries 
  
  // First read all the files (skip subdirectories)    
  struct stat s;
  while ( (dir_entry = readdir (dir)) != NULL  ) {
    full_path[0] = '\0';

    // Skip parrent and current directories 
    if (strcmp(dir_entry->d_name, "..") == 0 || strcmp(dir_entry->d_name, ".") == 0)
      continue;

    // Read the entry 
    append_file_name (full_path, dir_name);
    append_file_name (full_path, dir_entry->d_name);
    
    if (stat (full_path, &s)) {
      // TODO: verbose error message 
      // Proper error handling 
      printf ("Error doing fstatat on %s\n", full_path);
      perror ("fstatat failed");
      return -1;
    }

    if (S_ISDIR(s.st_mode)) {
      continue;
    } 
    else if (S_ISREG(s.st_mode)) {
      //printf ("Found a regular file: %s/%s\n", dir_name, dir_entry->d_name);
      
      // Generate a full path for the file 
      create_path (full_path, dir_name, dir_entry->d_name);

      // Createa and initialize webui_fentry structure 
      webui_fentry f;
      // remove leading .
      f.name_size = strlen (full_path);
      f.name = full_path;
      f.type = 0; //TODO switch to enum; 
      f.size = s.st_size; 
      // Read contents of the file 
      char buf[MAX_FILE_SIZE];
      get_file_content (f.name, f.size, buf);
      f.data = buf;

      // Appenf the entry to the blob
      webui_append_fentry (&f, blob);    
 
    }  
  }

  // Then iterate over subdirectories 

  // Reset the directory handle
  rewinddir(dir);

  while ( (dir_entry = readdir(dir)) != NULL ) {
    full_path[0] = '\0';

    // Skip parrent and current directories 
    if (strcmp(dir_entry->d_name, "..") == 0 || strcmp(dir_entry->d_name, ".") == 0)
      continue;

    // Read the entry 
    append_file_name (full_path, dir_name);
    append_file_name (full_path, dir_entry->d_name);
    
    if (stat (full_path, &s)) {
      // TODO: verbose error message 
      // Proper error handling 
      printf ("Error doing fstatat on %s\n", full_path);
      perror ("fstatat failed");
      return -1;
    }

    if (S_ISDIR(s.st_mode)) {
      //printf ("Found a directory: %s/%s\n", dir_name, dir_entry->d_name);
      create_path (full_path, dir_name, dir_entry->d_name);
      webui_dentry d;
      d.name_size = strlen (full_path);
      d.name = full_path;
      d.type = 1; //TODO switch to enum;

      webui_append_dentry (&d, blob);    
       
      traverse_target_dir (full_path, blob);
    } 
  }
  return 0;
} 

/// \brief dumps the file into memory
int get_file_content (const char* path, const size_t size, char *o) {
  FILE* fd = fopen (path, "r");
  
  if (fd == NULL) {
    // TODO: verbose error message 
    perror ("fopen failed");
    exit  (-1);
  }

  if (fread (o, 1, size, fd) != size) {
    // TODO: verbose error message
    perror ("fread failed");
    exit (-1);
  }
 
  fclose (fd);
  return 0;
}

/// \brief Initialize Web UI blob data structure 
int webui_data_blob_init (webui_data_blob* blob, size_t offset) {

  // TODO: assert that offset < WEBUI_DATABLOB_INITIAL_SIZE * 3

  blob->data = malloc (WEBUI_DATABLOB_INITIAL_SIZE * 2);

  if (blob->data == NULL) {
    // TODO Verbose error message 
    perror ("malloc failed");
    exit(-1);
  }

  blob->alloc_size = WEBUI_DATABLOB_INITIAL_SIZE * 2;
  blob->size = offset;

  return 0;
}

/// \brief Cleans up Web UI data blob structure
int webui_data_blob_clean (webui_data_blob* blob) {
  free(blob->data);
  return 0;
}

/// \brief Calculates the size of the fentry 
int get_fentry_size (const webui_fentry* fentry) {
  return  (WEBUI_ENTRY_NAME_SIZE_FIELD_LEN + 
          fentry->name_size + 
          WEBUI_ENTRY_TYPE_FIELD_LEN + 
          WEBUI_FENTRY_SIZE_FIELD_LEN + 
          fentry->size);
}

  
int webui_append_fentry (const webui_fentry* fentry, webui_data_blob* blob) {
  // Make sure there is enough memory allocaed 
  if (blob->alloc_size < (blob->size + get_fentry_size(fentry)) ) {
    //TODO Realloc
    perror("Realloc");
    exit (-1);
  }  

  // Copy file entry fields into the blob 
  size_t offset = blob->size; 
  
  // Size of the name field
  size_t name_size_int = fentry->name_size - 1; // remove leading .
  memmove ( &blob->data[offset], &name_size_int, WEBUI_ENTRY_NAME_SIZE_FIELD_LEN);
  offset += WEBUI_ENTRY_NAME_SIZE_FIELD_LEN;

  // Name field
  // remove leading .  
  memmove (&blob->data[offset], &fentry->name[1], name_size_int);
  offset += name_size_int;

  // Type field 
  memmove (&blob->data[offset], &fentry->type, WEBUI_ENTRY_TYPE_FIELD_LEN);
  offset += WEBUI_ENTRY_TYPE_FIELD_LEN;

  // Size of the file 
  memmove (&blob->data[offset], &fentry->size, WEBUI_FENTRY_SIZE_FIELD_LEN);
  offset += WEBUI_FENTRY_SIZE_FIELD_LEN;
  
  // File contents  
  memmove (&blob->data[offset], fentry->data, fentry->size);
  offset += fentry->size; 

  blob->size = offset;

  return 0;
} 

/// \brief Calculates the size of the fentry 
int get_dentry_size (const webui_dentry* dentry) {
  return  (WEBUI_ENTRY_NAME_SIZE_FIELD_LEN + 
          dentry->name_size + 
          WEBUI_ENTRY_TYPE_FIELD_LEN);
}

int webui_append_dentry (const webui_dentry* dentry, webui_data_blob* blob) {
 
   // Make sure there is enough memory allocaed 
  if (blob->alloc_size < (blob->size + get_dentry_size(dentry)) ) {
    //TODO Realloc
    perror("Realloc");
    exit (-1);
  }  

  // Copy file entry fields into the blob 
  size_t offset = blob->size; 
  
  // Size of the name field
  memmove ( &blob->data[offset], &dentry->name_size, WEBUI_ENTRY_NAME_SIZE_FIELD_LEN);
  offset += WEBUI_ENTRY_NAME_SIZE_FIELD_LEN;

  // Name field 
  // remove leading .
  memmove (&blob->data[offset], &dentry->name[1], dentry->name_size - 1);
  offset += (dentry->name_size - 1);

  // Type field 
  memmove (&blob->data[offset], &dentry->type, WEBUI_ENTRY_TYPE_FIELD_LEN);
  offset += WEBUI_ENTRY_TYPE_FIELD_LEN;
  
  blob->size = offset;
  
  return 0;
}

