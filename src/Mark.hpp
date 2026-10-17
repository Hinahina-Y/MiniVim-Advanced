#ifndef MINIVIM_MARK_HPP
#define MINIVIM_MARK_HPP

#include <string>
#include <map>

#include "Types.hpp"

namespace sjtu
{
    class Mark
    {
    public:
        void NewMark(char name,Position pos);
        void DeleteMark(size_t row_up,size_t row_down);//删除[up,down)行的所有mark
        void EditMark(size_t row,int sum);//将row行及以下的mark坐标全部增加sum行(sum可为负数)
        Position GetMarkPosition(char name);
    private:
        std::map <char,Position> mark_pos_;
    };

}
#endif // MINIVIM_UNDO_HPP