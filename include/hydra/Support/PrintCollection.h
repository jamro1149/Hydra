#include <string>
#include "llvm/Support/raw_ostream.h"

template <typename Container>
void printCollection(const Container &c, llvm::raw_ostream &O,
                     const std::string &s) {
  O << s << " are ";
  for (const auto &v : c) {
    O << v << ", ";
  }
  O << "\n";
}
