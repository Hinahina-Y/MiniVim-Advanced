#include <cmath>
#include <fstream>

#include "Mark.hpp"
namespace sjtu
{
    void Mark::NewMark(char name, Position pos)
    {
        mark_pos_[name] = pos;
        return;
    }
    Position Mark::GetMarkPosition(char name)
    {
        if (mark_pos_.find(name) != mark_pos_.end())
        {
            return mark_pos_[name];
        }
        throw std::runtime_error("Error:Wrong mark!");
    }
    void Mark::EditMark(size_t row, int sum) // 将row行及以下(>=row)的mark坐标全部增加sum行(sum可为负数)
    {
        for (std::map<char, Position>::iterator it = mark_pos_.begin(); it != mark_pos_.end(); it++)
        {
            if (it->second.row_ >= row)
            {
                it->second.row_ = std::max(0, int(it->second.row_) + sum);
            }
        }
    }
    void Mark::DeleteMark(size_t row_up, size_t row_down) // 删除[up,down)行的所有mark
    {
        std::map<char, Position>::iterator it = mark_pos_.begin();
        while (it != mark_pos_.end())
        {
            if (it->second.row_ >= row_up && it->second.row_ < row_down)
            {
                it = mark_pos_.erase(it);
            }
            else
            {
                it++;
            }
        }
    }
}