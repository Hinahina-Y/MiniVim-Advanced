#include <cmath>

#include "Editor.hpp" //这是因为有些和撤回相关的操作需要editor实现，放在这里节约Editor.cpp大小

namespace sjtu
{
    void UnDo_ReDo::Renew_Time()
    {
        Timetag_ = time(nullptr);
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
        undo_mark_ = buffer.mark_;
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
        size_t Seconds = 1 + (size_t)(std::fabs(std::difftime(Previous_Timetag, Timetag_)));
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
        buffer.mark_ = undo_mark_;
        window.SetCursor(buffer, pos_, false);
        window.EnsureCursorVisible(buffer);
        Renew_Time();
    }

    void Editor::DealUndoList(Position pos)
    {
        bool is_modified = false;
        if (!undo_list.empty())
        {
            is_modified = true;
            if (undo_list[undo_position_ - 1].GetUndoLineCount() != buffer_.GetLineCount())
            {
                is_modified = false;
            }
            else
            {
                for (size_t i = 0; i < undo_list[undo_position_ - 1].GetUndoLineCount(); i++)
                {
                    if (undo_list[undo_position_ - 1].GetUndoLineAt(i) != buffer_.GetLineAt(i))
                    {
                        is_modified = false;
                        break;
                    }
                }
            }
        }
        if (is_modified)
        {
            return;
        }
        UnDo_ReDo u_r;
        u_r.Get_Do_Information(buffer_, window_);
        if (undo_position_ < undo_list.size())
        {
            undo_list.erase(undo_list.begin() + undo_position_, undo_list.end());
        }
        undo_list.push_back(u_r);
        undo_position_++;
        while (undo_list.size() > 100 && undo_position_ != 0)
        {
            undo_list.erase(undo_list.begin());
            undo_position_--;
        }
        if (undo_list.size() > 1 && undo_position_ > 1)
        {
            undo_list[undo_position_ - 2].Renew_Position(pos);
        }
    }

    void Editor::Handle_UndoAndRedo(bool ruletype)
    {
        window_.Renew_Lastparser();
        size_t undocount{1};
        size_t previous_position = undo_position_;
        if (ruletype)
        {
            if (undo_position_ >= undo_list.size())
            {
                message_ = "No Undo Happened!";
                return;
            }
            if (editoraction_.count_.has_value())
            {
                undocount = *editoraction_.count_;
            }
            if (undo_position_ + undocount >= undo_list.size() + 1)
            {
                message_ = "No enough undo,redo:" + std::to_string(undo_list.size() - undo_position_) + " steps";
                undo_position_ = undo_list.size();
            }
            else
            {
                message_ = "Redo Succeed!";
                undo_position_ += undocount;
            }
        }
        else
        {
            if (undo_position_ == 0 || undo_list.size() == 0)
            {
                message_ = "No Change Happened!";
                return;
            }
            if (editoraction_.count_.has_value())
            {
                undocount = *editoraction_.count_;
            }
            if (undo_position_ - 1 < undocount)
            {
                message_ = "No enough changes,undo:" + std::to_string(undo_position_ - 1) + " steps";
                undo_position_ = 1;
            }
            else
            {
                message_ = "Undo Succeed!";
                undo_position_ -= undocount;
            }
        }
        message_ += "  " + undo_list[undo_position_ - 1].GetUndoTime(undo_list[previous_position - 1].GetTimeTag());
        undo_list[undo_position_ - 1].ApplyUndo(buffer_, window_);
        window_.SetCursor(buffer_, window_.GetCursor(), false);
        window_.RenewCount(std::nullopt);
        normal_parser_.Reset_editoraction(editoraction_);
    }
    void Editor::Handle_LineUndo()
    {
        window_.Renew_Lastparser();
        size_t undocount{1};
        size_t previous_position = undo_position_;
        if (undo_position_ <= 1 || undo_list.size() == 0)
        {
            return;
        }
        buffer_.copyLine(undo_list[undo_position_-2].GetUndoLineAt(window_.GetCursor().row_),window_.GetCursor().row_);
        window_.SetCursor(buffer_, window_.GetCursor(), false);
        window_.RenewCount(std::nullopt);
        normal_parser_.Reset_editoraction(editoraction_);
        DealUndoList(window_.GetCursor());
    }
}