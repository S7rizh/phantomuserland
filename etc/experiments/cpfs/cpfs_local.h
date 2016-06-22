/**
 *
 * CPFS
 *
 * Copyright (C) 2016-2016 Dmitry Zavalishin, dz@dz.ru
 *
 * Internal structures.
 *
 *
**/


typedef uint_64_t cpfs_blkno_t; // disk block number
typedef uint_64_t cpfs_ino_t;   // number of inode
typedef uint_32_t cpfs_direntno_t; // number of a dir entry in a directory 'file'
typedef uint_64_t cpfs_fpos_t;  // file position or size
typedef uint_64_t cpfs_time_t;  // file c/m/a time - TODO units, 0 time?



struct cpfs_sb
{
    uint32_t            sb_magic_0;

    uint32_t            ninode;                 // Number of inodes
    cpfs_blkno_t        itable_pos;             // Disk block where inode table starts? Need?
    cpfs_blkno_t        itable_end;             // First unused block of inode table, this one and rest is not initialized. Used for fats mkfs.

    cpfs_blkno_t        disk_size;
    cpfs_blkno_t        first_unallocated; 	// Number of block at end of FS we didn't use at all. From this point up to the end all blocks are free. 0 if not used. Used for fats mkfs.
    cpfs_blkno_t        free_list;              // Head of free block list, or 0 if none

};

extern struct cpfs_sb fs_sb;


struct cpfs_dir_entry
{
    cpfs_ino_t          inode;
    char                name[CPFS_MAX_FNAME_LEN];
};


struct cpfs_inode
{
    cpfs_fpos_t         fsize;
    uint32_t            nlinks; // allways 0 or 1 in this verstion, made for future extensions, if 0 - inode record is free.

    cpfs_time_t         ctime; // created
    cpfs_time_t         atime; // accessed
    cpfs_time_t         mtime; // modified
    cpfs_time_t         vtime; // version of file forked (not used, will mark time when this backup version of file is forked from main version)

    cpfs_blkno_t        acl;    // disk block with access control list, unused now
    cpfs_blkno_t        log;    // disk block of audit log list, not used now, will contain log of all operations to support transactions

    cpfs_blkno_t        blocks0[CPFS_INO_DIR_BLOCKS];
    cpfs_blkno_t        blocks1; // Block no of list of 1-level indirect blocks list
    cpfs_blkno_t        blocks2; // Block no of list of 2-level indirect blocks list
    cpfs_blkno_t        blocks3; // Block no of list of 3-level indirect blocks list
};

// curr size of inode is 344 bytes, we'll allocate 512 for any case


errno_t 		cpfs_init_sb(void);
void 			cpfs_sb_lock();
errno_t			cpfs_sb_unlock_write(); // if returns error, sb is not written and IS NOT UNLOCKED
void 			cpfs_sb_unlock();


// TODO multiple fs?


cpfs_blkno_t    	cpfs_alloc_disk_block( void );
void            	cpfs_free_disk_block( cpfs_blkno_t blk );


cpfs_blkno_t    	cpfs_block_4_inode( cpfs_ino_t ino );
cpfs_blkno_t    	cpfs_find_block_4_file( cpfs_ino_t ino, cpfs_blkno_t logical ); // maps logical blocks to physical, block must be allocated
cpfs_blkno_t    	cpfs_alloc_block_4_file( cpfs_ino_t ino, cpfs_blkno_t logical ); // allocates logical block, returns physical blk pos, block must NOT be allocated
// no file trim?

struct cpfs_inode *     cpfs_lock_ino( cpfs_ino_t ino ); // makes sure that inode is in memory and no one modifies it
void                    cpfs_touch_ino( cpfs_ino_t ino ); // marks inode as dirty, will be saved to disk on unlock
void                    cpfs_unlock_ino( cpfs_ino_t ino ); // flushes inode to disk before unlocking it, if touched



cpfs_ino_t      	cpfs_alloc_inode( void );
void            	cpfs_free_inode( cpfs_ino ino ); // deletes file



// TODO problem: linear scan can be very slow, need tree?

errno_t			cpfs_alloc_dirent( cpfs_ino_t dir_ino, const char *fname, cpfs_ino_t file_ino ); // allocate a new dir entry in a dir
errno_t			cpfs_free_dirent( cpfs_ino_t dir_ino, const char *fname ); // free dir entry (write 0 to inode field)
errno_t			cpfs_namei( cpfs_ino_t dir_ino, const char *fname, cpfs_ino_t *file_ino ); // find name


// Finds/reads/creates/updates dir entry
errno_t                 cpfs_dir_scan( cpfs_ino_t dir_ino, const char *fname, cpfs_ino_t *file_ino, int flags );

#define CPFS_DIR_SCAN_MUST_EXIST 	(1<<1) // returns ENOENT if not exist
#define CPFS_DIR_SCAN_WRITE 		(1<<2) // writes given ino to dir entry, not reads

// TODO dirent name cache - in memory hash? Not really as usual client software opens file once in its life?



void *     		cpfs_lock_blk(   cpfs_blkno_t blk ); // makes sure that block is in memory 
void                    cpfs_touch_blk(  cpfs_blkno_t blk ); // marks block as dirty, will be saved to disk on unlock
void                    cpfs_unlock_blk( cpfs_blkno_t blk ); // flushes block to disk before unlocking it, if touched



// Internal read/wrire impl, used by user calls and internal directory io code

errno_t         	cpfs_ino_file_read  ( cpfs_ino_t ino, cpfs_size_t pos, const void *data, cpfs_size_t size );
errno_t         	cpfs_ino_file_write ( cpfs_ino_t ino, cpfs_size_t pos, const void *data, cpfs_size_t size );


