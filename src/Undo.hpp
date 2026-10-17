#ifndef MINIVIM_UNDO_HPP
#define MINIVIM_UNDO_HPP

#include <string>
#include <ctime>

#include "Buffer.hpp"
#include "Types.hpp"
#include "Mark.hpp"
#include "Command.hpp"
#include "Window.hpp"

namespace sjtu
{
    class UnDo_ReDo // 由于撤销重做处理起来比较hard，于是放主处理部分
    {
    public:
        void Renew_Time();
        void Renew_Position(Position pos = {0, 0});
        void Get_Do_Information(Buffer &buffer, Window &window);
        const std::string &GetUndoLineAt(std::size_t row) const;
        const size_t GetUndoLineCount() const;
        const Position &GetUndoCursor() const;
        void ApplyUndo(Buffer &buffer, Window &window);
        std::time_t GetTimeTag();
        std::string GetUndoTime(std::time_t Previous_Timetag) const;

    private:
        Position pos_;
        std::vector<std::string> Undo_lines_;
        EditorAction ActionWay_;
        std::time_t Timetag_;
        Mark undo_mark_;
    };

}
#endif // MINIVIM_UNDO_HPP