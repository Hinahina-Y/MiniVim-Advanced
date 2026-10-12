#include <cmath>

#include "Undo.hpp"

namespace sjtu
{
    void UnDo_ReDo::Renew_Time()
    {
        Timetag_=time(nullptr);
    }
    
    void UnDo_ReDo::Renew_Position(Position pos)
    {
        pos_ = pos;
    }
    void UnDo_ReDo::Get_Do_Information(Buffer &buffer, Window &window)
    {
        size_t buffersize = buffer.GetLineCount();
        Undo_lines_.clear();
        for (size_t i = 0; i < buffersize; i++)
        {
            Undo_lines_.push_back(buffer.GetLineAt(i));
        }
        Renew_Position(window.GetCursor());
        Renew_Time();
    }
    const size_t UnDo_ReDo::GetUndoLineCount() const
    {
        return Undo_lines_.size();
    }
    const std::string &UnDo_ReDo::GetUndoLineAt(std::size_t row) const
    {
        return Undo_lines_[row];
    }
    const Position &UnDo_ReDo::GetUndoCursor() const
    {
        return pos_;
    }
    std::time_t UnDo_ReDo::GetTimeTag()
    {
        return Timetag_;
    }

    std::string UnDo_ReDo::GetUndoTime(std::time_t Previous_Timetag) const
    {
        size_t Seconds = 1+(size_t)(std::fabs(std::difftime(Previous_Timetag, Timetag_)));
        if (Seconds > 600)
        {
            struct std::tm PreviousTime{};
            localtime_r(&Previous_Timetag, &PreviousTime);
            char s[64];
            std::strftime(s, sizeof(s), "%Y-%m-%d %H:%M:%S", &PreviousTime);
            return std::string(s);
        }
        std::string ReturnString = std::to_string(Seconds) + " second";
        if (Seconds > 2)
        {
            ReturnString += "s";
        }
        ReturnString += " ago";
        return ReturnString;
    }

    void UnDo_ReDo::ApplyUndo(Buffer &buffer, Window &window)
    {
        buffer.copyBuffer(Undo_lines_);
        window.SetCursor(buffer, pos_, false);
        window.EnsureCursorVisible(buffer);
        Renew_Time();
    }
}