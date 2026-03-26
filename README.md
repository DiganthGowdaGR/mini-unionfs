# Mini UnionFS – Lightweight Union File System using FUSE

## Overview

Mini UnionFS is a lightweight implementation of a Union File System built using FUSE (Filesystem in Userspace). It combines multiple directories into a single unified view, allowing users to interact with files as if they exist in one location.

This project demonstrates core file system concepts such as layering, copy-on-write, and file masking. It is useful for learning operating systems and system programming.

---

## Key Concepts

### Union File System
A Union File System merges multiple directories into a single virtual filesystem. Files from all directories appear together in one structure.

### Copy-on-Write (CoW)
When a file from the lower directory is modified, it is first copied to the upper directory. The modification is applied to the copied file, preserving the original.

### Whiteout Files
When deleting a file, it is not removed from the lower directory. Instead, a hidden file (e.g., `.wh.filename`) is created in the upper directory to hide it.

### Layer Priority
If a file exists in both directories, the version in the upper directory overrides the lower directory.

---

## Project Structure

```
mini-unionfs/
├── mini_unionfs.c      # Main FUSE implementation
├── Makefile            # Build instructions
├── test_unionfs.sh     # Testing script
└── README.md           # Documentation
```

---

## Requirements

- Linux system (Ubuntu recommended)
- FUSE3

Install dependencies:

```bash
sudo apt update
sudo apt install fuse3 libfuse3-dev
```

---

## Build Instructions

```bash
make
```

This will generate the executable:

```
mini_unionfs
```

---

## Usage

### Create directories

```bash
mkdir lower upper mnt
```

### Add sample file

```bash
echo "Hello from lower layer" > lower/file.txt
```

### Mount filesystem

```bash
./mini_unionfs lower upper mnt
```

### Access merged directory

```bash
ls mnt
cat mnt/file.txt
```

---

## Example Operations

### Modify a file

```bash
echo "Modified content" >> mnt/file.txt
```

The file is copied to the upper directory and then modified.

### Delete a file

```bash
rm mnt/file.txt
```

A whiteout file is created in the upper directory.

### Create a new file

```bash
echo "New file" > mnt/new.txt
```

The file is stored in the upper directory.

---

## Run Tests

```bash
chmod +x test_unionfs.sh
./test_unionfs.sh
```

---

## Unmount Filesystem

```bash
fusermount3 -u mnt
```

---

## Features

- Union filesystem layering
- Copy-on-write support
- Whiteout file handling
- Lightweight FUSE implementation
- POSIX-like file operations

---

## Use Cases

- Container file systems
- Overlay-based storage systems
- Backup and snapshot systems
- OS and filesystem learning

---

## Future Improvements

- Multi-layer support
- Performance optimization
- Logging and debugging tools
- GUI visualization

---

## Author
## PES2UG24CS823
## PES2UG23CS379
## PES2UG23CS363
## PES2UG23CS336
