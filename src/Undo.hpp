#ifndef MINIVIM_UNDO_HPP
#define MINIVIM_UNDO_HPP

#include <filesystem>
#include <string>

#include "Buffer.hpp"
#include "Types.hpp"
#include "Command.hpp"
#include "Window.hpp"

namespace sjtu
{
    class UnDo_ReDo // 由于撤销重做处理起来比较hard，于是放主处理部分
    {
    public:
        void Get_Do_Information(Buffer& buffer, Window &window);
        const std::string &GetUndoLineAt(std::size_t row) const;
        const size_t GetUndoLineCount() const;
        const Position& GetUndoCursor() const;
        void Renew_Position(Position pos={0,0});
        void ApplyUndo(Buffer& buffer,Window& window);
    
    private:
        Position pos_;
        std::vector<std::string> Undo_lines_;
        std::vector<int> ApplyKind_; // 对于一些简单操作，我们可以试图在后续进行优化，而不是每次都存储全文
        EditorAction ActionWay_;
    };

}
#endif // MINIVIM_UNDO_HPP