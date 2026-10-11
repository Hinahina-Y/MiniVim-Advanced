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
                return GenerateCommand(ActionKind::InsertBefore);
            case 'a':
                return GenerateCommand(ActionKind::InsertAfter);
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
                return GenerateCommand(ActionKind::Undo);
            case 'r' & 0x1FU: // ctrl+r
                return GenerateCommand(ActionKind::Redo);
            case 'U':
                return GenerateCommand(ActionKind::LineUndo);
            case 'r':
                return GenerateCommand(ActionKind::Replace);
            case 'x':
                return {ActionKind::Delete, Motion::Right, std::nullopt};
            case 'X':
                return {ActionKind::Delete, Motion::Left, std::nullopt};
            case 'g':
                return {ActionKind::Jump_To, std::nullopt, std::nullopt};
            case 'G':
                return {ActionKind::Jump_Max, Motion::Goto_First_Noblank, std::nullopt};
            case 'J':
                return {ActionKind::MergeLine, std::nullopt, std::nullopt};
            case 'b':
                return {ActionKind::Move_word, Motion::Up, std::nullopt};
            case 'e':
                return {ActionKind::Move_word, Motion::Down, std::nullopt};
            case 'w':
                return {ActionKind::Move_word, Motion::Right, std::nullopt};
            case 'B':
                return {ActionKind::Move_WORD, Motion::Up, std::nullopt};
            case 'E':
                return {ActionKind::Move_WORD, Motion::Down, std::nullopt};
            case 'W':
                return {ActionKind::Move_WORD, Motion::Right, std::nullopt};
            case '%':
                return {ActionKind::Match, std::nullopt, std::nullopt};
            case 'd':
                return {ActionKind::d_Delete, std::nullopt, std::nullopt};
            case 'D':
                return {ActionKind::d_Delete, Motion::Goto_Back, std::nullopt};
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
            return GenerateCommand(ActionKind::Delete);
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
            case ActionKind::d_Delete:
                editoraction_1.kind_ = editoraction_2.kind_;
                editoraction_1.motion_ = editoraction_2.motion_;
                return true;
            case ActionKind::Redo:
            case ActionKind::Undo:
            case ActionKind::Replace:
            case ActionKind::Delete:
            case ActionKind::Jump_To:
                editoraction_1.kind_ = editoraction_2.kind_;
                editoraction_1.motion_ = std::nullopt;
                return true;
            case ActionKind::Jump_Max: // 要考虑G
                editoraction_1.kind_ = ActionKind::Jump_To;
                editoraction_1.motion_ = editoraction_2.motion_;
                return true;
            case ActionKind::Match:
                editoraction_1.kind_ = ActionKind::Jump_To;
                editoraction_1.motion_ = Motion::Down;
                return true;
            }
        }
        return false;
    }

    bool NormalModeParser::Merge_editoraction_delete(EditorAction &editoraction_1, const EditorAction &editoraction_2)
    {
        if (editoraction_2.kind_ == ActionKind::d_Delete && !editoraction_2.motion_.has_value() && !editoraction_1.motion_.has_value()) // dd
        {
            editoraction_1.motion_ = Motion::Go_Delete;
            return true;
        }
        if (editoraction_2.kind_ == ActionKind::Move) // dj dk dl dh d0 d$ d^
        {
            if(editoraction_2.motion_==Motion::Left_allow_crossline)//d Backspace
            {
                return false;
            }
            editoraction_1.motion_ = editoraction_2.motion_;
            return true;
        }
        /*
        if (editoraction_2.kind_ == ActionKind::Jump_To || editoraction_2.kind_ == ActionKind::Jump_Max) // dg dG 考虑交给g处理
        {
            if (editoraction_2.motion_.has_value()&&editoraction_1.count_.has_value()) // d+[num]+G
            {
                editoraction_1.motion_ = Motion::Go_Delete;
                editoraction_1.kind_=editoraction_2.kind_;
            }
            else
            {
                editoraction_1.motion_ = Motion::Go_Delete;
                editoraction_1.kind_ = editoraction_2.kind_;
            }
            return true;
        }*/
        return false;
    }

    void NormalModeParser::Merge_editoraction(EditorAction &editoraction_1, const EditorAction &editoraction_2)
    {
        if (editoraction_2.count_.has_value() || (editoraction_1.kind_ == ActionKind::None && editoraction_1.count_.has_value())) // 处理数字开头
        {
            if (Merge_editoraction_number(editoraction_1, editoraction_2))
                return;
        }
        if (editoraction_1.kind_ == ActionKind::d_Delete) // 处理d+[x]
        {
            if (Merge_editoraction_delete(editoraction_1, editoraction_2))
                return;
        }
        if (editoraction_1.kind_ == ActionKind::Jump_To) // 处理g+[x]
        {
            if (editoraction_2.kind_ == ActionKind::Jump_To) // ([num]+)gg
            {
                if (!editoraction_2.motion_.has_value())
                {
                    if (editoraction_1.motion_ == Motion::Go_Delete)
                    {
                    }
                    editoraction_1.motion_ = Motion::Goto_First_Noblank;
                }
                return;
            }
            if ((editoraction_2.kind_ == ActionKind::Move_word || editoraction_2.kind_ == ActionKind::Move_WORD) && editoraction_2.motion_ == Motion::Down) // ge gE
            {
                editoraction_1.kind_ = editoraction_2.kind_;
                editoraction_1.motion_ = Motion::Left;
                return;
            }
            Reset_editoraction(editoraction_1);
            editoraction_1.kind_ = ActionKind::Undefined;
            return;
        }
        if (editoraction_1.kind_ == ActionKind::LineUndo)
        {
            return;
        }
        if (editoraction_1.kind_ != ActionKind::None) // 不存在的组合，认为输入错误，直接操作中断
        {
            Reset_editoraction(editoraction_1);
            editoraction_1.kind_ = ActionKind::Undefined;
            return;
        }
        editoraction_1 = editoraction_2;
    }

    void NormalModeParser::Reset_editoraction(EditorAction &editoraction_1)
    {
        editoraction_1.count_ = std::nullopt;
        editoraction_1.kind_ = ActionKind::None;
        editoraction_1.motion_ = std::nullopt;
    }

    EditorAction NormalModeParser::GenerateMotion(Motion motion)
    {
        return {ActionKind::Move, motion, std::nullopt};
    }

    EditorAction NormalModeParser::GenerateCommand(ActionKind kind)
    {
        return {kind, std::nullopt, std::nullopt};
    }

} // namespace sjtu
