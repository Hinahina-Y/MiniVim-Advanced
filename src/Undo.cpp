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
        pos_=pos;
    }
    void UnDo_ReDo::ApplyUndo(Buffer &buffer, Window &window)
    {
        buffer.copyBuffer(Undo_lines_);
        window.SetCursor(buffer, pos_, false);
        window.EnsureCursorVisible(buffer);
    }
}