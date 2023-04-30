// gob combined files 
/*
#  header
#  uint32_t signeture "GCF0" gob combine file Version 0
#  uint32_t file count
#  uint32_t reserve[2]
#
#  files
#  uint32_t size  0 is end of GCF file
#  uint8_t [size] 
#  ....
*/
#ifndef GOB_COMBINED_FILES_HPP
#define GOB_COMBINED_FILES_HPP

#include <cstdint>
#include <SdFat.h>

namespace gob
{

struct CombinedFilesHeader
{
    uint32_t signeture{}; // "GCF0"
    uint32_t files{};
    uint32_t reserved0{};
    uint32_t reserved1{};
};

class CombinedFiles
{
  public:
    CombinedFiles() {}
    ~CombinedFiles() { if(_file) {_file.close(); } }
    uint32_t readed() const { return _current ? _current - 1 : 0; }

    //
    bool open(const char* path)
    {
        _header = {};
        _current = 0;

        close();
        _file.open(path);

        size_t sz{};
        if(_file)
        {
            sz = _file.read(&_header, sizeof(_header));
            if(sz != sizeof(_header)) { close(); return false; }
        }
        return (bool)_file && sz == sizeof(_header) && _header.signeture == 0x30464347;
    }
    
    void close() { if(_file) { _file.close(); }}

    // Read 1 file block
    size_t read(uint8_t* buf, size_t sz)
    {
        if(!_file || eof()) { return 0; }

        uint32_t len{};
        _file.read(&len, sizeof(len));
        ++_current;

        if(len == 0 || // Empty file
           len == 0xFFFFFFFF // End of file
           ) { return 0; }
        
        uint32_t drop{};
        if(len > sz) { drop = len - sz; }

        //printf("drop:%u\n", drop);
        
        auto r = _file.read(buf, std::min<uint32_t>(sz, len));
        if(drop)
        {
            auto pos = _file.position();
            _file.seek(pos + drop);
        }
        return r;
    }

    // Rewind to 1st file
    bool rewind()
    {
        _current = 0;
        return _file ? _file.seek(sizeof(_header)) : false;
    }
    
    explicit inline operator bool() const { return (bool)_file; }
    inline uint32_t files() const { return _header.files; }
    inline bool eof() const { return _file && _current >= files(); }
    
  private:
    FsFile _file;
    uint32_t _current{};
    CombinedFilesHeader _header{};
};
//
}
#endif
