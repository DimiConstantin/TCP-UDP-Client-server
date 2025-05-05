#pragma once

#include <iostream>
#include <string.h>
#include <string>
#include <vector>

std::vector<std::string> split_topic(const std::string &s);
bool match_topic(const std::string &topic, const std::string &pattern);