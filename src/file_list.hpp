#ifndef FILE_LIST_HPP
#define FILE_LIST_HPP

#include <SdFat.h>
#include <WString.h>
#include <vector>
#include <algorithm>
#include <esp_random.h> // esp_random() is hardware RNG. (No random seed initialization is required)

// For std::shuffle
struct ESP32Rng
{
    using result_type = uint32_t;
    inline explicit ESP32Rng(result_type maxValue) : _maxValue(maxValue) { assert(_maxValue); }
    inline result_type min() { return 0; }
    inline result_type max() { return _maxValue; }
    result_type operator()() { return esp_random() % max(); }
  private:
    result_type _maxValue{1};
};

// WARNING: path must be in the ASCII range. (KANJI character is denied)
class FileList
{
  public:
    FileList() { _list.reserve(64); }
    uint32_t make(const char* base, const char* ext = "gcf")
    {
        _base = base;
        _cur = 0;
        _list.clear();

        Serial.printf("base dir:[%s]\n", base);
        
        FsFile dir;
        if(!dir.open(_base.c_str())) { return 0; }

        FsFile f;
        while(f.openNext(&dir, O_RDONLY))
        {
            if(f.isDir()) { continue; }

            char path[256];
            f.getName(path, sizeof(path));
            Serial.printf("list:[%s]\n", path);
            if(path[0] == '.' || getExt(path) != "gcf") { continue; }
            _list.emplace_back(path);
            ++_files;
            f.close();
        }
        sort();
        return _files;
    }

    int32_t current() const { return _cur; }
    int32_t files() const { return _files; }

    String getCurrent() const { return _cur < _list.size() ? _list[_cur] : String(""); }
    String getCurrentFullpath() const
    {
        return _base + "/" + getCurrent();
    }

    void prev() { if(!_list.empty() && --_cur < 0) { _cur = _list.size() - 1; } }
    void next()
    {
        if(!_list.empty() && ++_cur >= _list.size())
        {
            _cur = 0;
            // If it's a shuffle, shuffle again when you rewind
            if(_shuffle) { shuffle(); }
        }
    }

    void sort() { _shuffle = false; std::sort(_list.begin(), _list.end()); }
    void shuffle()
    {
        _shuffle = true;
        if(!_list.empty())
        {
            std::shuffle(_list.begin(), _list.end(), ESP32Rng(_list.size()));
        }
    }

    static String changeExt(const String& path, const char* ext)
    {
        String s(path);
        auto idx = s.lastIndexOf('.');
        if(idx >= 0) { s = path.substring(0, idx + 1) + ext; }
        else         { s += String('.') + ext; }
        return s;
    }

    static String getExt(const String& path)
    {
        String ext;
        auto idx = path.lastIndexOf('.');
        if(idx >= 0) { ext = path.substring(idx + 1); }
        return ext;
    }
    
  protected:
    String _base{};
    std::vector<String> _list{};
    int32_t _cur{};
    int32_t _files{};
    bool _shuffle{}; //
};
#endif
