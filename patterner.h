#ifndef PATTERNER_H_AB6883F5
#define PATTERNER_H_AB6883F5

#include <verbly.h>
#include <random>

class patterner {
public:

  patterner(std::string datafile, verbly::database& data, std::mt19937& rng);

  std::string generate();

private:

  std::map<std::string, std::vector<std::string>> groups_;
  verbly::database& data_;
  std::mt19937& rng_;
};

#endif /* end of include guard: PATTERNER_H_AB6883F5 */
