/*!
  @file gob_gcf_file.hpp
  @brief Accessor for GCF
 */
#ifndef GOB_GCF_FILE_HPP
#define GOB_GCF_FILE_HPP

#include "gob_gcf.hpp"
#include <SdFat.h>

namespace gob
{

class GCFFile
{
  public:
    static constexpr uint32_t Signature = 0x30464347; // "GCF0"

    GCFFile() {}
    ~GCFFile() { if(_file) {_file.close(); } }

    inline uint32_t blocks() const { return _header.blocks; }
    inline uint32_t readCount() const { return _current ? _current - 1 : 0; }
    inline bool eof() const { return _file && _current >= blocks(); }
    explicit inline operator bool() const { return (bool)_file; }

    bool open(const String& path) { return open(path.c_str()); }
    bool open(const char* path)
    {
        _header = {};
        _current = _blockHead = 0;
        close();

        if(!_file.open(path) ||
           _file.read(&_header, sizeof(_header)) != sizeof(_header) ||
           _header.signature != GCFHeader::Signature)
        {
            return false;
        }
        _blockHead = _file.position();
        return true;
    }
    void close() { if(_file) { _file.close(); }}

    // Read 1 block
    uint32_t read(uint8_t* buf, uint32_t sz)
    {
        if(!_file || eof()) { return 0; }

        uint32_t len{};
        _file.read(&len, sizeof(len));
        ++_current;
        
        if(len == 0 || // Empty file
           len == GCFBlock::Terminator // End of file
           ) { return 0; }
        
        uint32_t skip{};
        if(len > sz) { skip = len - sz; }
        
        auto r = _file.read(buf, std::min<uint32_t>(sz, len));
        if(skip)
        {
            auto pos = _file.position();
            _file.seek(pos + skip);
        }
        return r;
    }

    // Rewind to 1st JPEG
    bool rewind()
    {
        _current = 0;
        return _file ? _file.seek(_blockHead) : false;
    }
    
  private:
    FsFile _file;
    GCFHeader _header{};
    uint32_t _current{};
    uint32_t _blockHead{}; // Head of block
};
//
}
#endif
