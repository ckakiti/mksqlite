/**
 *  mksqlite: A MATLAB Interface to SQLite
 * 
 *  @file      typed_blobs.hpp
 *  @brief     Packing MATLAB data in a memory block with type information for storing as SQL BLOB
 *  @details   
 *  @author    Martin Kortmann <mail@kortmann.de>
 *  @author    Andreas Martin  <andi.martin@gmx.net>
 *  @version   2.0
 *  @date      2008-2014
 *  @copyright Distributed under LGPL
 *  @pre       
 *  @warning   
 *  @bug       
 */

#pragma once 

#include "config.h"
#include "global.hpp"
#include "utils.hpp"

#ifdef _WIN32
  #define GCC_PACKED_STRUCT
  #pragma pack(1)
#else
  #define GCC_PACKED_STRUCT __attribute__((packed))
#endif
  
  /* 
   * Size of blob-header identifies type 1 or type 2 (with compression feature).
   *
   * Blobs of type mxUnknown_Class respects serialized (streamed) data and should be handled
   * as mxChar_Class thus. Before packing data into a typed blob the caller is 
   * responsible to ensure not to deal with mxUnknown_Class data.
   *
   * In this module it's countless if the data is serialized or not, since it's a simple char
   * array in that case. 
   */

  
/* Store type and dimensions of MATLAB vectors/arrays in BLOBs 
 * native and free of matlab types, to provide data sharing 
 * with other applications
 * Set by mksqlite('typedBLOBs', <integer value>)
 * where <integer value> is one of enum typed_blobs_e:
 */
typedef enum {
  TYBLOB_NO = 0,       // no typed blobs
  TYBLOB_ARRAYS,       // ability of storaging multidimensional non-complex arrays as typed blobs
  TYBLOB_COMPRESSED,   // ability of storing compressed data
  
  // Limit for bound checking only
  TYBLOB_MAX_ID = TYBLOB_COMPRESSED
} typed_blobs_e;


/* length definitions for typed header fields */
#define TBH_MAGIC_MAXLEN      14
#define TBH_PLATFORM_MAXLEN   11
#define TBH_COMPRID_MAXLEN    12
#define TBH_ENDIAN_MAXLEN      2



/* fields to identify valid headers */
extern const char  TBH_MAGIC[];     /* constant, see below */
extern       char  TBH_platform[];  /* set by main module */
extern       char  TBH_endian[];    /* set by main module */


#ifdef MAIN_MODULE

/* Implementations */

static typed_blobs_e typed_blobs_mode = TYBLOB_NO; // Default is off

namespace old_version { int check_compatibility(void); };

void typed_blobs_init()
{
    mxArray *plhs[3] = {0};
    
    assert( old_version::check_compatibility() );
    
    if( 0 == mexCallMATLAB( 3, plhs, 0, NULL, "computer" ) )
    {
        mxGetString( plhs[0], TBH_platform, TBH_PLATFORM_MAXLEN );
        mxGetString( plhs[2], TBH_endian, TBH_ENDIAN_MAXLEN );

        utils_destroy_array( plhs[0] );
        utils_destroy_array( plhs[1] );
        utils_destroy_array( plhs[2] );
    }
}

void typed_blobs_mode_set( typed_blobs_e mode )
{
    typed_blobs_mode = mode;
}

typed_blobs_e typed_blobs_mode_get()
{
    return typed_blobs_mode;
}

bool typed_blobs_mode_check( typed_blobs_e mode )
{
    return typed_blobs_mode == mode;
}

/*static*/ const char  TBH_MAGIC[TBH_MAGIC_MAXLEN]         = "mkSQLite.tbh\0";
/*static*/       char  TBH_platform[TBH_PLATFORM_MAXLEN]   = {0};
/*static*/       char  TBH_endian[TBH_ENDIAN_MAXLEN]       = {0};

#endif

// typed BLOB header agreement
// typed_BLOB_header_base is the unique and mandatory header prelude for typed blob headers
struct GCC_PACKED_STRUCT TypedBLOBHeaderBase 
{
  char    m_magic[TBH_MAGIC_MAXLEN];        // + 14 small fail-safe header check
  int16_t m_ver;                            // +  2 Struct size as kind of header version number for later backwards compatibility (may increase only!)
  int32_t m_clsid;                          // +  4 Matlab ClassID of variable (see mxClassID)
  char    m_platform[TBH_PLATFORM_MAXLEN];  // + 11 Computer architecture: PCWIN, PCWIN64, GLNX86, GLNXA64, MACI, MACI64, SOL64
  char    m_endian;                         // +  1 Byte order: 'L'ittle endian or 'B'ig endian
                                            // = 32 Bytes (+4 bytes for int32_t m_nDims[1] later)
  
  void init( mxClassID clsid )
  {
    strcpy( m_magic,    TBH_MAGIC );
    strcpy( m_platform, TBH_platform );
    
    m_ver       = (int16_t)sizeof( *this );
    m_clsid     = (int32_t)clsid; 
    m_endian    = TBH_endian[0];
  }
  
  
  bool validMagic()
  {
    return 0 == strncmp( m_magic, TBH_MAGIC, TBH_MAGIC_MAXLEN );
  }
  
  // check if MATLAB variable has valid class id to store in a typed blob
  static
  bool validClsid( mxClassID clsid )
  {
    switch( clsid )
    {
      // standard types can be stored in any typed blob
      case mxLOGICAL_CLASS:
      case    mxCHAR_CLASS:
      case  mxDOUBLE_CLASS:
      case  mxSINGLE_CLASS:
      case    mxINT8_CLASS:
      case   mxUINT8_CLASS:
      case   mxINT16_CLASS:
      case  mxUINT16_CLASS:
      case   mxINT32_CLASS:
      case  mxUINT32_CLASS:
      case   mxINT64_CLASS:
      case  mxUINT64_CLASS:
        return true;
      default:
        // no other types supported
        return false;
    }
  }
  

  // check a data type given
  static
  bool validClsid( const mxArray* pItem )
  {
    return pItem && validClsid( mxGetClassID( pItem ) );
  }
  
  
  // check self data type
  bool validClsid()
  {
    return validClsid( (mxClassID)m_clsid );
  }
  
  
  // check if originate platform equals to running one
  bool validPlatform()
  {
    return TBH_endian[0] == m_endian && 0 == strncmp( TBH_platform, m_platform, TBH_PLATFORM_MAXLEN );
  }
  
  
  // get data size of an item in bytes
  static
  size_t getDataSize( const mxArray* pItem )
  {
    size_t data_size = 0;
    
    if( pItem )
    {
      size_t szElement   = mxGetElementSize( pItem );
      size_t cntElements = mxGetNumberOfElements( pItem );
      
      data_size = szElement * cntElements;
    }
    
    return data_size;
  }
};


// 2nd version of typed blobs with compression feature
/* 
 * IMPORTANT: NEVER ADD VIRTUAL FUNCTIONS TO HEADER CLASSES DERIVED FROM BASE!
 * Reason: Size of struct wouldn't match since a hidden vtable pointer 
 *         would be attached then!
 */
struct GCC_PACKED_STRUCT TypedBLOBHeaderCompressed : public TypedBLOBHeaderBase 
{
  /* name of the compression algorithm used. Other algorithms
   * possible in future..?
   */
  char m_compression[12];
  
//  void init( mxClassID clsid, const char* strCompressorType )
  void init( mxClassID clsid )
  {
    TypedBLOBHeaderBase::init( clsid );
    setCompressor( "" );
  }

  void setCompressor( const char* strCompressorType )
  {
    strncpy( m_compression, strCompressorType, sizeof( m_compression ) );
  }
  
  // for now, all compressor types should be valid
  bool validCompression()
  {
    // \todo add checking?
    return 1;
  }
};


/* Template to append data and its dimensions uniquely to a typed BLOB header */
template< typename HeaderBaseType >
struct GCC_PACKED_STRUCT TBHData : public HeaderBaseType
{
  // Number of dimensions, followed by sizes of each dimension
  int32_t m_nDims[1];  
  // (BLOB data follows after last dimension size...)
  
  void init( mxClassID clsid, mwSize nDims, const mwSize* pSize )
  {
    HeaderBaseType::init( clsid );
    HeaderBaseType::m_ver = sizeof( *this );
    
    assert( nDims >= 0 );
    assert( !nDims || pSize );
    
    m_nDims[0] = nDims;
    for( int i = 0; i < nDims; i++ )
    {
      m_nDims[i+1] = (int32_t)pSize[i];
    }
  }
  
  
  // set class id and dimension information of an item
  void init( const mxArray* pItem )
  {
    assert( pItem );
    mxClassID clsid = mxGetClassID( pItem );
    mwSize nDims = mxGetNumberOfDimensions( pItem );
    const mwSize* dimensions = mxGetDimensions( pItem );
    
    init( clsid, nDims, dimensions );
  }
  
  
  // version information is stored as struct size. compare if
  // current size equals to assumed
  bool validVer()
  {
      return sizeof(*this) == (size_t)HeaderBaseType::m_ver;
  }
  
  
  // get a pointer to data begin with number of dimensions given
  // first data byte starts after last dimension
  void* getData( mwSize nDims )
  {
    return (void*)&m_nDims[ nDims + 1 ];
  }
  
  
  // get a pointer to self data begin
  void* getData()
  {
    return getData( m_nDims[0] );
  }
  
  
  // get the offset from structure begin to data begin with given
  // number of dimensions
  static
  size_t dataOffset( mwSize nDims )
  {
    //return offsetof( TBHData, m_nDims[nDims+1] ); /* doesn't work on linux gcc 4.1.2 */
    TBHData* p = (TBHData*)1024; // p must be something other than 0, due to compiler checking
    return (char*)&p->m_nDims[nDims+1] - (char*)p;
  }
  
  
  // get the offset from structure begin to self data begin
  size_t dataOffset()
  {
    return dataOffset( m_nDims[0] );
  }
  
  
  // get data size in bytes, returns 0 on error
  size_t getDataSize()
  {
    return utils_elbytes( (mxClassID)HeaderBaseType::m_clsid );
  }
  
  
  // create a numeric array suitable for self item data
  // if doCopyData is true, item data is copied into array
  mxArray* createNumericArray( bool doCopyData )
  {
    mwSize nDims = m_nDims[0];
    mwSize* dimensions = new mwSize[nDims];
    mxArray* pItem = NULL;
    mxClassID clsid = (mxClassID)HeaderBaseType::m_clsid;
    
    for( int i = 0; i < nDims; i++ )
    {
      dimensions[i] = (mwSize)m_nDims[i+1];
    }
    
    pItem = mxCreateNumericArray( nDims, dimensions, clsid, mxREAL );
    delete[] dimensions;
    
    // copy hosted item data into numeric array
    if( pItem && doCopyData )
    {
      memcpy( mxGetData( pItem ), getData(), TypedBLOBHeaderBase::getDataSize( pItem ) );
    }
    
    return pItem;
  }
  
private: 
  // Disabling create instances! 
  // Scheme represented by this template, and others derived, do only shadow 
  // memory got from allocators (malloc)
  TBHData();
  TBHData( const TBHData& );
  TBHData& operator=( const TBHData& );

};

typedef TBHData<TypedBLOBHeaderBase>       TypedBLOBHeaderV1;
typedef TBHData<TypedBLOBHeaderCompressed> TypedBLOBHeaderV2;


///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
/* Test backward compatibility, will be removed in future releases */
namespace old_version {
    /* Store type and dimensions of MATLAB vectors/arrays in BLOBs */
    static bool use_typed_blobs   = false;
    static const char TBH_MAGIC[] = "mkSQLite.tbh";
    static char TBH_platform[11]    = {0};
    static char TBH_endian[2]       = {0};

    // typed BLOB header agreement
    // native and free of matlab types, to provide data sharing with other applications

#ifdef _WIN32
#pragma pack( push )
#pragma pack()
#endif

    typedef struct 
    {
      char magic[sizeof(TBH_MAGIC)];  // small fail-safe header check
      int16_t ver;                    // Struct size as kind of header version number for later backwards compatibility (may increase only!)
      int32_t clsid;                  // Matlab ClassID of variable (see mxClassID)
      char platform[11];              // Computer architecture: PCWIN, PCWIN64, GLNX86, GLNXA64, MACI, MACI64, SOL64
      char endian;                    // Byte order: 'L'ittle endian or 'B'ig endian
      int32_t sizeDims[1];            // Number of dimensions, followed by sizes of each dimension
                                      // First byte after header at &tbh->sizeDims[tbh->sizeDims[0]+1]
    } TypedBLOBHeader;

#ifdef _WIN32
#pragma pack( pop )
#endif

#define TBH_DATA(tbh)            ((void*)&tbh->sizeDims[tbh->sizeDims[0]+1])
#define TBH_DATA_OFFSET(nDims)   ((ptrdiff_t)&((TypedBLOBHeader*) 0)->sizeDims[nDims+1])

    int check_compatibility()
    {
        TypedBLOBHeaderV1*  tbh1         = (TypedBLOBHeaderV1*)1024;
        TypedBLOBHeader*    old_struct   = (TypedBLOBHeader*)1024;

        if(     (void*)&tbh1->m_ver          == (void*)&old_struct->ver
            &&  (void*)&tbh1->m_clsid        == (void*)&old_struct->clsid
            &&  (void*)&tbh1->m_platform[0]  == (void*)&old_struct->platform[0]
            &&  (void*)&tbh1->m_endian       == (void*)&old_struct->endian
            &&  (void*)&tbh1->m_nDims        == (void*)&old_struct->sizeDims
            &&   TBH_DATA_OFFSET(2)          == TypedBLOBHeaderV1::dataOffset(2) )
        {
            return 1;
        } 
        else 
        {
            return 0;
        }
    }

#undef TBH_DATA
#undef TBH_DATA_OFFSET
};
