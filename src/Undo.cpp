#include "Undo.hpp"

namespace sjtu
{
    void UnDo_ReDo::Get_Do_Information(Buffer &buffer, Window &window)
    {
        size_t buffersize = buffer.GetLineCount();
        Undo_lines_.clear();
        for (size_t i = 0; i < buffersize; i++)
        {
            Undo_lines_.push_back(buffer.GetLineAt(i));
        }
        pos_ = window.GetCursor();
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
    void UnDo_ReDo::Renew_Position(Position pos)
    {
        pos_ = pos;
    }
    std::time_t UnDo_ReDo::GetTimeTag()
    {
        return Timetag_;
    }

    std::string UnDo_ReDo::GetUndoTime(std::time_t previous_timetag) const
    {
        size_t Seconds = (size_t)(std::abs(std::difftime(previous_timetag, Timetag_)));
        if (Seconds > 600)
        {
            struct std::tm PreviousTime;
            localtime_r(&previous_timetag, &PreviousTime);
            char s[64];
            std::strftime(s, sizeof(s), "%Y-%m-%d %H:%M:%S", &PreviousTime);
            return std::string(s);
        }
        std::string ReturnString = std::to_string(Seconds) + " second";
        if (Seconds > 1)
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
    }
}