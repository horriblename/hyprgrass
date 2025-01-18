#include "VecSet.hpp"

template <class T> bool VecSet<T>::has(const T x) {
    for (const auto& i : this->set) {
        if (i == x) {
            return true;
        }
    }

    return false;
}

template <class T> bool VecSet<T>::insert(const T x) {
    if (this->has(x)) {
        return true;
    }

    this->set.push_back(x);
    return false;
}

template <class T> bool VecSet<T>::remove(const T x) {
    for (size_t i = 0; i < this->set.size(); i++) {
        if (this->set[i] == x) {
            if (i != this->set.size() - 1) {
                this->set[i] = this->set.back();
            }

            this->set.pop_back();
            return true;
        }
    }

    return false;
}

template <class T> void VecSet<T>::clear() {
    this->set.clear();
}

template <class T> const std::vector<T>& VecSet<T>::all() const {
    return this->set;
}
