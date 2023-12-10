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

#include <unistd.h>
using namespace std;
using namespace seedcup;



int simulateServer(GameMsg& msg,ActionType action,int gameRoundCount)//return 1 for push  2 for place 3 for move 4 for get item 0 for fail -1 for silent 
{
    if(action== SILENT)
		return -1;
    auto& player = msg.players[msg.player_id];
    auto& grid = msg.grid;
    if (action == PLACED)
    {
        if((grid[player->x][player->y].bomb_id == -1)&&(player->bomb_now_num < player->bomb_max_num))
		{
			std::shared_ptr<Bomb> bombtemp = std::make_shared<Bomb>();
            bombtemp->bomb_id = -100 + gameRoundCount;
			bombtemp->last_place_round = msg.game_round - 1;//ATTENTION:  WARNING this 
            //bombtemp->last_place_round = msg.game_round;//ATTENTION:  WARNING this  SO STRANGE
			bombtemp->x = player->x;
			bombtemp->y = player->y;
			bombtemp->player_id = player->player_id;
			bombtemp->bomb_range = player->bomb_range;
            bombtemp->bomb_status = BOMB_SILENT;
			player->bomb_now_num++;
			grid[player->x][player->y].bomb_id = bombtemp->bomb_id;
			msg.bombs.insert({ bombtemp->bomb_id, bombtemp });
			return 2;
		}
		else
		{
#if ON_DEBUG
            printf("[Debug] Bomb placed failed at %d,%d\n",player->x,player->y);
            if (grid[player->x][player->y].bomb_id != -1)
                printf("[Debug] There is bomb Id %d\n", grid[player->x][player->y].bomb_id);
            if (player->bomb_now_num >= player->bomb_max_num)
                printf("[Debug] Bomb num exceed\n");
#endif
			return 0;
		}
    }
    else
    {
        int targetx, targety, currentx, currenty;
        currentx = player->x;
        currenty = player->y;
        targetx = currentx;
        targety = currenty;
        switch (action)
        {
        case MOVE_UP:targetx--;
            break;
        case MOVE_DOWN:targetx++;
            break;
        case MOVE_RIGHT:targety++;
            break;
        case MOVE_LEFT:targety--;
            break;
        }
        if (targetx < 0 || targetx >= grid.size() || targety < 0 || targety >= grid.size())
        {
#if ON_DEBUG
            printf("[Debug] Move failed onto %d,%d\n", targetx, targety);
            printf("[Debug] Move out of range\n");
#endif
            return 0;
        }
        if (grid[targetx][targety].block_id != -1)
        {
#if ON_DEBUG
            printf("[Debug] Move failed onto %d,%d\n", targetx, targety);
            printf("[Debug] Move into block\n");
#endif
            return 0;
        }
        if (grid[targetx][targety].bomb_id != -1)
        {
            msg.bombs[grid[targetx][targety].bomb_id]->bomb_status = (BombStatus)action;
            return 1;
        }
        if (grid[targetx][targety].item != NULLITEM)
        {
            switch (grid[targetx][targety].item)
            {
                case HP:
                    player->hp += (player->hp == 3) ? 0 : 1;
				    break;
                case INVINCIBLE:
                    player->invincible_time = 15;
					break;
                case SHIELD:
                    player->shield_time = 30;
                    break;
                case BOMB_RANGE:
					player->bomb_range++;
					break;
                case BOMB_NUM:
                    player->bomb_max_num++;
                    break;
                case SPEED:
                    player->speed++;
                    break;
                case GLOVES:
                    player->has_gloves = true;
                    break;

            }
            grid[targetx][targety].item = NULLITEM;
            player->score += 100;
            msg.grid[player->x][player->y].player_ids.erase(player->player_id);
            player->x = targetx;
            player->y = targety;
            msg.grid[player->x][player->y].player_ids.insert(player->player_id);
            return 4;
        }
        else
        {
            msg.grid[player->x][player->y].player_ids.erase(player->player_id);
            player->x = targetx;
            player->y = targety;
            msg.grid[player->x][player->y].player_ids.insert(player->player_id);
            return 3;
        }
    }
    return 0;
}


//Virtual Bomb placed id start by -100
int respondAct(GameMsg& msg, SeedCup& server)
{
#if ON_DEBUG
    printf("[Debug] The round is %d\n", msg.game_round);
#endif
    auto& player = msg.players[msg.player_id];
    ActionType action = SILENT;
    vector<ActionType> actionTempArray(player->speed + 10, SILENT);//for enough space

    for (int count = 0; count < player->speed; count++)
    {
        action = act(msg, server, count); 
        simulateServer(msg, action, count);//There can be optimize for illegal move
        actionTempArray[count] = action;
    }
    //speed may change during the current round
    vector<ActionType> actionArray(player->speed, SILENT);
    copy(actionTempArray.begin(), actionTempArray.begin() + player->speed, actionArray.begin());
    //usleep(20000);
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
