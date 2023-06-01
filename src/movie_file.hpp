/*
  Make GCF + wav and GMV common
*/
#ifndef MOVIE_FILE_HPP
#define MOVIE_FILE_HPP

#include "gob_gcf_file.hpp"
#include "gob_gmv_file.hpp"
#include "file_list.hpp"

class MovieFile
{
  public:
    MovieFile() {}
    virtual ~MovieFile() {}

    uint32_t baseFps() const { return _baseFps; }
    virtual  uint32_t readCount() const = 0;
    virtual bool eof() const = 0;
    virtual uint32_t maxFrames() const = 0;
    virtual const gob::wav_header_t& wavHeader() const = 0;
    
    virtual bool open(const String& path) = 0;
    virtual void close() = 0;
    virtual uint32_t readImage(uint8_t* buf, uint32_t sz) = 0;
    virtual uint32_t readWav(uint8_t* buf, uint32_t sz) = 0;
    virtual void rewind() = 0;

  protected:
    // Get the base fps from pathname
    uint32_t parsePath(const String& path)
    {
        // "foo.{num}.ext"
        // num is frame rate
        auto idx = path.lastIndexOf('.');
        if(idx < 2) { return 0; }
        auto idx2 = path.lastIndexOf('.', idx - 1);
        if(idx2 < 0) { return 0; }
        String frs = path.substring(idx2 + 1, idx);
        int fr = frs.toInt();
        return fr < 1 ? 0 : fr;
    }
    uint32_t _baseFps{};
};

class MovieFileGCF : public MovieFile
{
  public:
    MovieFileGCF() : MovieFile() {}
    virtual ~MovieFileGCF() { close(); }

    virtual uint32_t readCount() const override{ return _gcf.readCount(); }
    virtual bool eof() const override { return _gcf.eof(); }
    virtual uint32_t maxFrames() const override { return _gcf.blocks(); }
    virtual const gob::wav_header_t& wavHeader() const override { return _wheader; }
    
    virtual bool open(const String& path) override
    {
        close();
        _baseFps = parsePath(path);
        if(!_baseFps) { return false; }
        String wpath = FileList::changeExt(path, "wav");
        if(_gcf.open(path) && _wfile.open(wpath.c_str()))
        {
            auto sz = _wfile.read(&_wheader, sizeof(_wheader));
            _wfile.seek(0);
            return sz == sizeof(_wheader);
        }
        return false;
    }

    virtual void close() override
    {
        _gcf.close(); _wfile.close();
    }

    virtual uint32_t readImage(uint8_t* buf, uint32_t sz) override
    {
        return _gcf.read(buf, sz);
    }
    
    virtual uint32_t readWav(uint8_t* buf, uint32_t sz) override
    {
        if(sz < _wfile.size()) return 0; // If I can't read it all, don't read it.
        return _wfile.read(buf, sz);
    }

    virtual void rewind() override
    {
        _gcf.rewind();
        _wfile.seek(0);
    }
    
  private:
    gob::GCFFile _gcf{};
    FsFile _wfile{}; // Wav file
    gob::wav_header_t _wheader{};
};

class MovieFileGMV : public MovieFile
{
  public:
    MovieFileGMV() : MovieFile() {}
    virtual ~MovieFileGMV() { close(); }

    virtual uint32_t readCount() const override { return _gmv.readCount(); }
    virtual bool eof() const override { return _gmv.eof(); }
    virtual uint32_t maxFrames() const override { return _gmv.blocks(); }
    virtual const gob::wav_header_t& wavHeader() const override { return _gmv.wavHeader(); }
    
    virtual bool open(const String& path) override
    {
        _baseFps = 0;
        if(_gmv.open(path))
        {
            _baseFps = _gmv.fps();
            return _baseFps > 0;
        }
        return false;
    }
    virtual void close() override { _gmv.close(); }

    // Read image and wav togther
    virtual uint32_t readImage(uint8_t* buf, uint32_t sz) override
    {
        return _gmv.readBlock(buf, sz);
    }

    // Wav block is read together with image and returns only the size.
    virtual uint32_t readWav(uint8_t* , uint32_t) override
    {
        return _gmv.wavSize();
    }
    virtual void rewind() override { _gmv.rewind(); }
    
  private:
    gob::GMVFile _gmv{};
};


#endif
