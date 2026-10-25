/*
Types.hpp
这里放各模块共用的简单类型
注意字符下标和显示列不是一回事,一个Tab在Buffer中只有一个字符,在屏幕上可能占多列.
*/
#ifndef MINIVIM_TYPES_HPP
#define MINIVIM_TYPES_HPP

#include <cstddef>

namespace sjtu
{

    struct Position
    {
        // Buffer中的行号和字符下标;插入模式的column_可以等于该行长度
        size_t row_{0};
        size_t column_{0};
    };

    inline bool operator<(const Position &a, const Position &b)
    {
        if (a.row_ != b.row_)
            return a.row_ < b.row_;
        return a.column_ < b.column_;
    }

    inline bool operator==(const Position &a, const Position &b)
    {
        return a.row_ == b.row_ && a.column_ == b.column_;
    }

    inline bool operator<=(const Position &a, const Position &b)
    {
        return a < b || a == b;
    }

    inline bool operator!=(const Position &a, const Position &b)
    {
        return a.row_ != b.row_ || a.column_ != b.column_;
    }

    struct ScreenSize
    {
        // 终端的总行数和总列数,包括底部用于显示命令或提示的那一行
        size_t rows_{0};
        size_t columns_{0};
    };

    struct Viewport
    {
        // top_是顶部可见的Buffer行号,left_是左侧可见的显示列
        // rows_和columns_是正文区域的高度和宽度,不包含底部提示行
        size_t top_{0};
        size_t left_{0};
        size_t rows_{1};
        size_t columns_{1};
    };

    // 分别表示普通模式、插入模式和冒号命令行模式
    enum class Mode
    {
        Normal,
        Insert,
        CommandLine,
        Visual,
        Visual_Line,
    };

    // Basic中支持的四个移动方向,可以由hjkl等按键生成
    enum class Motion
    {
        Left,
        Down,
        Up,
        Right,
        Goto_Front,
        Goto_Back,
        Go_Down_Line,
        Go_Up_Line,
        Goto_First_Noblank,
        Left_allow_crossline,
        Right_allow_crossline,
        Go_Delete,
        Go_Indent,
        Go_Copy,
    };

    // 用于分门别类，比如是按字符移动还是按格子移动
    enum class ActionType
    {
        Ordinary, // 通常的做法，一般指单这个操作对应的基础ActionKind，不加任何组合的情况
        word,
        WORD,
        Line,
        Delete,
        To,    // 以访问某个特定行或者特定列的方式执行这个ActionKind
        Max,   // 以最大情况执行这个ActionKind
        Min,
        Half,  //只执行一半，无视count
        Match, // 匹配
    };

} // namespace sjtu

#endif // MINIVIM_TYPES_HPP
