// Build: gcc -O2 -std=c17 -Wall -Wextra mkfs_builder.c -o mkfs_builder
#define _FILE_OFFSET_BITS 64
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>
#include <time.h>
#include <assert.h>

#define BS 4096u               // block size
#define INODE_SIZE 128u
#define ROOT_INO 1u

uint64_t g_random_seed = 0; // This should be replaced by seed value from the CLI.

// below contains some basic structures you need for your project
// you are free to create more structures as you require

#pragma pack(push, 1)
typedef struct {
    // CREATE YOUR SUPERBLOCK HERE
    // ADD ALL FIELDS AS PROVIDED BY THE SPECIFICATION
    uint32_t magic;               // 0x4D565346
    uint32_t version;             // 1
    uint32_t block_size;          // 4096
    uint64_t total_blocks;        // size_kib * 1024 / 4096
    uint64_t inode_count;         // from CLI
    uint64_t inode_bitmap_start;  // 1
    uint64_t inode_bitmap_blocks; // 1
    uint64_t data_bitmap_start;   // 2
    uint64_t data_bitmap_blocks;  // 1
    uint64_t inode_table_start;   // 3
    uint64_t inode_table_blocks;  // calculated
    uint64_t data_region_start;   // calculated
    uint64_t data_region_blocks;  // calculated
    uint64_t root_inode;          // 1
    uint64_t mtime_epoch;         // build time
    uint32_t flags;               // 0
    
    // THIS FIELD SHOULD STAY AT THE END
    // ALL OTHER FIELDS SHOULD BE ABOVE THIS
    uint32_t checksum;            // crc32(superblock[0..4091])
} superblock_t;
#pragma pack(pop)
_Static_assert(sizeof(superblock_t) == 116, "superblock must fit in one block");

#pragma pack(push,1)
typedef struct {
    // CREATE YOUR INODE HERE
    // IF CREATED CORRECTLY, THE STATIC_ASSERT ERROR SHOULD BE GONE
    uint16_t mode;           // file/directory mode
    uint16_t links;          // link count
    uint32_t uid;            // user id (0)
    uint32_t gid;            // group id (0)
    uint64_t size_bytes;     // size in bytes
    uint64_t atime;          // access time
    uint64_t mtime;          // modification time
    uint64_t ctime;          // creation time
    uint32_t direct[12];     // direct block pointers
    uint32_t reserved_0;     // 0
    uint32_t reserved_1;     // 0
    uint32_t reserved_2;     // 0
    uint32_t proj_id;        // project id
    uint32_t uid16_gid16;    // 0
    uint64_t xattr_ptr;      // 0

    // THIS FIELD SHOULD STAY AT THE END
    // ALL OTHER FIELDS SHOULD BE ABOVE THIS
    uint64_t inode_crc;   // low 4 bytes store crc32 of bytes [0..119]; high 4 bytes 0

} inode_t;
#pragma pack(pop)
_Static_assert(sizeof(inode_t)==INODE_SIZE, "inode size mismatch");

#pragma pack(push,1)
typedef struct {
    // CREATE YOUR DIRECTORY ENTRY STRUCTURE HERE
    // IF CREATED CORRECTLY, THE STATIC_ASSERT ERROR SHOULD BE GONE
    uint32_t inode_no;   // inode number (0 if free)
    uint8_t type;        // 1=file, 2=directory
    char name[58];       // filename/dirname

    uint8_t  checksum; // XOR of bytes 0..62
} dirent64_t;
#pragma pack(pop)
_Static_assert(sizeof(dirent64_t)==64, "dirent size mismatch");


// ==========================DO NOT CHANGE THIS PORTION=========================
// These functions are there for your help. You should refer to the specifications to see how you can use them.
// ====================================CRC32====================================
uint32_t CRC32_TAB[256];
void crc32_init(void){
    for (uint32_t i=0;i<256;i++){
        uint32_t c=i;
        for(int j=0;j<8;j++) c = (c&1)?(0xEDB88320u^(c>>1)):(c>>1);
        CRC32_TAB[i]=c;
    }
}
uint32_t crc32(const void* data, size_t n){
    const uint8_t* p=(const uint8_t*)data; uint32_t c=0xFFFFFFFFu;
    for(size_t i=0;i<n;i++) c = CRC32_TAB[(c^p[i])&0xFF] ^ (c>>8);
    return c ^ 0xFFFFFFFFu;
}
// ====================================CRC32====================================

// WARNING: CALL THIS ONLY AFTER ALL OTHER SUPERBLOCK ELEMENTS HAVE BEEN FINALIZED
static uint32_t superblock_crc_finalize(superblock_t *sb) {
    sb->checksum = 0;
    uint32_t s = crc32((void *) sb, BS - 4);
    sb->checksum = s;
    return s;
}

// WARNING: CALL THIS ONLY AFTER ALL OTHER SUPERBLOCK ELEMENTS HAVE BEEN FINALIZED
void inode_crc_finalize(inode_t* ino){
    uint8_t tmp[INODE_SIZE]; memcpy(tmp, ino, INODE_SIZE);
    // zero crc area before computing
    memset(&tmp[120], 0, 8);
    uint32_t c = crc32(tmp, 120);
    ino->inode_crc = (uint64_t)c; // low 4 bytes carry the crc
}

// WARNING: CALL THIS ONLY AFTER ALL OTHER SUPERBLOCK ELEMENTS HAVE BEEN FINALIZED
void dirent_checksum_finalize(dirent64_t* de) {
    const uint8_t* p = (const uint8_t*)de;
    uint8_t x = 0;
    for (int i = 0; i < 63; i++) x ^= p[i];   // covers ino(4) + type(1) + name(58)
    de->checksum = x;
}

void print_usage(const char* prog_name) {
    fprintf(stderr, "Usage: %s --image <output.img> --size-kib <180..4096> --inodes <128..512>\n", prog_name);
}

int main(int argc, char* argv[]) {
    crc32_init();
    
    char* image_name = NULL;
    int size_kib = 0;
    int inodes = 0;
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--image") == 0 && i + 1 < argc) {
            image_name = argv[++i];
        } else if (strcmp(argv[i], "--size-kib") == 0 && i + 1 < argc) {
            size_kib = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--inodes") == 0 && i + 1 < argc) {
            inodes = atoi(argv[++i]);
        } else {
            print_usage(argv[0]);
            return 1;
        }
    }
    
    // Validate arguments
    if (!image_name || size_kib < 180 || size_kib > 4096 || size_kib % 4 != 0 || 
        inodes < 128 || inodes > 512) {
        print_usage(argv[0]);
        return 1;
    }
    
    // Calculate file system layout
    uint64_t total_blocks = (size_kib * 1024) / BS;
    uint64_t inode_table_blocks = (inodes * INODE_SIZE + BS - 1) / BS;  // round up
    uint64_t inode_table_start = 3;  // after superblock, inode bitmap, data bitmap
    uint64_t data_region_start = inode_table_start + inode_table_blocks;
    uint64_t data_region_blocks = total_blocks - data_region_start;
    
    // Validate we have enough space
    if (data_region_blocks <= 0) {
        fprintf(stderr, "Error: Not enough space for data region\n");
        return 1;
    }
    
    time_t build_time = time(NULL);
    
    // Create and fill superblock
    superblock_t sb;
    memset(&sb, 0, sizeof(sb));
    sb.magic = 0x4D565346;
    sb.version = 1;
    sb.block_size = BS;
    sb.total_blocks = total_blocks;
    sb.inode_count = inodes;
    sb.inode_bitmap_start = 1;
    sb.inode_bitmap_blocks = 1;
    sb.data_bitmap_start = 2;
    sb.data_bitmap_blocks = 1;
    sb.inode_table_start = inode_table_start;
    sb.inode_table_blocks = inode_table_blocks;
    sb.data_region_start = data_region_start;
    sb.data_region_blocks = data_region_blocks;
    sb.root_inode = ROOT_INO;
    sb.mtime_epoch = build_time;
    sb.flags = 0;
    superblock_crc_finalize(&sb);
    
    // Create root directory inode
    inode_t root_inode;
    memset(&root_inode, 0, sizeof(root_inode));
    root_inode.mode = 0040000;  // directory mode (0040000)8
    root_inode.links = 2;       // . and ..
    root_inode.uid = 0;
    root_inode.gid = 0;
    root_inode.size_bytes = 2 * sizeof(dirent64_t);  // . and .. entries
    root_inode.atime = build_time;
    root_inode.mtime = build_time;
    root_inode.ctime = build_time;
    root_inode.direct[0] = data_region_start;  // first data block
    for (int i = 1; i < 12; i++) {
        root_inode.direct[i] = 0;
    }
    root_inode.reserved_0 = 0;
    root_inode.reserved_1 = 0;
    root_inode.reserved_2 = 0;
    root_inode.proj_id = 0;  // Could be set to group ID if needed
    root_inode.uid16_gid16 = 0;
    root_inode.xattr_ptr = 0;
    inode_crc_finalize(&root_inode);
    
    // Create . and .. directory entries
    dirent64_t dot_entry, dotdot_entry;
    memset(&dot_entry, 0, sizeof(dot_entry));
    dot_entry.inode_no = ROOT_INO;
    dot_entry.type = 2;  // directory
    strcpy(dot_entry.name, ".");
    dirent_checksum_finalize(&dot_entry);
    
    memset(&dotdot_entry, 0, sizeof(dotdot_entry));
    dotdot_entry.inode_no = ROOT_INO;
    dotdot_entry.type = 2;  // directory
    strcpy(dotdot_entry.name, "..");
    dirent_checksum_finalize(&dotdot_entry);
    
    // Open output file
    FILE* fp = fopen(image_name, "wb");
    if (!fp) {
        fprintf(stderr, "Error: Cannot create output file '%s': %s\n", image_name, strerror(errno));
        return 1;
    }
    
    // Write superblock (block 0)
    uint8_t block_buffer[BS];
    memset(block_buffer, 0, BS);
    memcpy(block_buffer, &sb, sizeof(sb));
    fwrite(block_buffer, 1, BS, fp);
    
    // Write inode bitmap (block 1) - mark root inode as used
    memset(block_buffer, 0, BS);
    block_buffer[0] = 0x01;  // bit 0 set (inode #1 used)
    fwrite(block_buffer, 1, BS, fp);
    
    // Write data bitmap (block 2) - mark first data block as used
    memset(block_buffer, 0, BS);
    block_buffer[0] = 0x01;  // bit 0 set (first data block used)
    fwrite(block_buffer, 1, BS, fp);
    
    // Write inode table - root inode at position 0
    for (uint64_t block = 0; block < inode_table_blocks; block++) {
        memset(block_buffer, 0, BS);
        if (block == 0) {
            // First block contains root inode at index 0
            memcpy(block_buffer, &root_inode, sizeof(root_inode));
        }
        fwrite(block_buffer, 1, BS, fp);
    }
    
    // Write data region - first block contains root directory entries
    for (uint64_t block = 0; block < data_region_blocks; block++) {
        memset(block_buffer, 0, BS);
        if (block == 0) {
            // First data block contains . and .. entries
            memcpy(block_buffer, &dot_entry, sizeof(dot_entry));
            memcpy(block_buffer + sizeof(dot_entry), &dotdot_entry, sizeof(dotdot_entry));
        }
        fwrite(block_buffer, 1, BS, fp);
    }
    
    fclose(fp);
    
    printf("MiniVSFS image '%s' created successfully:\n", image_name);
    printf("  Size: %d KiB (%" PRIu64 " blocks)\n", size_kib, total_blocks);
    printf("  Inodes: %d\n", inodes);
    printf("  Data blocks: %" PRIu64 "\n", data_region_blocks);
    
    return 0;
}