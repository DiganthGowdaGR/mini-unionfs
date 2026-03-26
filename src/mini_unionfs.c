#define FUSE_USE_VERSION 35
#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>

struct mini_unionfs_state {
    char *lower_dir;
    char *upper_dir;
};

#define STATE ((struct mini_unionfs_state *) fuse_get_context()->private_data)

int resolve_path(const char *path, char *resolved_path) {
    char upper_path[1024];
    char lower_path[1024];
    char whiteout_path[1024];

    const char *filename = strrchr(path, '/');
    char dir_part[512], file_part[512];
    if (filename) {
        strncpy(file_part, filename + 1, sizeof(file_part));
        strncpy(dir_part, path, filename - path + 1);
        dir_part[filename - path + 1] = '\0';
    } else {
        strcpy(file_part, path);
        strcpy(dir_part, "/");
    }

    snprintf(whiteout_path, sizeof(whiteout_path),
             "%s%s.wh.%s", STATE->upper_dir, dir_part, file_part);

    if (access(whiteout_path, F_OK) == 0) return -ENOENT;

    snprintf(upper_path, sizeof(upper_path), "%s%s", STATE->upper_dir, path);
    if (access(upper_path, F_OK) == 0) {
        strcpy(resolved_path, upper_path);
        return 0;
    }

    snprintf(lower_path, sizeof(lower_path), "%s%s", STATE->lower_dir, path);
    if (access(lower_path, F_OK) == 0) {
        strcpy(resolved_path, lower_path);
        return 0;
    }

    return -ENOENT;
}

static int unionfs_getattr(const char *path, struct stat *stbuf,
                            struct fuse_file_info *fi) {
    char resolved[1024];
    int res = resolve_path(path, resolved);
    if (res != 0) return res;
    res = lstat(resolved, stbuf);
    if (res == -1) return -errno;
    return 0;
}

static int unionfs_readdir(const char *path, void *buf,
                            fuse_fill_dir_t filler, off_t offset,
                            struct fuse_file_info *fi,
                            enum fuse_readdir_flags flags) {
    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);

    char upper_path[1024], lower_path[1024];
    snprintf(upper_path, sizeof(upper_path), "%s%s", STATE->upper_dir, path);
    snprintf(lower_path, sizeof(lower_path), "%s%s", STATE->lower_dir, path);

    DIR *dp = opendir(upper_path);
    if (dp) {
        struct dirent *de;
        while ((de = readdir(dp)) != NULL) {
            if (strcmp(de->d_name, ".") == 0 ||
                strcmp(de->d_name, "..") == 0) continue;
            if (strncmp(de->d_name, ".wh.", 4) == 0) continue;
            filler(buf, de->d_name, NULL, 0, 0);
        }
        closedir(dp);
    }

    dp = opendir(lower_path);
    if (dp) {
        struct dirent *de;
        while ((de = readdir(dp)) != NULL) {
            if (strcmp(de->d_name, ".") == 0 ||
                strcmp(de->d_name, "..") == 0) continue;
            char whiteout[1024];
            snprintf(whiteout, sizeof(whiteout),
                     "%s%s/.wh.%s", STATE->upper_dir, path, de->d_name);
            if (access(whiteout, F_OK) == 0) continue;
            char upper_file[1024];
            snprintf(upper_file, sizeof(upper_file),
                     "%s%s/%s", STATE->upper_dir, path, de->d_name);
            if (access(upper_file, F_OK) == 0) continue;
            filler(buf, de->d_name, NULL, 0, 0);
        }
        closedir(dp);
    }
    return 0;
}

int cow_copy(const char *path) {
    char lower_path[1024], upper_path[1024];
    snprintf(lower_path, sizeof(lower_path), "%s%s", STATE->lower_dir, path);
    snprintf(upper_path, sizeof(upper_path), "%s%s", STATE->upper_dir, path);

    int src = open(lower_path, O_RDONLY);
    if (src < 0) return -errno;
    int dst = open(upper_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (dst < 0) { close(src); return -errno; }

    char buf[4096];
    ssize_t n;
    while ((n = read(src, buf, sizeof(buf))) > 0)
        write(dst, buf, n);

    close(src);
    close(dst);
    return 0;
}

static int unionfs_open(const char *path, struct fuse_file_info *fi) {
    char upper_path[1024], lower_path[1024];
    snprintf(upper_path, sizeof(upper_path), "%s%s", STATE->upper_dir, path);
    snprintf(lower_path, sizeof(lower_path), "%s%s", STATE->lower_dir, path);

    if ((fi->flags & O_WRONLY) || (fi->flags & O_RDWR)) {
        if (access(upper_path, F_OK) != 0 && access(lower_path, F_OK) == 0) {
            int res = cow_copy(path);
            if (res != 0) return res;
        }
    }
    return 0;
}

static int unionfs_read(const char *path, char *buf, size_t size,
                         off_t offset, struct fuse_file_info *fi) {
    char resolved[1024];
    int res = resolve_path(path, resolved);
    if (res != 0) return res;

    int fd = open(resolved, O_RDONLY);
    if (fd < 0) return -errno;
    res = pread(fd, buf, size, offset);
    if (res < 0) res = -errno;
    close(fd);
    return res;
}

static int unionfs_write(const char *path, const char *buf, size_t size,
                          off_t offset, struct fuse_file_info *fi) {
    char upper_path[1024];
    snprintf(upper_path, sizeof(upper_path), "%s%s", STATE->upper_dir, path);

    int fd = open(upper_path, O_WRONLY | O_CREAT, 0644);
    if (fd < 0) return -errno;
    int res = pwrite(fd, buf, size, offset);
    if (res < 0) res = -errno;
    close(fd);
    return res;
}

static int unionfs_unlink(const char *path) {
    char upper_path[1024], lower_path[1024];
    snprintf(upper_path, sizeof(upper_path), "%s%s", STATE->upper_dir, path);
    snprintf(lower_path, sizeof(lower_path), "%s%s", STATE->lower_dir, path);

    if (access(upper_path, F_OK) == 0) {
        if (unlink(upper_path) < 0) return -errno;
    }

    if (access(lower_path, F_OK) == 0) {
        const char *fname = strrchr(path, '/');
        char whiteout[1024];
        if (fname) {
            char dir[512];
            strncpy(dir, path, fname - path + 1);
            dir[fname - path + 1] = '\0';
            snprintf(whiteout, sizeof(whiteout),
                     "%s%s.wh.%s", STATE->upper_dir, dir, fname + 1);
        } else {
            snprintf(whiteout, sizeof(whiteout),
                     "%s/.wh.%s", STATE->upper_dir, path);
        }
        int fd = open(whiteout, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (fd < 0) return -errno;
        close(fd);
    }
    return 0;
}

static int unionfs_create(const char *path, mode_t mode,
                           struct fuse_file_info *fi) {
    char upper_path[1024];
    snprintf(upper_path, sizeof(upper_path), "%s%s", STATE->upper_dir, path);
    int fd = open(upper_path, O_CREAT | O_WRONLY | O_TRUNC, mode);
    if (fd < 0) return -errno;
    close(fd);
    return 0;
}

static int unionfs_mkdir(const char *path, mode_t mode) {
    char upper_path[1024];
    snprintf(upper_path, sizeof(upper_path), "%s%s", STATE->upper_dir, path);
    if (mkdir(upper_path, mode) < 0) return -errno;
    return 0;
}

static int unionfs_rmdir(const char *path) {
    char upper_path[1024];
    snprintf(upper_path, sizeof(upper_path), "%s%s", STATE->upper_dir, path);
    if (rmdir(upper_path) < 0) return -errno;
    return 0;
}

static struct fuse_operations unionfs_oper = {
    .getattr  = unionfs_getattr,
    .readdir  = unionfs_readdir,
    .open     = unionfs_open,
    .read     = unionfs_read,
    .write    = unionfs_write,
    .create   = unionfs_create,
    .unlink   = unionfs_unlink,
    .mkdir    = unionfs_mkdir,
    .rmdir    = unionfs_rmdir,
};

int main(int argc, char *argv[]) {
    if (argc < 4) {
        fprintf(stderr, "Usage: %s <lower_dir> <upper_dir> <mount_point>\n", argv[0]);
        return 1;
    }

    struct mini_unionfs_state *state = malloc(sizeof(*state));
    state->lower_dir = realpath(argv[1], NULL);
    state->upper_dir = realpath(argv[2], NULL);

    char *fuse_argv[] = { argv[0], argv[3], "-f", "-s", NULL };
    int fuse_argc = 4;

    return fuse_main(fuse_argc, fuse_argv, &unionfs_oper, state);
}
