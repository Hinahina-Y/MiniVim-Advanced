#include "Window.hpp"
#include "TextLayout.hpp"

#include <algorithm>
#include <cctype>

namespace sjtu
{

    namespace // 一些辅助函数
    {
        int WordJudge(char c, bool rule_type) // 0word 1WORD
        {
            if (c == ' ' || c == '\r' || c == '\n' || c == '\t')
            {
                return 0;
            }
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_')
            {
                return 1;
            }
            if (rule_type)
            {
                return 1;
            }
            return 2;
        }
    }

    void Window::Resize(ScreenSize terminal_size)
    {
        // 底部留一行给命令或提示,其余作为正文区域;正文行数和列数都至少取1
        // 修改视口即可
        viewport_.columns_ = std::max(terminal_size.columns_, (size_t)1);
        viewport_.rows_ = std::max(terminal_size.rows_ - 1, (size_t)1);
    }

    void Window::ApplyMotion(const Buffer &buffer, Motion motion)
    {
        // 1. 根据方向调用对应的移动函数,Basic中每次移动一步,在Advanced中你可以改变count/添加别的case
        // 2. 将行列限制在Normal模式的合法范围内(Buffer应始终至少有一行)
        // 3. 调整视口,让移动后的光标可见(EnsureCursorVisible)
        size_t count = 1;
        if (remaining_count_.has_value())
        {
            count = *remaining_count_;
        }
        switch (motion)
        {
        case Motion::Left:
            MoveLeft(buffer, count);
            break;
        case Motion::Right:
            MoveRight(buffer, count);
            break;
        case Motion::Down:
            MoveDown(buffer, count);
            break;
        case Motion::Up:
            MoveUp(buffer, count);
            break;
        case Motion::Goto_Front:
            MoveLeft(buffer, cursor_.column_);
            break;
        case Motion::Goto_Back:
            MoveRight(buffer, buffer.GetLineAt(cursor_.row_).size());
            count -= 1;
            while (count--)
            {
                MoveDown(buffer, 1);
                MoveRight(buffer, buffer.GetLineAt(cursor_.row_).size());
            }
            break;
        case Motion::Go_Down_Line: // 这项我们要求只在光标在行末调用
            if (cursor_.row_ + 1 != buffer.GetLineCount())
            {
                MoveLeft(buffer, cursor_.column_);
                MoveDown(buffer, 1);
            }
            break;
        case Motion::Go_Up_Line: // 这项我们要求只在光标在行首调用
            if (cursor_.row_ != 0)
            {
                MoveUp(buffer, 1);
                MoveRight(buffer, buffer.GetLineAt(cursor_.row_).size());
            }
            break;
        case Motion::Goto_First_Noblank:
            MoveLeft(buffer, cursor_.column_);
            while (cursor_.column_ < buffer.GetLineAt(cursor_.row_).size() && (buffer.GetLineAt(cursor_.row_)[cursor_.column_] == ' ' || buffer.GetLineAt(cursor_.row_)[cursor_.column_] == '\t'))
            {
                cursor_.column_++;
            }
            break;
        case Motion::Left_allow_crossline:
            while (count--)
            {
                if (cursor_.column_ == 0)
                {
                    if (cursor_.row_ != 0)
                    {
                        MoveUp(buffer, 1);
                        MoveRight(buffer, buffer.GetLineAt(cursor_.row_).size());
                    }
                    else
                    {
                        return;
                    }
                }
                else
                {
                    MoveLeft(buffer, 1);
                }
            }
            break;
        case Motion::Right_allow_crossline:
            while (count--)
            {
                if (cursor_.column_ + 1 == buffer.GetLineAt(cursor_.row_).size())
                {
                    if (cursor_.row_ + 1 != buffer.GetLineCount())
                    {
                        MoveDown(buffer, 1);
                        MoveLeft(buffer, cursor_.column_);
                    }
                    else
                    {
                        return;
                    }
                }
                else
                {
                    MoveRight(buffer, 1);
                }
            }
            break;
        }

        EnsureCursorVisible(buffer);
    }

    void Window::ApplyMoveword(const Buffer &buffer, Motion motion, bool rule_type)
    {
        size_t count{1};
        std::string line;
        if (remaining_count_.has_value())
        {
            count = *remaining_count_;
            remaining_count_ = std::nullopt;
        }
        size_t IsOneWord = 0, Meet_Boundary = false;
        if (motion == Motion::Down)
        {
            ApplyMotion(buffer, Motion::Right_allow_crossline);
        }
        if (motion == Motion::Up)
        {
            ApplyMotion(buffer, Motion::Left_allow_crossline);
        }
        line = buffer.GetLineAt(cursor_.row_);
        line += " ";
        IsOneWord = WordJudge(line[cursor_.column_], rule_type);
        while (count > 0 && !Meet_Boundary)
        {
            if (motion == Motion::Right || motion == Motion::Down)
            {
                cursor_.column_++;
                if (cursor_.column_ == line.size())
                {
                    if (cursor_.row_ + 1 == buffer.GetLineCount())
                    {
                        Meet_Boundary = true;
                        break;
                    }
                    cursor_.column_ = 0;
                    cursor_.row_++;
                    line = buffer.GetLineAt(cursor_.row_);
                    line += " ";
                }
            }
            else if (motion == Motion::Left || motion == Motion::Up)
            {
                if (cursor_.row_ == 0 && cursor_.column_ == 0)
                {
                    Meet_Boundary = true;
                    break;
                }
                if (cursor_.column_ == 0)
                {
                    cursor_.row_--;
                    line = buffer.GetLineAt(cursor_.row_);
                    line += " ";
                    cursor_.column_ = line.size();
                }
                cursor_.column_--;
            }
            if (WordJudge(line[cursor_.column_], rule_type) != IsOneWord && (((IsOneWord != 0) && (motion == Motion::Down || motion == Motion::Up)) || ((IsOneWord != 1) && !(motion == Motion::Down || motion == Motion::Up))))
            {
                count--;
            }
            else if (WordJudge(line[cursor_.column_], rule_type) == 2 || IsOneWord == 2)
            {
                count--;
            }
            IsOneWord = WordJudge(line[cursor_.column_], rule_type);
        }
        if (!Meet_Boundary && motion == Motion::Down)
        {
            ApplyMotion(buffer, Motion::Left_allow_crossline);
        }
        if (!Meet_Boundary && motion == Motion::Up)
        {
            SetCursor(buffer, cursor_, false);
            ApplyMotion(buffer, Motion::Right_allow_crossline);
        }
        SetCursor(buffer, cursor_, false);
    }

    void Window::ApplyDelete(Buffer &buffer, Motion motion)
    {
        size_t count = 1;
        if (remaining_count_.has_value())
        {
            count = *remaining_count_;
        }
        switch (motion)
        {
        case Motion::Left:
            while (cursor_.column_ > 0 && count > 0)
            {
                buffer.EraseCharacter(cursor_.row_, cursor_.column_);
                cursor_.column_--;
                count--;
            }
            break;
        case Motion::Right:
            while (count > 0 && buffer.GetLineAt(cursor_.row_).size() != 0)
            {
                buffer.EraseCharacter(cursor_.row_, cursor_.column_ + 1);
                SetCursor(buffer, cursor_, false);
                count--;
            }
            break;
        }
    }

    void Window::Apply_d_Delete(Buffer &buffer, Motion motion, std::optional<ActionType> type)
    {
        size_t count{1};
        if (remaining_count_.has_value())
        {
            count = *remaining_count_;
        }
        if (!type.has_value())
        {
            return;
        }
        switch (*type)
        {
        case ActionType::Ordinary:
        {
            switch (motion)
            {
            case Motion::Go_Delete: // dd
                ApplyMotion(buffer, Motion::Goto_Front);
                buffer.EraseLines(cursor_.row_, cursor_.row_ + count);
                break;
            case Motion::Goto_Back: // D
                buffer.EraseLines(cursor_.row_ + 1, cursor_.row_ + count);
                buffer.EraseCharacters(cursor_.row_, cursor_.column_, buffer.GetLineAt(cursor_.row_).size());
                break;
            case Motion::Goto_Front:
                buffer.EraseCharacters(cursor_.row_, 0, cursor_.column_);
                break;
            case Motion::Left:
            case Motion::Right:
                ApplyDelete(buffer, motion);
                break;
            case Motion::Up:
                ApplyMotion(buffer, Motion::Goto_Front);
                buffer.EraseLines(cursor_.row_ - count, cursor_.row_ + 1);
                break;
            case Motion::Down:
                ApplyMotion(buffer, Motion::Goto_Front);
                buffer.EraseLines(cursor_.row_, cursor_.row_ + count + 1);
                break;
            }
            break;
        }
        case ActionType::word:
        {
            Position now_cursor_ = cursor_;
            switch (motion)
            {
            case Motion::Left:
                ApplyMoveword(buffer, motion, false);
                buffer.EraseCharacters(cursor_.row_,cursor_.column_+1,now_cursor_.row_,now_cursor_.column_);
                cursor_.column_++;
                SetCursor(buffer,cursor_,false);
                break;
            case Motion::Right:
                ApplyMoveword(buffer, motion, false);
                buffer.EraseCharacters(now_cursor_.row_,now_cursor_.column_,cursor_.row_,cursor_.column_);
                SetCursor(buffer,now_cursor_,false);
                break;
            case Motion::Up:
                ApplyMoveword(buffer, motion, false);
                buffer.EraseCharacters(cursor_.row_,cursor_.column_+1,now_cursor_.row_,now_cursor_.column_);
                cursor_.column_++;
                SetCursor(buffer,cursor_,false);
                break;
            case Motion::Down:
                ApplyMoveword(buffer, motion, false);
                buffer.EraseCharacters(now_cursor_.row_,now_cursor_.column_,cursor_.row_,cursor_.column_+1);
                SetCursor(buffer,now_cursor_,false);
                break;
            }
            break;
        }
        case ActionType::WORD:
        {
            Position now_cursor_ = cursor_;
            switch (motion)
            {
            case Motion::Left:
                ApplyMoveword(buffer, motion, true);
                buffer.EraseCharacters(cursor_.row_,cursor_.column_+1,now_cursor_.row_,now_cursor_.column_);
                cursor_.column_++;
                SetCursor(buffer,cursor_,false);
                break;
            case Motion::Right:
                ApplyMoveword(buffer, motion, true);
                buffer.EraseCharacters(now_cursor_.row_,now_cursor_.column_,cursor_.row_,cursor_.column_);
                SetCursor(buffer,now_cursor_,false);
                break;
            case Motion::Up:
                ApplyMoveword(buffer, motion, true);
                buffer.EraseCharacters(cursor_.row_,cursor_.column_+1,now_cursor_.row_,now_cursor_.column_);
                cursor_.column_++;
                SetCursor(buffer,cursor_,false);
                break;
            case Motion::Down:
                ApplyMoveword(buffer, motion, true);
                buffer.EraseCharacters(now_cursor_.row_,now_cursor_.column_,cursor_.row_,cursor_.column_+1);
                SetCursor(buffer,now_cursor_,false);
                break;
            }
            break;
        }
        }
    }

    void Window::EnsureCursorVisible(const Buffer &buffer)
    {
        // 1. 光标高于或低于可见区域时,调整top_,使光标刚好进入区域
        // 2. 把光标的字符下标换算成显示列,再用相同思路调整left_
        if (cursor_.row_ < viewport_.top_)
        {
            viewport_.top_ = cursor_.row_;
        }
        else if (cursor_.row_ >= viewport_.top_ + viewport_.rows_)
        {
            viewport_.top_ = cursor_.row_ - viewport_.rows_ + 1;
        }
        size_t displayed_column = BufferColumnToRenderColumn(buffer.GetLineAt(cursor_.row_), cursor_.column_);
        if (displayed_column < viewport_.left_)
        {
            viewport_.left_ = displayed_column;
        }
        else if (displayed_column >= viewport_.left_ + viewport_.columns_)
        {
            viewport_.left_ = displayed_column - viewport_.columns_ + 1;
        }
    }

    void Window::RenewCount(std::optional<size_t> expected_count)
    {
        remaining_count_ = expected_count;
    }

    void Window::Renew_Lastparser(char lastparser)
    {
        if (!Last_parser_.has_value())
        {
            Last_parser_ = "";
        }
        Last_parser_ = *Last_parser_ + std::string(1, lastparser);
    }

    void Window::Renew_Lastparser()
    {
        Last_parser_.reset();
    }

    std::string Window::GetBottomValue() const
    {
        std::string bottom_value = "";
        if (Last_parser_.has_value())
        {
            bottom_value = *Last_parser_;
        }
        return bottom_value;
    }

    std::optional<std::string> Window::GetLastparser() const
    {
        return Last_parser_;
    }

    const Position &Window::GetCursor() const
    {
        // 返回当前光标位置的只读引用
        return cursor_;
    }

    const Viewport &Window::GetViewport() const
    {
        // 返回当前可见区域的只读引用,供Renderer绘制
        return viewport_;
    }

    void Window::SetCursor(const Buffer &buffer, Position position, bool allow_line_end)
    {
        // 1. 先限制行号,再根据该行长度和allow_line_end限制列号
        // 2. 用新位置更新上下移动时的目标显示列
        // 3. 调整视口,保证光标可见
        if (position.row_ >= buffer.GetLineCount())
        {
            position.row_ = buffer.GetLineCount() - 1;
        }
        std::string line = buffer.GetLineAt(position.row_);
        if (position.column_ > line.size())
        {
            position.column_ = line.size();
        }
        if (line.size() == position.column_ && !allow_line_end && position.column_ != 0)
        {
            position.column_ -= 1;
        }
        cursor_ = position;
        EnsureCursorVisible(buffer);
    }

    void Window::MoveLeft(const Buffer &buffer, std::size_t count)
    {
        // 向左移动count个字符,最多到行首,并更新目标显示列
        if (count != 0 && cursor_.column_ != 0)
        {
            desired_column_ = 0;
        }
        if (cursor_.column_ >= count)
        {
            cursor_.column_ -= count;
        }
        else
        {
            cursor_.column_ = 0;
        }
        return;
    }

    void Window::MoveRight(const Buffer &buffer, std::size_t count)
    {
        // 向右移动count个字符,最多到最后一个字符,并更新目标显示列
        if (count != 0 && cursor_.column_ != buffer.GetLineAt(cursor_.row_).size())
        {
            desired_column_ = 0;
        }
        if (cursor_.column_ + count <= buffer.GetLineAt(cursor_.row_).size())
        {
            cursor_.column_ += count;
        }
        else
        {
            cursor_.column_ = buffer.GetLineAt(cursor_.row_).size();
        }
        return;
    }

    void Window::MoveUp(const Buffer &buffer, std::size_t count)
    {
        // 先算目标行,最多到第一行,再将期望的显示列换算成目标行的字符下标
        // 经过短行时不要更新desired_screen_column_,这样继续移动到长行时能回到原来的列
        const std::string &line_old = buffer.GetLineAt(cursor_.row_);
        if (cursor_.row_ >= count)
        {
            cursor_.row_ -= count;
        }
        else
        {
            cursor_.row_ = 0;
        }
        const std::string &line_new = buffer.GetLineAt(cursor_.row_);
        size_t Row_Length = line_new.size();
        if (desired_column_ == 0)
        {
            desired_column_ = BufferColumnToRenderColumn(line_old, cursor_.column_);
        }
        cursor_.column_ = std::min(RenderColumnToBufferColumn(line_new, desired_column_), Row_Length);
        return;
    }

    void Window::MoveDown(const Buffer &buffer, std::size_t count)
    {
        // 先算目标行,最多到最后一行,再根据desired_screen_column_寻找目标字符
        // 与向上移动一样,保留期望显示列
        const std::string &line_old = buffer.GetLineAt(cursor_.row_);
        if (cursor_.row_ + count < buffer.GetLineCount())
        {
            cursor_.row_ += count;
        }
        else
        {
            cursor_.row_ = buffer.GetLineCount() - 1;
        }
        const std::string &line_new = buffer.GetLineAt(cursor_.row_);
        size_t Row_Length = line_new.size();
        if (desired_column_ == 0)
        {
            desired_column_ = BufferColumnToRenderColumn(line_old, cursor_.column_);
        }
        cursor_.column_ = std::min(RenderColumnToBufferColumn(line_new, desired_column_), Row_Length);
        return;
    }

} // namespace sjtu
