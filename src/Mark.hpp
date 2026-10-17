#ifndef MINIVIM_MARK_HPP
#define MINIVIM_MARK_HPP

#include <string>
#include <map>

#include "Types.hpp"

namespace sjtu
{
    class Mark
    {
    public:
        void NewMark(char name,Position pos);
    private:
        std::map <char,Position> mark_pos_;
    };

}
#endif // MINIVIM_UNDO_HPP