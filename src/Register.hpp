#ifndef MINIVIM_REGISTER_HPP
#define MINIVIM_REGISTER_HPP

#include <vector>
#include <map>

#include "Window.hpp"
#include "Buffer.hpp"
#include "Types.hpp"
#include "Key.hpp"
#include "Command.hpp"

// 处理寄存器相关逻辑，如复制黏贴
namespace sjtu
{
    enum class RegKind
    {
        MotionReg,
        charReg,
        lineReg,
    };

    class Register
    {
    private:
        std::vector<KeyEvent> reg1_{}; // 行为寄存器
        std::vector<std::string> reg2_{}; // 字符寄存器
        RegKind reg_kind{0};//1 motion 2 char 3 line
        size_t reg1_ptr{0};
        void set_reg_kind(RegKind kind);
        void Copy_into_Reg(const Buffer &buffer,Position start_pos,Position end_pos);
        void Copy_into_Reg(const Buffer &buffer,size_t row_up,size_t row_down);
        void paste_outo_buffer(Buffer &buffer,Window &window,Position start_pos,size_t count) const;
    public:
        
        void Execute_paste(Buffer &buffer,Window &window,EditorAction editoraction);
        bool Execute_copy(Buffer &buffer,Window &window,EditorAction editoraction);
        bool Copy_into_Reg(const std::vector<std::string> &lines);
        void Record_key(KeyEvent key);
        KeyEvent Get_key();
        bool Finished();
        void Reset_ptr();
        void ClearRecord();
        void Remove_back();
        void addRegister(Register &&register1);
    };


} // namespace sjtu
#endif