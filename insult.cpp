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

const auto QUEUE_TIMEOUT = std::chrono::minutes(1);
const auto POLL_TIMEOUT = std::chrono::minutes(5);
const auto GEN_TIMEOUT = std::chrono::hours(1);

int main(int argc, char** argv)
{
  if (argc != 2)
  {
    std::cout << "usage: insult [configfile]" << std::endl;
    return -1;
  }

  std::string configfile(argv[1]);
  YAML::Node config = YAML::LoadFile(configfile);

  twitter::auth auth(
    config["consumer_key"].as<std::string>(),
    config["consumer_secret"].as<std::string>(),
    config["access_key"].as<std::string>(),
    config["access_secret"].as<std::string>());

  twitter::client client(auth);

  std::random_device randomDevice;
  std::mt19937 rng(randomDevice());

  try
  {
    std::set<twitter::user_id> blocks = client.getBlocks();

    verbly::database database(config["verbly_datafile"].as<std::string>());
    patterner pgen(config["forms_file"].as<std::string>(), database, rng);

    std::list<std::tuple<std::string, bool, twitter::tweet_id>> postQueue;

    auto startedTime = std::chrono::system_clock::now();

    auto queueTimer = std::chrono::system_clock::now();
    auto pollTimer = std::chrono::system_clock::now();
    auto genTimer = std::chrono::system_clock::now();

    for (;;)
    {
      auto currentTime = std::chrono::system_clock::now();

      if (currentTime >= genTimer)
      {
        std::string doc = pgen.generate();
        doc.resize(140);

        postQueue.emplace_back(std::move(doc), false, 0);

        genTimer = currentTime + GEN_TIMEOUT;
      }

      if (currentTime >= pollTimer)
      {
        pollTimer = currentTime;

        try
        {
          std::list<twitter::tweet> newTweets =
            client.getMentionsTimeline().poll();

          for (const twitter::tweet& tweet : newTweets)
          {
            auto createdTime =
              std::chrono::system_clock::from_time_t(tweet.getCreatedAt());

            if (
              // Ignore tweets from before the bot started up
              createdTime > startedTime
              // Ignore retweets
              && !tweet.isRetweet()
              // Ignore tweets from yourself
              && tweet.getAuthor() != client.getUser()
              // Ignore tweets from blocked users
              && !blocks.count(tweet.getAuthor().getID()))
            {
              std::string original = tweet.getText();
              std::string canonical;

              std::transform(
                std::begin(original),
                std::end(original),
                std::back_inserter(canonical),
                [] (char ch)
                {
                  return std::tolower(ch);
                });

              if (canonical.find("@teammeanies") != std::string::npos)
              {
                std::string doc = tweet.generateReplyPrefill(client.getUser());
                doc += pgen.generate();
                doc.resize(140);

                postQueue.emplace_back(std::move(doc), true, tweet.getID());
              }
            }
          }
        } catch (const twitter::rate_limit_exceeded&)
        {
          // Wait out the rate limit (10 minutes here and 5 below = 15).
          pollTimer += std::chrono::minutes(10);
        } catch (const twitter::twitter_error& e)
        {
          std::cout << "Twitter error while polling: " << e.what() << std::endl;
        }

        pollTimer += std::chrono::minutes(POLL_TIMEOUT);
      }

      if ((currentTime >= queueTimer) && (!postQueue.empty()))
      {
        auto post = postQueue.front();
        postQueue.pop_front();

        std::cout << std::get<0>(post) << std::endl;

        try
        {
          if (std::get<1>(post))
          {
            client.replyToTweet(std::get<0>(post), std::get<2>(post));
          } else {
            client.updateStatus(std::get<0>(post));
          }
        } catch (const twitter::twitter_error& error)
        {
          std::cout << "Twitter error while tweeting: " << error.what()
            << std::endl;
        }

        queueTimer = currentTime + std::chrono::minutes(QUEUE_TIMEOUT);
      }

      auto soonestTimer = genTimer;

      if (pollTimer < soonestTimer)
      {
        soonestTimer = pollTimer;
      }

      if ((queueTimer < soonestTimer) && (!postQueue.empty()))
      {
        soonestTimer = queueTimer;
      }

      int waitlen =
        std::chrono::duration_cast<std::chrono::seconds>(
          soonestTimer - currentTime).count();

      if (waitlen == 1)
      {
        std::cout << "Sleeping for 1 second..." << std::endl;
      } else if (waitlen < 60)
      {
        std::cout << "Sleeping for " << waitlen << " seconds..." << std::endl;
      } else if (waitlen == 60)
      {
        std::cout << "Sleeping for 1 minute..." << std::endl;
      } else if (waitlen < 60*60)
      {
        std::cout << "Sleeping for " << (waitlen/60) << " minutes..."
          << std::endl;
      } else if (waitlen == 60*60)
      {
        std::cout << "Sleeping for 1 hour..." << std::endl;
      } else if (waitlen < 60*60*24)
      {
        std::cout << "Sleeping for " << (waitlen/60/60) << " hours..."
          << std::endl;
      } else if (waitlen == 60*60*24)
      {
        std::cout << "Sleeping for 1 day..." << std::endl;
      } else {
        std::cout << "Sleeping for " << (waitlen/60/60/24) << " days..."
          << std::endl;
      }

      std::this_thread::sleep_until(soonestTimer);
    }
  } catch (std::invalid_argument& e)
  {
    std::cout << e.what() << std::endl;
    return -1;
  }
}
