# MiniVSFS - Mini Virtual File System

This is a project implementing a mini virtual file system with the following components:

## Files

### Core Implementation
- `mkfs_builder.c` - File system builder implementation
- `mkfs_adder.c` - File addition utility implementation

### Skeleton Files
- `mkfs_builder_skeleton.c` - Skeleton code for file system builder
- `mkfs_adder_skeleton.c` - Skeleton code for file addition utility

### Test Files
- `file_9.txt` - Test file 9
- `file_15.txt` - Test file 15
- `file_22.txt` - Test file 22
- `file_26.txt` - Test file 26

## Description

This project implements a mini virtual file system (MiniVSFS) that provides basic file system functionality including:
- File system creation and initialization
- File addition to the file system
- Proper file system structure management

## Usage

Compile the source files:
```bash
gcc -o mkfs_builder mkfs_builder.c
gcc -o mkfs_adder mkfs_adder.c
```

Use the tools to create and manage your virtual file system.