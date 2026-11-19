/*
 * POSIX functions that Rust std references but ESP-IDF does not implement.
 *
 * Without these, the linker takes them from the toolchain's libnosys, whose
 * dir.o also defines opendir/mkdir/readdir/... and then clashes with the
 * ESP-IDF vfs implementations ("multiple definition of `opendir'").
 */

#include <errno.h>
#include <sys/stat.h>

int chmod(const char *path, mode_t mode)
{
    (void) path;
    (void) mode;
    errno = ENOSYS;
    return -1;
}
