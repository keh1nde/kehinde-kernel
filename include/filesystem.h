//
// Created by Kehinde Adeoso on 5/1/26.
//

#ifndef RASPBERRY_PI_OPERATING_SYSTEM_FILESYSTEM_H
#define RASPBERRY_PI_OPERATING_SYSTEM_FILESYSTEM_H
#include <stdint-gcc.h>


typedef struct block block_t;
struct inode;

constexpr uint64_t BLOCK_SIZE = 4096;
constexpr uint64_t MAX_SIZE = 255;
constexpr uint64_t INVALID_INO = 0;
constexpr uint64_t ROOT_INO = 1;

static inode* inode_list_head = nullptr;

struct block {
	block_t* next;
	uint8_t data[BLOCK_SIZE - sizeof(block_t*)];
};

struct inode {
	uint64_t ino;
	uint8_t inode_type;
	uint64_t inode_size;
	block_t* first_block;
	inode* next;
};

struct dirent {
	uint64_t inode_id;
	char name[MAX_SIZE + 1];
	// include static_assert(sizeof(dirent) == 264); in remaining implementation
};

// End constants, begin main methods

// Initializes the inode list, allocates inode 1 as the root directory.
// Must be called after kheap_init.
void fs_init();


// Walks an absolute path ("/foo/bar/baz") from the root.
// Returns the inode number, or INVALID_INO if any component doesn't exist.
uint64_t fs_lookup(const char* path);

// Creates a new inode of `type` (1=file, 2=dir) inside the directory `parent_ino`,
// with the given name. Returns the new inode number, or INVALID_INO on failure
// (parent isn't a dir, name already exists, name too long, OOM).
// For directories, `fs_mkdir(parent, name)` is just `fs_create(parent, name, 2)`.
uint64_t fs_create(uint64_t parent_ino, const char* name, uint8_t type);


// Removes the dirent `name` from `parent_ino`. If the unlinked inode now has
// zero references, frees its data blocks and the inode itself.
// Returns 0 on success, -1 on failure (no such entry, parent isn't a dir,
// target is a non-empty directory).
int fs_unlink(uint64_t parent_ino, const char* name);

// Reads up to `len` bytes from file `ino` starting at `offset` into `buf`.
// Returns bytes actually read (may be < len at EOF), or -1 on error
// (no such inode, inode isn't a file, buf is null).
// Reading past EOF returns 0, not an error.
int64_t fs_read(uint64_t ino, uint64_t offset, uint64_t len, void* buf);

// Writes `len` bytes from `buf` into file `ino` starting at `offset`.
// Grows the file (allocates new blocks) if `offset + len > size`.
// Returns bytes written, or -1 on error.
int64_t fs_write(uint64_t ino, uint64_t offset, uint64_t len, void* buf);


// Copies the `index`-th dirent of `dir_ino` into `*out`.
// Returns 1 if a dirent was returned, 0 if `index` is past the end, -1 on error.
// Caller iterates: for (i = 0; fs_readdir(d, i, &ent) == 1; i++) { ... }
int64_t  fs_readdir(uint64_t dir_ino, uint64_t index, dirent* out);

// End main methods, begin helper methods

bool name_matches(const char* path, uint64_t start, uint64_t end, const char* name);

uint64_t parse_path(const char* path, uint64_t* ends, uint64_t max_segments);

inode* find_inode(uint64_t ino);

dirent* dirent_at(const inode *dir, uint64_t idx);

#endif //RASPBERRY_PI_OPERATING_SYSTEM_FILESYSTEM_H
