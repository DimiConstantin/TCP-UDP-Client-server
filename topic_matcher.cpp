#include "topic_matcher.h"

std::vector<std::string> split_topic(const std::string &s)
{
    char buf[1501];
    memcpy(buf, s.data(), s.size());
    buf[s.size()] = '\0';

    std::vector<std::string> tokens;
    char *token = strtok(buf, "/");
    while (token)
    {
        tokens.emplace_back(token);
        token = strtok(NULL, "/");
    }
    return tokens;
}

bool match_topic(const std::string &topic, const std::string &pattern)
{
    std::vector<std::string> T = split_topic(topic);
    std::vector<std::string> P = split_topic(pattern);

    int t = 0, p = 0;
    int star_p = -1; 
    int star_t = 0;
    int Tn = (int)T.size(), Pn = (int)P.size();

    while (t < Tn)
    {
        if (p < Pn && (P[p] == T[t] || P[p] == "+"))
        {
            ++t;
            ++p;
        }
        else if (p < Pn && P[p] == "*")
        {
            star_p = p++;
            star_t = t;
        }
        else if (star_p != -1)
        {
            p = star_p + 1;
            t = ++star_t;
        }
        else
        {
            return false;
        }
    }
    while (p < Pn && P[p] == "*")
    {
        ++p;
    }
    return p == Pn;
}
