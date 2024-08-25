#include <vector>

#include <hyprland/src/helpers/memory/Memory.hpp>
#include <hyprland/src/protocols/core/Seat.hpp>

// Probably not compatible with move semantics, I really don't know
template <class T> class VecSet {
  public:
    bool has(const T x);

    // returns whether or not it already exists prior to insert
    bool insert(const T x);

    // returns whether or not x was found in the set
    bool remove(const T x);

    void clear();

    const std::vector<T>& all() const;

  private:
    std::vector<T> set;
};

// For some reason if I put this anywhere else the symbols don't get compiled in.
// C++ is a beauty
template class VecSet<Hyprutils::Memory::CWeakPointer<CWLTouchResource>>;
