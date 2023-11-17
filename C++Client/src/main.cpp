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
    auto& player = msg.players[msg.player_id];
    ActionType action = SILENT;
    vector<ActionType> actionArray(player->speed, SILENT);
    //vector<Bomb> bombTemp(player->speed);
    //std::shared_ptr<Bomb> tempBombptr;
    for (int count = 0; count < player->speed; count++)
    {
        action = act(msg, server, count); 
        msg.grid[player->x][player->y].player_ids.erase(player->player_id);
        actionArray[count] = action;
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
            //cout << "Warning,you placed a bomb\n";
            if ((msg.grid[player->x][player->y].bomb_id == -1) && (player->bomb_now_num < player->bomb_max_num))
            {
                std::shared_ptr<Bomb> bombtemp = std::make_shared<Bomb>();
                bombtemp->bomb_id = -100 + count;
                bombtemp->last_place_round = msg.game_round - 1;//ATTENTION:  WARNING this 
                bombtemp->x = player->x;
                bombtemp->y = player->y;
                bombtemp->player_id = player->player_id;
                bombtemp->bomb_range = player->bomb_range;
                player->bomb_now_num++;
                msg.grid[player->x][player->y].bomb_id = bombtemp->bomb_id;
                msg.bombs.insert({ bombtemp->bomb_id, bombtemp });
            }
            break;
        default:
            break;
        }
        msg.grid[player->x][player->y].player_ids.insert(player->player_id);
    }
    server.TakeAction(actionArray);
    return 0;
}



int main() {
  SeedCup seedcup("../config.json","R92bSdWJxcIOXEeDgSmgIA==");
  int ret = seedcup.Init();
  if (ret != 0) {
    std::cout << seedcup.get_last_error();
    return ret;
  }
  //cout << "init client success" << endl;
  seedcup.RegisterCallBack(
      respondAct,


      [](int player_id, const std::vector<std::pair<int, int>> &scores,
         const std::vector<int> &winners) -> int {
        // 打印所有人的分数
        for (int i = 0; i < scores.size(); i++) {
          //cout << "[" << scores[i].first << "," << scores[i].second << "] ";
        }
        // 打印获胜者列表
        //cout << endl;
        for (int i = 0; i < winners.size(); i++) {
          //cout << winners[i] << " ";
        }
        //cout << endl;
        return 0;
      });
  ret = seedcup.Run();
  if (ret != 0) {
    std::cout << seedcup.get_last_error();
    return ret;
  }
  return 0;
}
