// Make playback file list
#include <SdFat.h>
#include <M5Unified.h>
#include "file_list.hpp"
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

FileList::FileList()
{
    _list.reserve(64);
}

uint32_t FileList::make(const char* base, const char* ext)
{
    _base = base;
    _cur = 0;
    _list.clear();
    
    M5_LOGI("base dir:[%s]", base);
        
    FsFile dir;
    if(!dir.open(_base.c_str())) { return 0; }

    FsFile f;
    while(f.openNext(&dir, O_RDONLY))
    {
        if(f.isDir()) { continue; }

        char path[256];
        f.getName(path, sizeof(path));
        if(path[0] == '.' || getExt(path) != ext) { continue; }

        M5_LOGD("list:[%s]", path);
        _list.emplace_back(path);
        f.close();
    }
    sort();
    return _list.size();
}

void FileList::shuffle()
{
    _shuffle = true;
    if(!_list.empty())
    {
        std::shuffle(_list.begin(), _list.end(), ESP32Rng(_list.size()));
    }
}

String FileList::changeExt(const String& path, const char* ext)
{
    String s(path);
    auto idx = s.lastIndexOf('.');
    if(idx >= 0) { s = path.substring(0, idx + 1) + ext; }
    else         { s += String('.') + ext; }
    return s;
}

String FileList::getExt(const String& path)
{
    String ext;
    auto idx = path.lastIndexOf('.');
    if(idx >= 0) { ext = path.substring(idx + 1); }
    return ext;
}
