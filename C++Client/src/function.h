#include "include/elements/parse.h"
#include "include/seedcup.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
using namespace std;
using namespace seedcup;

#define ON_DEBUG 1

ActionType act(GameMsg& msg, SeedCup& server, int currentNum);