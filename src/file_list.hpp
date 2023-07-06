// Make playback file list
#ifndef FILE_LIST_HPP
#define FILE_LIST_HPP

#include <WString.h>
#include <vector>

// WARNING: path must be in the ASCII range. (KANJI character is denied)
// 警告: ファイル名は ASCII の範囲である事 (漢字やかな文字禁止)
class FileList
{
  public:
    FileList();

    uint32_t make(const char* base, const char* ext = "gcf");
    
    inline int32_t current() const { return _cur; }
    inline int32_t files() const { return _list.size(); }

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
    void shuffle();

    static String changeExt(const String& path, const char* ext);
    static String getExt(const String& path);
    
  protected:
    String _base{"/"};
    std::vector<String> _list{};
    int32_t _cur{};
    bool _shuffle{};
};
#endif
