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

ActionType act(GameMsg& msg, SeedCup& server, int currentNum);