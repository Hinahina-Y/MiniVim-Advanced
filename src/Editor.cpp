#include "Editor.hpp"
#include "TextLayout.hpp"

#include <algorithm>
#include <cctype>
#include <exception>
#include <time.h>

// 以下提示均可用于Basic部分的实现,部分函数在Advanced部分中需要修改
// 在该文件中,有些函数我们完整保留了正确的实现,有些函数去掉了一些,其余的完全需要你自己填写
// 我选择按row,column都0-first,对需要部分(已知只有Terminal)我会加1.
namespace sjtu
{

    namespace
    {
        // 匿名namespace,提供了只给当前文件使用的辅助函数

        std::string Trim(std::string value)
        {
            // 去掉字符串两端的空白,保留中间的内容;全部是空白时返回空字符串
            // 你可以分别从两端找到第一个非空白字符,注意反向迭代器转回正向迭代器时的边界
            int l = 0, r = value.size() - 1;
            while (l <= r && std::isspace(value[l]))
            {
                l++;
            }
            while (r >= l && std::isspace(value[r]))
            {
                r--;
            }
            if (r < l)
            {
                return "";
            }
            return value.substr(l, r - l + 1);
        }

        // 判断是否为ASCII可打印字符,Tab由插入模式另外处理
        bool IsPrintable(char value) { return value >= 0x20U && value < 0x7FU; }

    } // namespace

    // 用path初始化文件缓冲区,Terminal构造时会准备好终端输入环境
    Editor::Editor(const std::filesystem::path &path) : buffer_(path), terminal_() {};

    void Editor::Run()
    {
        // 每轮先刷新画面,再读取并处理一个按键;退出循环后清屏
        while (running_)
        {
            RefreshScreen();
            ProcessKey(terminal_.ReadKey());
        }
        terminal_.ClearScreen();
    }

    // 返回编辑器是否还需要继续运行
    bool Editor::IsRunning() const noexcept { return false; }

    void Editor::RefreshScreen()
    {
        // 1. 获取终端大小(GetScreenSize),更新窗口可显示的范围,并让光标落在可见区域内
        // 2. 把当前模式、命令和提示打包成RenderState
        // 3. 让Renderer生成一帧字符串,再交给Terminal输出
        // 这里是你唯一需要调用Terminal中的接口的地方,请调用WriteOutput
        window_.Resize(terminal_.GetScreenSize());
        window_.EnsureCursorVisible(buffer_);
        terminal_.WriteOutput(renderer_.Render(buffer_, window_, RenderState{mode_, command_, message_, command_cursor}));
    }

    void Editor::Largely_Reset()
    {
        window_.Renew_Lastparser();
        window_.RenewCount(std::nullopt);
        normal_parser_.Reset_editoraction(editoraction_);
    }

    void Editor::ProcessKey(KeyEvent key)
    {
        // 1. (可选)先处理所有模式都能使用的Ctrl-Q,用于紧急退出
        // 2. 按当前模式分发给命令行或插入模式的处理函数 (完成)
        // 3. Normal模式下清除旧提示,将按键交给parser,再执行生成的Action
        if (key.IsControl('Q'))
        {
            running_ = false;
            return;
        }
        if (undo_list.empty())
        {
            DealUndoList();
        }
        switch (mode_)
        {
        case Mode::Normal:
            message_ = "";
            if (Need_Arguments_) // 如果现在输入的字符是参数而不是操作
            {
                Execute_withArgument(key);
                break;
            }
            if (key.code_ == KeyCode::Character)
            {
                window_.Renew_Lastparser(key.value_);
            }
            Execute(normal_parser_.Feed(key));
            break;
        case Mode::Insert:
            HandleInsert(key);
            break;
        case Mode::CommandLine:
            HandleCommandLine(key);
            break;
        }
    }

    void Editor::Execute(const EditorAction &action)
    {
        try // 检查是否有组合按键的操作
        {
            normal_parser_.Merge_editoraction(editoraction_, action);
        }
        catch (const std::runtime_error &e)
        {
            normal_parser_.Reset_editoraction(editoraction_);
            message_ = e.what();
            Largely_Reset();
            return;
        }
        // 先处理Del删除指令(其他指令都是处理，这个是撤回)
        if (normal_parser_.EditorAction_Equal_Key(action, {KeyCode::Delete, ' '}) && window_.GetLastparser().has_value())
        {
            std::string s_line = *window_.GetLastparser();
            window_.RenewCount(editoraction_.count_);
            window_.Renew_Lastparser();
            normal_parser_.Reset_editoraction(editoraction_);
            for (size_t index = 0; index + 1 < s_line.size(); index++) // 去除最后一个字符，重新读取
            {
                window_.Renew_Lastparser(s_line[index]);
                normal_parser_.Merge_editoraction(editoraction_, normal_parser_.Feed({KeyCode::Character, s_line[index]}));
                window_.RenewCount(editoraction_.count_);
            }
            return;
        }

        if (editoraction_.kind_ == ActionKind::Undefined)
        {
            Largely_Reset();
            return;
        }
        window_.RenewCount(editoraction_.count_); // 更新含[Count]的组合按键Count值
        size_t count{1}, index{1};
        bool Modified = false;
        std::string s_line;
        Position now_cursor = window_.GetCursor();
        count = 1;
        if (editoraction_.count_.has_value())
            count = *editoraction_.count_;

        // 根据Action的种类调用对应模块
        try
        {
            switch (editoraction_.kind_)
            {
            case ActionKind::None:
                return;
            case ActionKind::Move:
                // 交给Window吧
                if (!editoraction_.type_.has_value()) // 防止一些意外情况
                {
                    Largely_Reset();
                    message_ = "ERROR:UNKNOWN-Move (with no type).";
                    return;
                }
                switch (*editoraction_.type_)
                {
                case ActionType::Ordinary:
                    window_.ApplyMotion(buffer_, *editoraction_.motion_);
                    break;
                case ActionType::word:
                case ActionType::WORD:
                    window_.ApplyMoveword(buffer_, *editoraction_.motion_, *editoraction_.type_ == ActionType::WORD);
                    break;
                case ActionType::Match:
                    Position match_pos = window_.Parenthesis_Matching(buffer_);
                    if (match_pos.row_ < buffer_.GetLineCount())
                        window_.SetCursor(buffer_, match_pos, false);
                    else
                        message_ = "Error:Patterns not found";
                    break;
                }
                window_.SetCursor(buffer_, window_.GetCursor(), false);
                Largely_Reset();
                return;
            case ActionKind::Undo: // 处理撤回
            case ActionKind::Redo: // 处理回退
                if (editoraction_.type_.has_value())
                {
                    switch (*editoraction_.type_)
                    {
                    case ActionType::Ordinary:
                        Handle_UndoAndRedo(editoraction_.kind_ == ActionKind::Redo);
                        break;
                    case ActionType::Line:
                        if (Can_LineUndo)
                            break;
                    }
                }
                return;
            case ActionKind::Replace:
                Need_Arguments_ = true; // 我们需要参数，所以这次处理不掉
                return;
            case ActionKind::Delete:
                if (editoraction_.motion_.has_value() && editoraction_.type_.has_value()) // 确定三个参数，1.向哪个方向删，2.以什么规则删 3.删多少（缺省为1)
                {
                    window_.ApplyDelete(buffer_, *editoraction_.motion_, editoraction_.type_); // 真正的删除操作
                    window_.SetCursor(buffer_, window_.GetCursor(), editoraction_.into_mode_ == Mode::Insert);
                    window_.EnsureCursorVisible(buffer_);
                    DealUndoList(now_cursor);
                    if (editoraction_.into_mode_ == Mode::Insert) // change
                        EnterInsert(window_.GetCursor());
                    Largely_Reset();
                }
                return;
            case ActionKind::Jump_To: // 确定3个参数 1.往什么方向跳跃 2.跳多少(缺省为1) 3.跳跃规则
                if (editoraction_.motion_.has_value() && editoraction_.type_.has_value())
                {
                    window_.ApplyJumpto(buffer_, *editoraction_.motion_, *editoraction_.type_);
                    window_.SetCursor(buffer_, window_.GetCursor(), false); // 保险手段，虽然理论上在ApplyJumpto里已经设置好了光标
                    window_.EnsureCursorVisible(buffer_);
                    Largely_Reset();
                }
                return;
            case ActionKind::Indent:
            case ActionKind::Unindent:
                if (editoraction_.motion_.has_value() && editoraction_.type_.has_value())
                {
                    window_.ApplyIndent(buffer_, *editoraction_.motion_, *editoraction_.type_, editoraction_.kind_ == ActionKind::Indent);
                    window_.SetCursor(buffer_, window_.GetCursor(), false);
                    window_.EnsureCursorVisible(buffer_);
                    DealUndoList(now_cursor);
                    Largely_Reset();
                }
                return;
            case ActionKind::Mark:
                Need_Arguments_ = true; // 我们需要参数，所以这次处理不掉
                return;
            case ActionKind::MergeLine: // 处理J
                for (index = 1; index <= count; index++)
                {
                    if (window_.GetCursor().row_ + 1 < buffer_.GetLineCount())
                    {
                        s_line = buffer_.GetLineAt(window_.GetCursor().row_);
                        if (s_line.size() > 0 && s_line[s_line.size() - 1] != ' ')
                        {
                            buffer_.InsertCharacter(window_.GetCursor().row_, s_line.size(), ' ');
                            s_line = buffer_.GetLineAt(window_.GetCursor().row_ + 1);
                            index = 0;
                            while (index < s_line.size() && (s_line[index] == ' ' || s_line[index] == '\t'))
                            {
                                index++;
                                buffer_.EraseCharacter(window_.GetCursor().row_ + 1, index);
                            }
                            buffer_.JoinLine(window_.GetCursor().row_);
                            window_.SetCursor(buffer_, window_.GetCursor(), false);
                        }
                        Modified = true;
                    }
                    else
                        break;
                }
                Largely_Reset();
                if (Modified)
                    DealUndoList(now_cursor);
                return;
            case ActionKind::InsertBefore:
                // 进入InsertMode,Editor自己就有对应方法
                Insert_Keys_.clear();
                if (editoraction_.motion_.has_value())
                {
                    switch (*editoraction_.motion_)
                    {
                    case Motion::Up:
                        buffer_.SplitLine(now_cursor.row_, 0);
                        window_.SetCursor(buffer_, {now_cursor.row_, 0}, true);
                        break;
                    case Motion::Goto_First_Noblank:
                        window_.ApplyMotion(buffer_, *editoraction_.motion_);
                        break;
                    default:
                        break;
                    }
                }
                EnterInsert(window_.GetCursor());
                return;
            case ActionKind::InsertAfter:
                // 从当前Char之后进入InsertMode
                // 特判:如果Buffer表示当前Cursor所在的行是空的怎么办?
                Insert_Keys_.clear();
                if (editoraction_.motion_.has_value())
                {
                    switch (*editoraction_.motion_)
                    {
                    case Motion::Down:
                        buffer_.SplitLine(now_cursor.row_ + 1, 0);
                        window_.SetCursor(buffer_, {now_cursor.row_ + 1, 0}, true);
                        Insert_Keys_.push_back({KeyCode::Enter, '\r'});
                        break;
                    case Motion::Goto_Back:
                        window_.ApplyMotion(buffer_, *editoraction_.motion_);
                        break;
                    case Motion::Right:
                        window_.ApplyMotion(buffer_, Motion::Right);
                    default:
                        break;
                    }
                }
                window_.SetCursor(buffer_, window_.GetCursor(), true);
                EnterInsert(window_.GetCursor());
                return;
            case ActionKind::EnterCommandLine:
                // 进入Command Mode
                // 记得清空当前的message之类的遗留状态
                message_ = "";
                command_ = "";
                commandline_mode_ = ":";
                mode_ = Mode::CommandLine;
                normal_parser_.Reset_editoraction(editoraction_);
                return;
            }
        }
        catch (const std::runtime_error &e)
        {
            Largely_Reset();
            message_ = e.what();
            return;
        }
        Largely_Reset();
    }

    void Editor::Execute_withArgument(KeyEvent key)
    {
        Position now_cursor = window_.GetCursor();
        if (key.code_ == KeyCode::Delete || key.code_ == KeyCode::Backspace)
        {
            window_.Renew_Lastparser();
            Need_Arguments_ = false;
            editoraction_.kind_ = ActionKind::None;
            return;
        }
        switch (editoraction_.kind_)
        {
        case ActionKind::Replace:
        {
            size_t count{1};
            if (editoraction_.count_.has_value())
                count = *editoraction_.count_;
            Position cursor_pos = window_.GetCursor();
            size_t line_length = buffer_.GetLineAt(cursor_pos.row_).size();
            if (key.code_ == KeyCode::Escape)
            {
                Need_Arguments_ = false;
                normal_parser_.Reset_editoraction(editoraction_);
                window_.Renew_Lastparser();
                window_.RenewCount(std::nullopt);
                DealUndoList(now_cursor);
                return;
            }
            if (cursor_pos.column_ + count > line_length)
            {
                message_ = "WRONG:No Enough characters.";
            }
            else if (key.code_ == KeyCode::Character)
            {
                size_t i = 0;
                while (i < count)
                {
                    buffer_.ReplaceCharacter(cursor_pos.row_, cursor_pos.column_ + i, key.value_);
                    i++;
                }
                if (count > 1)
                {
                    window_.RenewCount(count - 1);
                    window_.ApplyMotion(buffer_, Motion::Right);
                    window_.SetCursor(buffer_, window_.GetCursor(), false);
                    window_.EnsureCursorVisible(buffer_);
                }
            }
            else if (key.code_ == KeyCode::Enter)
            {
                size_t i = 0;
                while (i < count)
                {
                    buffer_.SplitLine(cursor_pos.row_, cursor_pos.column_ + 1);
                    buffer_.EraseCharacter(cursor_pos.row_, cursor_pos.column_ + 1);
                    window_.ApplyMotion(buffer_, Motion::Go_Down_Line);
                    cursor_pos = window_.GetCursor();
                    i++;
                }
                window_.ApplyMotion(buffer_, Motion::Go_Up_Line);
                window_.SetCursor(buffer_, window_.GetCursor(), false);
                window_.EnsureCursorVisible(buffer_);
            }
            Need_Arguments_ = false;
            Largely_Reset();
            DealUndoList(now_cursor);
            return;
        }
        case ActionKind::Mark:
        {
            Need_Arguments_ = false;
            if (key.code_ != KeyCode::Character)
            {
                message_ = "Error:Wrong mark!";
                Largely_Reset();
                return;
            }
            if (!editoraction_.type_.has_value())
            {
                buffer_.mark_.NewMark(key.value_, window_.GetCursor());
                Largely_Reset();
                return;
            }
            try
            {
                switch (*editoraction_.type_)
                {
                case ActionType::To:
                    window_.SetCursor(buffer_, buffer_.mark_.GetMarkPosition(key.value_), false);
                    break;
                }
            }
            catch (const std::runtime_error &e)
            {
                message_ = e.what();
            }
            Largely_Reset();
        }
        }
    }

    // Insert模式下Editor对于KeyEvent的处理.
    void Editor::HandleInsert(KeyEvent key)
    {
        // 在该函数中你需要同时照顾Buffer和Window的状态
        // 1. 若是Escape退出插入模式
        // 2. Enter在光标处分行,光标移动到新行开头
        // 3. Backspace删除前一个字符;若在行首且不是第一行,则与上一行合并
        // 4. 可打印字符和Tab插入当前位置,光标向后移动一列
        // 修改内容后记得同步Window中的光标,插入模式允许光标位于line.size()
        Position cursor_position = window_.GetCursor();
        switch (key.code_)
        {
        case KeyCode::Escape:
            LeaveInsert();
            break;
        case KeyCode::Enter:
            buffer_.SplitLine(cursor_position.row_, cursor_position.column_);
            window_.ApplyMotion(buffer_, Motion::Go_Down_Line);
            if (editoraction_.count_.has_value())
                Insert_Keys_.push_back(key);
            // window_.SetCursor(buffer_, window_.GetCursor(), true);
            break;
        case KeyCode::Backspace:
            if (cursor_position.column_ == 0)
            {
                if (cursor_position.row_ != 0)
                {
                    window_.ApplyMotion(buffer_, Motion::Go_Up_Line);
                    buffer_.JoinLine(window_.GetCursor().row_);
                }
            }
            else
            {
                buffer_.EraseCharacter(cursor_position.row_, cursor_position.column_);
                window_.ApplyMotion(buffer_, Motion::Left);
            }
            // window_.SetCursor(buffer_, window_.GetCursor(), true);
            if (editoraction_.count_.has_value())
            {
                if (Insert_Keys_.size() > 0 && Insert_Keys_[Insert_Keys_.size() - 1].code_ != KeyCode::Backspace)
                    Insert_Keys_.pop_back();
                else
                    Insert_Keys_.push_back(key);
            }
            break;
        case KeyCode::Delete:
            if (cursor_position.column_ + 1 <= buffer_.GetLineAt(cursor_position.row_).size())
            {
                buffer_.EraseCharacter(cursor_position.row_, cursor_position.column_ + 1);
            }
            else if (cursor_position.column_ == buffer_.GetLineAt(cursor_position.row_).size() && cursor_position.row_ + 1 < buffer_.GetLineCount())
            {
                buffer_.JoinLine(cursor_position.row_);
            }
            if (editoraction_.count_.has_value())
                Insert_Keys_.push_back(key);
            break;
        case KeyCode::Character:
            buffer_.InsertCharacter(cursor_position.row_, cursor_position.column_, key.value_);
            window_.ApplyMotion(buffer_, Motion::Right);
            if (editoraction_.count_.has_value())
                Insert_Keys_.push_back(key);
            // window_.SetCursor(buffer_, window_.GetCursor(), true);
            break;
        case KeyCode::ArrowLeft:
            window_.ApplyMotion(buffer_, Motion::Left);
            normal_parser_.Reset_editoraction(editoraction_); // 移动了，就不管count了
            break;
        case KeyCode::ArrowRight:
            window_.ApplyMotion(buffer_, Motion::Right);
            normal_parser_.Reset_editoraction(editoraction_);
            break;
        case KeyCode::ArrowUp:
            window_.ApplyMotion(buffer_, Motion::Up);
            normal_parser_.Reset_editoraction(editoraction_);
            break;
        case KeyCode::ArrowDown:
            window_.ApplyMotion(buffer_, Motion::Down);
            normal_parser_.Reset_editoraction(editoraction_);
            break;
        }
        return;
        // throw std::runtime_error("Not implemented.");
    }
    void Editor::EnterInsert(Position position)
    {
        // 切换到Insert模式,设置插入位置并清除旧提示;允许光标停在行尾字符之后(清除旧提示还未完成)
        message_ = "";
        command_ = "";
        window_.RenewCount(std::nullopt);
        window_.Renew_Lastparser();
        window_.SetCursor(buffer_, window_.GetCursor(), true);
        mode_ = Mode::Insert;
        last_cursor_ = window_.GetCursor();
        return;
    }

    void Editor::LeaveInsert()
    {
        // 从插入位置回到Normal模式的字符位置:不在行首时先左移一列,再限制光标范围
        if (editoraction_.count_.has_value() && *editoraction_.count_ > 1 && Insert_Keys_.size() > 0)
        {
            if (editoraction_.kind_ == ActionKind::InsertBefore && editoraction_.motion_ == Motion::Up)
            {
                Insert_Keys_.push_back({KeyCode::Enter, '\r'});
            }
            size_t count = 1, expect_count = *editoraction_.count_;
            normal_parser_.Reset_editoraction(editoraction_);
            while (count < expect_count)
            {
                for (size_t index = 0; index < Insert_Keys_.size(); index++)
                {
                    HandleInsert(Insert_Keys_[index]);
                }
                count++;
            }
        }
        Insert_Keys_.clear();
        window_.ApplyMotion(buffer_, Motion::Left);
        window_.SetCursor(buffer_, window_.GetCursor(), false);
        DealUndoList(last_cursor_);
        mode_ = Mode::Normal;
        Largely_Reset();
        return;
    }

    void Editor::HandleCommandLine(KeyEvent key) // 主要完成输入
    {
        // 命令内容保存在command_中,不修改Buffer
        // Escape取消命令,Enter执行命令,Backspace/Delete删除末尾字符,可打印字符追加到末尾
        // 注意空命令不能再删除字符
        switch (key.code_)
        {
        case KeyCode::Escape:
            LeaveCommandLine();
            return;
        case KeyCode::Enter:
            ExecuteCommandLine();
            LeaveCommandLine();
            return;
        case KeyCode::Backspace:
        case KeyCode::Delete:
            if (command_cursor != 0)
            {
                command_.erase(command_.begin() + command_cursor - 1);
                command_cursor -= 1;
            }
            else if (command_.empty())
            {
                LeaveCommandLine();
                return;
            }
            break;
        case KeyCode::Character:
            command_.insert(command_.begin() + command_cursor, key.value_);
            command_cursor++;
            break;
        case KeyCode::ArrowLeft:
            if (command_cursor != 0)
            {
                command_cursor--;
            }
            break;
        case KeyCode::ArrowRight:
            if (command_cursor != command_.size())
            {
                command_cursor++;
            }
            break;
        }
    }

    void Editor::ExecuteCommandLine()
    {
        // 1. 保存去掉首尾空白后的命令(用trim),再退出命令行模式,因为退出会清空command_
        // 2. 空命令直接返回,否则按第一个空格或Tab拆成命令名和参数
        // 3. 在Basic部分中你会发现最后命令就一个命令名,直接根据要求的命令名执行
        // 4. 无法识别的命令写入message_,供下一次刷新显示
        std::string trim_command = Trim(command_);
        if (trim_command.empty())
        {
            return;
        }
        size_t index_firstspace = 0;
        while (index_firstspace < trim_command.size() && trim_command[index_firstspace] != ' ' && trim_command[index_firstspace] != ' ')
        {
            index_firstspace++;
        }
        std::string argument = ""; // 参数
        bool force_target = false; // 记录感叹号
        if (index_firstspace < trim_command.size())
        {
            argument = trim_command.substr(index_firstspace);
            argument = Trim(argument);
            trim_command = trim_command.substr(0, index_firstspace);
        }
        if (trim_command[trim_command.size() - 1] == '!')
        {
            force_target = true;
            trim_command = trim_command.substr(0, trim_command.size() - 1);
        }
        if (trim_command == "q" || trim_command == "quit") // 处理quit
        {
            if (!force_target && buffer_.IsModified())
            {
                message_ = "WARNING:No save since last change.";
                return;
            }
            running_ = false;
            return;
        }
        else if (trim_command == "w" || trim_command == "wq") // 处理write
        {
            if (SaveBuffer((std::filesystem::path)argument))
            {
                if (trim_command == "wq")
                {
                    running_ = false;
                    return;
                }
            }
            else if (trim_command == "wq" && force_target)
            {
                running_ = false;
                return;
            }
        }
        else if (trim_command == "e" || trim_command == "edit")
        {
            if (!force_target && buffer_.IsModified())
            {
                message_ = "WARNING:No save since last change.";
                return;
            }
            buffer_ = (Buffer)((std::filesystem::path)argument);
        }
        else
        {
            message_ = "Error:Wrong Command!";
            return;
        }
        return;
    }

    void Editor::LeaveCommandLine()
    {
        // 恢复Normal模式并清空正在输入的命令
        command_ = "";
        command_cursor = 0;
        mode_ = Mode::Normal;
    }

    bool Editor::SaveBuffer(const std::filesystem::path &path)
    {
        // 1. path为空时调用Save,否则调用SaveAs
        // 2. 捕获保存时的异常,把错误写入message_并返回false
        // 3. 成功后生成包含文件名和行数的提示,返回true,供wq判断是否可以退出
        if (!buffer_.IsModified() && !(buffer_.GetLineCount() == 1 && buffer_.GetLineAt(0) == ""))
        {
            message_ = "No thing renewed.";
            return true;
        }
        try
        {
            if (path.empty())
            {
                buffer_.Save();
            }
            else
            {
                buffer_.SaveAs(path);
            }
        }
        catch (const std::runtime_error &e)
        {
            message_ = e.what();
            return false;
        }
        size_t total_lines = buffer_.GetLineCount();
        if (buffer_.GetLineAt(total_lines - 1) == "")
        {
            total_lines--;
        }
        message_ = "File Name:" + buffer_.GetDisplayName() + " total lines:" + std::to_string(total_lines);
        return true;
    }

} // namespace sjtu
