/* Copyright (c) 2021 Dennis Wölfing
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* kernel/include/dennix/kernel/ext234fs.h
 * ext2/ext3/ext4 filesystem driver.
 */

#ifndef KERNEL_EXT234FS_H
#define KERNEL_EXT234FS_H

#include <dennix/kernel/dynarray.h>
#include <dennix/kernel/endian.h>
#include <dennix/kernel/filesystem.h>

struct SuperBlock {
    little_uint32_t s_inodes_count;
    little_uint32_t s_blocks_count;
    little_uint32_t s_r_blocks_count;
    little_uint32_t s_free_blocks_count;
    little_uint32_t s_free_inodes_count;
    little_uint32_t s_first_data_block;
    little_uint32_t s_log_block_size;
    little_uint32_t s_log_frag_size;
    little_uint32_t s_blocks_per_group;
    little_uint32_t s_frags_per_group;
    little_uint32_t s_inodes_per_group;
    little_uint32_t s_mtime;
    little_uint32_t s_wtime;
    little_uint16_t s_mnt_count;
    little_uint16_t s_max_mnt_count;
    little_uint16_t s_magic;
    little_uint16_t s_state;
    little_uint16_t s_errors;
    little_uint16_t s_minor_rev_level;
    little_uint32_t s_lastcheck;
    little_uint32_t s_checkinterval;
    little_uint32_t s_creator_os;
    little_uint32_t s_rev_level;
    little_uint16_t s_def_resuid;
    little_uint16_t s_def_resgid;

    // The following fields are only valid if s_rev_level >= 1.
    little_uint32_t s_first_ino;
    little_uint16_t s_inode_size;
    little_uint16_t s_block_group_nr;
    little_uint32_t s_feature_compat;
    little_uint32_t s_feature_incompat;
    little_uint32_t s_feature_ro_compat;
    char s_uuid[16];
    char s_volume_name[16];
    char s_last_mounted[64];
    little_uint32_t s_algo_bitmap;
    little_uint8_t s_prealloc_blocks;
    little_uint8_t s_prealloc_dir_blocks;
    little_uint16_t s_reserved_gdt_blocks;
    char s_journal_uuid[16];
    little_uint32_t s_journal_inum;
    little_uint32_t s_journal_dev;
    little_uint32_t s_last_orphan;

    // These fields are new in ext4.
    little_uint32_t s_hash_seed[4];
    little_uint8_t s_def_hash_version;
    little_uint8_t s_jnl_backup_type;
    little_uint16_t s_desc_size;
    little_uint32_t s_default_mount_opts;
    little_uint32_t s_first_meta_bg;
    little_uint32_t s_mkfs_time;
    little_uint32_t s_jnl_blocks[17];
    little_uint32_t s_blocks_count_hi;
    little_uint32_t s_r_blocks_count_hi;
    little_uint32_t s_free_blocks_count_hi;
    little_uint16_t s_min_extra_isize;
    little_uint16_t s_want_extra_isize;
    little_uint32_t s_flags;
    little_uint16_t s_raid_stride;
    little_uint16_t s_mmp_update_interval;
    little_uint64_t s_mmp_block;
    little_uint32_t s_raid_stripe_width;
    little_uint8_t s_log_groups_per_flex;
    little_uint8_t s_checksum_type;
    little_uint8_t s_encryption_level;
    little_uint8_t reserved;
    little_uint64_t s_kbytes_written;
    little_uint32_t s_snapshot_inum;
    little_uint32_t s_snapshot_id;
    little_uint64_t s_snapshot_r_blocks_count;
    little_uint32_t s_snapshot_list;
    little_uint32_t s_error_count;
    little_uint32_t s_first_error_time;
    little_uint32_t s_first_error_ino;
    little_uint64_t s_first_error_block;
    char s_first_error_func[32];
    little_uint32_t s_first_error_line;
    little_uint32_t s_last_error_time;
    little_uint32_t s_last_error_ino;
    little_uint32_t s_last_error_line;
    little_uint32_t s_last_error_block;
    char s_last_error_func[32];
    char s_mount_opts[64];
    little_uint32_t s_usr_quota_inum;
    little_uint32_t s_grp_quota_inum;
    little_uint32_t s_overhead_clusters;
    little_uint32_t s_backup_bgs[2];
    little_uint8_t s_encrypt_algos[4];
    char s_encrypt_pw_salt[16];
    little_uint32_t s_lpf_ino;
    little_uint32_t s_prj_quota_inum;
    little_uint32_t s_checksum_seed;
    little_uint8_t s_wtime_hi;
    little_uint8_t s_mtime_hi;
    little_uint8_t s_mkfs_time_hi;
    little_uint8_t s_lastcheck_hi;
    little_uint8_t s_first_error_time_hi;
    little_uint8_t s_last_error_time_hi;
    little_uint8_t s_first_error_errcode;
    little_uint8_t s_last_error_errcode;
    little_uint16_t s_encoding;
    little_uint16_t s_encoding_flags;
    char padding[380];
    little_uint32_t s_checksum;
};

struct Inode {
    little_uint16_t i_mode;
    little_uint16_t i_uid;
    little_uint32_t i_size;
    little_uint32_t i_atime;
    little_uint32_t i_ctime;
    little_uint32_t i_mtime;
    little_uint32_t i_dtime;
    little_uint16_t i_gid;
    little_uint16_t i_links_count;
    little_uint32_t i_blocks;
    little_uint32_t i_flags;
    little_uint32_t i_osd1;
    little_uint32_t i_block[15];
    little_uint32_t i_generation;

    // The next two are only valid if s_rev_level >= 1.
    little_uint32_t i_file_acl;
    little_uint32_t i_size_high;
    little_uint32_t i_faddr;
    little_uint8_t i_osd2[12];

    little_uint16_t i_extra_isize;
    little_uint16_t i_checksum_hi;
    little_uint32_t i_ctime_extra;
    little_uint32_t i_mtime_extra;
    little_uint32_t i_atime_extra;
    little_uint32_t i_crtime;
    little_uint32_t i_crtime_extra;
    little_uint32_t i_version_hi;
    little_uint32_t i_projid;
};

struct BlockGroupDescriptor {
    little_uint32_t bg_block_bitmap;
    little_uint32_t bg_inode_bitmap;
    little_uint32_t bg_inode_table;
    little_uint16_t bg_free_blocks_count;
    little_uint16_t bg_free_inodes_count;
    little_uint16_t bg_used_dirs_count;
    // These fields are new in ext4.
    little_uint16_t bg_flags;
    little_uint32_t bg_exclude_bitmap_lo;
    little_uint16_t bg_block_bitmal_csum_lo;
    little_uint16_t bg_inode_bitmap_csum_lo;
    little_uint16_t bg_itable_unused_lo;
    little_uint16_t bg_checksum;
    little_uint32_t bg_block_bitmap_hi;
    little_uint32_t bg_inode_bitmap_hi;
    little_uint32_t bg_inode_table_hi;
    little_uint16_t bg_free_blocks_count_hi;
    little_uint16_t bg_free_inodes_count_hi;
    little_uint16_t bg_used_dirs_count_hi;
    little_uint16_t bg_itable_unused_hi;
    little_uint32_t bg_exclude_bitmap_hi;
    little_uint16_t bg_block_bitmal_csum_hi;
    little_uint16_t bg_inode_bitmap_csum_hi;
    little_uint32_t reserved;
};

struct DirectoryEntry {
    little_uint32_t inode;
    little_uint16_t rec_len;
    little_uint8_t name_len;
    little_uint8_t file_type;
    char name[];
};

#define INCOMPAT_FILETYPE 0x2
#define INCOMPAT_64BIT 0x80

#define RO_COMPAT_SPARSE_SUPER 0x1
#define RO_COMPAT_LARGE_FILE 0x2
#define RO_COMPAT_EXTRA_ISIZE 0x40

#define SUPPORTED_INCOMPAT_FEATURES (INCOMPAT_FILETYPE | INCOMPAT_64BIT)
#define SUPPORTED_RO_FEATURES \
        (RO_COMPAT_SPARSE_SUPER | RO_COMPAT_LARGE_FILE | RO_COMPAT_EXTRA_ISIZE)

#define STATE_CLEAN 0x1

class Ext234Vnode;

class Ext234Fs : public FileSystem {
public:
    Ext234Fs(const Reference<Vnode>& device, const SuperBlock* superBlock,
            const Reference<Vnode>& mountPoint, bool readonly);
    ino_t createInode(uint64_t blockGroup, mode_t mode);
    bool deallocateInode(ino_t ino, bool dir);
    void dropVnodeReference(ino_t ino);
    void finishDropVnodeReference();
    uint64_t getBlockGroup(ino_t ino);
    struct timespec getInodeATime(const Inode* inode);
    struct timespec getInodeCTime(const Inode* inode);
    struct timespec getInodeMTime(const Inode* inode);
    uint64_t getInodeSize(const Inode* inode);
    Reference<Vnode> getRootDir() override;
    Reference<Ext234Vnode> getVnode(ino_t ino);
    Reference<Ext234Vnode> getVnodeIfOpen(ino_t ino);
    bool hasIncompatFeature(uint32_t feature);
    bool onUnmount() override;
    bool readInodeData(const Inode* inode, off_t offset, void* buffer,
            size_t size);
    bool resizeInode(ino_t ino, Inode* inode, off_t newSize);
    void setTime(struct timespec* ts, little_uint32_t* time,
            little_uint32_t* extraTime);
    bool writeInode(const Inode* inode, uint64_t inodeAddress);
    bool writeInodeData(const Inode* inode, off_t offset, const void* buffer,
            size_t size);
private:
    uint64_t allocateBlock(uint64_t blockGroup);
    uint64_t allocateBlockInGroup(uint64_t blockGroup,
            BlockGroupDescriptor* bg, uint32_t freeBlocks);
    ino_t allocateInode(uint64_t blockGroup, bool dir);
    ino_t allocateInodeInGroup(uint64_t blockGroup,
            BlockGroupDescriptor* bg, uint32_t freeInodes, bool dir);
    bool deallocateBlock(uint64_t blockNumber);
    bool decreaseInodeBlockCount(Inode* inode, uint64_t oldBlockCount,
            uint64_t newBlockCount);
    uint64_t getBlockCount(uint64_t fileSize);
    uint64_t getInodeBlockAddress(const Inode* inode, uint64_t block);
    bool hasReadOnlyFeature(uint32_t feature);
    bool increaseInodeBlockCount(ino_t ino, Inode* inode,
            uint64_t oldBlockCount, uint64_t newBlockCount);
    bool read(void* buffer, size_t size, off_t offset);
    bool readBlockGroupDesc(uint64_t blockGroup, BlockGroupDescriptor* bg);
    bool readInode(uint64_t ino, Inode* inode, uint64_t& inodeAddress);
    bool write(const void* buffer, size_t size, off_t offset);
    bool writeSuperBlock();
public:
    uint64_t blockSize;
    dev_t dev;
    size_t inodeSize;
    Reference<Vnode> mountPoint;
    bool readonly;
private:
    Reference<Vnode> device;
    uint64_t groupCount;
    size_t gdtSize;
    kthread_mutex_t mutex;
    size_t openVnodes;
    SuperBlock superBlock;
    DynamicArray<Ext234Vnode*, ino_t> vnodes;
};

class Ext234Vnode : public Vnode {
public:
    Ext234Vnode(Ext234Fs* fs, ino_t ino, const Inode* inode,
            uint64_t inodeAddress);
    ~Ext234Vnode();
    int chmod(mode_t mode) override;
    int chown(uid_t uid, gid_t gid) override;
    int ftruncate(off_t length) override;
    Reference<Vnode> getChildNode(const char* name) override;
    Reference<Vnode> getChildNode(const char* path, size_t length) override;
    size_t getDirectoryEntries(void** buffer, int flags) override;
    char* getLinkTarget() override;
    bool isSeekable() override;
    int link(const char* name, const Reference<Vnode>& vnode) override;
    off_t lseek(off_t offset, int whence) override;
    int mkdir(const char* name, mode_t mode) override;
    int mount(FileSystem* filesystem) override;
    void onLink() override;
    bool onUnlink(bool force) override;
    Reference<Vnode> open(const char* name, int flags, mode_t mode) override;
    short poll() override;
    ssize_t pread(void* buffer, size_t size, off_t offset, int flags) override;
    ssize_t pwrite(const void* buffer, size_t size, off_t offset, int flags)
            override;
    ssize_t readlink(char* buffer, size_t size) override;
    void removeReference() const override;
    int rename(const Reference<Vnode>& oldDirectory, const char* oldName,
            const char* newName) override;
    Reference<Vnode> resolve() override;
    int symlink(const char* linkTarget, const char* name) override;
    int unlink(const char* name, int flags) override;
    int unmount() override;
    int utimens(struct timespec atime, struct timespec mtime) override;
protected:
    void updateTimestamps(bool access, bool status, bool modification) override;
private:
    bool addChildNode(const char* name, size_t nameLength, ino_t ino,
            unsigned char dt);
    uint64_t findDirectoryEntry(const char* name, size_t nameLength,
            DirectoryEntry* de);
    Reference<Vnode> getChildNodeUnlocked(const char* path, size_t length);
    int linkUnlocked(const char* name, size_t nameLength,
            const Reference<Vnode>& vnode);
    int unlinkUnlocked(const char* name, int flags);
    bool updateParent(const Reference<Ext234Vnode>& parent);
    void writeTimestamps();
private:
    Ext234Fs* filesystem;
    Inode inode;
    uint64_t inodeAddress;
    bool inodeModified;
    FileSystem* mounted;
};

#endif
