#include "io61.hh"
#include <sys/types.h>
#include <sys/stat.h>
#include <climits>
#include <cerrno>

// io61.cc
//    YOUR CODE HERE!
inline __attribute__((always_inline)) size_t min(size_t a, size_t b) {
    return a < b ? a : b;
}

inline __attribute__((always_inline)) size_t round_down(off_t n, off_t mod) {
    return n - (n % mod);
}

// io61_file
//    Data structure for io61 file wrappers. Add your own stuff.

#define BUF_SIZE 4096

struct io61_file {
    int fd = -1;     // file descriptor
    int mode;
    unsigned char cbuf[BUF_SIZE];
    off_t tag = 0;		// starting position of buffer
    off_t end_tag = 0;		// ending position of buffer
    off_t pos_tag = 0;		// current file position};
};

// io61_fdopen(fd, mode)
//    Returns a new io61_file for file descriptor `fd`. `mode` is either
//    O_RDONLY for a read-only file or O_WRONLY for a write-only file.
//    You need not support read/write files.

io61_file* io61_fdopen(int fd, int mode) {
    assert(fd >= 0);
    io61_file* f = new io61_file;
    f->fd = fd;
    f->mode = mode;
    return f;
}


// io61_close(f)
//    Closes the io61_file `f` and releases all its resources.

int io61_close(io61_file* f) {
    io61_flush(f);
    int r = close(f->fd);
    delete f;
    return r;
}

// io61_fill(f)
//     Fill the read cache with new data, starting from file offset `end_tag`.
//     Only called for read caches.
int io61_fill(io61_file* f) {
    // Check invariants.
    assert(f->tag <= f->pos_tag && f->pos_tag <= f->end_tag);
    assert(f->end_tag - f->pos_tag <= BUF_SIZE);

    // Reset the cache to empty.
    f->tag = f->pos_tag = f->end_tag;
    // Read data.
    ssize_t n = read(f->fd, f->cbuf, BUF_SIZE);
    if (n >= 0) {
        f->end_tag = f->tag + n;
    } 
    return n;
}


// io61_readc(f)
//    Reads a single (unsigned) byte from `f` and returns it. Returns EOF,
//    which equals -1, on end of file or error.

int io61_readc(io61_file* f) {
    assert(f->tag <= f->pos_tag && f->pos_tag <= f->end_tag);
    assert(f->end_tag - f->pos_tag <= BUF_SIZE);

    if (f->pos_tag == f->end_tag) {
	// update cache
	if (io61_fill(f) <= 0) {
	    // if 0 byte is filled, then EOF
	    return -1;
	}
    }
    unsigned char c = f->cbuf[f->pos_tag++ - f->tag];
    // printf("%ld, %ld, %ld\n", f->tag, f->pos_tag, f->end_tag);
    return c; 
}


// io61_read(f, buf, sz)
//    Reads up to `sz` bytes from `f` into `buf`. Returns the number of
//    bytes read on success. Returns 0 if end-of-file is encountered before
//    any bytes are read, and -1 if an error is encountered before any
//    bytes are read.
//
//    Note that the return value might be positive, but less than `sz`,
//    if end-of-file or error is encountered before all `sz` bytes are read.
//    This is called a “short read.”

ssize_t io61_read(io61_file* f, unsigned char* buf, size_t sz) {
    // Check invariants.
    assert(f->tag <= f->pos_tag && f->pos_tag <= f->end_tag);
    assert(f->end_tag - f->pos_tag <= BUF_SIZE);

    int nr;
    int nr_t = 0;	// total number of bytes read
    while (sz > 0) {
	if (f->pos_tag == f->end_tag) {
	    // update cache
	    if (io61_fill(f) <= 0) {
		// if 0 byte is filled, then EOF
		return nr_t;
	    }
	}
	size_t size = f->end_tag - f->pos_tag;
	nr = min(sz, size);
	memcpy(buf, &f->cbuf[f->pos_tag - f->tag], nr);
	f->pos_tag += nr;
	buf += nr;
	sz -= nr;
	nr_t += nr;
    }
    return nr_t;
}


// io61_writec(f)
//    Write a single character `ch` to `f`. Returns 0 on success and
//    -1 on error.

int io61_writec(io61_file* f, int ch) {
    // check invariants.
    assert(f->tag <= f->pos_tag && f->pos_tag <= f->end_tag);
    assert(f->end_tag - f->pos_tag <= BUF_SIZE);

    if (f->pos_tag - f->tag == BUF_SIZE) {
	// cache full
	if (io61_flush(f) < 0) {
	    return -1;
	}
    }
    f->cbuf[f->pos_tag++ - f->tag] = ch;
    f->end_tag = f->pos_tag;
    return 0;
}


// io61_write(f, buf, sz)
//    Writes `sz` characters from `buf` to `f`. Returns `sz` on success.
//    Can write fewer than `sz` characters when there is an error, such as
//    a drive running out of space. In this case io61_write returns the
//    number of characters written, or -1 if no characters were written
//    before the error occurred.

ssize_t io61_write(io61_file* f, const unsigned char* buf, size_t sz) {
    // Check invariants.
    assert(f->tag <= f->pos_tag && f->pos_tag <= f->end_tag);
    assert(f->end_tag - f->pos_tag <= BUF_SIZE);
    assert(f->pos_tag == f->end_tag);

    int nw;
    int nw_t = 0;	// total number of bytes written
    while (sz > 0) {
	if (f->pos_tag - f->tag == BUF_SIZE) {
	    io61_flush(f);
	}
	size_t size = BUF_SIZE - (f->pos_tag - f->tag);
	nw = min(sz, size);
	memcpy(&f->cbuf[f->pos_tag - f->tag], &buf[nw_t], nw);
	sz -= nw;
	f->pos_tag += nw;
	f->end_tag = f->pos_tag;
	nw_t += nw;
    }
    if (nw_t == 0) {
	return -1;
    }
    return nw_t;
}


// io61_flush(f)
//    Forces a write of any cached data written to `f`. Returns 0 on
//    success. Returns -1 if an error is encountered before all cached
//    data was written.
//
//    If `f` was opened read-only, `io61_flush(f)` returns 0. If may also
//    drop any data cached for reading.

int io61_flush(io61_file* f) {
    // Check invariants.
    assert(f->tag <= f->pos_tag && f->pos_tag <= f->end_tag);
    assert(f->end_tag - f->pos_tag <= BUF_SIZE);

    switch (f->mode) {
	case O_RDONLY:
	    return 0;

	case O_WRONLY: 
	{
	    // write cache invariant
	    assert(f->pos_tag == f->end_tag);

	    // write cached data
	    ssize_t n = write(f->fd, f->cbuf, f->pos_tag - f->tag);
 	    if (n != f->pos_tag - f->tag) {
		return -1;
	    }
	    f->tag = f->pos_tag;
	    return 0;
	}
	default:
	    return -1;
    }
}

// io61_seek(f, pos)
//    Changes the file pointer for file `f` to `pos` bytes into the file.
//    Returns 0 on success and -1 on failure.

int io61_seek(io61_file* f, off_t pos) {
    switch (f->mode) {
	case O_RDONLY: 
	{
	    // if seek outside cache, fill cache
	    if (pos >= f->tag && pos < f->end_tag) {
		f->pos_tag = pos;
		return 0;
	    } 
	    off_t r = round_down(pos, BUF_SIZE);
	    off_t p = lseek(f->fd, r, SEEK_SET);
	    if (p != r) {
		return -1;
	    }
	    f->tag = f->pos_tag = f->end_tag = r;	// keep assertions happy
	    if (io61_fill(f) < 0) {
		return -1;
	    }
	    f->pos_tag = pos;
	    return 0;
	}
	case O_WRONLY: 
	{
	    // flush cache
	    if (io61_flush(f) < 0) {
		return -1;
	    }	
	    off_t p = lseek(f->fd, pos, SEEK_SET);
	    if (p != pos) {
		return -1;
	    }
	    f->tag = f->pos_tag = f->end_tag = pos;
	    return 0;
	}
	default:
	    return -1;
    }
}


// You shouldn't need to change these functions.

// io61_open_check(filename, mode)
//    Opens the file corresponding to `filename` and returns its io61_file.
//    If `!filename`, returns either the standard input or the
//    standard output, depending on `mode`. Exits with an error message if
//    `filename != nullptr` and the named file cannot be opened.

io61_file* io61_open_check(const char* filename, int mode) {
    int fd;
    if (filename) {
        fd = open(filename, mode, 0666);
    } else if ((mode & O_ACCMODE) == O_RDONLY) {
        fd = STDIN_FILENO;
    } else {
        fd = STDOUT_FILENO;
    }
    if (fd < 0) {
        fprintf(stderr, "%s: %s\n", filename, strerror(errno));
        exit(1);
    }
    return io61_fdopen(fd, mode & O_ACCMODE);
}


// io61_fileno(f)
//    Returns the file descriptor associated with `f`.

int io61_fileno(io61_file* f) {
    return f->fd;
}


// io61_filesize(f)
//    Returns the size of `f` in bytes. Returns -1 if `f` does not have a
//    well-defined size (for instance, if it is a pipe).

off_t io61_filesize(io61_file* f) {
    struct stat s;
    int r = fstat(f->fd, &s);
    if (r >= 0 && S_ISREG(s.st_mode)) {
        return s.st_size;
    } else {
        return -1;
    }
}
