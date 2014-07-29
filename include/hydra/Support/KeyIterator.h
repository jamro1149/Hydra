#ifndef HYDRA_KEY_ITERATOR_H
#define HYDRA_KEY_ITERATOR_H

// this iterator allows one to iterate through the keys of an std::map, or
// other map-like container

namespace hydra {
  template <typename base_iter> class key_iterator : public base_iter {
  public:
    key_iterator(const base_iter &other) : base_iter{ other } {}
    auto operator*() -> decltype(base_iter::operator*().first) {
      return base_iter::operator*().first;
    }
  };
}

#endif
