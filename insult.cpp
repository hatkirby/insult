#include <yaml-cpp/yaml.h>
#include <iostream>
#include <sstream>
#include <verbly.h>
#include <fstream>
#include <twitter.h>
#include <random>
#include <chrono>
#include <thread>
#include <algorithm>
#include "patterner.h"

int main(int argc, char** argv)
{
  if (argc != 2)
  {
    std::cout << "usage: insult [configfile]" << std::endl;
    return -1;
  }

  std::string configfile(argv[1]);
  YAML::Node config = YAML::LoadFile(configfile);

  twitter::auth auth;
  auth.setConsumerKey(config["consumer_key"].as<std::string>());
  auth.setConsumerSecret(config["consumer_secret"].as<std::string>());
  auth.setAccessKey(config["access_key"].as<std::string>());
  auth.setAccessSecret(config["access_secret"].as<std::string>());

  twitter::client client(auth);

  std::random_device randomDevice;
  std::mt19937 rng(randomDevice());

  try
  {
    verbly::database database(config["verbly_datafile"].as<std::string>());
    patterner pgen(config["forms_file"].as<std::string>(), database, rng);

    std::cout << "Starting streaming..." << std::endl;

    twitter::stream userStream(client, [&pgen, &client]
      (const twitter::notification& n) {
        if (n.getType() == twitter::notification::type::tweet)
        {
          if ((!n.getTweet().isRetweet())
            && (n.getTweet().getAuthor() != client.getUser()))
          {
            std::string original = n.getTweet().getText();
            std::string canonical;

            std::transform(std::begin(original), std::end(original),
              std::back_inserter(canonical), [] (char ch)
            {
              return std::tolower(ch);
            });

            if (canonical.find("@teammeanies") != std::string::npos)
            {
              std::string doc =
                n.getTweet().generateReplyPrefill(client.getUser());

              doc += pgen.generate();
              doc.resize(140);

              try
              {
                client.replyToTweet(doc, n.getTweet());
              } catch (const twitter::twitter_error& error)
              {
                std::cout << "Twitter error while tweeting: "
                  << error.what() << std::endl;
              }
            }
          }
        }
      });

    std::this_thread::sleep_for(std::chrono::minutes(1));

    for (;;)
    {
      std::cout << "Generating tweet..." << std::endl;

      std::string action = pgen.generate();
      action.resize(140);

      std::cout << action << std::endl;

      try
      {
        client.updateStatus(action);

        std::cout << "Tweeted!" << std::endl;
      } catch (const twitter::twitter_error& e)
      {
        std::cout << "Twitter error: " << e.what() << std::endl;
      }

      std::cout << "Waiting..." << std::endl;

      std::this_thread::sleep_for(std::chrono::hours(1));
    }
  } catch (std::invalid_argument& e)
  {
    std::cout << e.what() << std::endl;
    return -1;
  }
}
