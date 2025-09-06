#define _FILE_OFFSET_BITS 64
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <inttypes.h>
#include <errno.h>
#include <time.h>
#include <assert.h>
#include <sys/stat.h>

#define BS 4096u
#define INODE_SIZE 128u
#define ROOT_INO 1u
#define DIRECT_MAX 12

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
    char* input_name;
    char* output_name;
    char* file_name;
} cli_args_t;

// Parse command line arguments
int parse_args(int argc, char* argv[], cli_args_t* args) {
    if (argc != 7) {
        fprintf(stderr, "Usage: %s --input <input.img> --output <output.img> --file <filename>\n", argv[0]);
        return -1;
    }
    
    // Initialize args
    args->input_name = NULL;
    args->output_name = NULL;
    args->file_name = NULL;
    
    // Parse arguments
    for (int i = 1; i < argc; i += 2) {
        if (strcmp(argv[i], "--input") == 0) {
            args->input_name = argv[i + 1];
        } else if (strcmp(argv[i], "--output") == 0) {
            args->output_name = argv[i + 1];
        } else if (strcmp(argv[i], "--file") == 0) {
            args->file_name = argv[i + 1];
        } else {
            fprintf(stderr, "Unknown argument: %s\n", argv[i]);
            return -1;
        }
    }
    
    // Validate arguments
    if (!args->input_name) {
        fprintf(stderr, "Error: --input is required\n");
        return -1;
    }
    if (!args->output_name) {
        fprintf(stderr, "Error: --output is required\n");
        return -1;
    }
    if (!args->file_name) {
        fprintf(stderr, "Error: --file is required\n");
        return -1;
    }
    
    return 0;
}

// Find first free inode in bitmap
uint32_t find_free_inode(uint8_t* inode_bitmap, uint64_t inode_count) {
    for (uint32_t byte_idx = 0; byte_idx < (inode_count + 7) / 8; byte_idx++) {
        if (inode_bitmap[byte_idx] != 0xFF) { // not all bits are set
            for (int bit_idx = 0; bit_idx < 8; bit_idx++) {
                uint32_t inode_num = byte_idx * 8 + bit_idx + 1; // 1-indexed
                if (inode_num > inode_count) break;
                if (!(inode_bitmap[byte_idx] & (1 << bit_idx))) {
                    return inode_num;
                }
            }
        }
    }
    return 0; // no free inode found
}

// Find first free data block in bitmap
uint32_t find_free_data_block(uint8_t* data_bitmap, uint64_t data_region_blocks) {
    for (uint32_t byte_idx = 0; byte_idx < (data_region_blocks + 7) / 8; byte_idx++) {
        if (data_bitmap[byte_idx] != 0xFF) { // not all bits are set
            for (int bit_idx = 0; bit_idx < 8; bit_idx++) {
                uint32_t block_num = byte_idx * 8 + bit_idx;
                if (block_num >= data_region_blocks) break;
                if (!(data_bitmap[byte_idx] & (1 << bit_idx))) {
                    return block_num;
                }
            }
        }
    }
    return 0xFFFFFFFF; // no free block found
}

// Set bit in bitmap
void set_bitmap_bit(uint8_t* bitmap, uint32_t bit_number) {
    uint32_t byte_idx = bit_number / 8;
    uint32_t bit_idx = bit_number % 8;
    bitmap[byte_idx] |= (1 << bit_idx);
}

// Add directory entry to root directory
int add_directory_entry(uint8_t* image, superblock_t* sb, uint32_t inode_num, const char* filename) {
    // Get root directory data block
    uint8_t* inode_table = image + sb->inode_table_start * BS;
    inode_t* root_inode = (inode_t*)inode_table; // root is first inode
    
    uint8_t* root_data = image + root_inode->direct[0] * BS;
    
    // Find free directory entry (skip . and .. entries)
    dirent64_t* entries = (dirent64_t*)root_data;
    uint32_t max_entries = BS / sizeof(dirent64_t);
    
    for (uint32_t i = 2; i < max_entries; i++) { // start after . and ..
        if (entries[i].inode_no == 0) { // free entry
            entries[i].inode_no = inode_num;
            entries[i].type = 1; // file
            strncpy(entries[i].name, filename, 58);
            entries[i].name[57] = '\0'; // ensure null termination
            dirent_checksum_finalize(&entries[i]);
            
            // Update root directory size and link count
            root_inode->size_bytes += sizeof(dirent64_t);
            root_inode->links += 1;
            inode_crc_finalize(root_inode);
            
            return 0;
        }
    }
    
    fprintf(stderr, "Error: No space for new directory entry\n");
    return -1;
}

// Get basename from path
const char* get_basename(const char* path) {
    const char* basename = strrchr(path, '/');
    return basename ? basename + 1 : path;
}

int main(int argc, char* argv[]) {
    crc32_init();
    
    cli_args_t args;
    
    // Parse command line arguments
    if (parse_args(argc, argv, &args) != 0) {
        return EXIT_FAILURE;
    }
    
    // Read input file size
    FILE* input_file = fopen(args.file_name, "rb");
    if (!input_file) {
        perror("Failed to open input file");
        return EXIT_FAILURE;
    }
    
    fseek(input_file, 0, SEEK_END);
    long file_size = ftell(input_file);
    fseek(input_file, 0, SEEK_SET);
    
    if (file_size < 0) {
        perror("Failed to get file size");
        fclose(input_file);
        return EXIT_FAILURE;
    }
    
    // Check if file is too large for 12 direct blocks
    uint64_t max_file_size = 12ULL * BS;
    if ((uint64_t)file_size > max_file_size) {
        fprintf(stderr, "Error: File too large (%ld bytes). Maximum supported size is %" PRIu64 " bytes.\n", 
                file_size, max_file_size);
        fclose(input_file);
        return EXIT_FAILURE;
    }
    
    // Read file content
    uint8_t* file_content = malloc(file_size);
    if (!file_content) {
        fprintf(stderr, "Failed to allocate memory for file content\n");
        fclose(input_file);
        return EXIT_FAILURE;
    }
    
    if (fread(file_content, 1, file_size, input_file) != (size_t)file_size) {
        perror("Failed to read file content");
        free(file_content);
        fclose(input_file);
        return EXIT_FAILURE;
    }
    fclose(input_file);
    
    // Read file system image
    FILE* fs_input = fopen(args.input_name, "rb");
    if (!fs_input) {
        perror("Failed to open file system image");
        free(file_content);
        return EXIT_FAILURE;
    }
    
    // Get image size
    fseek(fs_input, 0, SEEK_END);
    long image_size = ftell(fs_input);
    fseek(fs_input, 0, SEEK_SET);
    
    uint8_t* image = malloc(image_size);
    if (!image) {
        fprintf(stderr, "Failed to allocate memory for image\n");
        free(file_content);
        fclose(fs_input);
        return EXIT_FAILURE;
    }
    
    if (fread(image, 1, image_size, fs_input) != (size_t)image_size) {
        perror("Failed to read file system image");
        free(file_content);
        free(image);
        fclose(fs_input);
        return EXIT_FAILURE;
    }
    fclose(fs_input);
    
    // Parse superblock
    superblock_t* sb = (superblock_t*)image;
    
    // Validate magic number
    if (sb->magic != 0x4D565346) {
        fprintf(stderr, "Error: Invalid file system magic number\n");
        free(file_content);
        free(image);
        return EXIT_FAILURE;
    }
    
    // Get bitmaps
    uint8_t* inode_bitmap = image + sb->inode_bitmap_start * BS;
    uint8_t* data_bitmap = image + sb->data_bitmap_start * BS;
    uint8_t* inode_table = image + sb->inode_table_start * BS;
    
    // Find free inode
    uint32_t free_inode = find_free_inode(inode_bitmap, sb->inode_count);
    if (free_inode == 0) {
        fprintf(stderr, "Error: No free inodes available\n");
        free(file_content);
        free(image);
        return EXIT_FAILURE;
    }
    
    // Calculate blocks needed for file
    uint32_t blocks_needed = (file_size + BS - 1) / BS; // ceil division
    if (blocks_needed > DIRECT_MAX) {
        fprintf(stderr, "Error: File requires %u blocks, but only %d direct blocks are supported\n", 
                blocks_needed, DIRECT_MAX);
        free(file_content);
        free(image);
        return EXIT_FAILURE;
    }
    
    // Find free data blocks
    uint32_t data_blocks[DIRECT_MAX];
    for (uint32_t i = 0; i < blocks_needed; i++) {
        data_blocks[i] = find_free_data_block(data_bitmap, sb->data_region_blocks);
        if (data_blocks[i] == 0xFFFFFFFF) {
            fprintf(stderr, "Error: No free data blocks available\n");
            free(file_content);
            free(image);
            return EXIT_FAILURE;
        }
        // Temporarily mark as allocated to avoid duplicate allocation
        set_bitmap_bit(data_bitmap, data_blocks[i]);
    }
    
    // Mark inode as allocated in bitmap
    set_bitmap_bit(inode_bitmap, free_inode - 1); // bitmap is 0-indexed
    
    // Create new inode
    time_t now = time(NULL);
    inode_t* new_inode = (inode_t*)(inode_table + (free_inode - 1) * INODE_SIZE);
    memset(new_inode, 0, sizeof(inode_t));
    new_inode->mode = 0100000; // file mode (octal)
    new_inode->links = 1;
    new_inode->uid = 0;
    new_inode->gid = 0;
    new_inode->size_bytes = file_size;
    new_inode->atime = (uint64_t)now;
    new_inode->mtime = (uint64_t)now;
    new_inode->ctime = (uint64_t)now;
    
    // Set direct block pointers
    for (uint32_t i = 0; i < blocks_needed; i++) {
        new_inode->direct[i] = sb->data_region_start + data_blocks[i];
    }
    for (uint32_t i = blocks_needed; i < DIRECT_MAX; i++) {
        new_inode->direct[i] = 0; // unused
    }
    
    new_inode->reserved_0 = 0;
    new_inode->reserved_1 = 0;
    new_inode->reserved_2 = 0;
    new_inode->proj_id = 0;
    new_inode->uid16_gid16 = 0;
    new_inode->xattr_ptr = 0;
    inode_crc_finalize(new_inode);
    
    // Write file data to allocated blocks
    uint8_t* file_ptr = file_content;
    size_t remaining = file_size;
    for (uint32_t i = 0; i < blocks_needed; i++) {
        uint8_t* block_ptr = image + (sb->data_region_start + data_blocks[i]) * BS;
        size_t to_write = remaining > BS ? BS : remaining;
        memcpy(block_ptr, file_ptr, to_write);
        // Zero fill remainder of block
        if (to_write < BS) {
            memset(block_ptr + to_write, 0, BS - to_write);
        }
        file_ptr += to_write;
        remaining -= to_write;
    }
    
    // Add directory entry
    const char* basename = get_basename(args.file_name);
    if (add_directory_entry(image, sb, free_inode, basename) != 0) {
        free(file_content);
        free(image);
        return EXIT_FAILURE;
    }
    
    // Update superblock checksum
    superblock_crc_finalize(sb);
    
    // Write output image
    FILE* fs_output = fopen(args.output_name, "wb");
    if (!fs_output) {
        perror("Failed to create output file");
        free(file_content);
        free(image);
        return EXIT_FAILURE;
    }
    
    if (fwrite(image, 1, image_size, fs_output) != (size_t)image_size) {
        perror("Failed to write output image");
        free(file_content);
        free(image);
        fclose(fs_output);
        return EXIT_FAILURE;
    }
    
    fclose(fs_output);
    free(file_content);
    free(image);
    
    printf("Successfully added file '%s' to file system\n", basename);
    printf("Allocated inode: %u\n", free_inode);
    printf("File size: %ld bytes (%u blocks)\n", file_size, blocks_needed);
    
    return EXIT_SUCCESS;
}