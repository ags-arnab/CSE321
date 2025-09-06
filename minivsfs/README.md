# MiniVSFS: A Simple File System Implementation

This project implements MiniVSFS, a simplified inode-based file system with two main programs:

- **mkfs_builder**: Creates empty MiniVSFS file system images
- **mkfs_adder**: Adds files to existing MiniVSFS images

## Project Structure

```
minivsfs/
├── mkfs_builder.c          # File system builder implementation
├── mkfs_adder.c           # File adder implementation  
├── mkfs_builder_skeleton.c  # Original skeleton file
├── mkfs_adder_skeleton.c    # Original skeleton file
├── file_9.txt             # Test file (73 bytes)
├── file_15.txt            # Test file (74 bytes)
├── file_22.txt            # Test file (71 bytes)
├── file_26.txt            # Test file (72 bytes)
└── README.md              # This file
```

## Building

Compile both programs using:

```bash
gcc -O2 -std=c17 -Wall -Wextra mkfs_builder.c -o mkfs_builder
gcc -O2 -std=c17 -Wall -Wextra mkfs_adder.c -o mkfs_adder
```

## Usage

### mkfs_builder

Creates a new MiniVSFS file system image with an empty root directory.

```bash
./mkfs_builder --image <output.img> --size-kib <180..4096> --inodes <128..512>
```

**Parameters:**
- `--image`: Output image filename
- `--size-kib`: Total image size in kilobytes (must be multiple of 4, range 180-4096)
- `--inodes`: Number of inodes in the file system (range 128-512)

**Example:**
```bash
./mkfs_builder --image filesystem.img --size-kib 256 --inodes 128
```

### mkfs_adder

Adds a file from the current directory to an existing MiniVSFS image.

```bash
./mkfs_adder --input <input.img> --output <output.img> --file <filename>
```

**Parameters:**
- `--input`: Input MiniVSFS image file
- `--output`: Output image file (can be same as input for in-place modification)
- `--file`: File in current directory to add to the file system

**Example:**
```bash
./mkfs_adder --input filesystem.img --output filesystem_with_file.img --file test.txt
```

## Testing

### Basic Test Sequence

1. Create a file system:
```bash
./mkfs_builder --image test.img --size-kib 256 --inodes 128
```

2. Add test files:
```bash
./mkfs_adder --input test.img --output test1.img --file file_9.txt
./mkfs_adder --input test1.img --output test2.img --file file_15.txt
```

3. Verify with hexdump:
```bash
hexdump -C test2.img | head -20
```

### Expected Output

**mkfs_builder output:**
```
MiniVSFS image 'test.img' created successfully:
  Size: 256 KiB (64 blocks)
  Inodes: 128
  Data blocks: 57
```

**mkfs_adder output:**
```
File 'file_9.txt' added successfully to image 'test1.img'
  Inode: 2
  Size: 73 bytes (1 blocks)
```

## File System Specifications

- **Block Size**: 4096 bytes
- **Inode Size**: 128 bytes  
- **Layout**: Superblock | Inode Bitmap | Data Bitmap | Inode Table | Data Region
- **Root Directory**: Always inode #1, contains "." and ".." entries
- **File Allocation**: First-fit policy for both inodes and data blocks
- **Link Counting**: Root starts with 2 links, +1 for each file added
- **Checksums**: CRC32 for superblock/inodes, XOR for directory entries

### Superblock Structure (116 bytes)

| Field | Size | Value | Description |
|-------|------|-------|-------------|
| magic | 4 | 0x4D565346 | "MVSF" signature |
| version | 4 | 1 | File system version |
| block_size | 4 | 4096 | Block size in bytes |
| total_blocks | 8 | calculated | Total blocks in image |
| inode_count | 8 | from CLI | Number of inodes |
| inode_bitmap_start | 8 | 1 | Inode bitmap start block |
| inode_bitmap_blocks | 8 | 1 | Inode bitmap block count |
| data_bitmap_start | 8 | 2 | Data bitmap start block |
| data_bitmap_blocks | 8 | 1 | Data bitmap block count |
| inode_table_start | 8 | 3 | Inode table start block |
| inode_table_blocks | 8 | calculated | Inode table block count |
| data_region_start | 8 | calculated | Data region start block |
| data_region_blocks | 8 | calculated | Data region block count |
| root_inode | 8 | 1 | Root directory inode number |
| mtime_epoch | 8 | build time | File system creation time |
| flags | 4 | 0 | Reserved flags |
| checksum | 4 | calculated | CRC32 of preceding fields |

### Inode Structure (128 bytes)

| Field | Size | Description |
|-------|------|-------------|
| mode | 2 | 0o040000 (dir) or 0o100000 (file) |
| links | 2 | Link count |
| uid/gid | 4+4 | User/group ID (always 0) |
| size_bytes | 8 | File size in bytes |
| atime/mtime/ctime | 8+8+8 | Access/modify/create times |
| direct[12] | 48 | Direct block pointers |
| reserved | 12 | Reserved fields |
| proj_id | 4 | Project ID |
| uid16_gid16 | 4 | Legacy UID/GID |
| xattr_ptr | 8 | Extended attributes pointer |
| inode_crc | 8 | CRC32 checksum |

### Directory Entry (64 bytes)

| Field | Size | Description |
|-------|------|-------------|
| inode_no | 4 | Inode number (0 if free) |
| type | 1 | 1=file, 2=directory |
| name | 58 | Null-terminated filename |
| checksum | 1 | XOR checksum of first 63 bytes |

## Hexdump Analysis Examples

### Superblock (Block 0)
```
00000000  46 53 56 4d 01 00 00 00  00 10 00 00 40 00 00 00  |FSVM........@...|
```
- `46 53 56 4d` = "MVSF" magic number
- `01 00 00 00` = version 1
- `00 10 00 00` = block_size 4096 
- `40 00 00 00 00 00 00 00` = total_blocks 64

### Inode Bitmap (Block 1, offset 0x1000)
```
00001000  01 00 00 00 00 00 00 00  ...
```
- `01` = 0b00000001 = inode #1 (root) is allocated

### Root Directory Inode (Block 3, offset 0x3000)
```
00003000  00 40 02 00 00 00 00 00  00 00 00 00 80 00 00 00  |.@..............|
```
- `00 40` = 0x4000 = directory mode
- `02 00` = links = 2 (for "." and "..")
- `80 00 00 00 00 00 00 00` = size = 128 bytes (2 directory entries)

### Root Directory Entries (Data block, offset 0x7000)
```
00000000  01 00 00 00 02 2e 00 00  ... 2d                   |.......-|
00000040  01 00 00 00 02 2e 2e 00  ... 03                   |.......|
```
- Entry 1: inode=1, type=2(dir), name=".", checksum=0x2d
- Entry 2: inode=1, type=2(dir), name="..", checksum=0x03

## Error Handling

The programs handle various error conditions gracefully:

- **Invalid CLI parameters**: Returns usage message
- **File not found**: Descriptive error message
- **Insufficient space**: Warns about space limitations  
- **File too large**: Rejects files requiring >12 direct blocks
- **Invalid file system**: Validates magic number
- **Permission errors**: Reports file access issues

## Implementation Details

### Key Features Implemented

1. **Complete structure definitions** matching PDF specifications exactly
2. **Proper little-endian binary format** compliance  
3. **CRC32 checksums** for data integrity
4. **First-fit allocation policy** as specified
5. **Correct link counting semantics** (root directory link increment)
6. **Robust error handling** for all edge cases
7. **Binary I/O operations** with proper block alignment

### Critical Implementation Notes

- **1-indexed inodes**: Inode numbering starts at 1, but array access is 0-indexed
- **Link counting**: Root directory link count increases by 1 for each file added
- **Block allocation**: Uses first-fit policy for both inodes and data blocks
- **Checksums**: Must be calculated last after all other fields are finalized
- **File modes**: 0o040000 for directories, 0o100000 for regular files

### Testing Verification

The implementation has been thoroughly tested and verified:

- ✅ Superblock structure and layout
- ✅ Inode and data bitmap management
- ✅ Directory entry creation and checksums
- ✅ File data storage and retrieval
- ✅ Link counting correctness
- ✅ Error handling for edge cases
- ✅ Binary format compliance via hexdump analysis

The file system successfully creates valid MiniVSFS images and correctly adds files while maintaining all data structure integrity requirements specified in the project documentation.