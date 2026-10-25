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
            if (playing_name.has_value() && playing_times > 0) // 执行宏
            {
                if (Processmacro())
                    continue;
                DealUndoList(last_cursor_);
            }
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
        terminal_.WriteOutput(renderer_.Render(buffer_, window_, RenderState{mode_, command_, message_, recording_name.has_value() ? "recording @" + std::string(1, *recording_name) : "", command_cursor, commandline_mode_}));
    }

    void Editor::Largely_Reset()
    {
        window_.Renew_Lastparser();
        window_.RenewCount(std::nullopt);
        normal_parser_.Reset_editoraction(editoraction_);
    }

    bool Editor::Processmacro()
    {
        if (playing_register.Finished())
        {
            playing_register.Reset_ptr();
            playing_times--;
            if (playing_times == 0)
            {
                playing_name = std::nullopt;
                return false;
            }
        }
        ProcessKey(playing_register.Get_key());
        return true;
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
        if (recording_name.has_value())
        {
            recording_register_.Record_key(key); // 录制
        }
        switch (mode_)
        {
        case Mode::Normal:
            message_ = "";
            if (key.code_ == KeyCode::Character)
                window_.Renew_Lastparser(key.value_);
            if (Need_Arguments_) // 如果现在输入的字符是参数而不是操作
            {
                Execute_withArgument(key);
                break;
            }
            Execute(normal_parser_.Feed(key));
            break;
        case Mode::Insert:
            HandleInsert(key);
            break;
        case Mode::CommandLine:
            HandleCommandLine(key);
            break;
        case Mode::Visual:
        case Mode::Visual_Line:
            if (key.code_ == KeyCode::Character)
                window_.Renew_Lastparser(key.value_);
            if (key.code_ == KeyCode::Escape)
            {
                mode_ = Mode::Normal;
                break;
            }
            if (Need_Arguments_) // 如果现在输入的字符是参数而不是操作
            {
                Execute_withArgument(key);
                break;
            }
            HandleVisual(key);
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
            case ActionKind::Waiting_Argument:
                Need_Arguments_ = true; // 需要参数
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
                        message_ = "Error:Patterns could not be found";
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
                        Handle_LineUndo();
                    }
                }
                return;
            case ActionKind::Replace:
                Need_Arguments_ = true; // 我们需要参数，所以这次处理不掉
                return;
            case ActionKind::Delete:
                if (editoraction_.motion_.has_value() && editoraction_.type_.has_value()) // 确定三个参数，1.向哪个方向删，2.以什么规则删 3.删多少（缺省为1)
                {
                    Register temporary_register;
                    if (temporary_register.Execute_copy(buffer_, window_, editoraction_))
                        register_[editoraction_.argument_name_].addRegister(std::move(temporary_register)); // 先把要删除的元素拷入寄存器
                    window_.SetCursor(buffer_, now_cursor, false);                                          // 光标复位
                    window_.ApplyDelete(buffer_, *editoraction_.motion_, editoraction_.type_);              // 真正的删除操作
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
            case ActionKind::Copy:
                if (editoraction_.motion_.has_value() && editoraction_.type_.has_value())
                {
                    Register temporary_register;
                    if (temporary_register.Execute_copy(buffer_, window_, editoraction_))
                        register_[editoraction_.argument_name_].addRegister(std::move(temporary_register));
                    window_.EnsureCursorVisible(buffer_);
                    Largely_Reset();
                }
                return;
            case ActionKind::Paste:
                if (editoraction_.motion_.has_value() && editoraction_.type_.has_value())
                {
                    if (register_.find(editoraction_.argument_name_) != register_.end())
                    {
                        register_[editoraction_.argument_name_].Execute_paste(buffer_, window_, editoraction_);
                        window_.EnsureCursorVisible(buffer_);
                        DealUndoList(now_cursor);
                    }
                    else
                    {
                        message_ = "Error:Nothing in register " + std::string(1, editoraction_.argument_name_);
                    }
                    Largely_Reset();
                }
                return;
            case ActionKind::Recording:
                if (recording_name.has_value())
                {
                    recording_register_.Remove_back();
                    register_[*recording_name].addRegister(std::move(recording_register_));
                    recording_name = std::nullopt;
                    Largely_Reset();
                }
                else
                {
                    Need_Arguments_ = true;
                    return;
                }
                break;
            case ActionKind::Play:
                if (editoraction_.argument_name_ == '@')
                {
                    Execute_withArgument({KeyCode::Character, '@'});
                    return;
                }
                else
                {
                    Need_Arguments_ = true;
                    return;
                }
            case ActionKind::Mark:
                Need_Arguments_ = true; // 我们需要参数，所以这次处理不掉
                return;
            case ActionKind::Shift:
                window_.ApplyShift(buffer_, *editoraction_.motion_);
                Largely_Reset();
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
            case ActionKind::ShiftMode:
                message_ = "";
                mode_ = editoraction_.into_mode_;
                if (editoraction_.into_mode_ == Mode::Visual || editoraction_.into_mode_ == Mode::Visual_Line)
                    window_.SetAnchor(window_.GetCursor());
                Largely_Reset();
                return;
            case ActionKind::Search:
                if (editoraction_.into_mode_ == Mode::CommandLine)
                {
                    if (*editoraction_.motion_ == Motion::Left_allow_crossline)
                        commandline_mode_ = "?";
                    if (*editoraction_.motion_ == Motion::Right_allow_crossline)
                        commandline_mode_ = "/";
                    mode_ = Mode::CommandLine;
                    Largely_Reset();
                    return;
                }
                else
                {
                    if (!last_search_motion.has_value())
                    {
                        message_ = "Error:No searching history.";
                        Largely_Reset();
                        return;
                    }
                    if ((*editoraction_.motion_ == Motion::Up) ^ (*last_search_motion == Motion::Left_allow_crossline))
                        window_.ApplySearch(buffer_, Motion::Right_allow_crossline, last_search_string);
                    else
                        window_.ApplySearch(buffer_, Motion::Left_allow_crossline, last_search_string);
                }
                return;
            case ActionKind::Adjust:
                if (editoraction_.type_.has_value() && editoraction_.motion_.has_value())
                {
                    window_.AdjustViewport(buffer_, *editoraction_.motion_, *editoraction_.type_);
                    Largely_Reset();
                    return;
                }
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
            const Position &cursor_pos = window_.GetCursor();

            if (key.code_ == KeyCode::Escape)
            {
                Need_Arguments_ = false;
                Largely_Reset();
                return;
            }
            if (mode_ == Mode::Normal)
            {
                size_t line_length = buffer_.GetLineAt(cursor_pos.row_).size();
                if (cursor_pos.column_ + count > line_length)
                {
                    message_ = "WRONG:No Enough characters.";
                }

                if (key.code_ == KeyCode::Character)
                {
                    window_.ApplyReplace(buffer_, key.value_, cursor_pos, {cursor_pos.row_, cursor_pos.column_ + count});
                    window_.ApplyMotion(buffer_, Motion::Left); // 左移一位
                }
                else if (key.code_ == KeyCode::Enter)
                {
                    size_t i = 0;
                    while (i < count)
                    {
                        buffer_.SplitLine(cursor_pos.row_, cursor_pos.column_ + 1);
                        buffer_.EraseCharacter(cursor_pos.row_, cursor_pos.column_ + 1);
                        window_.ApplyMotion(buffer_, Motion::Go_Down_Line);
                        i++;
                    }
                    window_.ApplyMotion(buffer_, Motion::Go_Up_Line);
                    window_.SetCursor(buffer_, window_.GetCursor(), false);
                    window_.EnsureCursorVisible(buffer_);
                }
                window_.SetCursor(buffer_, window_.GetCursor(), false);
            }
            else if (mode_ == Mode::Visual)
            {
                const Position &anchor_pos = window_.GetAnchor();
                if (anchor_pos == cursor_pos)
                    return;
                else if (anchor_pos < cursor_pos)
                    window_.ApplyReplace(buffer_, key.value_, anchor_pos, cursor_pos);
                else
                    window_.ApplyReplace(buffer_, key.value_, {cursor_pos.row_, cursor_pos.column_ + 1}, {anchor_pos.row_, anchor_pos.column_ + 1});
            }
            else if (mode_ == Mode::Visual_Line)
            {
                const Position &anchor_pos = window_.GetAnchor();
                size_t row_a = cursor_pos.row_, row_b = anchor_pos.row_;
                if (row_a > row_b)
                    std::swap(row_a, row_b);
                window_.ApplyReplace(buffer_, key.value_, {row_a, 0}, {row_b, buffer_.GetLineAt(row_b).size()});
            }
            window_.EnsureCursorVisible(buffer_);
            Need_Arguments_ = false;
            mode_ = Mode::Normal; // 切换为普通模式
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
        case ActionKind::Waiting_Argument:
            if (key.code_ != KeyCode::Character)
            {
                Largely_Reset();
                Need_Arguments_ = false;
                return;
            }
            editoraction_.argument_name_ = key.value_;
            editoraction_.kind_ = ActionKind::None;
            Need_Arguments_ = false;
            return;

        case ActionKind::Recording:
            if (key.code_ != KeyCode::Character)
            {
                Largely_Reset();
                return;
            }
            Largely_Reset();
            recording_name = key.value_;
            recording_register_.ClearRecord();
            Need_Arguments_ = false;
            return;
        case ActionKind::Play:
            if (key.code_ != KeyCode::Character)
            {
                Largely_Reset();
                return;
            }
            Need_Arguments_ = false;
            playing_times = 1;
            if (editoraction_.count_.has_value())
            {
                playing_times = *editoraction_.count_;
            }
            if (key.value_ == '@')
            {
                if (last_playing_name.has_value())
                {
                    playing_name = last_playing_name;
                }
                else
                {
                    return;
                }
            }
            else
            {
                last_playing_name = playing_name;
                playing_name = key.value_;
            }
            if (register_.find(*playing_name) == register_.end())
            {
                playing_name = std::nullopt;
                playing_times = 0;
                return;
            }
            playing_register = register_[*playing_name];
            playing_register.Reset_ptr();
            last_cursor_ = window_.GetCursor();
            Largely_Reset();
            return;
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
                Insert_Keys_.push_back({KeyCode::Enter, '\r'});
            size_t count = 1, expect_count = *editoraction_.count_;
            normal_parser_.Reset_editoraction(editoraction_);
            while (count < expect_count)
            {
                for (size_t index = 0; index < Insert_Keys_.size(); index++)
                    HandleInsert(Insert_Keys_[index]);
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
            switch (commandline_mode_[0])
            {
            case ':':
                ExecuteCommandLine();
                break;
            case '/':
                if (command_ == "")
                    break;
                last_search_motion = Motion::Right_allow_crossline;
                last_search_string = command_;
                try
                {
                    window_.ApplySearch(buffer_, *last_search_motion, last_search_string);
                }
                catch (std::runtime_error &e)
                {
                    message_ = e.what();
                }
                break;
            case '?':
                if (command_ == "")
                    break;
                last_search_motion = Motion::Left_allow_crossline;
                last_search_string = command_;
                try
                {
                    window_.ApplySearch(buffer_, *last_search_motion, last_search_string);
                }
                catch (std::runtime_error &e)
                {
                    message_ = e.what();
                }
                break;
            default:
                break;
            }
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
            buffer_.ReadFile((std::filesystem::path)argument);
            buffer_.mark_.DeleteMark(0, buffer_.GetLineCount());
            undo_list.clear();
            undo_position_ = 0;
            DealUndoList();
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

    void Editor::HandleVisual(KeyEvent key) // 处理Visual模式
    {
        EditorAction action = normal_parser_.Feed(key);
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

        if (editoraction_.kind_ == ActionKind::Undefined)
        {
            Largely_Reset();
            return;
        }
        window_.RenewCount(editoraction_.count_); // 更新含[Count]的组合按键Count值
        size_t count{1}, index{1};
        bool Modified = false;
        Position now_cursor = window_.GetCursor();
        const Position cursor_pos{window_.GetCursor()}, anchor_pos{window_.GetAnchor()};
        if (editoraction_.count_.has_value())
            count = *editoraction_.count_;

        // 根据Action的种类调用对应模块
        try
        {
            switch (editoraction_.kind_)
            {
            case ActionKind::None:
                return;
            case ActionKind::Waiting_Argument:
                Need_Arguments_ = true; // 需要参数
                return;
            case ActionKind::Move:
                // 交给Window吧，move即可
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
                        window_.SetCursor(buffer_, match_pos, true);
                    else
                        message_ = "Error:Patterns could not be found";
                    break;
                }
                window_.SetCursor(buffer_, window_.GetCursor(), true);
                Largely_Reset();
                return;
            case ActionKind::Undo:
            case ActionKind::Redo: // 导致退出VisualMode
                mode_ = Mode::Normal;
                return;
            case ActionKind::Replace:
                Need_Arguments_ = true; // 我们需要参数，所以这次处理不掉
                return;
            case ActionKind::Delete:
                if (mode_ == Mode::Visual && cursor_pos != anchor_pos)
                {
                    if (anchor_pos < cursor_pos)
                    {
                        register_[editoraction_.argument_name_].Copy_into_Reg(buffer_, anchor_pos, cursor_pos);
                        buffer_.EraseCharacters(anchor_pos.row_, anchor_pos.column_, cursor_pos.row_, cursor_pos.column_);
                        window_.SetCursor(buffer_, anchor_pos, true);
                    }
                    else
                    {
                        register_[editoraction_.argument_name_].Copy_into_Reg(buffer_, {cursor_pos.row_, cursor_pos.column_ + 1}, {anchor_pos.row_, anchor_pos.column_ + 1});
                        buffer_.EraseCharacters(cursor_pos.row_, cursor_pos.column_ + 1, anchor_pos.row_, anchor_pos.column_ + 1);
                        window_.SetCursor(buffer_, cursor_pos, true);
                    }
                }
                else if (mode_ == Mode::Visual_Line)
                {
                    if (anchor_pos.row_ < cursor_pos.row_)
                    {
                        register_[editoraction_.argument_name_].Copy_into_Reg(buffer_, anchor_pos.row_, cursor_pos.row_ + 1);
                        buffer_.EraseLines(anchor_pos.row_, cursor_pos.row_ + 1);
                        if (anchor_pos.row_ > 0)
                            window_.SetCursor(buffer_, {anchor_pos.row_ - 1, 0}, true);
                        else
                            window_.SetCursor(buffer_, {0, 0}, true);
                    }
                    else
                    {
                        register_[editoraction_.argument_name_].Copy_into_Reg(buffer_, cursor_pos.row_, anchor_pos.row_ + 1);
                        buffer_.EraseLines(cursor_pos.row_, anchor_pos.row_ + 1);
                        if (cursor_pos.row_ > 0)
                            window_.SetCursor(buffer_, {cursor_pos.row_ - 1, 0}, true);
                        else
                            window_.SetCursor(buffer_, {0, 0}, true);
                    }
                }
                window_.SetCursor(buffer_, window_.GetCursor(), editoraction_.into_mode_ == Mode::Insert);
                window_.EnsureCursorVisible(buffer_);
                DealUndoList(now_cursor);
                mode_ = Mode::Normal;
                if (editoraction_.into_mode_ == Mode::Insert) // change
                    EnterInsert(window_.GetCursor());
                Largely_Reset();
                return;
            case ActionKind::Jump_To: // 确定3个参数 1.往什么方向跳跃 2.跳多少(缺省为1) 3.跳跃规则
                if (editoraction_.motion_.has_value() && editoraction_.type_.has_value())
                {
                    window_.ApplyJumpto(buffer_, *editoraction_.motion_, *editoraction_.type_);
                    window_.EnsureCursorVisible(buffer_);
                    Largely_Reset();
                }
                return;
            case ActionKind::Indent:
                if (mode_ == Mode::Visual_Line)
                {
                    for (size_t row = std::min(cursor_pos.row_, anchor_pos.row_); row <= std::max(cursor_pos.row_, anchor_pos.row_) && row < buffer_.GetLineCount(); row++)
                    {
                        buffer_.InsertCharacters(row, 0, '\t', 2 * (*editoraction_.count_));
                        window_.RenewCount(2 * (*editoraction_.count_));
                        window_.ApplyMotion(buffer_, Motion::Right);
                    }
                }
                window_.SetCursor(buffer_, window_.GetCursor(), false);
                window_.EnsureCursorVisible(buffer_);
                DealUndoList(now_cursor);
                mode_ = Mode::Normal;
                Largely_Reset();
                return;
            case ActionKind::Unindent:
                if (mode_ == Mode::Visual_Line)
                {
                    for (size_t row = std::min(cursor_pos.row_, anchor_pos.row_); row <= std::max(cursor_pos.row_, anchor_pos.row_) && row < buffer_.GetLineCount(); row++)
                    {
                        buffer_.EraseIndent(row, 8 * (*editoraction_.count_));
                        window_.RenewCount(2 * (*editoraction_.count_));
                        window_.ApplyMotion(buffer_, Motion::Left);
                    }
                }
                window_.SetCursor(buffer_, window_.GetCursor(), false);
                window_.EnsureCursorVisible(buffer_);
                DealUndoList(now_cursor);
                mode_ = Mode::Normal;
                Largely_Reset();
                return;
            case ActionKind::Copy:
                if (mode_ == Mode::Visual && window_.GetCursor() != window_.GetAnchor())
                {
                    if (anchor_pos < cursor_pos)
                        register_[editoraction_.argument_name_].Copy_into_Reg(buffer_, anchor_pos, cursor_pos);
                    else
                        register_[editoraction_.argument_name_].Copy_into_Reg(buffer_, {cursor_pos.row_, cursor_pos.column_ + 1}, {anchor_pos.row_, anchor_pos.column_ + 1});
                }
                else if (mode_ == Mode::Visual_Line)
                {
                    if (anchor_pos.row_ < cursor_pos.row_)
                        register_[editoraction_.argument_name_].Copy_into_Reg(buffer_, anchor_pos.row_, cursor_pos.row_ + 1);
                    else
                        register_[editoraction_.argument_name_].Copy_into_Reg(buffer_, cursor_pos.row_, anchor_pos.row_ + 1);
                }
                mode_ = Mode::Normal;
                return;
            case ActionKind::Paste:
                if (editoraction_.motion_.has_value() && editoraction_.type_.has_value())
                {
                    if (register_.find(editoraction_.argument_name_) != register_.end())
                    {
                        register_[editoraction_.argument_name_].Execute_paste(buffer_, window_, editoraction_);
                        window_.EnsureCursorVisible(buffer_);
                        DealUndoList(now_cursor);
                    }
                    else
                    {
                        message_ = "Error:Nothing in register " + std::string(1, editoraction_.argument_name_);
                    }
                    Largely_Reset();
                }
                return;
            case ActionKind::Recording:
                if (recording_name.has_value())
                {
                    recording_register_.Remove_back();
                    register_[*recording_name].addRegister(std::move(recording_register_));
                    recording_name = std::nullopt;
                    Largely_Reset();
                }
                else
                {
                    Need_Arguments_ = true;
                    return;
                }
                break;
            case ActionKind::Play:
                if (editoraction_.argument_name_ == '@')
                {
                    Execute_withArgument({KeyCode::Character, '@'});
                    return;
                }
                else
                {
                    Need_Arguments_ = true;
                    return;
                }
            case ActionKind::Mark:
                Need_Arguments_ = true; // 我们需要参数，所以这次处理不掉
                return;
            case ActionKind::Shift:
                if (mode_ == Mode::Visual && window_.GetCursor() != window_.GetAnchor())
                {
                    if (anchor_pos < cursor_pos)
                    {
                        for (size_t row = anchor_pos.row_; row <= cursor_pos.row_ && row < buffer_.GetLineCount(); row++)
                        {
                            for (size_t column = 0; column < buffer_.GetLineAt(row).size(); column++)
                            {
                                if ((Position){row, column} < anchor_pos || cursor_pos <= (Position){row, column})
                                    continue;
                                buffer_.ReplaceCharacter(row, column, Assist::ShiftLetterCase(buffer_.GetLineAt(row)[column]));
                            }
                        }
                    }
                    else
                    {
                        for (size_t row = cursor_pos.row_; row <= anchor_pos.row_ && row < buffer_.GetLineCount(); row++)
                        {
                            for (size_t column = 0; column < buffer_.GetLineAt(row).size(); column++)
                            {
                                if (anchor_pos < (Position){row, column} || (Position){row, column} <= cursor_pos)
                                    continue;
                                buffer_.ReplaceCharacter(row, column, Assist::ShiftLetterCase(buffer_.GetLineAt(row)[column]));
                            }
                        }
                    }
                }
                if (mode_ == Mode::Visual_Line)
                {
                    for (size_t row = std::min(cursor_pos.row_, anchor_pos.row_); row <= std::max(cursor_pos.row_, anchor_pos.row_) && row < buffer_.GetLineCount(); row++)
                    {
                        for (size_t column = 0; column < buffer_.GetLineAt(row).size(); column++)
                        {
                            buffer_.ReplaceCharacter(row, column, Assist::ShiftLetterCase(buffer_.GetLineAt(row)[column]));
                        }
                    }
                }
                window_.SetCursor(buffer_, window_.GetCursor(), false);
                window_.EnsureCursorVisible(buffer_);
                DealUndoList(now_cursor);
                mode_ = Mode::Normal;
                Largely_Reset();
                return;
                Largely_Reset();
                return;
            case ActionKind::MergeLine: // 处理J
                for (index = 1; index <= count; index++)
                {
                    if (window_.GetCursor().row_ + 1 < buffer_.GetLineCount())
                    {
                        std::string_view s_line = buffer_.GetLineAt(window_.GetCursor().row_);
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
                mode_ = Mode::Normal;
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
            case ActionKind::ShiftMode:
                message_ = "";
                mode_ = Mode::Normal;
                Largely_Reset();
                return;
            case ActionKind::Adjust:
                if (editoraction_.type_.has_value() && editoraction_.motion_.has_value())
                {
                    window_.AdjustViewport(buffer_, *editoraction_.motion_, *editoraction_.type_);
                    Largely_Reset();
                    return;
                }
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

    bool Editor::SaveBuffer(const std::filesystem::path &path)
    {
        // 1. path为空时调用Save,否则调用SaveAs
        // 2. 捕获保存时的异常,把错误写入message_并返回false
        // 3. 成功后生成包含文件名和行数的提示,返回true,供wq判断是否可以退出
        if (!buffer_.IsModified() && !(buffer_.GetLineCount() == 1 && buffer_.GetLineAt(0) == "") && (buffer_.Same_path(path)))
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
