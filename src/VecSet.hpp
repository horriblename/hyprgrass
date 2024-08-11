#include <vector>

// Probably not compatible with move semantics, I really don't know
template <class T> class VecSet {
  public:
    bool has(const T x);

    // returns whether or not it already exists prior to insert
    bool insert(const T x);

    // returns whether or not x was found in the set
    bool remove(const T x);

    void clear();

  private:
    std::vector<T> set;
};
