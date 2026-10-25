#include "Editor.hpp"
#include "Register.hpp"

#include <algorithm>
#include <cctype>
#include <exception>

// 处理寄存器相关逻辑，如复制黏贴
namespace sjtu
{
    void Register::Copy_into_Reg(const Buffer &buffer, Position start_pos, Position end_pos) // 复制[start,end)位置的值
    {
        if (end_pos < start_pos)
            return;
        if (reg_kind != RegKind::charReg)
        {
            return;
        }
        size_t row = start_pos.row_, index = 0;
        if (row == end_pos.row_)
        {
            reg2_.push_back(buffer.GetLineAt(row).substr(start_pos.column_, end_pos.column_ - start_pos.column_));
            return;
        }
        while (row < end_pos.row_ && row < buffer.GetLineCount())
        {
            if (row == start_pos.row_)
                reg2_.push_back(buffer.GetLineAt(start_pos.row_).substr(start_pos.column_));
            else
                reg2_.push_back(buffer.GetLineAt(row));
            row++;
        }
        if (row < buffer.GetLineCount())
            reg2_.push_back(buffer.GetLineAt(row).substr(0, end_pos.column_));
        return;
    }
    void Register::Copy_into_Reg(const Buffer &buffer, size_t row_up, size_t row_down) // 复制[up,down)行的内容
    {
        if (reg_kind != RegKind::lineReg)
        {
            return;
        }
        if (row_down > buffer.GetLineCount())
        {
            row_down = buffer.GetLineCount();
        }
        for (size_t row = row_up; row < row_down; row++)
        {
            reg2_.push_back(buffer.GetLineAt(row));
        }
    }

    bool Register::Copy_into_Reg(const std::vector<std::string> &lines)
    {
        reg2_ = lines;
        return !lines.empty();
    }

    void Register::set_reg_kind(RegKind kind)
    {
        reg_kind = kind;
    }
    void Register::paste_outo_buffer(Buffer &buffer, Window &window, Position start_pos, size_t count = 1) const
    {
        if (reg_kind == RegKind::charReg) // char
        {
            window.SetCursor(buffer, start_pos, true);
            while (count > 0)
            {
                size_t index{0};
                while (index < reg2_.size())
                {
                    buffer.InsertString(window.GetCursor().row_, window.GetCursor().column_, reg2_[index]);
                    window.SetCursor(buffer, {window.GetCursor().row_, window.GetCursor().column_ + reg2_[index].size()}, true);
                    if (index + 1 < reg2_.size())
                    {
                        buffer.SplitLine(window.GetCursor().row_, window.GetCursor().column_);
                        window.ApplyMotion(buffer, Motion::Go_Down_Line);
                    }
                    index++;
                }
                count--;
            }
            return;
        }
        if (reg_kind == RegKind::lineReg) // line
        {
            for (size_t count_i = 0; count_i < count; count_i++)
            {
                buffer.InsertLines(start_pos.row_ + count_i * reg2_.size(), reg2_);
            }
            return;
        }
    }

    bool Register::Execute_copy(Buffer &buffer, Window &window, EditorAction editoraction)
    {
        Window temporary_window = window;
        size_t count{1};
        reg2_.clear();
        if (editoraction.count_.has_value())
        {
            count = *editoraction.count_;
        }
        switch (*editoraction.type_)
        {
        case ActionType::Ordinary:
            switch (*editoraction.motion_)
            {
            case Motion::Go_Copy:
            case Motion::Go_Delete: // Go_Delete处理dd
                set_reg_kind(RegKind::lineReg);
                Copy_into_Reg(buffer, window.GetCursor().row_, window.GetCursor().row_ + count);
                return !(reg2_.empty());
            case Motion::Down:
                set_reg_kind(RegKind::lineReg);
                Copy_into_Reg(buffer, window.GetCursor().row_, window.GetCursor().row_ + count + 1);
                return !(reg2_.empty());
            case Motion::Up:
                set_reg_kind(RegKind::lineReg);
                window.ApplyMotion(buffer, Motion::Up);
                window.SetCursor(buffer, window.GetCursor(), false);
                Copy_into_Reg(buffer, window.GetCursor().row_, temporary_window.GetCursor().row_ + 1);
                return !(reg2_.empty());
            case Motion::Left:
            case Motion::Right:
            case Motion::Goto_Front:
            case Motion::Goto_Back:
            case Motion::Goto_First_Noblank:
            case Motion::Left_allow_crossline:
                set_reg_kind(RegKind::charReg);
                temporary_window.ApplyMotion(buffer, *editoraction.motion_);
                if (temporary_window.GetCursor() < window.GetCursor())
                {
                    std::swap(temporary_window, window);
                }
                Copy_into_Reg(buffer, window.GetCursor(), temporary_window.GetCursor());
                window.SetCursor(buffer, window.GetCursor(), false);
                return !(reg2_.empty());
            default:
                break;
            }
            break;

        case ActionType::word:
        case ActionType::WORD:
            switch (*editoraction.motion_)
            {
            case Motion::Left:
            case Motion::Right:
            case Motion::Up:
            case Motion::Down:
                set_reg_kind(RegKind::charReg);
                temporary_window.ApplyMoveword(buffer, *editoraction.motion_, *editoraction.type_ == ActionType::WORD);
                if (temporary_window.GetCursor() < window.GetCursor())
                {
                    Position temp_cursor;
                    temp_cursor = window.GetCursor();
                    window.SetCursor(buffer, temporary_window.GetCursor(), true);
                    temporary_window.SetCursor(buffer, temp_cursor, true);
                }
                Copy_into_Reg(buffer, window.GetCursor(), temporary_window.GetCursor());
                window.SetCursor(buffer, window.GetCursor(), false);
                return !(reg2_.empty());
            }
            break;
        case ActionType::To:
            set_reg_kind(RegKind::lineReg);
            temporary_window.ApplyJumpto(buffer, *editoraction.motion_, *editoraction.type_);
            if (temporary_window.GetCursor() < window.GetCursor())
            {
                Position temp_cursor;
                temp_cursor = window.GetCursor();
                window.SetCursor(buffer, temporary_window.GetCursor(), false);
                temporary_window.SetCursor(buffer, temp_cursor, true);
            }
            Copy_into_Reg(buffer, window.GetCursor().row_, temporary_window.GetCursor().row_ + 1);
            return !(reg2_.empty());
        case ActionType::Match:
            set_reg_kind(RegKind::charReg);
            Position match_pos = window.Parenthesis_Matching(buffer);
            if (match_pos.row_ == buffer.GetLineCount())
            {
                return false;
            }
            if (match_pos < window.GetCursor())
            {
                Position temp_cursor = match_pos;
                match_pos = window.GetCursor();
                window.SetCursor(buffer, temp_cursor, true);
            }
            else
            {
                match_pos.column_+1;
            }
            Copy_into_Reg(buffer, window.GetCursor(), match_pos);
            window.SetCursor(buffer, window.GetCursor(), false);
            return !(reg2_.empty());
        }
        return false;
    }

    void Register::Execute_paste(Buffer &buffer, Window &window, EditorAction editoraction)
    {
        size_t count = {1};
        if (editoraction.count_.has_value())
            count = *editoraction.count_;
        if (reg_kind == RegKind::charReg)
        {
            if (editoraction.motion_ == Motion::Left)
                paste_outo_buffer(buffer, window, window.GetCursor(), count);
            else if (editoraction.motion_ == Motion::Right)
                paste_outo_buffer(buffer, window, {window.GetCursor().row_, window.GetCursor().column_ + 1}, count);
        }
        if (reg_kind == RegKind::lineReg)
        {
            if (editoraction.motion_ == Motion::Left)
            {
                paste_outo_buffer(buffer, window, window.GetCursor(), count);
                window.ApplyMotion(buffer, Motion::Goto_Front);
            }
            else if (editoraction.motion_ == Motion::Right)
            {
                paste_outo_buffer(buffer, window, {window.GetCursor().row_ + 1, window.GetCursor().column_}, count);
                window.SetCursor(buffer, {window.GetCursor().row_ + 1, window.GetCursor().column_}, false);
                window.ApplyMotion(buffer, Motion::Goto_Front);
            }
        }
        window.SetCursor(buffer, window.GetCursor(), false);
    }

    void Register::Record_key(KeyEvent key)
    {
        reg1_.push_back(key);
    }

    KeyEvent Register::Get_key()
    {
        reg1_ptr++;
        return reg1_[reg1_ptr - 1];
    }

    bool Register::Finished()
    {
        return reg1_ptr >= reg1_.size();
    }

    void Register::Reset_ptr()
    {
        reg1_ptr = 0;
    }

    void Register::ClearRecord()
    {
        reg1_.clear();
        reg1_ptr = 0;
    }

    void Register::Remove_back()
    {
        if (reg1_.size() > 0 && reg1_[reg1_.size() - 1].code_ == KeyCode::Character && reg1_[reg1_.size() - 1].value_ == 'q')
        {
            reg1_.pop_back();
        }
    }

    void Register::addRegister(Register &&register1)
    {
        if (register1.reg_kind == RegKind::MotionReg)
        {
            reg_kind = RegKind::MotionReg;
            reg1_ = std::move(register1.reg1_);
            reg1_ptr = 0;
        }
        else
        {
            reg_kind = register1.reg_kind;
            reg2_ = std::move(register1.reg2_);
        }
    }

} // namespace sjtu
