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
	const inode* curr = find_inode(ROOT_INO);
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

uint64_t fs_create(uint64_t parent_ino, const char* name, const uint8_t type) {
	inode *parent = find_inode(parent_ino);
	if (!parent) return INVALID_INO;
	if (parent->inode_type != 2) return INVALID_INO; // The inode isn't a directory.

	// Validate name
	if (__builtin_strlen(name) >= MAX_SIZE || name[0] == '\0') return INVALID_INO;

	// Check if name exists in the directory
	const uint64_t dirents_per_block = sizeof(block::data) / sizeof(dirent);

	for (int i = 0; i < parent->inode_size; i++) {
		const dirent* d = dirent_at(parent, i);
		// Check if d->name matches, if so, return invalid
		if (!d) return INVALID_INO;
		if (name_matches_single(d->name, name)) return INVALID_INO;
	}

	// Allocate and initialize a new inode
	inode* new_inode = static_cast<inode *>(kmalloc(sizeof(inode)));
	new_inode->ino = next_ino++;
	new_inode->inode_type = type;
	new_inode->inode_size = 0;
	new_inode->first_block = nullptr;
	new_inode->next = inode_list_head;
	inode_list_head = new_inode;

	// Insert new dirent into parent inode.
	if (parent->inode_size % dirents_per_block == 0) {
		append_block(parent);
	}

	// Insert dirent into parent inode
	const uint64_t block_index = parent->inode_size / dirents_per_block;
	const uint64_t slot = parent->inode_size % dirents_per_block;

	block_t* curr_block = parent->first_block;

	for (uint64_t j = 0; j < block_index; j++) {
		if (curr_block) curr_block = curr_block->next;
		else break; // dir itself is null, so there's no next
	}

	// Write a directory entry in the parent's block containing information
	parent->inode_size++;
	dirent *new_dirent = &reinterpret_cast<dirent*>(curr_block->data)[slot];
	new_dirent->inode_id = new_inode->ino;

	int i = 0;
	while (name[i] != '\0') {
		new_dirent->name[i] = name[i];
		i++;
	}
	new_dirent->name[i] = '\0';

	return new_inode->ino;
}

int fs_unlink(const uint64_t parent_ino, const char *name) {
	// Get parent. If parent does not exist or is not a directory, return.
	inode* parent = find_inode(parent_ino);
	if (!parent) return -1; // inode with matching number not found
	if (parent->inode_type != 2 || parent->inode_size == 0) return -1; // not a directory, or the directory is empty

	// Find a dirent with a matching name. If not found return -1
	dirent* subject_dirent = nullptr;
	for (int i = 0; i < parent->inode_size; i++) {
		subject_dirent = dirent_at(parent, i);
		if (!subject_dirent) return -1;
		if (name_matches_single(subject_dirent->name, name)) break;
	}

	// If we exit the loop without a match, or if subject_dirent remains null.
	if (!subject_dirent || !name_matches_single(subject_dirent->name, name)) return -1;

	// Swap the dirent-to-be-deleted with the entry it points to
	dirent* last_dirent = dirent_at(parent, parent->inode_size - 1);
	if (!last_dirent) return -1;

	// Fail if the inode to be deleted is a non-empty directory.
	inode* subject_inode = find_inode(subject_dirent->inode_id);
	if (!subject_inode || (subject_inode->inode_type == 2 && subject_inode->inode_size != 0)) return -1;

	if (subject_dirent == last_dirent) {
		parent->inode_size--;
		return 0;
	}

	*subject_dirent = *last_dirent;
	parent->inode_size--;
	return 0;
}

int64_t fs_read(const uint64_t ino, const uint64_t offset, uint64_t len, void *buf) {
	uint8_t* dst = static_cast<uint8_t *>(buf);

	if (!buf) return -1;

	const uint64_t block_index = offset / sizeof(block::data);
	const uint64_t byte_offset = offset % sizeof(block::data);

	const inode* subject = find_inode(ino);
	if (!subject) return -1; // The inode does not exist
	if (subject->inode_type != 1) return -1; // inode doesn't represent a file

	const block_t* current_block = subject->first_block;


	for (int i = 0; i < block_index; i++) {
		if (current_block) current_block = current_block->next;
		else return -1; // There are no blocks allocated, so there's nothing to read.
	}

	if (offset >= subject->inode_size) return 0;

	if (offset + len > subject->inode_size) {
		len = subject->inode_size - offset;
	}


	uint64_t bytes_copied = 0;
	uint64_t block_offset = byte_offset;

	while (bytes_copied < len) {
		const uint64_t bytes_left_in_block = sizeof(block::data) - block_offset;
		const uint64_t chunk = min(len - bytes_copied, bytes_left_in_block);

		for (int i = 0; i < chunk; i++) {
			dst[bytes_copied + i] = current_block->data[block_offset + i];
		}

		bytes_copied += chunk;
		block_offset = 0;

		current_block = current_block->next;
	}
	return len;

}

int64_t fs_write(uint64_t ino, uint64_t offset, uint64_t len, void *buf) {
	if (!buf) return -1;
	const uint8_t* src = static_cast<uint8_t*>(buf);

	inode* subject = find_inode(ino);
	if (!subject) return -1;
	if (subject->inode_type != 1) return -1;

	const uint64_t block_index = offset / sizeof(block::data);
	const uint64_t byte_offset = offset % sizeof(block::data);

	uint64_t curr_inode_blocks = 0;

	block_t* pointer = subject->first_block;
	while (pointer) {
		curr_inode_blocks++;
		pointer = pointer->next;
	}

	uint64_t remaining_space = sizeof(block::data) * curr_inode_blocks;


	if (offset + len > remaining_space) {
		uint64_t added_blocks = 0;
		while (offset + len > remaining_space) {
			append_block(subject);
			added_blocks++;
			remaining_space = sizeof(block::data) * (curr_inode_blocks + added_blocks);
		}
	}
	block_t* current_block = subject->first_block;

	for (int i = 0; i < block_index; i++) {
		if (current_block) current_block = current_block->next;
		else return -1; // There are no blocks allocated, so there's nothing to read.
	}

	uint64_t bytes_written = 0;
	uint64_t block_offset = byte_offset;

	while (bytes_written < len) {
		const uint64_t bytes_left_in_block = sizeof(block::data) - block_offset;
		const uint64_t chunk = min(len - bytes_written, bytes_left_in_block);

		for (int i = 0; i < chunk; i++) {
			current_block->data[block_offset + i] = src[bytes_written + i];
		}

		bytes_written += chunk;
		block_offset = 0;

		current_block = current_block->next;
	}

	if (offset + len > subject->inode_size) {
		subject->inode_size = offset+len;
	}
	return len;
}

int64_t fs_readdir(const uint64_t dir_ino, const uint64_t index, dirent *out) {
	inode* subject = find_inode(dir_ino);
	if (!subject) return -1;
	if (subject->inode_type != 2) return -1;
	if (!out) return -1;
	if (index >= subject->inode_size) return 0;

	dirent* subject_dirent = dirent_at(subject, index);
	if (!subject_dirent) return -1;

	*out = *subject_dirent;

	return 1;
}

int fs_stat(const uint64_t ino, uint64_t* size_out, uint8_t* type_out) {
	const inode* n = find_inode(ino);
	if (!n) return -1;
	if (size_out) *size_out = n->inode_size;
	if (type_out) *type_out = n->inode_type;
	return 0;
}

// ++++++ BEGIN HELPER FUNCTIONS ++++++

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

	// current max of 15 dirents per block. Impl is resilient to size changes.
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
