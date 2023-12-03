#ifndef __FANSHUTOU_NONCOPYABLE_H__
#define __FANSHUTOU_NONCOPYABLE_H__

namespace fst {

class Noncopyable {
public:
    Noncopyable() = default;

    ~Noncopyable() = default;

    Noncopyable(const Noncopyable&) = delete;

    Noncopyable& operator=(const Noncopyable&) = delete;
};

}

#endif
