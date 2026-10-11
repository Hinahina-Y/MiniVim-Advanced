/*
Window.hpp
Window保存光标位置和正文的可见区域,根据Buffer的行数和行长限制移动范围.
移动只改变光标和视口,文件内容的修改由Buffer完成.
*/
#ifndef MINIVIM_WINDOW_HPP
#define MINIVIM_WINDOW_HPP

#include "Buffer.hpp"
#include "Types.hpp"
#include <optional>

namespace sjtu {

class Window {

public:
    void Resize(ScreenSize terminal_size);

    void ApplyMotion(const Buffer& buffer, Motion motion);

    void ApplyMoveword(const Buffer& buffer, Motion motion,bool rule_type);

    void EnsureCursorVisible(const Buffer& buffer);

    void Apply_d_Delete(Buffer &buffer,Motion motion);

    void ApplyDelete(Buffer &buffer,Motion motion);

    void RenewCount(std::optional<size_t> expected_count);//更新window里的剩余count值
    void Renew_Lastparser(char lastparser);//记录上一个按键
    void Renew_Lastparser();//清空按键记录
    std::string GetBottomValue() const;
    std::optional<std::string> GetLastparser() const;

    //allow_line_end为true时允许停在最后一个字符之后,供插入模式使用
    void SetCursor(const Buffer& buffer, Position position, bool allow_line_end);
    const Position& GetCursor() const ;
    const Viewport& GetViewport() const ;

private:
    void MoveLeft(const Buffer& buffer, size_t count);
    void MoveRight(const Buffer& buffer, size_t count);
    void MoveUp(const Buffer& buffer, size_t count);
    void MoveDown(const Buffer& buffer, size_t count);

    Position cursor_{}; //Buffer中的光标位置
    Viewport viewport_{}; //正文可见区域及其滚动偏移
    size_t desired_column_{0}; //上下移动时希望保持的显示列,经过短行时也保留这个目标

    std::optional<size_t> remaining_count_{};//记录留存的数字
    std::optional<std::string>  Last_parser_{};//记录之前的按键
};

} // namespace sjtu

#endif // MINIVIM_WINDOW_HPP
