#define _FILE_OFFSET_BITS 64
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>
#include <time.h>
#include <sys/stat.h>

#define BS 4096u
#define INODE_SIZE 128u
#define ROOT_INO 1u
#define DIRECT_MAX 12
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
    fprintf(stderr, "Usage: %s --input <input.img> --output <output.img> --file <filename>\n", prog_name);
}

int find_free_inode(uint8_t* inode_bitmap, uint64_t inode_count) {
    // First-fit allocation for inodes (1-indexed)
    for (uint64_t i = 0; i < inode_count; i++) {
        uint64_t byte_idx = i / 8;
        uint64_t bit_idx = i % 8;
        if (!(inode_bitmap[byte_idx] & (1 << bit_idx))) {
            return i + 1; // 1-indexed
        }
    }
    return -1; // No free inode
}

int find_free_data_block(uint8_t* data_bitmap, uint64_t data_blocks) {
    // First-fit allocation for data blocks
    for (uint64_t i = 0; i < data_blocks; i++) {
        uint64_t byte_idx = i / 8;
        uint64_t bit_idx = i % 8;
        if (!(data_bitmap[byte_idx] & (1 << bit_idx))) {
            return i;
        }
    }
    return -1; // No free data block
}

void set_bitmap_bit(uint8_t* bitmap, int bit_number) {
    uint64_t byte_idx = bit_number / 8;
    uint64_t bit_idx = bit_number % 8;
    bitmap[byte_idx] |= (1 << bit_idx);
}

int main(int argc, char* argv[]) {
    crc32_init();
    
    char* input_name = NULL;
    char* output_name = NULL;
    char* file_name = NULL;
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--input") == 0 && i + 1 < argc) {
            input_name = argv[++i];
        } else if (strcmp(argv[i], "--output") == 0 && i + 1 < argc) {
            output_name = argv[++i];
        } else if (strcmp(argv[i], "--file") == 0 && i + 1 < argc) {
            file_name = argv[++i];
        } else {
            print_usage(argv[0]);
            return 1;
        }
    }
    
    if (!input_name || !output_name || !file_name) {
        print_usage(argv[0]);
        return 1;
    }
    
    // Check if file exists and get its size
    struct stat file_stat;
    if (stat(file_name, &file_stat) != 0) {
        fprintf(stderr, "Error: Cannot access file '%s': %s\n", file_name, strerror(errno));
        return 1;
    }
    
    if (!S_ISREG(file_stat.st_mode)) {
        fprintf(stderr, "Error: '%s' is not a regular file\n", file_name);
        return 1;
    }
    
    uint64_t file_size = file_stat.st_size;
    uint64_t blocks_needed = (file_size + BS - 1) / BS; // Round up
    
    if (blocks_needed > DIRECT_MAX) {
        fprintf(stderr, "Error: File too large (requires %" PRIu64 " blocks, max %d)\n", blocks_needed, DIRECT_MAX);
        return 1;
    }
    
    // Open input image
    FILE* input_fp = fopen(input_name, "rb");
    if (!input_fp) {
        fprintf(stderr, "Error: Cannot open input file '%s': %s\n", input_name, strerror(errno));
        return 1;
    }
    
    // Read superblock
    superblock_t sb;
    fread(&sb, sizeof(sb), 1, input_fp);
    
    // Validate magic number
    if (sb.magic != 0x4D565346) {
        fprintf(stderr, "Error: Invalid file system magic number\n");
        fclose(input_fp);
        return 1;
    }
    
    // Read entire image into memory for easier manipulation
    fseek(input_fp, 0, SEEK_END);
    long image_size = ftell(input_fp);
    fseek(input_fp, 0, SEEK_SET);
    
    uint8_t* image_data = malloc(image_size);
    fread(image_data, 1, image_size, input_fp);
    fclose(input_fp);
    
    // Get pointers to different sections
    uint8_t* inode_bitmap = image_data + sb.inode_bitmap_start * BS;
    uint8_t* data_bitmap = image_data + sb.data_bitmap_start * BS;
    inode_t* inode_table = (inode_t*)(image_data + sb.inode_table_start * BS);
    
    // Find free inode
    int free_inode_num = find_free_inode(inode_bitmap, sb.inode_count);
    if (free_inode_num < 0) {
        fprintf(stderr, "Error: No free inodes available\n");
        free(image_data);
        return 1;
    }
    
    // Find free data blocks
    int free_blocks[DIRECT_MAX];
    int blocks_found = 0;
    for (uint64_t i = 0; i < sb.data_region_blocks && (uint64_t)blocks_found < blocks_needed; i++) {
        if (find_free_data_block(data_bitmap, sb.data_region_blocks) >= 0) {
            uint64_t byte_idx = i / 8;
            uint64_t bit_idx = i % 8;
            if (!(data_bitmap[byte_idx] & (1 << bit_idx))) {
                free_blocks[blocks_found++] = i;
            }
        }
    }
    
    if ((uint64_t)blocks_found < blocks_needed) {
        fprintf(stderr, "Error: Not enough free data blocks (need %" PRIu64 ", found %d)\n", blocks_needed, blocks_found);
        free(image_data);
        return 1;
    }
    
    // Read file content
    FILE* file_fp = fopen(file_name, "rb");
    if (!file_fp) {
        fprintf(stderr, "Error: Cannot open file '%s': %s\n", file_name, strerror(errno));
        free(image_data);
        return 1;
    }
    
    // Mark inode as used
    set_bitmap_bit(inode_bitmap, free_inode_num - 1); // Convert to 0-indexed for bitmap
    
    // Mark data blocks as used
    for (uint64_t i = 0; i < blocks_needed; i++) {
        set_bitmap_bit(data_bitmap, free_blocks[i]);
    }
    
    // Create new inode for the file
    time_t current_time = time(NULL);
    inode_t* new_inode = &inode_table[free_inode_num - 1]; // Convert to 0-indexed for array
    memset(new_inode, 0, sizeof(inode_t));
    new_inode->mode = 0100000; // file mode (0100000)8
    new_inode->links = 1;
    new_inode->uid = 0;
    new_inode->gid = 0;
    new_inode->size_bytes = file_size;
    new_inode->atime = current_time;
    new_inode->mtime = current_time;
    new_inode->ctime = current_time;
    
    // Set direct block pointers
    for (uint64_t i = 0; i < blocks_needed; i++) {
        new_inode->direct[i] = sb.data_region_start + free_blocks[i];
    }
    for (uint64_t i = blocks_needed; i < DIRECT_MAX; i++) {
        new_inode->direct[i] = 0;
    }
    
    new_inode->reserved_0 = 0;
    new_inode->reserved_1 = 0;
    new_inode->reserved_2 = 0;
    new_inode->proj_id = 0;
    new_inode->uid16_gid16 = 0;
    new_inode->xattr_ptr = 0;
    inode_crc_finalize(new_inode);
    
    // Write file data to allocated blocks
    for (uint64_t i = 0; i < blocks_needed; i++) {
        uint64_t block_offset = (sb.data_region_start + free_blocks[i]) * BS;
        uint64_t bytes_to_read = (i == blocks_needed - 1) ? 
                                (file_size - i * BS) : BS;
        
        fread(image_data + block_offset, 1, bytes_to_read, file_fp);
        
        // Zero-pad the rest of the block if needed
        if (bytes_to_read < BS) {
            memset(image_data + block_offset + bytes_to_read, 0, BS - bytes_to_read);
        }
    }
    fclose(file_fp);
    
    // Add directory entry to root directory
    inode_t* root_inode = &inode_table[ROOT_INO - 1];
    uint64_t root_data_block_offset = root_inode->direct[0] * BS;
    dirent64_t* root_entries = (dirent64_t*)(image_data + root_data_block_offset);
    
    // Find free directory entry slot
    int entries_per_block = BS / sizeof(dirent64_t);
    int free_entry_idx = -1;
    for (int i = 0; i < entries_per_block; i++) {
        if (root_entries[i].inode_no == 0) {
            free_entry_idx = i;
            break;
        }
    }
    
    if (free_entry_idx < 0) {
        fprintf(stderr, "Error: Root directory is full\n");
        free(image_data);
        return 1;
    }
    
    // Create new directory entry
    dirent64_t* new_entry = &root_entries[free_entry_idx];
    memset(new_entry, 0, sizeof(dirent64_t));
    new_entry->inode_no = free_inode_num;
    new_entry->type = 1; // file
    
    // Extract just the filename without path
    const char* basename = strrchr(file_name, '/');
    basename = basename ? basename + 1 : file_name;
    strncpy(new_entry->name, basename, 57);
    new_entry->name[57] = '\0'; // Ensure null termination
    
    dirent_checksum_finalize(new_entry);
    
    // Update root directory size and link count
    root_inode->size_bytes += sizeof(dirent64_t);
    root_inode->links += 1; // CRITICAL: Increment root link count as per PDF spec
    root_inode->mtime = current_time;
    inode_crc_finalize(root_inode);
    
    // Update superblock checksum
    memcpy(image_data, &sb, sizeof(sb));
    superblock_crc_finalize((superblock_t*)image_data);
    
    // Write output image
    FILE* output_fp = fopen(output_name, "wb");
    if (!output_fp) {
        fprintf(stderr, "Error: Cannot create output file '%s': %s\n", output_name, strerror(errno));
        free(image_data);
        return 1;
    }
    
    fwrite(image_data, 1, image_size, output_fp);
    fclose(output_fp);
    free(image_data);
    
    printf("File '%s' added successfully to image '%s'\n", file_name, output_name);
    printf("  Inode: %d\n", free_inode_num);
    printf("  Size: %" PRIu64 " bytes (%" PRIu64 " blocks)\n", file_size, blocks_needed);
    
    return 0;
}