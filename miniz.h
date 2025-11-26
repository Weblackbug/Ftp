/* miniz.h v1.15 - public domain deflate/inflate/zlib-subset/zip implementation
   Rich Geldreich <richgel99@gmail.com>, last updated Oct. 13, 2013 */
#ifndef MINIZ_H
#define MINIZ_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>

/* ------------------- Low-level Compression API Definitions */

/* Set MINIZ_NO_STDIO to disable all stdio.h usage (and the mz_zip_reader_init_file() family of functions). */
/* #define MINIZ_NO_STDIO */

/* Set MINIZ_NO_TIME to disable all time.h usage (and the mz_zip_reader_init_file() family of functions). */
/* #define MINIZ_NO_TIME */

/* Set MINIZ_NO_ARCHIVE_APIS to disable all ZIP archive API's. */
/* #define MINIZ_NO_ARCHIVE_APIS */

#ifndef MINIZ_NO_STDIO
#include <stdio.h>
#endif

#ifndef MINIZ_NO_TIME
#include <time.h>
#endif

/* Return status codes. MZ_PARAM_ERROR is non-standard. */
enum { MZ_OK = 0, MZ_STREAM_END = 1, MZ_NEED_DICT = 2, MZ_ERRNO = -1, MZ_STREAM_ERROR = -2, MZ_DATA_ERROR = -3, MZ_MEM_ERROR = -4, MZ_BUF_ERROR = -5, MZ_VERSION_ERROR = -6, MZ_PARAM_ERROR = -10000 };

/* Compression levels: 0-9 are the standard zlib-style levels, 10 is best possible compression (not zlib compatible, and may be very slow). */
enum { MZ_NO_COMPRESSION = 0, MZ_BEST_SPEED = 1, MZ_BEST_COMPRESSION = 9, MZ_UBER_COMPRESSION = 10, MZ_DEFAULT_LEVEL = 6, MZ_DEFAULT_STRATEGY = 0 };

/* Window bits */
#define MZ_DEFAULT_WINDOW_BITS 15

/* Memory allocation functions */
typedef void *(*mz_alloc_func)(void *opaque, size_t items, size_t size);
typedef void (*mz_free_func)(void *opaque, void *address);
typedef void *(*mz_realloc_func)(void *opaque, void *address, size_t items, size_t size);

struct mz_internal_state;

/* z_stream structure */
typedef struct mz_stream_s
{
  const unsigned char *next_in;     /* pointer to next byte to read */
  unsigned int avail_in;            /* number of bytes available at next_in */
  unsigned long total_in;           /* total number of bytes consumed so far */

  unsigned char *next_out;          /* pointer to next byte to write */
  unsigned int avail_out;           /* number of bytes that can be written to next_out */
  unsigned long total_out;          /* total number of bytes produced so far */

  char *msg;                        /* error msg (unused) */
  struct mz_internal_state *state;  /* internal state, allocated by z_stream */

  mz_alloc_func zalloc;             /* optional memory allocation function */
  mz_free_func zfree;               /* optional memory deallocation function */
  void *opaque;                     /* optional private data passed to zalloc/zfree */

  int data_type;                    /* best guess about the data type: binary or text (unused) */
  unsigned long adler;              /* adler32 value of the uncompressed data */
  unsigned long reserved;           /* reserved for future use */
} mz_stream;

typedef mz_stream *mz_streamp;

/* ZIP archive API's */

#ifndef MINIZ_NO_ARCHIVE_APIS

enum { MZ_ZIP_MAX_IO_BUF_SIZE = 64*1024, MZ_ZIP_MAX_ARCHIVE_FILENAME_SIZE = 260, MZ_ZIP_MAX_ARCHIVE_FILE_COMMENT_SIZE = 256 };

typedef struct
{
  unsigned int m_file_index;
  unsigned int m_central_dir_ofs;
  unsigned short m_version_made_by;
  unsigned short m_version_needed;
  unsigned short m_bit_flag;
  unsigned short m_method;
  time_t m_time;
  unsigned int m_crc32;
  unsigned int m_comp_size;
  unsigned int m_uncomp_size;
  unsigned short m_internal_attr;
  unsigned int m_external_attr;
  unsigned int m_local_header_ofs;
  unsigned int m_comment_size;
  char m_filename[MZ_ZIP_MAX_ARCHIVE_FILENAME_SIZE];
  char m_comment[MZ_ZIP_MAX_ARCHIVE_FILE_COMMENT_SIZE];
} mz_zip_archive_file_stat;

typedef size_t (*mz_file_read_func)(void *pOpaque, unsigned int file_ofs, void *pBuf, size_t n);
typedef size_t (*mz_file_write_func)(void *pOpaque, unsigned int file_ofs, const void *pBuf, size_t n);

struct mz_zip_internal_state;

typedef struct
{
  unsigned int m_archive_size;
  unsigned int m_central_directory_file_ofs;
  unsigned int m_total_files;
  unsigned int m_central_dir_size;
  
  unsigned int m_init_flags;
  
  mz_file_read_func m_pRead;
  mz_file_write_func m_pWrite;
  void *m_pIO_opaque;
  
  struct mz_zip_internal_state *m_pState;

} mz_zip_archive;

typedef enum {
  MZ_ZIP_MODE_INVALID = 0,
  MZ_ZIP_MODE_READING = 1,
  MZ_ZIP_MODE_WRITING = 2,
  MZ_ZIP_MODE_WRITING_HAS_BEEN_FINALIZED = 3
} mz_zip_mode;

typedef enum {
  MZ_ZIP_FLAG_CASE_SENSITIVE = 0x0100,
  MZ_ZIP_FLAG_IGNORE_PATH = 0x0200,
  MZ_ZIP_FLAG_COMPRESSED_DATA = 0x0400,
  MZ_ZIP_FLAG_DO_NOT_SORT_CENTRAL_DIRECTORY = 0x0800,
  MZ_ZIP_FLAG_VALIDATE_LOCATE_FILE_AT_END_OF_ARCHIVE = 0x1000,
  MZ_ZIP_FLAG_WRITE_ZIP64 = 0x2000,
  MZ_ZIP_FLAG_WRITE_ALLOW_READING = 0x4000,
  MZ_ZIP_FLAG_ASCII_FILENAME = 0x8000
} mz_zip_flags;

/* Initialize zip archive for writing */
int mz_zip_writer_init_file(mz_zip_archive *pZip, const char *pFilename, unsigned int size_to_reserve_at_beginning);
int mz_zip_writer_add_file(mz_zip_archive *pZip, const char *pArchive_name, const char *pSrc_filename, const void *pComment, unsigned short comment_size, unsigned int level_and_flags);
int mz_zip_writer_add_mem(mz_zip_archive *pZip, const char *pArchive_name, const void *pBuf, size_t buf_size, unsigned int level_and_flags);
int mz_zip_writer_finalize_archive(mz_zip_archive *pZip);
int mz_zip_writer_end(mz_zip_archive *pZip);

/* Error handling */
const char *mz_zip_get_error_string(int mz_err);

#endif /* MINIZ_NO_ARCHIVE_APIS */

#ifdef __cplusplus
}
#endif

#endif /* MINIZ_H */
