/* miniz.c v1.15 - Simplified for Zip Writing Only */
#include "miniz.h"
#include <string.h>
#include <assert.h>

#define MZ_MAX(a,b) (((a)>(b))?(a):(b))
#define MZ_MIN(a,b) (((a)<(b))?(a):(b))
#define MZ_READ_LE16(p) ((p)[0] | ((p)[1] << 8))
#define MZ_READ_LE32(p) ((p)[0] | ((p)[1] << 8) | ((p)[2] << 16) | ((p)[3] << 24))
#define MZ_WRITE_LE16(p, v) ((p)[0] = (unsigned char)(v), (p)[1] = (unsigned char)((v) >> 8))
#define MZ_WRITE_LE32(p, v) ((p)[0] = (unsigned char)(v), (p)[1] = (unsigned char)((v) >> 8), (p)[2] = (unsigned char)((v) >> 16), (p)[3] = (unsigned char)((v) >> 24))

/* CRC32 */
static const unsigned long mz_crc32_table[256] = {
  0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3,
  0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988, 0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91,
  0x1db71064, 0x6ab020f2, 0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
  0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9, 0xfa0f3d63, 0x8d080df5,
  0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172, 0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b,
  0x35b5a8fa, 0x42b2986c, 0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
  0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423, 0xcfba9599, 0xb8bda50f,
  0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924, 0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d,
  0x76dc4190, 0x01db7106, 0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
  0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d, 0x91646c97, 0xe6635c01,
  0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e, 0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457,
  0x65b0d9c6, 0x12b7e950, 0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
  0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7, 0xa4d1c46d, 0xd3d6f4fb,
  0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0, 0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9,
  0x5005713c, 0x270241aa, 0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
  0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81, 0xb7bd5c3b, 0xc0ba6cad,
  0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a, 0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683,
  0xe3630b12, 0x94643b84, 0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
  0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb, 0x196c3671, 0x6e6b06e7,
  0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc, 0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5,
  0xd6d6a3e8, 0xa1d1937e, 0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
  0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55, 0x316e8eef, 0x4669be79,
  0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236, 0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f,
  0xc5ba3bbe, 0xb2bd0b28, 0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
  0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f, 0x72076785, 0x05005713,
  0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38, 0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21,
  0x86d3d2d4, 0xf1d4e242, 0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
  0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69, 0x616bffd3, 0x166ccf45,
  0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2, 0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db,
  0xaed16a4a, 0xd9d65adc, 0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
  0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693, 0x54de5729, 0x23d967bf,
  0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94, 0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
};

unsigned long mz_crc32(unsigned long crc, const unsigned char *ptr, size_t buf_len)
{
  unsigned long i;
  crc = ~crc;
  for (i = 0; i < buf_len; i++)
    crc = (crc >> 8) ^ mz_crc32_table[(crc ^ ptr[i]) & 0xFF];
  return ~crc;
}

/* Internal ZIP state */
struct mz_zip_internal_state {
  mz_zip_mode m_zip_mode;
  mz_zip_archive_file_stat *m_pState;
  void *m_pMem;
};

/* Simple Zip Writer Implementation (Store Only for simplicity in this snippet, 
   but structure allows adding deflate later if needed. 
   For now, we implement STORE (no compression) to ensure robustness without 5000 lines of deflate code.)
*/

int mz_zip_writer_init_file(mz_zip_archive *pZip, const char *pFilename, unsigned int size_to_reserve_at_beginning)
{
  if (!pZip || !pFilename) return 0;
  memset(pZip, 0, sizeof(mz_zip_archive));
  
  pZip->m_pIO_opaque = fopen(pFilename, "wb");
  if (!pZip->m_pIO_opaque) return 0;

  pZip->m_pWrite = (mz_file_write_func)NULL; // Use stdio
  pZip->m_init_flags = MZ_ZIP_FLAG_WRITE_ZIP64; // Default
  
  return 1;
}

static int mz_file_write(mz_zip_archive *pZip, const void *pBuf, size_t n)
{
    if (pZip->m_pWrite) return pZip->m_pWrite(pZip->m_pIO_opaque, 0, pBuf, n) == n;
    return fwrite(pBuf, 1, n, (FILE*)pZip->m_pIO_opaque) == n;
}

static int mz_file_seek(mz_zip_archive *pZip, unsigned int offset)
{
    return fseek((FILE*)pZip->m_pIO_opaque, offset, SEEK_SET) == 0;
}

static unsigned int mz_file_tell(mz_zip_archive *pZip)
{
    return (unsigned int)ftell((FILE*)pZip->m_pIO_opaque);
}

int mz_zip_writer_add_file(mz_zip_archive *pZip, const char *pArchive_name, const char *pSrc_filename, const void *pComment, unsigned short comment_size, unsigned int level_and_flags)
{
    FILE *pFile = fopen(pSrc_filename, "rb");
    if (!pFile) return 0;

    fseek(pFile, 0, SEEK_END);
    unsigned int size = (unsigned int)ftell(pFile);
    fseek(pFile, 0, SEEK_SET);

    void *pBuf = malloc(size);
    if (!pBuf) { fclose(pFile); return 0; }
    fread(pBuf, 1, size, pFile);
    fclose(pFile);

    int res = mz_zip_writer_add_mem(pZip, pArchive_name, pBuf, size, level_and_flags);
    free(pBuf);
    return res;
}

int mz_zip_writer_add_mem(mz_zip_archive *pZip, const char *pArchive_name, const void *pBuf, size_t buf_size, unsigned int level_and_flags)
{
    /* Write Local File Header */
    unsigned int local_header_ofs = mz_file_tell(pZip);
    unsigned int crc = mz_crc32(0, (const unsigned char*)pBuf, buf_size);
    unsigned int name_len = (unsigned int)strlen(pArchive_name);
    
    unsigned char header[30];
    MZ_WRITE_LE32(header, 0x04034b50); // Signature
    MZ_WRITE_LE16(header + 4, 20); // Version needed
    MZ_WRITE_LE16(header + 6, 0); // Flags
    MZ_WRITE_LE16(header + 8, 0); // Method (STORE)
    MZ_WRITE_LE16(header + 10, 0); // Time (TODO)
    MZ_WRITE_LE16(header + 12, 0); // Date (TODO)
    MZ_WRITE_LE32(header + 14, crc); // CRC32
    MZ_WRITE_LE32(header + 18, (unsigned int)buf_size); // Compressed size
    MZ_WRITE_LE32(header + 22, (unsigned int)buf_size); // Uncompressed size
    MZ_WRITE_LE16(header + 26, name_len); // Filename len
    MZ_WRITE_LE16(header + 28, 0); // Extra len

    if (!mz_file_write(pZip, header, 30)) return 0;
    if (!mz_file_write(pZip, pArchive_name, name_len)) return 0;
    if (!mz_file_write(pZip, pBuf, buf_size)) return 0;

    /* Add to Central Directory List */
    if (!pZip->m_pState) {
        pZip->m_pState = (struct mz_zip_internal_state*)malloc(sizeof(struct mz_zip_internal_state));
        memset(pZip->m_pState, 0, sizeof(struct mz_zip_internal_state));
    }

    struct mz_zip_internal_state *pState = pZip->m_pState;
    
    // Resize array
    size_t new_size = (pZip->m_total_files + 1) * sizeof(mz_zip_archive_file_stat);
    mz_zip_archive_file_stat *new_stats = (mz_zip_archive_file_stat*)realloc(pState->m_pState, new_size);
    if (!new_stats) return 0;
    pState->m_pState = new_stats;

    mz_zip_archive_file_stat *pStat = &pState->m_pState[pZip->m_total_files];
    memset(pStat, 0, sizeof(mz_zip_archive_file_stat));
    
    pStat->m_local_header_ofs = local_header_ofs;
    pStat->m_crc32 = crc;
    pStat->m_comp_size = (unsigned int)buf_size;
    pStat->m_uncomp_size = (unsigned int)buf_size;
    pStat->m_method = 0; // STORE
    strncpy(pStat->m_filename, pArchive_name, MZ_ZIP_MAX_ARCHIVE_FILENAME_SIZE-1);
    
    pZip->m_total_files++;
    return 1;
}

int mz_zip_writer_finalize_archive(mz_zip_archive *pZip)
{
    if (!pZip->m_pState) return 1; // Empty zip
    struct mz_zip_internal_state *pState = pZip->m_pState;
    
    unsigned int cd_start_ofs = mz_file_tell(pZip);
    
    for (unsigned int i = 0; i < pZip->m_total_files; i++) {
        mz_zip_archive_file_stat *pStat = &pState->m_pState[i];
        unsigned int name_len = (unsigned int)strlen(pStat->m_filename);
        
        unsigned char header[46];
        MZ_WRITE_LE32(header, 0x02014b50); // CD Signature
        MZ_WRITE_LE16(header + 4, 20); // Version made by
        MZ_WRITE_LE16(header + 6, 20); // Version needed
        MZ_WRITE_LE16(header + 8, 0); // Flags
        MZ_WRITE_LE16(header + 10, 0); // Method
        MZ_WRITE_LE16(header + 12, 0); // Time
        MZ_WRITE_LE16(header + 14, 0); // Date
        MZ_WRITE_LE32(header + 16, pStat->m_crc32);
        MZ_WRITE_LE32(header + 20, pStat->m_comp_size);
        MZ_WRITE_LE32(header + 24, pStat->m_uncomp_size);
        MZ_WRITE_LE16(header + 28, name_len); // Filename len
        MZ_WRITE_LE16(header + 30, 0); // Extra len
        MZ_WRITE_LE16(header + 32, 0); // Comment len
        MZ_WRITE_LE16(header + 34, 0); // Disk number start
        MZ_WRITE_LE16(header + 36, 0); // Internal attr
        MZ_WRITE_LE32(header + 38, 0); // External attr
        MZ_WRITE_LE32(header + 42, pStat->m_local_header_ofs); // Offset of local header
        
        if (!mz_file_write(pZip, header, 46)) return 0;
        if (!mz_file_write(pZip, pStat->m_filename, name_len)) return 0;
    }
    
    unsigned int cd_size = mz_file_tell(pZip) - cd_start_ofs;
    
    // End of Central Directory Record
    unsigned char eocd[22];
    MZ_WRITE_LE32(eocd, 0x06054b50); // EOCD Signature
    MZ_WRITE_LE16(eocd + 4, 0); // Disk number
    MZ_WRITE_LE16(eocd + 6, 0); // Disk number with CD
    MZ_WRITE_LE16(eocd + 8, (unsigned short)pZip->m_total_files); // Num entries on this disk
    MZ_WRITE_LE16(eocd + 10, (unsigned short)pZip->m_total_files); // Total num entries
    MZ_WRITE_LE32(eocd + 12, cd_size); // Size of CD
    MZ_WRITE_LE32(eocd + 16, cd_start_ofs); // Offset of CD
    MZ_WRITE_LE16(eocd + 20, 0); // Comment len
    
    if (!mz_file_write(pZip, eocd, 22)) return 0;
    
    // Cleanup
    if (pState->m_pState) free(pState->m_pState);
    free(pState);
    pZip->m_pState = NULL;
    
    return 1;
}

int mz_zip_writer_end(mz_zip_archive *pZip)
{
    if (pZip->m_pIO_opaque) fclose((FILE*)pZip->m_pIO_opaque);
    return 1;
}

const char *mz_zip_get_error_string(int mz_err) { return "Error"; }
