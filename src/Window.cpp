#include "Window.hpp"
#include "TextLayout.hpp"

#include <algorithm>
#include <cctype>

namespace sjtu
{

    namespace Assist // 一些辅助函数
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
        int Braces(char c)
        {
            switch (c)
            {
            case '(':
                return 1;
            case ')':
                return -1;
            case '[':
                return 2;
            case ']':
                return -2;
            case '{':
                return 3;
            case '}':
                return -3;
            default:
                return 0;
            }
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
        IsOneWord = Assist::WordJudge(line[cursor_.column_], rule_type);
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
            if (Assist::WordJudge(line[cursor_.column_], rule_type) != IsOneWord && (((IsOneWord != 0) && (motion == Motion::Down || motion == Motion::Up)) || ((IsOneWord != 1) && !(motion == Motion::Down || motion == Motion::Up))))
            {
                count--;
            }
            else if (Assist::WordJudge(line[cursor_.column_], rule_type) == 2 || IsOneWord == 2)
            {
                count--;
            }
            IsOneWord = Assist::WordJudge(line[cursor_.column_], rule_type);
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

    void Window::ApplyJumpto(const Buffer &buffer, Motion motion, ActionType type) // 跳转至
    {
        size_t count{1};
        if (remaining_count_.has_value())
            count = *remaining_count_;
        switch (type)
        {
        case ActionType::Ordinary:
        case ActionType::To:
            switch (motion)
            {
            case Motion::Down:
                if (count > 100)
                    count = 100;
                count = count * buffer.GetLineCount() / 100;
                if (count == 0)
                    count++;
                [[fallthrough]];
            case Motion::Goto_First_Noblank:
                SetCursor(buffer, {count - 1, 0}, false);
                ApplyMotion(buffer, Motion::Goto_First_Noblank);
                break;
            }
            break;
        case ActionType::Max:
            switch (motion)
            {
            case Motion::Goto_First_Noblank:
                SetCursor(buffer, {buffer.GetLineCount() - 1, 0}, false);
                ApplyMotion(buffer, Motion::Goto_First_Noblank);
                break;
            }
            break;
        }
    }

    void Window::DeleteCharacters(Buffer &buffer, Motion motion)
    {
        size_t count{1};
        Position previous_cursor_{cursor_};
        if (remaining_count_.has_value())
            count = *remaining_count_;
        switch (motion)
        {
        case Motion::Left:
            ApplyMotion(buffer, motion);
            buffer.EraseCharacters(cursor_.row_, cursor_.column_, previous_cursor_.column_);
            break;
        case Motion::Right:
            ApplyMotion(buffer, motion);
            buffer.EraseCharacters(cursor_.row_, previous_cursor_.column_, cursor_.column_);
            SetCursor(buffer, previous_cursor_, false);
            break;
        case Motion::Left_allow_crossline:
            ApplyMotion(buffer, motion);
            buffer.EraseCharacters(cursor_.row_, cursor_.column_, previous_cursor_.row_, previous_cursor_.column_);
            break;
        }
    }

    void Window::ApplyDelete(Buffer &buffer, Motion motion, std::optional<ActionType> type)
    {
        size_t count{1};
        if (remaining_count_.has_value())
            count = *remaining_count_;
        if (!type.has_value())
            return;

        switch (*type) // 其实各种type的处理逻辑都是一样的，先根据motion移动到对应位置，再删去移动过程途径的字符，不同type和motion只有细微区别，但是时间不足以去把所有逻辑统一了，只能先复制代码重复了。
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
            case Motion::Left_allow_crossline:
                DeleteCharacters(buffer, motion);
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
        case ActionType::WORD:
        {
            Position previous_cursor_{cursor_};
            switch (motion)
            {
            case Motion::Left:
                ApplyMoveword(buffer, motion, *type == ActionType::WORD);
                buffer.EraseCharacters(cursor_.row_, cursor_.column_, previous_cursor_.row_, previous_cursor_.column_);
                cursor_.column_++;
                SetCursor(buffer, cursor_, false);
                break;
            case Motion::Right:
                ApplyMoveword(buffer, motion, *type == ActionType::WORD);
                buffer.EraseCharacters(previous_cursor_.row_, previous_cursor_.column_, cursor_.row_, cursor_.column_);
                SetCursor(buffer, previous_cursor_, false);
                break;
            case Motion::Up:
                ApplyMoveword(buffer, motion, *type == ActionType::WORD);
                buffer.EraseCharacters(cursor_.row_, cursor_.column_ + 1, previous_cursor_.row_, previous_cursor_.column_);
                cursor_.column_++;
                SetCursor(buffer, cursor_, false);
                break;
            case Motion::Down:
                ApplyMoveword(buffer, motion, *type == ActionType::WORD);
                buffer.EraseCharacters(previous_cursor_.row_, previous_cursor_.column_, cursor_.row_, cursor_.column_ + 1);
                SetCursor(buffer, previous_cursor_, false);
                break;
            }
            break;
        }
        case ActionType::To:
        case ActionType::Max: // Delete Max行为未定义，所以直接归结到JumpMAX里，节省分类成本
        {
            Position previous_cursor_{cursor_};
            ApplyJumpto(buffer, motion, *type);
            if (cursor_.row_ >= previous_cursor_.row_)
            {
                buffer.EraseLines(previous_cursor_.row_, cursor_.row_ + 1);
                SetCursor(buffer, {previous_cursor_.row_, 0}, false);
            }
            else
            {
                buffer.EraseLines(cursor_.row_, previous_cursor_.row_ + 1);
                SetCursor(buffer, {cursor_.row_, 0}, false);
            }
            break;
        }
        case ActionType::Delete: // Del
        {
            switch (motion)
            {
            case Motion::Right:
                DeleteCharacters(buffer, motion);
                break;
            }
            break;
        }
        case ActionType::Match:
        {
            Position previous_cursor_{cursor_};
            Position match_pos = Parenthesis_Matching(buffer);
            if (match_pos.row_ >= buffer.GetLineCount())
            {
                throw std::runtime_error("Error:Patterns not found");
                return;
            }
            if (previous_cursor_ < match_pos)
            {
                std::swap(match_pos, previous_cursor_);
            }
            buffer.EraseCharacters(match_pos.row_, match_pos.column_, previous_cursor_.row_, previous_cursor_.column_ + 1);
            SetCursor(buffer, match_pos, false);
            break;
        }
        }
    }

    void Window::ApplyIndent(Buffer &buffer, Motion motion, std::optional<ActionType> type, bool Is_Indent) // 处理缩进> <
    {
        size_t count{1};
        if (remaining_count_.has_value())
            count = *remaining_count_;
        if (!type.has_value())
            return;

        switch (*type)
        {
        case ActionType::Ordinary:
        {
            Position previous_cursor_{cursor_};
            switch (motion)
            {
            case Motion::Goto_Back:
            case Motion::Goto_Front:
            case Motion::Left:
            case Motion::Right:
                if (Is_Indent)
                {
                    buffer.InsertCharacters(cursor_.row_, 0, '\t', 2);
                    MoveRight(buffer, 8);
                }
                else
                {
                    buffer.EraseIndent(cursor_.row_, 8);
                    MoveLeft(buffer, 8);
                }
                break;
            case Motion::Left_allow_crossline:
            case Motion::Up:
                ApplyMotion(buffer, motion);
                desired_column_ = 0; // 处理一下期望列
                if (Is_Indent)
                {
                    for (size_t row = cursor_.row_; row <= previous_cursor_.row_; row++)
                    {
                        buffer.InsertCharacters(row, 0, '\t', 2);
                    }
                    MoveRight(buffer, 8);
                }
                else
                {
                    for (size_t row = cursor_.row_; row <= previous_cursor_.row_; row++)
                    {
                        buffer.EraseIndent(row, 8);
                    }
                    MoveLeft(buffer, 8);
                }
                break;
            case Motion::Down:
                ApplyMotion(buffer, motion);
                desired_column_ = 0; // 处理一下期望列
                if (Is_Indent)
                {
                    for (size_t row = previous_cursor_.row_; row <= cursor_.row_; row++)
                    {
                        buffer.InsertCharacters(row, 0, '\t', 2);
                    }
                    MoveRight(buffer, 8);
                }
                else
                {
                    for (size_t row = previous_cursor_.row_; row <= cursor_.row_; row++)
                    {
                        buffer.EraseIndent(row, 8);
                    }
                    MoveLeft(buffer, 8);
                }
                break;
            case Motion::Go_Indent:
                if (Is_Indent)
                {
                    buffer.InsertCharacters(cursor_.row_, 0, '\t', 2 * count);
                    MoveRight(buffer, 8 * count);
                }
                else
                {
                    buffer.EraseIndent(cursor_.row_, 8 * count);
                    MoveLeft(buffer, 8 * count);
                }
            }
            break;
        }
        case ActionType::word:
        case ActionType::WORD:
        {
            Position previous_cursor_{cursor_};
            switch (motion)
            {
            case Motion::Left:
            case Motion::Up:
                ApplyMoveword(buffer, motion, *type == ActionType::WORD);
                if (Is_Indent)
                {
                    for (size_t row = cursor_.row_; row <= previous_cursor_.row_; row++)
                        buffer.InsertCharacters(row, 0, '\t', 2);
                    MoveRight(buffer, 8);
                }
                else
                {
                    for (size_t row = cursor_.row_; row <= previous_cursor_.row_; row++)
                        buffer.EraseIndent(row, 8);
                    MoveLeft(buffer, 8);
                }
                break;
            case Motion::Right:
            case Motion::Down:
                ApplyMoveword(buffer, motion, *type == ActionType::WORD);
                if (Is_Indent)
                {
                    for (size_t row = previous_cursor_.row_; row <= cursor_.row_; row++)
                        buffer.InsertCharacters(row, 0, '\t', 2);
                    MoveRight(buffer, 8);
                }
                else
                {
                    for (size_t row = previous_cursor_.row_; row <= cursor_.row_; row++)
                        buffer.EraseIndent(row, 8);
                    MoveRight(buffer, 8);
                }
                break;
            }
            break;
        }
        case ActionType::To:
        case ActionType::Max:
        {
            Position previous_cursor_{cursor_};
            ApplyJumpto(buffer, motion, *type);
            if (Is_Indent)
            {
                for (size_t row = std::min(previous_cursor_.row_, cursor_.row_); row <= std::max(previous_cursor_.row_, cursor_.row_); row++)
                    buffer.InsertCharacters(row, 0, '\t', 2);
                MoveRight(buffer, 8);
            }
            else
            {
                for (size_t row = std::min(previous_cursor_.row_, cursor_.row_); row <= std::max(previous_cursor_.row_, cursor_.row_); row++)
                    buffer.EraseIndent(row, 8);
                MoveLeft(buffer, 8);
            }
            break;
        }
        case ActionType::Match:
        {
            Position previous_cursor_{cursor_};
            Position match_pos = Parenthesis_Matching(buffer);
            if (match_pos.row_ >= buffer.GetLineCount())
            {
                throw std::runtime_error("Error:Patterns not found");
                return;
            }
            cursor_ = match_pos;
            if (Is_Indent)
            {
                for (size_t row = std::min(previous_cursor_.row_, cursor_.row_); row <= std::max(previous_cursor_.row_, cursor_.row_); row++)
                    buffer.InsertCharacters(row, 0, '\t', 2);
                MoveRight(buffer, 8);
            }
            else
            {
                for (size_t row = std::min(previous_cursor_.row_, cursor_.row_); row <= std::max(previous_cursor_.row_, cursor_.row_); row++)
                    buffer.EraseIndent(row, 8);
                MoveLeft(buffer, 8);
            }
            break;
        }
        }
    }

    Position Window::Parenthesis_Matching(const Buffer &buffer) const // 从光标所在处开始进行括号匹配,返回匹配的位置，若没有匹配位置，返回最大行,0.
    {
        Window temporary_window;
        temporary_window.SetCursor(buffer, cursor_, true);
        temporary_window.RenewCount(std::nullopt);
        std::string_view line = buffer.GetLineAt(cursor_.row_);
        size_t index = cursor_.column_;
        Motion motion;
        int matching_type{0};
        int matching_sum{0};
        while (index < line.size() && Assist::Braces(line[index]) == 0)
            index++;
        if (index < line.size() && Assist::Braces(line[index]) != 0)
        {
            matching_type = std::abs(Assist::Braces(line[index]));
            matching_sum = Assist::Braces(line[index]) / matching_type;
        }
        else
            return {buffer.GetLineCount(), 0};
        if (matching_sum < 0)
            motion = Motion::Left_allow_crossline;
        else
            motion = Motion::Right_allow_crossline;
        temporary_window.SetCursor(buffer, {cursor_.row_, index}, false);
        while (matching_sum != 0) // 开始匹配
        {
            temporary_window.ApplyMotion(buffer, motion);
            line = buffer.GetLineAt(temporary_window.GetCursor().row_);
            if (temporary_window.cursor_.column_ < line.size() && matching_type == std::abs(Assist::Braces(line[temporary_window.GetCursor().column_])))
            {
                matching_sum += Assist::Braces(line[temporary_window.GetCursor().column_]) / matching_type;
            }
            if ((temporary_window.GetCursor().row_ == 0 && temporary_window.GetCursor().column_ == 0) || (temporary_window.GetCursor().row_ + 1 == buffer.GetLineCount() && (line.size() == 0 || temporary_window.GetCursor().column_ + 1 == line.size())))
            {
                break;
            }
        }
        if (matching_sum != 0)
        {
            return {buffer.GetLineCount(), 0};
        }
        return temporary_window.GetCursor();
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
