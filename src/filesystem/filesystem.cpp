//
// Created by Kehinde Adeoso on 5/1/26.
//

#include "../../include/filesystem.h"

#include "../../include/heap_alloc.h"


static uint64_t next_ino = 1;

void fs_init() {
	const auto root = static_cast<inode *>(kmalloc(sizeof(inode)));
	root->ino = next_ino;
	root->inode_type = 2;
	root->inode_size = 0;
	root->first_block = nullptr;
	root->next = inode_list_head;
	inode_list_head = root;

	next_ino++;
}

uint64_t fs_lookup(const char* path) {
	if (path[0] == '\0') return INVALID_INO;
	// Parse out the path
	uint64_t start = 1; // Starts at index 1
	uint64_t ends[16];
	const uint64_t n = parse_path(path, ends, 16);

	if (n == 0) return ROOT_INO; // We only received the root directory "/"

	// Now, traverse the path
	inode* curr = find_inode(ROOT_INO);
	if (!curr) return INVALID_INO; // Could be that no inode was allocated yet.

	// Do the following for each of the directories in the path:
	for (uint64_t i = 0; i < n; i++) {
		if (curr->inode_type != 2) return INVALID_INO; // inode does not represent a directory

		// Check and find matching dirent
		bool found = false;
		for (uint64_t j = 0; j < curr->inode_size; j++) {

			const dirent* d = dirent_at(curr, j);
			if (!d) return INVALID_INO;

			if (name_matches(path, start, ends[i], d->name)) {
				curr = find_inode(d->inode_id);
				start = ends[i] + 1;
				found = true;
				break;
			}
		}
		if (!found) return INVALID_INO;
	}
	return curr->ino;
}

bool name_matches(const char* path, const uint64_t start,
	const uint64_t end, const char* name) {
	for (uint64_t i = 0; i < end - start; i++) {
		if (path[start + i] != name[i]) return false;
		if (name[i] == '\0') return false; // name ended early
	}
	return name[end-start] == '\0';
}

uint64_t parse_path(const char* path, uint64_t* ends,
										const uint64_t max_segments) {
	uint64_t count = 0;
	uint64_t i = 0;

	while (path[i] != '\0' && count < max_segments) {
		// path[i] is '/'. Advance past it, then scan to the next '/' or end-of-string.
		i++;
		while (path[i] != '\0' && path[i] != '/') {
			i++;
		}
		ends[count++] = i;
	}

	return count;
}

inode* find_inode(const uint64_t ino) {
	inode* curr = inode_list_head;
	while (curr != nullptr) {
		if (curr->ino == ino) return curr;
		curr = curr->next;
	}
	return nullptr;
}

dirent* dirent_at(const inode *dir, const uint64_t idx) {
	if (idx >= dir->inode_size) return nullptr; // idx out of bounds

	const uint64_t dirents_per_block = sizeof(block::data) / sizeof(dirent);

	const uint64_t block_index = idx / dirents_per_block;
	const uint64_t slot = idx % dirents_per_block;

	block_t* curr_block = dir->first_block;
	if (!curr_block) return nullptr;

	for (uint64_t i = 0; i < block_index; i++) {
		if (curr_block) curr_block = curr_block->next;
		else return nullptr; // dir itself is null, so there's no next
	}

	return &reinterpret_cast<dirent *>(curr_block->data)[slot];
}

void append_block(inode* inode) {
	if (!inode->first_block) {
		inode->first_block = static_cast<block *>(kmalloc(BLOCK_SIZE));
		return;
	}
	block_t* curr_block = inode->first_block;
	while (curr_block->next) {
		curr_block = curr_block->next;
	}
	curr_block->next = static_cast<block *>(kmalloc(BLOCK_SIZE));
}

void name_copy(inode* dst, inode* src) {
	
}
