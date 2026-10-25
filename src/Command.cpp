#include "Command.hpp"

namespace sjtu
{

    EditorAction NormalModeParser::Feed(KeyEvent key)
    {
        // 根据传入的key生成Action,在Basic部分中你应该直接调用GenerateMotion
        if (key.code_ == KeyCode::Escape)
        {
            return {};
        }
        EditorAction editoraction{};
        std::optional<size_t> count{};
        if (key.value_ >= '0' && key.value_ <= '9') // 先判断是不是数字
        {
            count.emplace(key.value_ - '0');
            editoraction.count_ = count;
        }
        if (key.code_ == KeyCode::Character)
        {
            auto value = key.value_;
            switch (value)
            {
            case 'h':
                return GenerateMotion(Motion::Left);
            case 'l':
                return GenerateMotion(Motion::Right);
            case 'k':
                return GenerateMotion(Motion::Up);
            case 'j':
                return GenerateMotion(Motion::Down);
            case 'i':
                return {ActionKind::InsertBefore, Motion::Left, std::nullopt, std::nullopt, Mode::Insert};
            case 'I':
                return {ActionKind::InsertBefore, Motion::Goto_First_Noblank, std::nullopt, std::nullopt, Mode::Insert};
            case 'a':
                return {ActionKind::InsertAfter, Motion::Right, std::nullopt, std::nullopt, Mode::Insert};
            case 'A':
                return {ActionKind::InsertAfter, Motion::Goto_Back, std::nullopt, std::nullopt, Mode::Insert};
            case 'o':
                return {ActionKind::InsertAfter, Motion::Down, std::nullopt, std::nullopt, Mode::Insert};
            case 'O':
                return {ActionKind::InsertBefore, Motion::Up, std::nullopt, std::nullopt, Mode::Insert};
            case ':':
                return GenerateCommand(ActionKind::EnterCommandLine);
            case '0':
                editoraction = GenerateMotion(Motion::Goto_Front);
                editoraction.count_ = count;
                return editoraction;
            case '$':
                return GenerateMotion(Motion::Goto_Back);
            case '^':
                return GenerateMotion(Motion::Goto_First_Noblank);
            case 'u':
                return {ActionKind::Undo, std::nullopt, std::nullopt, ActionType::Ordinary};
            case 'r' & 0x1FU: // ctrl+r
                return {ActionKind::Redo, std::nullopt, std::nullopt, ActionType::Ordinary};
            case 'U':
                return {ActionKind::Undo, std::nullopt, std::nullopt, ActionType::Line};
            case 'r':
                return GenerateCommand(ActionKind::Replace);
            case 'x':
                return {ActionKind::Delete, Motion::Right, std::nullopt, ActionType::Ordinary};
            case 'X':
                return {ActionKind::Delete, Motion::Left, std::nullopt, ActionType::Ordinary};
            case 'g':
                return {ActionKind::Jump_To, std::nullopt, std::nullopt, std::nullopt};
            case 'G':
                return {ActionKind::Jump_To, Motion::Goto_First_Noblank, std::nullopt, ActionType::Max};
            case 'J':
                return {ActionKind::MergeLine, std::nullopt, std::nullopt, std::nullopt};
            case 'b':
                return {ActionKind::Move, Motion::Up, std::nullopt, ActionType::word};
            case 'e':
                return {ActionKind::Move, Motion::Down, std::nullopt, ActionType::word};
            case 'w':
                return {ActionKind::Move, Motion::Right, std::nullopt, ActionType::word};
            case 'B':
                return {ActionKind::Move, Motion::Up, std::nullopt, ActionType::WORD};
            case 'E':
                return {ActionKind::Move, Motion::Down, std::nullopt, ActionType::WORD};
            case 'W':
                return {ActionKind::Move, Motion::Right, std::nullopt, ActionType::WORD};
            case '%':
                return {ActionKind::Move, std::nullopt, std::nullopt, ActionType::Match};
            case 'd':
                return {ActionKind::Delete, std::nullopt, std::nullopt, std::nullopt};
            case 'D':
                return {ActionKind::Delete, Motion::Goto_Back, std::nullopt, ActionType::Ordinary};
            case 'c':
                return {ActionKind::Delete, std::nullopt, std::nullopt, std::nullopt, Mode::Insert};
            case 'C':
                return {ActionKind::Delete, Motion::Goto_Back, std::nullopt, ActionType::Ordinary, Mode::Insert};
            case '>':
                return {ActionKind::Indent, std::nullopt, std::nullopt, std::nullopt};
            case '<':
                return {ActionKind::Unindent, std::nullopt, std::nullopt, std::nullopt};
            case 'm':
                return {ActionKind::Mark, std::nullopt, std::nullopt, std::nullopt};
            case '`':
                return {ActionKind::Mark, std::nullopt, std::nullopt, ActionType::To};
            case '~':
                return {ActionKind::Shift, Motion::Right, std::nullopt, ActionType::Ordinary};
            case 'y':
                return {ActionKind::Copy, std::nullopt, std::nullopt, std::nullopt};
            case 'Y':
                return {ActionKind::Copy, Motion::Go_Copy, std::nullopt, ActionType::Ordinary};
            case 'p':
                return {ActionKind::Paste, Motion::Right, std::nullopt, ActionType::Ordinary};
            case 'P':
                return {ActionKind::Paste, Motion::Left, std::nullopt, ActionType::Ordinary};
            case 'q':
                return {ActionKind::Recording, std::nullopt, std::nullopt, std::nullopt};
            case '@':
                return {ActionKind::Play, std::nullopt, std::nullopt, std::nullopt};
            case 'Q':
                return {ActionKind::Play, std::nullopt, std::nullopt, std::nullopt, Mode::Normal, '@'};
            case '/':
                return {ActionKind::Search, Motion::Right_allow_crossline, std::nullopt, ActionType::Ordinary, Mode::CommandLine};
            case '?':
                return {ActionKind::Search, Motion::Left_allow_crossline, std::nullopt, ActionType::Ordinary, Mode::CommandLine};
            case 'n':
                return {ActionKind::Search, Motion::Up, std::nullopt, ActionType::Ordinary};
            case 'N':
                return {ActionKind::Search, Motion::Down, std::nullopt, ActionType::Ordinary};
            case 'E' & 0x1FU:
                return {ActionKind::Adjust, Motion::Down, std::nullopt, ActionType::Ordinary};
            case 'Y' & 0x1FU:
                return {ActionKind::Adjust, Motion::Down, std::nullopt, ActionType::Ordinary};
            case 'D' & 0x1FU:
                return {ActionKind::Adjust, Motion::Go_Down_Line, std::nullopt, ActionType::Half};
            case 'U' & 0x1FU:
                return {ActionKind::Adjust, Motion::Go_Up_Line, std::nullopt, ActionType::Half};
            case 'F' & 0x1FU:
                return {ActionKind::Adjust, Motion::Go_Down_Line, std::nullopt, ActionType::Ordinary};
            case 'B' & 0x1FU:
                return {ActionKind::Adjust, Motion::Go_Up_Line, std::nullopt, ActionType::Ordinary};
            case 'z':
                return {ActionKind::Adjust, std::nullopt, std::nullopt, std::nullopt};
            case 't': // 纯纯我们自己添加的，为了zt
                return {ActionKind::None, std::nullopt, std::nullopt, ActionType::Max};
            case '"':
                return {ActionKind::Waiting_Argument, std::nullopt, std::nullopt, std::nullopt,Mode::Normal,'"'};
            case 'v':
                return {ActionKind::ShiftMode, std::nullopt,std::nullopt, std::nullopt, Mode::Visual};
            case 'V':
                return {ActionKind::ShiftMode, std::nullopt,std::nullopt, std::nullopt, Mode::Visual_Line};
            default:
                break;
            }
            return editoraction;
        }

        switch (key.code_)
        {
        case KeyCode::ArrowLeft:
            return GenerateMotion(Motion::Left);
        case KeyCode::ArrowRight:
            return GenerateMotion(Motion::Right);
        case KeyCode::ArrowUp:
            return GenerateMotion(Motion::Up);
        case KeyCode::ArrowDown:
            return GenerateMotion(Motion::Down);
        case KeyCode::Backspace:
            return GenerateMotion(Motion::Left_allow_crossline);
        case KeyCode::Delete:
            return {ActionKind::Delete, Motion::Right, std::nullopt, ActionType::Delete};
        default:
            return {};
        }
        // 虽然不要求这些按键,但是我们给你的Terminal.hpp可以处理这些你键盘上的特殊按键并把他们放在了keycode里
        // 在Vim中,它们对应着hjkl.

        return editoraction;
    }

    bool NormalModeParser::Merge_editoraction_number(EditorAction &editoraction_1, const EditorAction &editoraction_2)
    {
        if (editoraction_2.count_.has_value()) // 先合并数字
        {
            if (editoraction_1.count_.has_value() && *(editoraction_1.count_) > 0) // 数字合并
            {
                if (*editoraction_1.count_ > (size_t)(1 << 28))
                {
                    throw std::runtime_error("Error:Too large!");
                    return true;
                }
                editoraction_1.count_ = (*(editoraction_1.count_) * 10 + *(editoraction_2.count_));
                return true;
            }
            else if (*editoraction_2.count_ == 0)
            {
                return false;
            }
            else
            {
                editoraction_1.count_ = editoraction_2.count_;
            }
            return true;
        }
        if (editoraction_1.kind_ != ActionKind::Move && editoraction_1.count_.has_value() && *(editoraction_1.count_) > 0) // 先处理数字+[x]的组合键,并且目前只有数字
        {
            switch (editoraction_2.kind_)
            {
            case ActionKind::Move:
            case ActionKind::Delete:
            case ActionKind::Indent:
            case ActionKind::Unindent:
            case ActionKind::InsertBefore:
            case ActionKind::InsertAfter:
            case ActionKind::Shift:
            case ActionKind::Adjust:
            case ActionKind::Copy:
            case ActionKind::Paste:
            case ActionKind::Play:
                if (editoraction_2.kind_ == ActionKind::Move && editoraction_2.type_.has_value() && *editoraction_2.type_ == ActionType::Match) // 特判%
                {
                    editoraction_1.kind_ = ActionKind::Jump_To;
                    editoraction_1.motion_ = Motion::Down;
                    editoraction_1.type_ = ActionType::Ordinary;
                    editoraction_1.into_mode_ = editoraction_2.into_mode_;
                    return true;
                }
                editoraction_1.kind_ = editoraction_2.kind_;
                editoraction_1.motion_ = editoraction_2.motion_;
                editoraction_1.type_ = editoraction_2.type_;
                editoraction_1.into_mode_ = editoraction_2.into_mode_;
                return true;
            case ActionKind::Redo:
            case ActionKind::Undo:
            case ActionKind::Replace:
                editoraction_1.kind_ = editoraction_2.kind_;
                editoraction_1.motion_ = std::nullopt;
                editoraction_1.type_ = editoraction_2.type_;
                editoraction_1.into_mode_ = editoraction_2.into_mode_;
                return true;
            case ActionKind::Jump_To: // 要考虑G
                if (editoraction_2.type_ == ActionType::Max)
                {
                    editoraction_1.kind_ = editoraction_2.kind_;
                    editoraction_1.motion_ = Motion::Goto_First_Noblank;
                    editoraction_1.type_ = ActionType::Ordinary;
                    editoraction_1.into_mode_ = editoraction_2.into_mode_;
                    return true;
                }
                editoraction_1.kind_ = editoraction_2.kind_;
                editoraction_1.motion_ = editoraction_2.motion_;
                editoraction_1.type_ = editoraction_2.type_;
                editoraction_1.into_mode_ = editoraction_2.into_mode_;
                return true;
            }
        }
        return false;
    }

    bool NormalModeParser::Merge_editoraction_delete(EditorAction &editoraction_1, const EditorAction &editoraction_2)
    {
        // 大部分情况，我们可以将c和d等价处理，C与D等价处理，except for cc dd
        if (editoraction_1.type_.has_value()) // 已经不是单d的情况了 如dg
        {
            switch (*editoraction_1.type_)
            {
            case ActionType::To:                                           // dg
                if (editoraction_2 == (KeyEvent){KeyCode::Character, 'g'}) // dgg
                    editoraction_1.motion_ = Motion::Goto_First_Noblank;
                else if (editoraction_2 == (KeyEvent){KeyCode::Character, 'e'}) // dge
                {
                    editoraction_1.motion_ = Motion::Left;
                    editoraction_1.type_ = ActionType::word;
                }
                else if (editoraction_2 == (KeyEvent){KeyCode::Character, 'E'}) // dgE
                {
                    editoraction_1.motion_ = Motion::Left;
                    editoraction_1.type_ = ActionType::WORD;
                }
                return true;
            }
            return false;
        }
        if (editoraction_2 == (KeyEvent){KeyCode::Character, 'g'}) // d+g
        {
            editoraction_1.motion_ = std::nullopt;
            editoraction_1.type_ = ActionType::To;
            return true;
        }
        if (editoraction_1.into_mode_ == Mode::Normal && editoraction_2 == (KeyEvent){KeyCode::Character, 'd'}) // d+d
        {
            editoraction_1.motion_ = Motion::Go_Delete;
            editoraction_1.type_ = ActionType::Ordinary;
            return true;
        }
        if (editoraction_1.into_mode_ == Mode::Insert && editoraction_2 == (KeyEvent){KeyCode::Character, 'c'}) // cc
        {
            editoraction_1.motion_ = Motion::Go_Delete;
            editoraction_1.type_ = ActionType::Ordinary;
            return true;
        }
        if (editoraction_2 == (KeyEvent){KeyCode::Character, 'G'}) // d+([num])+G
        {
            if (editoraction_1.count_.has_value())
                editoraction_1.type_ = ActionType::To;
            else
                editoraction_1.type_ = ActionType::Max;
            editoraction_1.motion_ = Motion::Goto_First_Noblank;
            return true;
        }
        if (editoraction_2 == (KeyEvent){KeyCode::Character, '%'}) // d+([num])+%
        {
            if (editoraction_1.count_.has_value())
            {
                editoraction_1.motion_ = Motion::Down;
                editoraction_1.type_ = ActionType::To;
            }
            else
            {
                editoraction_1.motion_ = Motion::Down;
                editoraction_1.type_ = ActionType::Match;
            }
            return true;
        }
        if (editoraction_2.kind_ == ActionKind::Move) // dj dk dl dh d0 d$ d^ de dw Backspace
        {
            editoraction_1.motion_ = editoraction_2.motion_;
            editoraction_1.type_ = editoraction_2.type_;
            return true;
        }
        return false;
    }

    bool NormalModeParser::Merge_editoraction_indent(EditorAction &editoraction_1, const EditorAction &editoraction_2)
    {
        // 大部分情况，我们可以将< 和 >对等处理
        if (editoraction_1.type_.has_value()) // 已经不是单<的情况了 如dg
        {
            switch (*editoraction_1.type_)
            {
            case ActionType::To:                                           // +g
                if (editoraction_2 == (KeyEvent){KeyCode::Character, 'g'}) // +gg
                    editoraction_1.motion_ = Motion::Goto_First_Noblank;
                return true;
            }
            return false;
        }
        if (editoraction_2 == (KeyEvent){KeyCode::Character, 'g'}) // +g
        {
            editoraction_1.motion_ = std::nullopt;
            editoraction_1.type_ = ActionType::To;
            return true;
        }
        if (editoraction_1.kind_ == ActionKind::Indent && editoraction_2 == (KeyEvent){KeyCode::Character, '>'}) // >>
        {
            editoraction_1.motion_ = Motion::Go_Indent;
            editoraction_1.type_ = ActionType::Ordinary;
            return true;
        }
        if (editoraction_1.kind_ == ActionKind::Unindent && editoraction_2 == (KeyEvent){KeyCode::Character, '<'}) // <<
        {
            editoraction_1.motion_ = Motion::Go_Indent;
            editoraction_1.type_ = ActionType::Ordinary;
            return true;
        }
        if (editoraction_2 == (KeyEvent){KeyCode::Character, 'G'}) // +([num])+G
        {
            if (editoraction_1.count_.has_value())
                editoraction_1.type_ = ActionType::To;
            else
                editoraction_1.type_ = ActionType::Max;
            editoraction_1.motion_ = Motion::Goto_First_Noblank;
            return true;
        }
        if (editoraction_2 == (KeyEvent){KeyCode::Character, '%'}) // +([num])+%
        {
            if (editoraction_1.count_.has_value())
            {
                editoraction_1.motion_ = Motion::Down;
                editoraction_1.type_ = ActionType::To;
            }
            else
            {
                editoraction_1.motion_ = Motion::Down;
                editoraction_1.type_ = ActionType::Match;
            }
            return true;
        }
        if (editoraction_2.kind_ == ActionKind::Move) // j k l h 0 $ ^ e w Backspace
        {
            editoraction_1.motion_ = editoraction_2.motion_;
            editoraction_1.type_ = editoraction_2.type_;
            return true;
        }

        return false;
    }

    bool NormalModeParser::Merge_editoraction_copy(EditorAction &editoraction_1, const EditorAction &editoraction_2)
    {
        if (editoraction_1.type_.has_value()) // 已经不是单y的情况了 如yg
        {
            switch (*editoraction_1.type_)
            {
            case ActionType::To:                                           // yg
                if (editoraction_2 == (KeyEvent){KeyCode::Character, 'g'}) // ygg
                    editoraction_1.motion_ = Motion::Goto_First_Noblank;
                else if (editoraction_2 == (KeyEvent){KeyCode::Character, 'e'}) // yge
                {
                    editoraction_1.motion_ = Motion::Left;
                    editoraction_1.type_ = ActionType::word;
                }
                else if (editoraction_2 == (KeyEvent){KeyCode::Character, 'E'}) // ygE
                {
                    editoraction_1.motion_ = Motion::Left;
                    editoraction_1.type_ = ActionType::WORD;
                }
                return true;
            }
            return false;
        }
        if (editoraction_2 == (KeyEvent){KeyCode::Character, 'g'}) // y+g
        {
            editoraction_1.motion_ = std::nullopt;
            editoraction_1.type_ = ActionType::To;
            return true;
        }
        if (editoraction_2 == (KeyEvent){KeyCode::Character, 'y'}) // yy
        {
            editoraction_1.motion_ = Motion::Go_Copy;
            editoraction_1.type_ = ActionType::Ordinary;
            return true;
        }
        if (editoraction_2 == (KeyEvent){KeyCode::Character, 'G'}) // y+([num])+G
        {
            if (editoraction_1.count_.has_value())
                editoraction_1.type_ = ActionType::To;
            else
                editoraction_1.type_ = ActionType::Max;
            editoraction_1.motion_ = Motion::Goto_First_Noblank;
            return true;
        }
        if (editoraction_2 == (KeyEvent){KeyCode::Character, '%'}) // y+([num])+%
        {
            if (editoraction_1.count_.has_value())
            {
                editoraction_1.motion_ = Motion::Down;
                editoraction_1.type_ = ActionType::To;
            }
            else
            {
                editoraction_1.motion_ = Motion::Down;
                editoraction_1.type_ = ActionType::Match;
            }
            return true;
        }
        if (editoraction_2.kind_ == ActionKind::Move) // yj yk yl yh y0 y$ y^ ye yw Backspace
        {
            editoraction_1.motion_ = editoraction_2.motion_;
            editoraction_1.type_ = editoraction_2.type_;
            return true;
        }
        return false;
    }

    void NormalModeParser::Merge_editoraction(EditorAction &editoraction_1, const EditorAction &editoraction_2)
    {
        if (editoraction_2.count_.has_value() || (editoraction_1.kind_ == ActionKind::None && editoraction_1.count_.has_value())) // 处理数字开头
        {
            if (Merge_editoraction_number(editoraction_1, editoraction_2))
                return;
        }
        if (editoraction_1.kind_ == ActionKind::Delete) // 处理d+[x]
        {
            if (Merge_editoraction_delete(editoraction_1, editoraction_2))
                return;
        }
        if (editoraction_1.kind_ == ActionKind::Indent || editoraction_1.kind_ == ActionKind::Unindent) // 处理d+[x]
        {
            if (Merge_editoraction_indent(editoraction_1, editoraction_2))
                return;
        }
        if (editoraction_1.kind_ == ActionKind::Copy)
        {
            if (Merge_editoraction_copy(editoraction_1, editoraction_2))
                return;
        }
        if (editoraction_1.kind_ == ActionKind::Jump_To) // 处理g+[x]
        {
            if (editoraction_2 == (KeyEvent){KeyCode::Character, 'g'}) // ([num]+)gg
            {
                editoraction_1.motion_ = Motion::Goto_First_Noblank;
                editoraction_1.type_ = ActionType::Ordinary;
                return;
            }
            if (editoraction_2.kind_ == ActionKind::Move && editoraction_2.type_.has_value() && (editoraction_2.type_ == ActionType::word || editoraction_2.type_ == ActionType::WORD) && editoraction_2.motion_.has_value() && editoraction_2.motion_ == Motion::Down) // ge gE
            {
                editoraction_1.kind_ = editoraction_2.kind_;
                editoraction_1.type_ = editoraction_2.type_;
                editoraction_1.motion_ = Motion::Left;
                return;
            }
            Reset_editoraction(editoraction_1);
            editoraction_1.kind_ = ActionKind::Undefined;
            return;
        }
        if (editoraction_1.kind_ == ActionKind::Adjust) // z
        {
            if (editoraction_2 == (KeyEvent){KeyCode::Character, 't'})
            {
                editoraction_1.motion_ = Motion::Goto_First_Noblank;
                editoraction_1.type_ = ActionType::Max;
                return;
            }
            else if (editoraction_2 == (KeyEvent){KeyCode::Character, 'z'})
            {
                editoraction_1.motion_ = Motion::Goto_First_Noblank;
                editoraction_1.type_ = ActionType::Half;
                return;
            }
            else if (editoraction_2 == (KeyEvent){KeyCode::Character, 'b'})
            {
                editoraction_1.motion_ = Motion::Goto_First_Noblank;
                editoraction_1.type_ = ActionType::Min;
                return;
            }
        }
        if (editoraction_1.kind_ != ActionKind::None) // 不存在的组合，认为输入错误，直接操作中断
        {
            Reset_editoraction(editoraction_1);
            editoraction_1.kind_ = ActionKind::Undefined;
            return;
        }
        char temp_c = editoraction_1.argument_name_;
        editoraction_1 = editoraction_2;
        editoraction_1.argument_name_ = temp_c; // 保持参数名不变
    }

    void NormalModeParser::Reset_editoraction(EditorAction &editoraction_1)
    {
        editoraction_1.count_ = std::nullopt;
        editoraction_1.kind_ = ActionKind::None;
        editoraction_1.motion_ = std::nullopt;
        editoraction_1.type_ = std::nullopt;
        editoraction_1.into_mode_ = Mode::Normal;
        editoraction_1.argument_name_ = '"';
    }

    EditorAction NormalModeParser::GenerateMotion(Motion motion)
    {
        return {ActionKind::Move, motion, std::nullopt, ActionType::Ordinary};
    }

    EditorAction NormalModeParser::GenerateCommand(ActionKind kind)
    {
        return {kind, std::nullopt, std::nullopt, std::nullopt};
    }

} // namespace sjtu
