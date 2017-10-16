#include "patterner.h"
#include <fstream>
#include <stdexcept>
#include <algorithm>

patterner::patterner(
  std::string datapath,
  verbly::database& data,
  std::mt19937& rng) :
    data_(data),
    rng_(rng)
{
  std::ifstream datafile(datapath);
  if (!datafile.is_open())
  {
    throw std::invalid_argument("Could not find datafile");
  }

  bool newgroup = true;
  std::string line;
  std::list<std::string> curgroups;
  while (getline(datafile, line))
  {
    if (line.back() == '\r')
    {
      line.pop_back();
    }

    if (newgroup)
    {
      curgroups = verbly::split<std::list<std::string>>(line, ",");
      newgroup = false;
    } else {
      if (line.empty())
      {
        newgroup = true;
      } else {
        for (std::string curgroup : curgroups)
        {
          groups_[curgroup].push_back(line);
        }
      }
    }
  }
}

std::string patterner::generate()
{
  std::string action = "{MAIN}";
  int tknloc;
  while ((tknloc = action.find("{")) != std::string::npos)
  {
    std::string token = action.substr(tknloc+1, action.find("}")-tknloc-1);
    std::string modifier;
    int modloc;
    if ((modloc = token.find(":")) != std::string::npos)
    {
      modifier = token.substr(modloc+1);
      token = token.substr(0, modloc);
    }

    std::string canontkn;
    std::transform(std::begin(token), std::end(token),
        std::back_inserter(canontkn), [] (char ch) {
      return std::toupper(ch);
    });

    std::string result;
    if (canontkn == "WORD")
    {
      result = data_.words(
        (verbly::word::forms(verbly::inflection::base) %=
          (verbly::form::complexity == 1)
            && (verbly::form::length == 4)
            && (verbly::form::proper == false)
            && (verbly::pronunciation::numOfSyllables == 1))
        && !(verbly::word::usageDomains %=
          (verbly::notion::wnid == 106718862))) // Blacklist ethnic slurs
        .first().getBaseForm().getText();
    } else if (canontkn == "\\N")
    {
      result = "\n";
    } else {
      auto group = groups_[canontkn];
      std::uniform_int_distribution<int> groupdist(0, group.size()-1);
      int groupind = groupdist(rng_);
      result = group[groupind];
    }

    if (modifier == "indefinite")
    {
      if ((result.length() > 1) && (isupper(result[0])) && (isupper(result[1])))
      {
        result = "an " + result;
      } else if ((result[0] == 'a') || (result[0] == 'e') || (result[0] == 'i') || (result[0] == 'o') || (result[0] == 'u'))
      {
        result = "an " + result;
      } else {
        result = "a " + result;
      }
    }

    std::string finalresult;
    if (islower(token[0]))
    {
      std::transform(std::begin(result), std::end(result), std::back_inserter(finalresult), [] (char ch) {
        return std::tolower(ch);
      });
    } else if (isupper(token[0]) && !isupper(token[1]))
    {
      auto words = verbly::split<std::list<std::string>>(result, " ");
      for (auto& word : words)
      {
        if (word[0] == '{')
        {
          word[1] = std::toupper(word[1]);

          for (int k=2; k<word.length(); k++)
          {
            if (std::isalpha(word[k]))
            {
              word[k] = std::tolower(word[k]);
            }
          }
        } else {
          word[0] = std::toupper(word[0]);
        }
      }

      finalresult = verbly::implode(std::begin(words), std::end(words), " ");
    } else {
      finalresult = result;
    }

    action.replace(tknloc, action.find("}")-tknloc+1, finalresult);
  }

  return action;
}
