#include "include/elements/parse.h"
#include "include/seedcup.h"
#include "function.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
using namespace std;
using namespace seedcup;

int respondAct(GameMsg& msg, SeedCup& server)
{
    ActionType action = SILENT;
    auto& player = msg.players[msg.player_id];
    for (int count = 0; count < player->speed; count++)
    {
        action = act(msg, server, count);
        server.TakeAction(action);        
        switch (action)
        {
        case MOVE_UP:
            if (player->x > 0)
                player->x--;
            break;
        case MOVE_DOWN:
            if (player->x < msg.grid.size()-1)
                player->x++;
            break;
        case MOVE_RIGHT:
            if (player->y < msg.grid.size() - 1)
                player->y++;
            break;
        case MOVE_LEFT:
            if (player->y > 0)
                player->y--;
            break;
        case PLACED:
            cout << "This function has no ability now,please check main.cpp respondact\n";
            break;
        default:break;
        }
    }
	return 0;
}


int main() {
  SeedCup seedcup("../config.json","cppsdk");
  int ret = seedcup.Init();
  if (ret != 0) {
    std::cout << seedcup.get_last_error();
    return ret;
  }
  cout << "init client success" << endl;
  seedcup.RegisterCallBack(
      respondAct,


      [](int player_id, const std::vector<std::pair<int, int>> &scores,
         const std::vector<int> &winners) -> int {
        // 打印所有人的分数
        for (int i = 0; i < scores.size(); i++) {
          cout << "[" << scores[i].first << "," << scores[i].second << "] ";
        }
        // 打印获胜者列表
        cout << endl;
        for (int i = 0; i < winners.size(); i++) {
          cout << winners[i] << " ";
        }
        cout << endl;
        return 0;
      });
  ret = seedcup.Run();
  if (ret != 0) {
    std::cout << seedcup.get_last_error();
    return ret;
  }
  return 0;
}
