#include "Renderer.hpp"
#include "TextLayout.hpp"

namespace sjtu
{

    // 我们在Render中保留了所有需要用到控制序列的部分逻辑,你不应该修改它们.
    // 啥是控制序列?它们是嵌入在文本流中的“指令”,告诉终端执行特定操作(比如移动光标、改变文字颜色、清屏或清除行内容).
    namespace
    {

        void AppendClearedLine(std::string &frame, std::string_view contents, std::size_t width, bool newline)
        {
            // 追加不超过width列的显示内容,再用ESC[K清除这一行余下的旧内容
            // 需要换行时同时输出回车和换行,因为raw模式关闭了终端的自动输出转换
            // 你在生成帧的时候生成每一行都应该调用这个函数
            frame.append(contents.substr(0, width));
            frame += "\x1b[K";
            if (newline)
            {
                frame += "\r\n";
            }
        }

        std::string CursorSequence(std::size_t row, std::size_t column)
        {
            // 生成定位光标的ANSI转义序列,这里接收的行列都从1开始
            return "\x1b[" + std::to_string(row) + ';' + std::to_string(column) + 'H';
        }

    }

    std::string Renderer::Render(const Buffer &buffer, const Window &window, const RenderState &state) const
    {
        // 1. 隐藏光标并回到屏幕左上角,开始拼接新的一帧(一帧就是一个长string)
        // 2. 按视口逐行取Buffer内容,展开Tab后再横向截取;超出文件的行显示~
        // 3. 绘制底部一行,优先显示命令行,其次是提示信息,最后是Insert模式标记
        // 4. 根据当前模式计算光标的屏幕位置,追加定位序列并重新显示光标
        // 你应该区分我们的Cursor光标(它代表当前用户在修改文件的哪个位置)和终端显示的白色亮条,白色亮条在屏幕中的位置是由光标位置和视口位置计算的
        auto &viewport = window.GetViewport();
        auto width = std::max<std::size_t>(viewport.columns_, 1);

        std::string frame;
        frame.reserve((viewport.rows_ + 1) * (width + 8));
        frame += "\x1b[?25l";
        frame += "\x1b[H";
        // 处理高亮
        Position now_cursor_{window.GetCursor()}, matched_cursor_{buffer.GetLineCount(), 0}; // 管理括号匹配导致高亮
        std::string line = buffer.GetLineAt(now_cursor_.row_);
        std::vector<std::pair<std::string, Position>> Highlight; // 需要高亮的单个字符位置
        Highlight.clear();
        if (now_cursor_.column_ < line.size() && Assist::Braces(line[now_cursor_.column_]) != 0) // 括号匹配
        {
            matched_cursor_ = window.Parenthesis_Matching(buffer);
        }
        if (matched_cursor_.row_ < buffer.GetLineCount()) // 括号匹配高亮
        {
            now_cursor_.column_ = BufferColumnToRenderColumn(buffer.GetLineAt(now_cursor_.row_), now_cursor_.column_);
            matched_cursor_.column_ = BufferColumnToRenderColumn(buffer.GetLineAt(matched_cursor_.row_), matched_cursor_.column_);
            Highlight.push_back({"\x1b[104;97m", now_cursor_});
            if (matched_cursor_.row_ >= viewport.top_ && matched_cursor_.row_ < viewport.top_ + viewport.rows_)
                Highlight.push_back({"\x1b[104;97m", matched_cursor_});
        }

        // Visual Mode 字符高亮
        const Position &cursor_pos{window.GetCursor()}, anchor_pos{window.GetAnchor()};
        std::string Line_Highlight_tag = "";
        if (state.mode_ == Mode::Visual)
        {
            if (anchor_pos < cursor_pos)
            {
                if (anchor_pos.row_ == cursor_pos.row_)
                {
                    for (size_t column = std::max(anchor_pos.column_, viewport.left_); column < std::min(cursor_pos.column_, viewport.left_ + viewport.columns_); column++)
                    {
                        Highlight.push_back({"\x1b[30;47m", {anchor_pos.row_, column}});
                    }
                }
                else
                {
                    if (anchor_pos.row_ >= viewport.top_)
                        for (size_t column = std::max(anchor_pos.column_, viewport.left_); column < std::min(buffer.GetLineAt(anchor_pos.row_).size(), viewport.left_ + viewport.columns_); column++)
                            Highlight.push_back({"\x1b[30;47m", {anchor_pos.row_, column}});
                    for (size_t column = std::max((size_t)0, viewport.left_); column < std::min(cursor_pos.column_, viewport.left_ + viewport.columns_); column++)
                        Highlight.push_back({"\x1b[30;47m", {cursor_pos.row_, column}});
                }
            }
            else if (cursor_pos < anchor_pos)
            {
                if (anchor_pos.row_ == cursor_pos.row_)
                    for (size_t column = std::max(cursor_pos.column_ + 1, viewport.left_); column < std::min(anchor_pos.column_ + 1, viewport.left_ + viewport.columns_); column++)
                        Highlight.push_back({"\x1b[30;47m", {anchor_pos.row_, column}});
                else
                {
                    for (size_t column = std::max(cursor_pos.column_ + 1, viewport.left_); column < std::min(buffer.GetLineAt(cursor_pos.row_).size(), viewport.left_ + viewport.columns_); column++)
                        Highlight.push_back({"\x1b[30;47m", {cursor_pos.row_, column}});
                    if (anchor_pos.row_ < viewport.top_ + viewport.rows_)
                        for (size_t column = std::max((size_t)0, viewport.left_); column < std::min(anchor_pos.column_ + 1, viewport.left_ + viewport.columns_); column++)
                            Highlight.push_back({"\x1b[30;47m", {anchor_pos.row_, column}});
                }
            }
        }

        // 正式输出
        for (std::size_t screen_row = 0; screen_row < viewport.rows_; ++screen_row)
        {
            frame += "\x1b[K";
            if (screen_row + viewport.top_ >= buffer.GetLineCount())
            {
                frame += "\x1b[94m";
                AppendClearedLine(frame, "~", width, false);
                frame += "\x1b[39m\r\n";
            }
            else
            {
                // 截取字符
                line = ExpandForDisplay(buffer.GetLineAt(screen_row + viewport.top_));
                size_t l = viewport.left_;
                size_t r = viewport.left_ + viewport.columns_ + (size_t)1;
                if (viewport.left_ >= line.size())
                {
                    AppendClearedLine(frame, "", width, true);
                    continue;
                }
                // 处理整行高亮
                Line_Highlight_tag = "";
                if (state.mode_ == Mode::Visual && screen_row + viewport.top_ > std::min(cursor_pos.row_, anchor_pos.row_) && screen_row + viewport.top_ < std::max(cursor_pos.row_, anchor_pos.row_))
                    Line_Highlight_tag = "\x1b[30;47m";
                else if (state.mode_ == Mode::Visual_Line && screen_row + viewport.top_ >= std::min(cursor_pos.row_, anchor_pos.row_) && screen_row + viewport.top_ <= std::max(cursor_pos.row_, anchor_pos.row_))
                    Line_Highlight_tag = "\x1b[30;47m";
                // 拼接整行高亮,处理字符高亮
                line = DisplayHighlight(Highlight, line.substr(l, r - l), {screen_row + viewport.top_, l}, r - l, Line_Highlight_tag);
                AppendClearedLine(frame, line, line.size() + 5, true);
            }
        }

        std::string bottom;
        std::string show_cursor_pos;
        std::string bottomvalue = "";
        if (state.mode_ == Mode::CommandLine)
        {
            bottom = state.commandline_mode_ + state.command_;
        }
        else if (!state.message_.empty())
        {
            bottom = state.message_;
            if (state.message_.substr(0, 6) == "Error:")
            {
                frame += "\x1b[37m";
                frame += "\x1b[41m";
            }
            if (state.message_.substr(0, 8) == "WARNING:")
            {
                frame += "\x1b[37m";
                frame += "\x1b[43m";
            }
        }
        else if (state.mode_ == Mode::Normal) // 这是未执行的指令，显示在右下角
        {
            if (state.Recording_ != "")
                bottom = state.Recording_;
            bottomvalue = window.GetBottomValue();
            bottomvalue += "    ";
        }
        else if (state.mode_ == Mode::Insert)
        {
            bottom = "-- INSERT --";
            if (state.Recording_ != "")
                bottom += " " + state.Recording_;
        }
        else if (state.mode_ == Mode::Visual)
        {
            bottom = "-- VISUAL --";
            if (state.Recording_ != "")
                bottom += " " + state.Recording_;
            bottomvalue = window.GetBottomValue();
            bottomvalue += "    ";
        }
        else if (state.mode_ == Mode::Visual_Line)
        {
            bottom = "-- VISUAL LINE --";
            if (state.Recording_ != "")
                bottom += " " + state.Recording_;
            bottomvalue = window.GetBottomValue();
            bottomvalue += "    ";
        }
        AppendClearedLine(frame, bottom, width, false);
        frame += "\x1b[0m";
        // 这里是提示3中的部分
        // 我们只会在CommandMode的时候检查一下底部的命令内容,在NormalMode不会看底部,所以message你可以随意写

        std::size_t cursor_row{0};
        std::size_t cursor_column{0};

        // 计算cursor_row和cursor_column即可
        if (state.mode_ == Mode::CommandLine)
        {
            cursor_row = viewport.rows_;
            cursor_column = 1 + BufferColumnToRenderColumn(state.command_, state.command_cursor);
            show_cursor_pos = std::to_string(BufferColumnToRenderColumn(state.command_, state.command_cursor) + 1) + "  "; // 进行展示光标位置
        }
        else
        {
            cursor_row = cursor_pos.row_ - viewport.top_;
            cursor_column = BufferColumnToRenderColumn(buffer.GetLineAt(cursor_pos.row_), cursor_pos.column_) - viewport.left_;
            show_cursor_pos = std::to_string(cursor_pos.row_ + 1) + "," + std::to_string(cursor_column + viewport.left_ + 1) + "  "; // 进行展示光标位置
        }

        if (bottom.size() + show_cursor_pos.size() + bottomvalue.size() >= viewport.columns_)
        {
            if (bottom.size() + 4 + show_cursor_pos.size() < viewport.columns_)
                AppendClearedLine(frame, "    " + show_cursor_pos, width, false);
        }
        else // 处理尾部对齐
        {
            size_t spacenum = viewport.columns_ - bottom.size() - show_cursor_pos.size() - bottomvalue.size();
            while (spacenum--)
                bottomvalue = " " + bottomvalue;
            frame += "\x1b[94m";
            AppendClearedLine(frame, bottomvalue + show_cursor_pos, width, false);
            frame += "\x1b[39m";
        }

        frame += CursorSequence(cursor_row + 1, cursor_column + 1);
        frame += "\x1b[?25h";
        return frame;
    }

    std::string Renderer::DisplayHighlight(std::vector<std::pair<std::string, Position>> &Highlight, std::string line, Position start_pos, size_t length, std::string Line_Highlight_tag = "") const
    {
        if (Line_Highlight_tag != "")
        {
            return Line_Highlight_tag + line + "\x1b[0m";
        }
        if (Highlight.empty())
        {
            return line;
        }
        std::string Return_string = "";
        const std::string_view temporary_string = line; // 辅助的字符串
        std::vector<size_t> temporary_vector;
        temporary_vector.clear();
        size_t index = 0;
        for (index = 0; index < temporary_string.size(); index++)
        {
            temporary_vector.push_back(0);
        }
        for (index = 0; index < Highlight.size(); index++)
        {
            if (Highlight[index].second.row_ == start_pos.row_ && Highlight[index].second.column_ >= start_pos.column_ && Highlight[index].second.column_ < start_pos.column_ + length) // 高亮字符在显示范围内,vector后面的字符会覆盖前面的
            {
                temporary_vector[Highlight[index].second.column_ - start_pos.column_] = index + 1;
            }
        }
        index = 0;
        for (size_t i = 0; i < temporary_vector.size(); i++)
        {
            if (temporary_vector[i] > 0)
                Return_string += Highlight[temporary_vector[i] - 1].first;
            Return_string += temporary_string[index];
            index++;
            if (temporary_vector[i] > 0 && (i + 1 >= temporary_vector.size() || temporary_vector[i + 1] == 0)) // 下一个字符没有高亮
                Return_string += "\x1b[0m";
        }
        index = 0;
        while (index < Highlight.size()) // 我们处理一下Highlight 去除已经高亮过的，防止重复访问，以节约时间
        {
            if (Highlight[index].second.row_ <= start_pos.row_) // 高亮字符已显示
                Highlight.erase(Highlight.begin() + index);
            else
                index++;
        }
        return Return_string;
    }

    std::string Renderer::ExpandForDisplay(std::string_view line)
    {
        // 从左到右扫描buffer中一整行的实际内容,并扩展到render应该输出的视图
        // 你应该在Render中调用这个函数,并把函数返回的结果按照视口剪切用于Render的某些行
        std::string Return_string = "";
        for (size_t index = 0; index < line.size(); index++)
        {
            if (line[index] != '\t')
                Return_string += line[index];
            else
            {
                size_t space_sum{NextScreenColumn(Return_string.size(), '\t')};
                while (space_sum > Return_string.size())
                    Return_string += " ";
            }
        }
        return Return_string;
    }
} // namespace sjtu
