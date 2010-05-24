
#if HAVE_UNIX


#include <unix/uufile.h>
#include <unix/uuprocess.h>
#include <kernel/unix.h>
#include <sys/unistd.h>
#include <sys/fcntl.h>
#include <dirent.h>
#include <phantom_types.h>

//int uu_find_fd( uuprocess_t *u, uufile_t * f );



int usys_open( int *err, uuprocess_t *u, const char *name, int flags, int mode )
{
    uufile_t * f = uu_namei( name );
    if( f == 0 )
    {
        *err = ENOENT;
        return -1;
    }

    mode &= ~u->umask;

    // TODO pass mode to open

    *err = f->fs->open( f, flags & O_CREAT, (flags & O_WRONLY) || (flags & O_RDWR) );
    if( *err )
        return -1;

    int fd = uu_find_fd( u, f );

    if( fd < 0 )
    {
        f->fs->close( f );
        *err = EMFILE;
        return -1;
    }

    return fd;
}

int usys_creat( int *err, uuprocess_t *u, const char *name, int mode )
{
    return usys_open( err, u, name, O_CREAT|O_WRONLY|O_TRUNC, mode );
}

int usys_read(int *err, uuprocess_t *u, int fd, void *addr, int count )
{
    CHECK_FD(fd);
    struct uufile *f = GETF(fd);

    int ret = f->ops->read( f, addr, count );

    if( ret < 0 ) *err = EIO;
    return ret;
}

int usys_write(int *err, uuprocess_t *u, int fd, void *addr, int count )
{
    CHECK_FD(fd);
    struct uufile *f = GETF(fd);

    int ret = f->ops->write( f, addr, count );

    if( ret < 0 ) *err = EIO;
    return ret;
}

int usys_close(int *err, uuprocess_t *u, int fd )
{
    CHECK_FD(fd);
    struct uufile *f = GETF(fd);

    uufs_t *fs = f->fs;
    *err = fs->close( f );

    u->fd[fd] = 0;

    return err ? -1 : 0;
}

int usys_lseek( int *err, uuprocess_t *u, int fd, int offset, int whence )
{
    CHECK_FD(fd);
    struct uufile *f = GETF(fd);

    size_t size = f->ops->getsize( f );

    if(size < 0)
    {
        *err = ESPIPE;
        return -1;
    }

    off_t pos = offset;

    switch(whence)
    {
    case SEEK_SET: break;
    case SEEK_CUR: pos += f->pos;
    case SEEK_END: pos += size;
    }

    if(pos < 0)
    {
        *err = EINVAL;
        return -1;
    }

    f->pos = pos;
    return f->pos;
}





int usys_fchmod( int *err, uuprocess_t *u, int fd, int mode )
{
    CHECK_FD(fd);
    struct uufile *f = GETF(fd);

    if( f->ops->chmod == 0)
    {
        *err = ENOSYS;
        goto err;
    }

    *err = f->ops->chmod( f, mode );
err:
    return *err ? -1 : 0;
}

int usys_fchown( int *err, uuprocess_t *u, int fd, int user, int grp )
{
    CHECK_FD(fd);
    struct uufile *f = GETF(fd);

    if( f->ops->chown == 0)
    {
        *err = ENOSYS;
        goto err;
    }

    *err = f->ops->chown( f, user, grp );
err:
    return *err ? -1 : 0;
}



int usys_ioctl( int *err, uuprocess_t *u, int fd, int request, void *data )
{
    CHECK_FD(fd);
    struct uufile *f = GETF(fd);

    if( !f->ops->ioctl)
    {
        *err = ENOTTY;
        return -1;
    }

    return f->ops->ioctl( f, err, request, data );
}


int usys_stat( int *err, uuprocess_t *u, const char *path, struct stat *data, int statlink )
{
    uufile_t * f = uu_namei( path );
    if( f == 0 )
    {
        *err = ENOENT;
        return -1;
    }

    if( !f->ops->stat )
    {
        *err = EACCES; // or what?
        return -1;
    }

    *err = f->ops->stat( f, data );

    f->fs->close( f );

    return *err ? -1 : 0;
}


int usys_fstat( int *err, uuprocess_t *u, int fd, struct stat *data, int statlink )
{
    CHECK_FD(fd);
    struct uufile *f = GETF(fd);

    if( !f->ops->stat )
    {
        *err = EACCES; // or what?
        return -1;
    }

    *err = f->ops->stat( f, data );
    return *err ? -1 : 0;
}


int usys_truncate( int *err, uuprocess_t *u, const char *path, off_t length)
{
    uufile_t * f = uu_namei( path );
    if( f == 0 )
    {
        *err = ENOENT;
        return -1;
    }

    if( !f->ops->setsize )
    {
        *err = EACCES; // or what?
        return -1;
    }

    *err = f->ops->setsize( f, length );

    f->fs->close( f );

    return *err ? -1 : 0;
}

int usys_ftruncate(int *err, uuprocess_t *u, int fd, off_t length)
{
    CHECK_FD(fd);
    struct uufile *f = GETF(fd);

    if( !f->ops->setsize )
    {
        *err = EACCES; // or what?
        return -1;
    }

    *err = f->ops->setsize( f, length );
    return *err ? -1 : 0;
}


// -----------------------------------------------------------------------
// Dirs
// -----------------------------------------------------------------------

int usys_chdir( int *err, uuprocess_t *u,  const char *path )
{
    uufile_t * f = uu_namei( path );
    if( f == 0 )
    {
        *err = ENOENT;
        return -1;
    }

    if( f->flags & UU_FILE_FLAG_DIR )
    {
        *err = ENOTDIR;
        goto err;
    }


#if 0
    if( f->ops->stat == 0)
    {
        *err = ENOTDIR;
        goto err;
    }

    struct stat sb;
    *err = f->ops->stat( f, &sb );
    if( *err )
        goto err;

    if( sb.st_mode & _S_IFDIR)
    {
#endif
        if(u->cwd)
            u->cwd->fs->close( u->cwd );
        u->cwd = f;
        return 0;
#if 0
    }

    *err = ENOTDIR;
#endif
err:
    f->fs->close( f );
    return -1;
}

int usys_fchdir( int *err, uuprocess_t *u,  int fd )
{
    CHECK_FD(fd);
    struct uufile *f = GETF(fd);

    if( f->flags & UU_FILE_FLAG_DIR )
    {
        *err = ENOTDIR;
        goto err;
    }

#if 0
    if( f->ops->stat == 0)
    {
        *err = ENOTDIR;
        goto err;
    }

    struct stat sb;
    *err = f->ops->stat( f, &sb );
    if( *err )
        goto err;

    if( sb.st_mode & _S_IFDIR)
    {
#endif
        if(u->cwd)
            u->cwd->fs->close( u->cwd );
        u->cwd = copy_uufile( f );
        return 0;
#if 0
    }

    *err = ENOTDIR;
#endif
err:
    return -1;
}


int usys_readdir(int *err, uuprocess_t *u, int fd, struct dirent *dirp )
{
    CHECK_FD(fd);
    struct uufile *f = GETF(fd);

    if( f->flags & UU_FILE_FLAG_DIR )
    {
        *err = ENOTDIR;
        return -1;
    }

    int len = f->ops->read( f, dirp, sizeof(struct dirent) );

    if( len == 0 )
        return 0;

    if( len == sizeof(struct dirent) )
        return 1;

    *err = EIO;
    return -1;
}



// -----------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------

// BUG - mutex!
int uu_find_fd( uuprocess_t *u, uufile_t *f  )
{
    int i;
    for( i = 0; i < MAX_UU_FD; i++ )
    {
        if( u->fd[i] == 0 )
        {
            u->fd[i] = f;
            return i;
        }
    }

    return -1;
}


#endif // HAVE_UNIX
