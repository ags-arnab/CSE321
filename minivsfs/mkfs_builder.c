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
#include <unistd.h>

#define BS 4096u               // block size
#define INODE_SIZE 128u
#define ROOT_INO 1u

uint64_t g_random_seed = 0; // This should be replaced by seed value from the CLI.

// Complete data structures based on specifications
#pragma pack(push, 1)
typedef struct {
    uint32_t magic;               // 0x4D565346
    uint32_t version;             // 1  
    uint32_t block_size;          // 4096
    uint64_t total_blocks;        // calculated
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
    uint16_t mode;                // file/directory mode
    uint16_t links;               // link count
    uint32_t uid;                 // 0
    uint32_t gid;                 // 0
    uint64_t size_bytes;          // file size
    uint64_t atime;               // access time (unix epoch)
    uint64_t mtime;               // modify time (unix epoch)
    uint64_t ctime;               // create time (unix epoch)
    uint32_t direct[12];          // direct block pointers
    uint32_t reserved_0;          // 0
    uint32_t reserved_1;          // 0
    uint32_t reserved_2;          // 0
    uint32_t proj_id;             // group ID
    uint32_t uid16_gid16;         // 0
    uint64_t xattr_ptr;           // 0

    // THIS FIELD SHOULD STAY AT THE END
    // ALL OTHER FIELDS SHOULD BE ABOVE THIS
    uint64_t inode_crc;   // low 4 bytes store crc32 of bytes [0..119]; high 4 bytes 0

} inode_t;
#pragma pack(pop)
_Static_assert(sizeof(inode_t)==INODE_SIZE, "inode size mismatch");

#pragma pack(push,1)
typedef struct {
    uint32_t inode_no;            // 0 if free
    uint8_t  type;                // 1=file, 2=dir
    char     name[58];            // name string
    uint8_t  checksum;            // XOR of bytes 0..62
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

// CLI argument structure
typedef struct {
    char* image_name;
    uint32_t size_kib;
    uint32_t inodes;
} cli_args_t;

// Parse command line arguments
int parse_args(int argc, char* argv[], cli_args_t* args) {
    if (argc != 7) {
        fprintf(stderr, "Usage: %s --image <name> --size-kib <180..4096> --inodes <128..512>\n", argv[0]);
        return -1;
    }
    
    // Initialize args
    args->image_name = NULL;
    args->size_kib = 0;
    args->inodes = 0;
    
    // Parse arguments
    for (int i = 1; i < argc; i += 2) {
        if (strcmp(argv[i], "--image") == 0) {
            args->image_name = argv[i + 1];
        } else if (strcmp(argv[i], "--size-kib") == 0) {
            args->size_kib = (uint32_t)atoi(argv[i + 1]);
        } else if (strcmp(argv[i], "--inodes") == 0) {
            args->inodes = (uint32_t)atoi(argv[i + 1]);
        } else {
            fprintf(stderr, "Unknown argument: %s\n", argv[i]);
            return -1;
        }
    }
    
    // Validate arguments
    if (!args->image_name) {
        fprintf(stderr, "Error: --image is required\n");
        return -1;
    }
    
    if (args->size_kib < 180 || args->size_kib > 4096 || args->size_kib % 4 != 0) {
        fprintf(stderr, "Error: --size-kib must be between 180-4096 and multiple of 4\n");
        return -1;
    }
    
    if (args->inodes < 128 || args->inodes > 512) {
        fprintf(stderr, "Error: --inodes must be between 128-512\n");
        return -1;
    }
    
    return 0;
}

// Calculate file system layout
int calculate_layout(cli_args_t* args, superblock_t* sb) {
    time_t now = time(NULL);
    
    // Calculate total blocks
    uint64_t total_blocks = (args->size_kib * 1024ULL) / BS;
    
    // Calculate inode table blocks needed
    uint64_t inodes_per_block = BS / INODE_SIZE; // 32 inodes per block
    uint64_t inode_table_blocks = (args->inodes + inodes_per_block - 1) / inodes_per_block; // ceil division
    
    // Calculate data region
    uint64_t metadata_blocks = 1 + 1 + 1 + inode_table_blocks; // superblock + inode bitmap + data bitmap + inode table
    if (metadata_blocks >= total_blocks) {
        fprintf(stderr, "Error: Not enough space for file system metadata\n");
        return -1;
    }
    uint64_t data_region_blocks = total_blocks - metadata_blocks;
    
    // Fill superblock
    memset(sb, 0, sizeof(superblock_t));
    sb->magic = 0x4D565346;
    sb->version = 1;
    sb->block_size = BS;
    sb->total_blocks = total_blocks;
    sb->inode_count = args->inodes;
    sb->inode_bitmap_start = 1;
    sb->inode_bitmap_blocks = 1;
    sb->data_bitmap_start = 2;
    sb->data_bitmap_blocks = 1;
    sb->inode_table_start = 3;
    sb->inode_table_blocks = inode_table_blocks;
    sb->data_region_start = 3 + inode_table_blocks;
    sb->data_region_blocks = data_region_blocks;
    sb->root_inode = ROOT_INO;
    sb->mtime_epoch = (uint64_t)now;
    sb->flags = 0;
    
    return 0;
}

// Create file system image
int create_filesystem(cli_args_t* args, superblock_t* sb) {
    FILE* fp = fopen(args->image_name, "wb");
    if (!fp) {
        perror("Failed to create image file");
        return -1;
    }
    
    time_t now = time(NULL);
    
    // Create a buffer for writing entire image
    size_t image_size = sb->total_blocks * BS;
    uint8_t* image = calloc(1, image_size);
    if (!image) {
        fprintf(stderr, "Failed to allocate memory for image\n");
        fclose(fp);
        return -1;
    }
    
    // Write superblock
    superblock_crc_finalize(sb);
    memcpy(image, sb, sizeof(superblock_t));
    
    // Create inode bitmap (block 1)
    uint8_t* inode_bitmap = image + BS;
    memset(inode_bitmap, 0, BS);
    // Set bit 0 (inode 1 - root directory) as allocated
    inode_bitmap[0] |= 0x01;
    
    // Create data bitmap (block 2)  
    uint8_t* data_bitmap = image + 2 * BS;
    memset(data_bitmap, 0, BS);
    // Set bit 0 (first data block for root directory) as allocated
    data_bitmap[0] |= 0x01;
    
    // Create inode table (starting at block 3)
    uint8_t* inode_table = image + sb->inode_table_start * BS;
    memset(inode_table, 0, sb->inode_table_blocks * BS);
    
    // Create root directory inode (inode 1 at index 0)
    inode_t* root_inode = (inode_t*)inode_table;
    memset(root_inode, 0, sizeof(inode_t));
    root_inode->mode = 0040000; // directory mode (octal)
    root_inode->links = 2; // . and .. entries
    root_inode->uid = 0;
    root_inode->gid = 0;
    root_inode->size_bytes = 2 * sizeof(dirent64_t); // . and .. entries
    root_inode->atime = (uint64_t)now;
    root_inode->mtime = (uint64_t)now;
    root_inode->ctime = (uint64_t)now;
    // Root directory uses first data block
    root_inode->direct[0] = (uint32_t)sb->data_region_start; 
    for (int i = 1; i < 12; i++) {
        root_inode->direct[i] = 0; // unused
    }
    root_inode->reserved_0 = 0;
    root_inode->reserved_1 = 0;
    root_inode->reserved_2 = 0;
    root_inode->proj_id = 0; // Use 0 as default group ID
    root_inode->uid16_gid16 = 0;
    root_inode->xattr_ptr = 0;
    inode_crc_finalize(root_inode);
    
    // Create root directory data block
    uint8_t* root_data = image + sb->data_region_start * BS;
    memset(root_data, 0, BS);
    
    // Create "." entry (current directory)
    dirent64_t* dot_entry = (dirent64_t*)root_data;
    dot_entry->inode_no = ROOT_INO;
    dot_entry->type = 2; // directory
    strncpy(dot_entry->name, ".", 58);
    dot_entry->name[57] = '\0'; // ensure null termination
    dirent_checksum_finalize(dot_entry);
    
    // Create ".." entry (parent directory - points to itself for root)
    dirent64_t* dotdot_entry = (dirent64_t*)(root_data + sizeof(dirent64_t));
    dotdot_entry->inode_no = ROOT_INO;
    dotdot_entry->type = 2; // directory
    strncpy(dotdot_entry->name, "..", 58);
    dotdot_entry->name[57] = '\0'; // ensure null termination
    dirent_checksum_finalize(dotdot_entry);
    
    // Write entire image to file
    if (fwrite(image, 1, image_size, fp) != image_size) {
        perror("Failed to write image");
        free(image);
        fclose(fp);
        return -1;
    }
    
    free(image);
    fclose(fp);
    return 0;
}

int main(int argc, char* argv[]) {
    crc32_init();
    
    cli_args_t args;
    superblock_t superblock;
    
    // Parse command line arguments
    if (parse_args(argc, argv, &args) != 0) {
        return EXIT_FAILURE;
    }
    
    // Calculate file system layout
    if (calculate_layout(&args, &superblock) != 0) {
        return EXIT_FAILURE;
    }
    
    // Create the file system
    if (create_filesystem(&args, &superblock) != 0) {
        return EXIT_FAILURE;
    }
    
    printf("Successfully created MiniVSFS image: %s\n", args.image_name);
    printf("Size: %u KiB (%" PRIu64 " blocks)\n", args.size_kib, superblock.total_blocks);
    printf("Inodes: %u\n", args.inodes);
    
    return EXIT_SUCCESS;
}