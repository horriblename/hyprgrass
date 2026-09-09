#pragma once

#include <utility>
#include <vector>

// Probably not compatible with move semantics, I really don't know
template <class T> class VecSet {
  public:
    VecSet() {}

    VecSet(const VecSet&)           = delete;
    VecSet operator=(const VecSet&) = delete;
    VecSet(VecSet&&)                = delete;
    VecSet operator=(VecSet&&)      = delete;

    bool has(const T x) {
        for (const auto& i : this->set) {
            if (i == x) {
                return true;
            }
        }

        return false;
    }

    // returns whether or not it already exists prior to insert
    bool insert(const T x) {
        if (this->has(x)) {
            return true;
        }

        this->set.push_back(x);
        return false;
    }

    // returns whether or not x was found in the set
    bool remove(const T x) {
        for (size_t i = 0; i < this->set.size(); i++) {
            if (this->set[i] == x) {
                std::swap(this->set[i], this->set.back());
                this->set.pop_back();
                return true;
            }
        }

        return false;
    }

    void clear() {
        this->set.clear();
    }

    const std::vector<T>& all() const {
        return this->set;
    }

  private:
    std::vector<T> set;
};
