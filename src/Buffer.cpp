#include <fstream>

#include "Buffer.hpp"

namespace sjtu
{

    Buffer::Buffer(const std::filesystem::path &path)
    {
        lines_.clear();
        if (path.empty())
        {
            lines_.push_back("");
            return;
        }
        std::string line;
        std::ifstream in(path); // 读取文件，由ai建议我这么写
        while (std::getline(in, line))
        {
            if (!line.empty() && line.back() == '\r') // 防止Windows文件带换行\r\n
            {
                line.pop_back();
            }
            lines_.push_back(line);
        }
        path_ = path;
        // 从path指向的文件构造Buffer,你需要打开文件并且把文件内容填充进Buffer,并正确初始化一些状态.
        // 注意path可能为空的边界情况
    }

    Buffer::Buffer(std::vector<std::string> lines, std::filesystem::path path)
    {
        // 暂时不知道有什么作用
    }

    void Buffer::copyBuffer(const std::vector<std::string> &lines)
    {
        lines_ = lines;
    }

    std::size_t Buffer::GetLineCount() const
    {
        // 返回文件行数
        return lines_.size();
    }

    const std::string &Buffer::GetLineAt(std::size_t row) const
    {
        // 返回第row行的内容
        return lines_[row];
    }

    std::string Buffer::GetDisplayName() const
    {
        // 返回文件名,若是新文件,返回"[No Name]"
        if (path_.empty())
        {
            return "[No Name]";
        }
        return path_.filename().string();
    }

    bool Buffer::IsModified() const
    {
        // 返回文件和上次保存比起来是否被修改过
        if (path_.empty())
        {
            return !(lines_.size() == 1 && lines_[0] == "");
        }
        std::string line;
        std::ifstream in(path_); // 读取文件，由ai建议我这么写
        int lines_index = 0;
        while (std::getline(in, line))
        {
            if (!line.empty() && line.back() == '\r') // 防止Windows文件带换行\r\n
            {
                line.pop_back();
            }
            if (lines_index >= lines_.size() || lines_[lines_index] != line)
            {
                return true;
            }
            lines_index++;
        }
        if (lines_index != lines_.size())
        {
            if (lines_index + 1 == lines_.size() && lines_[lines_index] == "")
            {
                return false;
            }
            return true;
        }
        return false;
    }

    void Buffer::InsertCharacter(std::size_t row, std::size_t column, char value)
    {
        if (row >= lines_.size() || column > lines_[row].size() + 1)
        {
            // 越界,暂时不知道如何处理
        }
        lines_[row].insert(lines_[row].begin() + column, value);
        // 在第row行第col列插入一个value, 注意越界检查
        return;
    }

    void Buffer::EraseCharacter(std::size_t row, std::size_t column) // 在这个位置按下删除
    {
        if (row >= lines_.size() || column > lines_[row].size())
        {
            throw std::runtime_error("越界了！");
            return;
            // 越界
        }
        lines_[row].erase(column - 1, 1);
    }

    void Buffer::EraseLines(std::size_t row_up, std::size_t row_down) // 删除行[row_up,row_down)
    {
        if (row_down <= row_up || row_up >= lines_.size())
        {
            return;
        }
        if (row_down > lines_.size())
        {
            row_down = lines_.size();
        }
        if (row_up < 0)
        {
            row_up = 0;
        }
        lines_.erase(lines_.begin() + row_up, lines_.begin() + row_down);
        if (lines_.empty())
        {
            lines_.push_back("");
        }
    }

    void Buffer::EraseCharacters(std::size_t row, std::size_t column_left, std::size_t column_right)
    {
        if (column_right <= column_left && column_left >= lines_[row].size())
        {
            return;
        }
        if (column_right > lines_[row].size())
        {
            column_right = lines_[row].size();
        }
        if (column_left < 0)
        {
            column_left = 0;
        }
        lines_[row].erase(lines_[row].begin() + column_left, lines_[row].begin() + column_right);
    }

    // 删除[left,right)的字母,这里left，right是两个坐标，中间字母指的是向右行走（允许跨行会遇到的字母).
    void Buffer::EraseCharacters(std::size_t row_left, std::size_t column_left, std::size_t row_right, std::size_t column_right)
    {
        bool crossline = false;
        if (row_left < 0)
        {
            row_left = 0;
            column_left = 0;
        }
        if (row_right >= lines_.size())
        {
            row_right = lines_.size() - 1;
            column_right = lines_[row_right].size() + 1;
        }
        if (row_left > row_right || (row_left == row_right && column_right <= column_left))
        {
            std::swap(row_left, row_right);
            std::swap(column_left, column_right);
        }
        if (column_left >= lines_[row_left].size())
        {
            return;
        }
        if (column_right > lines_[row_right].size())
        {
            column_right = lines_[row_right].size();
            crossline = true;
        }
        if (column_left < 0)
        {
            column_left = 0;
        }
        if (row_left == row_right)
        {

            lines_[row_left].erase(lines_[row_left].begin() + column_left, lines_[row_left].begin() + column_right);
            if (crossline)
                JoinLine(row_left);
        }
        else
        {
            lines_[row_left].erase(lines_[row_left].begin() + column_left, lines_[row_left].end());
            EraseLines(row_left + 1, row_right + 1);
            lines_[row_right].erase(lines_[row_left].begin(), lines_[row_left].begin() + column_right);
            JoinLine(row_left);
            if (crossline)
                JoinLine(row_left);
        }
    }

    void Buffer::ReplaceCharacter(std::size_t row, std::size_t column, char value)
    {
        lines_[row][column] = value;
    }

    void Buffer::SplitLine(std::size_t row, std::size_t column)
    {
        // 在第row行第col列分割,即在此处敲了回车键
        std::string line_new = lines_[row].substr(column);
        lines_[row] = lines_[row].substr(0, column);
        lines_.insert(lines_.begin() + row + 1, line_new);
    }

    void Buffer::JoinLine(std::size_t row)
    {
        // 把第row + 1行合并进第row行
        if (row + 1 >= lines_.size())
        {
            return;
        }
        lines_[row] += lines_[row + 1];
        lines_.erase(lines_.begin() + row + 1);
    }

    void Buffer::Achieve_LineUndo(std::size_t row, std::string line)
    {
        lines_[row] = line;
    }

    void Buffer::Save()
    {
        // 把文件内容保存, 直接调用WriteTo方法
        WriteTo(path_);
    }

    void Buffer::SaveAs(const std::filesystem::path &path)
    {
        WriteTo(path);
        path_ = path;
    }

    void Buffer::WriteTo(const std::filesystem::path &path) const
    {
        // 实际将缓冲区中的内容写入path指向的文件中
        if (path.empty())
        {
            throw std::runtime_error("Error:No File Name.");
            return;
        }
        std::ofstream out(path); // 写入文件，由ai建议我这么写
        if (!out)
        {
            throw std::runtime_error("Error:Failed to open file.");
            return;
        }
        for (size_t i = 0; i + 1 < lines_.size(); i++)
        {
            out << lines_[i] << '\n';
        }
        out << lines_[lines_.size() - 1];
        if (lines_[lines_.size() - 1] != "")
        {
            out << '\n';
        }
        if (!out)
        {
            throw std::runtime_error("Error:File Error.");
        }
        return;
    }

} // namespace sjtu
