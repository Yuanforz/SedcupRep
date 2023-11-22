#include "include/elements/parse.h"
#include "include/seedcup.h"
#include "function.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <queue>
#include <deque>
#include <unordered_map>
#include <vector>
using namespace std;
using namespace seedcup;

#include <termio.h>
#include <stdio.h>
#include <unistd.h>

const int CollectRange = 5;

enum Direction
{
    NULLD = 0,
    LEFT = 1,
    RIGHT = 2,
    UP = 3,
    DOWN = 4,
};

class POINT
{
public:
    POINT(int a = 0, int b = 0)
    {
        x = a;
        y = b;
    }
    POINT& operator=(POINT p)
    {
        if (this != &p)
        {
            this->x = p.x;
            this->y = p.y;
        }
        return *this;
    }
    bool operator==(POINT p)
    {
        return (x == p.x) && (y == p.y);
    }
    bool operator!=(POINT p)
    {
        return (x != p.x) || (y != p.y);
    }
    int  x;
    int  y;
};

//class CostPOINT :public POINT
//{
//public:
//    int currentcost;
//    CostPOINT(int a = 0, int b = 0, int c = 0)
//    {
//		x = a;
//		y = b;
//		currentcost = c;
//	}
//    bool operator < (const CostPOINT& a) const
//    {
//        return x < a.x;
//    }
//};


typedef struct tagPOSITION {
    POINT pos;
    int dangernum = 0;//n means bomb be placed before n
    bool isBomb = 0;
    bool isRechable = 0;
    POINT* parentPoint = nullptr;
    int currentCost = -1;
    //bool isItem = 0;
    //bool isRemovableBlock = 0;
    //bool AvailableDirection[4] = { 0 };
    //bool isNearable = 0;
    //std::shared_ptr<Area> pArea = NULL;
}POSITION;

void printMap(const GameMsg& msg);
void SpreadBomb(vector<vector<POSITION>>& array, GameMsg& msg, std::shared_ptr<Bomb> bombptr);
void FindSafeRegion(vector<vector<POSITION>>& array, GameMsg& msg);
void FindRechableRegion(vector<vector<POSITION>>& array, GameMsg& msg);
inline int calculateCost(int currentx, int currenty, int targetx, int targety);
void CalcCostMap(vector<vector<POSITION>>& array, GameMsg& msg);
POINT FindHiddenPos(vector<vector<POSITION>>& map, std::shared_ptr<seedcup::Player>player);
inline int HamiltonDist(int x1, int y1, int x2, int y2);
seedcup::ActionType FindWayPosition(vector<vector<POSITION>>& map, std::shared_ptr<seedcup::Player>player, POINT& target, POINT* nextStepPosition);
int FindEnemyID(GameMsg& msg);
inline int calcDistance(vector<vector<POSITION>>& map, GameMsg& msg, int targetx, int targety, int placex, int placey);
POINT FindSuitableAttackPosition(vector<vector<POSITION>>& map, GameMsg& msg);
seedcup::ActionType Attack(vector<vector<POSITION>>& map, GameMsg& msg, POINT* nextStepPosition);
POINT FindSuitableMinePosition(vector<vector<POSITION>>& map, GameMsg& msg);
seedcup::ActionType Mine(vector<vector<POSITION>>& map, GameMsg& msg, POINT* nextStepPosition);
int Judgecollectionnecessity(vector<vector<POSITION>>& map,GameMsg& msg);
POINT FindSuitableCollectionPosition(vector<vector<POSITION>>& map, GameMsg& msg);
seedcup::ActionType Collection(vector<vector<POSITION>>& map,GameMsg& msg,POINT* nextStepPosition);

ActionType act(GameMsg& msg, SeedCup& server, int currentNum)
{
  //std::cout.setstate(std::ios_base::failbit);
    auto& player = msg.players[msg.player_id];
    auto& map = msg.grid;
    printMap(msg);
    //static int prestate = 0;
    //int state = SetState(msg, prestate);
    vector<vector<POSITION>> positionmap(msg.grid.size(), vector<POSITION>(msg.grid.size()));
    ActionType returnnum = (ActionType)0;
    //vector<vector<int>> safemap(msg.grid.size(), vector<int>(msg.grid.size(), 0));
    for(int i=0;i<positionmap.size();i++)
        for (int j = 0; j < positionmap.size(); j++)
        {
            positionmap[i][j].pos = POINT(i, j);
		}
    FindRechableRegion(positionmap, msg);
    FindSafeRegion(positionmap, msg);//由于实现原理，请放置在findreachableregion函数之后
    CalcCostMap(positionmap, msg);
    //for (int i = 0; i < msg.grid.size(); i++)
    //{
    //    for (int j = 0; j < msg.grid.size(); j++)
    //        cout << positionmap[i][j].dangernum * (positionmap[i][j].isBomb ? -1 : 1);
    //        //cout << positionmap[i][j].isRechable;
    //    cout << endl;
    //}

    POINT nextStepPosition;
    static int prestate = 0;//1 Attack 2 Mine 3 Run away 4 collect
    static int count = 0;//counting
    if (positionmap[player->x][player->y].dangernum == 0)
    {
        if (positionmap[msg.players[FindEnemyID(msg)]->x][msg.players[FindEnemyID(msg)]->y].isRechable
            && (msg.players[FindEnemyID(msg)]->invincible_time == 0))
        {
            returnnum = Attack(positionmap, msg, &nextStepPosition);
            if (prestate == 1)
                count++;
            else
            {
                prestate = 1;
                count = 0;
            }
            cout << "TRY TO Attack\n";
        }
        else if(Judgecollectionnecessity(positionmap,msg))
        {
            returnnum = Collection(positionmap, msg, &nextStepPosition);
            if (prestate == 4)
                count++;
			else
            {
				prestate=4;
				count=0;
			}
			cout << "TRY TO Collect\n";
            cout << "Collect at" << nextStepPosition.x << " " << nextStepPosition.y << endl; 
		}
        else if (player->bomb_now_num < player->bomb_max_num)
        {
            returnnum = Mine(positionmap, msg, &nextStepPosition);
            if (prestate == 2)
                count++;
            else
            {
                prestate = 2;
                count = 0;
            }
            cout << "TRY TO Mine\n";
            cout << "Mine at" << nextStepPosition.x << " " << nextStepPosition.y << endl;
        }
        else
        {
			cout << "TRY TO MOVE\n";
			POINT hiddenPosition = FindHiddenPos(positionmap, player);
			if (prestate == 3)
				count++;
			else
			{
				prestate = 3;
				count = 0;
			}
			if (hiddenPosition.x < 0)
			{
				cout << "Sorry,point error\n";
				returnnum = SILENT;
			}
			else
			{
				returnnum = FindWayPosition(positionmap, player, hiddenPosition, &nextStepPosition);
                if (positionmap[nextStepPosition.x][nextStepPosition.y].dangernum != 0)
                    returnnum = SILENT;
			}
        }
        if (positionmap[nextStepPosition.x][nextStepPosition.y].dangernum != 0)
        {
            cout << "FORCING OUT\n";
            if (prestate == 3)
                count++;
            else
            {
                prestate = 3;
                count = 0;
            }
            POINT hiddenPosition = FindHiddenPos(positionmap, player);
            returnnum = FindWayPosition(positionmap, player, hiddenPosition, &nextStepPosition);
            if (positionmap[nextStepPosition.x][nextStepPosition.y].dangernum != 0)
                returnnum = SILENT;
        }
    }
    else
    {
        cout << "Danger!\n";
        POINT hiddenPosition = FindHiddenPos(positionmap, player);
        if (prestate == 3)
            count++;
        else
        {
            prestate = 3;
            count = 0;
        }
        if (hiddenPosition.x < 0)
        {
            cout << "Sorry,point error\n";
            returnnum = SILENT;
        }
        else
        {
            returnnum = FindWayPosition(positionmap, player, hiddenPosition, &nextStepPosition);
            //if (positionmap[nextStepPosition.x][nextStepPsition.y].dangernum > positionmap[player->x][player->y].dangernum)
            //    returnnum = SILENT;
        }
    }

    if (count > 20)
        returnnum = PLACED;

    switch (returnnum)
    {
    case MOVE_UP:cout << "UP\n";
        break;
    case MOVE_DOWN:cout << "DOWN\n";
        break;
    case MOVE_RIGHT:cout << "RIGHT\n";
        break;
    case MOVE_LEFT:cout << "LEFT\n";
        break;
    case PLACED:cout << "BOOOOOOOOOOOM\n";
        break;
    }
    cout << "count is" << count<<endl;
    // 设置随机操作
    return returnnum;
}

void printMap(const GameMsg& msg)
{
    printf("\x1b[H\x1b[2J");
    // 打印自己的id
    std::cout << "self:" << msg.player_id << std::endl;
    // 打印地图
    //cout << endl << endl;
    printf("\n\n");
    auto& grid = msg.grid;
    for (int i = 0; i < msg.grid.size(); i++) {
        for (int j = 0; j < msg.grid.size(); j++) {
            auto& area = grid[i][j];
            if (grid[i][j].block_id != -1) {
                if (msg.blocks.find(grid[i][j].block_id)->second->removable) {
                    printf("🟦");
                }
                else {
                    printf("墙");
                }
            }
            else if (area.player_ids.size() != 0) {
                if ((msg.players.find(msg.player_id)->second->x == i) &&
                    (msg.players.find(msg.player_id)->second->y == j))
                    printf("我");
                else
                    printf("敌");
            }
            else if (area.bomb_id != -1) {
                printf("💣");
            }
            else if (area.item != seedcup::NULLITEM) {
                switch (area.item) {
                case seedcup::BOMB_NUM:
                    cout << "💊";
                    break;
                case seedcup::BOMB_RANGE:
                    cout << "🧪";
                    break;
                case seedcup::INVENCIBLE:
                    cout << "🗽";
                    break;
                case seedcup::SHIELD:
                    cout << "🔰";
                    break;
                case seedcup::HP:
                    cout << "💖";
                    break;
                default:
                    cout << "🛼";
                    break;
                }
            }
            else {
                printf("⬛");
            }
        }
        printf("\n");
    }
}


void SpreadBomb(vector<vector<POSITION>>& array, GameMsg& msg, std::shared_ptr<Bomb> bombptr)
{
    //Attention:may change msg value (bombs.lastplacetime)
    //Attention:may change msg value(blocks)
    int targetnum = msg.game_round - bombptr->last_place_round;
    POINT startpoint = { bombptr->x, bombptr->y };
    array[startpoint.x][startpoint.y].dangernum = targetnum;
    array[startpoint.x][startpoint.y].isBomb = true;
    for (int yleft = startpoint.y - 1; yleft >= 0; yleft--)//left
    {
        if (startpoint.y - yleft > bombptr->bomb_range)
            break;
        if ((!array[startpoint.x][yleft].dangernum) && (msg.grid[startpoint.x][yleft].block_id != -1))//exist block
        {
            if (msg.blocks.find(msg.grid[startpoint.x][yleft].block_id)->second->removable)//removable
            {
                array[startpoint.x][yleft].dangernum = max(targetnum, array[startpoint.x][yleft].dangernum);//>0
                break;
            }
            else
                break;
        }
        else if (msg.grid[startpoint.x][yleft].bomb_id != -1)
        {
            //change msg
            if (msg.bombs[msg.grid[startpoint.x][yleft].bomb_id]->last_place_round > bombptr->last_place_round)
                msg.bombs[msg.grid[startpoint.x][yleft].bomb_id]->last_place_round = bombptr->last_place_round;
            continue;
        }
        else
            array[startpoint.x][yleft].dangernum = max(targetnum, array[startpoint.x][yleft].dangernum);//>0
    }
    for (int yright = startpoint.y + 1; yright < msg.grid.size(); yright++)//right
    {
        if (yright - startpoint.y > bombptr->bomb_range)
            break;
        if ((!array[startpoint.x][yright].dangernum) && (msg.grid[startpoint.x][yright].block_id != -1))
        {
            if (msg.blocks.find(msg.grid[startpoint.x][yright].block_id)->second->removable)//removable
            {
                array[startpoint.x][yright].dangernum = max(targetnum, array[startpoint.x][yright].dangernum);//>0
                break;
            }
            else
                break;
        }
        else if (msg.grid[startpoint.x][yright].bomb_id != -1)
        {
            //change msg
            if (msg.bombs[msg.grid[startpoint.x][yright].bomb_id]->last_place_round > bombptr->last_place_round)
                msg.bombs[msg.grid[startpoint.x][yright].bomb_id]->last_place_round = bombptr->last_place_round;
            continue;
        }
        else
            array[startpoint.x][yright].dangernum = max(targetnum, array[startpoint.x][yright].dangernum);//>0
    }
    for (int xup = startpoint.x - 1; xup >= 0; xup--)//up
    {
        if (startpoint.x - xup > bombptr->bomb_range)
            break;
        if ((!array[xup][startpoint.y].dangernum) && (msg.grid[xup][startpoint.y].block_id != -1))
        {
            if (msg.blocks.find(msg.grid[xup][startpoint.y].block_id)->second->removable)//removable
            {
                array[xup][startpoint.y].dangernum = max(targetnum, array[xup][startpoint.y].dangernum);//>0
                break;
            }
            else
                break;
        }
        else if (msg.grid[xup][startpoint.y].bomb_id != -1)
        {
            //change msg
            if (msg.bombs[msg.grid[xup][startpoint.y].bomb_id]->last_place_round > bombptr->last_place_round)
                msg.bombs[msg.grid[xup][startpoint.y].bomb_id]->last_place_round = bombptr->last_place_round;
            continue;
        }
        else
            array[xup][startpoint.y].dangernum = max(targetnum, array[xup][startpoint.y].dangernum);//>0
    }
    for (int xdown = startpoint.x + 1; xdown < msg.grid.size(); xdown++)//up
    {
        if (xdown - startpoint.x > bombptr->bomb_range)
            break;
        if ((!array[xdown][startpoint.y].dangernum) && (msg.grid[xdown][startpoint.y].block_id != -1))
        {
            if (msg.blocks.find(msg.grid[xdown][startpoint.y].block_id)->second->removable)//removable
            {
                array[xdown][startpoint.y].dangernum = max(targetnum, array[xdown][startpoint.y].dangernum);//>0
                break;
            }
            else
                break;
        }
        else if (msg.grid[xdown][startpoint.y].bomb_id != -1)
        {
            //change msg
            if (msg.bombs[msg.grid[xdown][startpoint.y].bomb_id]->last_place_round > bombptr->last_place_round)
                msg.bombs[msg.grid[xdown][startpoint.y].bomb_id]->last_place_round = bombptr->last_place_round;
            continue;
        }
        else
            array[xdown][startpoint.y].dangernum = max(targetnum, array[xdown][startpoint.y].dangernum);//>0
    }

}

void FindSafeRegion(vector<vector<POSITION>>& array, GameMsg& msg)//0 is safe,n is place before n-1 clock
{   //(0 is place this time i.e.last operation caused),-n means bomb is here
    //Attention: maybe there is bomb place in current round 
    //Attention: may change msg value(bombs.lastplacetime)
    //由于实现原理，请放置在findreachableregion函数之后
    //发现游戏新的判断机制，进行了一定的改进
    std::shared_ptr<Bomb> bombarr[10][100] = { 0 };
    std::shared_ptr<Bomb> bombptr;
    int bombIndex[10] = { 0 };
    for (auto &bombiterator : msg.bombs)
    {
        bombptr = bombiterator.second;
        cout << msg.game_round - bombptr->last_place_round << endl;
        bombarr[msg.game_round - bombptr->last_place_round][bombIndex[msg.game_round - bombptr->last_place_round]] = bombptr;
        bombIndex[msg.game_round - bombptr->last_place_round]++;
    }
    for (int i = 9; i >= 0; i--)
        for (int j = 0; j < bombIndex[i]; j++)
            SpreadBomb(array, msg, bombarr[i][j]);
    //for (int i = 0; i < array.size(); i++)
    //    for (int j = 0; j < array.size(); j++)
    //        array[i][j].dangernum = array[i][j].dangernum ? (array[i][j].dangernum + 1) : 0;
    std::shared_ptr<Player> enemy = msg.players[FindEnemyID(msg)];
    if ((enemy->invincible_time != 0) && (array[enemy->x][enemy->y].isRechable))
    {
        cout<<"Find enemy invincible"<<endl;
        //暂时不使用寻路算法，因为speed = 2时非常没必要，且需要针对机器人的可到达路径修改函数
        int x, y;
        for (int i = -1 * enemy->speed; i <= enemy->speed; i++)
            for (int j = -1 * enemy->speed; j <= enemy->speed; j++)
            {
                x = enemy->x + i;
                y = enemy->y + j;
                if (HamiltonDist(x, y, enemy->x, enemy->y) <= enemy->speed)
					if ((x >= 0) && (x < array.size()) && (y >= 0) && (y < array.size()))
						if (array[x][y].isRechable)
						{
							array[x][y].dangernum = array[x][y].dangernum >= 1 ? array[x][y].dangernum : 1;
							cout << "Update dangernum" << endl;
						}
            }
    }
}

void FindRechableRegion(vector<vector<POSITION>>& array, GameMsg& msg)
{//Attention: Bomb place under your place
    auto& map = msg.grid;
    POINT target;
    std::deque<POINT> waitedarr;
    vector<vector<bool>> checkedmap(map.size(), std::vector<bool>(map.size()));

    waitedarr.emplace_front(msg.players[msg.player_id]->x, msg.players[msg.player_id]->y);
    target.x = msg.players[msg.player_id]->x;
    target.y = msg.players[msg.player_id]->y;
    if ((target.x > 0) && (!checkedmap[target.x - 1][target.y]))
        waitedarr.emplace_front(target.x - 1, target.y);
    if ((target.y > 0) && (!checkedmap[target.x][target.y - 1]))
        waitedarr.emplace_front(target.x, target.y - 1);
    if ((target.x < map.size() - 1) && (!checkedmap[target.x + 1][target.y]))
        waitedarr.emplace_front(target.x + 1, target.y);
    if ((target.y < map.size() - 1) && (!checkedmap[target.x][target.y + 1]))
        waitedarr.emplace_front(target.x, target.y + 1);
    while (!waitedarr.empty())
    {
        target = waitedarr.front();
        waitedarr.pop_front();
        checkedmap[target.x][target.y] = true;
        if ((map[target.x][target.y].block_id != -1) ||
            (map[target.x][target.y].bomb_id != -1))
        {
            array[target.x][target.y].isRechable = false;
            continue;
        }
        else
        {
            array[target.x][target.y].isRechable = true;
        }
        if ((target.x > 0) && (!checkedmap[target.x - 1][target.y]))
            waitedarr.emplace_front(target.x - 1, target.y);
        if ((target.y > 0) && (!checkedmap[target.x][target.y - 1]))
            waitedarr.emplace_front(target.x, target.y - 1);
        if ((target.x < map.size() - 1) && (!checkedmap[target.x + 1][target.y]))
            waitedarr.emplace_front(target.x + 1, target.y);
        if ((target.y < map.size() - 1) && (!checkedmap[target.x][target.y + 1]))
            waitedarr.emplace_front(target.x, target.y + 1);
    }
}

void CalcCostMap(vector<vector<POSITION>>& array, GameMsg& msg)
{
    auto &player = msg.players[msg.player_id];
    std::function<bool(POINT&, POINT&)> cmp =
        [&array](POINT& a, POINT& b) {return array[a.x][a.y].currentCost > array[b.x][b.y].currentCost; };
    priority_queue<POINT, std::deque<POINT>, decltype(cmp)> frontier(cmp);
    frontier.push(POINT(player->x, player->y));
    array[player->x][player->y].currentCost = 0;
    vector<vector<bool> > checkedmap(array.size(), std::vector<bool>(array.size(), false));
    bool findover = false;
    POINT next;
    POINT current;
    int newcost = 0;
    int i, j = 0;
    cout << "Enter costcalc"<< endl;
    while (!frontier.empty())
    {
        current = frontier.top();
        checkedmap[current.x][current.y] = true;
        frontier.pop();
        cout << "Enter costcalc while" << current.x << current.y << endl;
        for (i = -1; i <= 1; i ++)
        {
            for (j = -1; j <= 1; j ++)
            {
                if (i * j != 0)continue;
                if (i == 0 && j == 0)continue;
                next = POINT(current.x + i, current.y + j);
                if (next.x >= 0 && next.x < array.size() && next.y >= 0 && next.y < array.size()
                    && array[next.x][next.y].isRechable)
                {
                    newcost = array[current.x][current.y].currentCost + calculateCost(current.x, current.y, next.x, next.y);
                    if (!checkedmap[next.x][next.y] || newcost < array[next.x][next.y].currentCost)//default value is -1
                    {
                        array[next.x][next.y].currentCost = newcost;
                        frontier.push(next);
                        array[next.x][next.y].parentPoint = &array[current.x][current.y].pos;
                        cout << "add" << next.x << next.y << endl;
                    }
                }
            }
        }
    }
}

POINT FindHiddenPos(vector<vector<POSITION>>& map, std::shared_ptr<seedcup::Player>player)
{
    POINT target = POINT(map.size() * 2, map.size() * 2);
    for (int i = 0; i < map.size(); i++)
        for (int j = 0; j < map.size(); j++)
            if (map[i][j].isRechable && (!map[i][j].dangernum) &&
                (abs(target.x - player->x) + abs(target.y - player->y) > abs(i - player->x) + abs(j - player->y)))
            {
                target.x = i;
                target.y = j;
            }
    POINT target2 = POINT(map.size() * 2, map.size() * 2);
    int count = 0;
    for (int i = 0; i < map.size(); i++)
        for (int j = 0; j < map.size(); j++)
            if (map[i][j].isRechable && (!map[i][j].dangernum) &&
                (abs(target2.x - player->x) + abs(target2.y - player->y) > abs(i - player->x) + abs(j - player->y)))
            {
                count = 0;
                if ((i > 0) && (j > 0))
                    count += map[i - 1][j].isRechable * map[i][j - 1].isRechable;
                if ((j > 0) && (i < map.size() - 1))
                    count += map[i][j - 1].isRechable * map[i + 1][j].isRechable;
                if ((i < map.size() - 1) && (j < map.size() - 1))
                    count += map[i + 1][j].isRechable * map[i][j + 1].isRechable;
                if ((j < map.size() - 1) && (i > 0))
                    count += map[i][j + 1].isRechable * map[i - 1][j].isRechable;
                if (count > 0)
                {
                    target2.x = i;
                    target2.y = j;
                }
            }
    if (target2.x < map.size())
    {
        target.x = target2.x;
        target.y = target2.y;
    }
    cout << "Find over,target is" << target.x << target.y;
    if (target.x > map.size())
        return POINT(-1, -1);
    else
        return target;
}

inline int HamiltonDist(int x1,int y1,int x2,int y2)
{
	return abs(x1 - x2) + abs(y1 - y2);
}

inline int calculateCost(int currentx, int currenty, int targetx, int targety)
{//A star Cost Function
    return abs(currentx - targetx) + abs(currenty - targety);
}

seedcup::ActionType FindWayPosition(vector<vector<POSITION>>& map, std::shared_ptr<seedcup::Player>player, POINT& target, POINT* nextStepPosition = nullptr)
{//不建议使用该函数计算距离，因为该函数未来可能会使用其他cost函数，从而对敌人的计算距离不准确
    POINT current = target;
    POINT playerpos = POINT(player->x, player->y);
    std::deque<POINT> path;
    Direction mincostDirection;
    cout << "target is" << target.x << target.y;
    if (!map[target.x][target.y].isRechable)
    {
        if (nextStepPosition != nullptr)
            *nextStepPosition = playerpos;
        return seedcup::SILENT;
    }
    while (current != playerpos)
    {
		path.emplace_front(current.x, current.y);
        if (map[current.x][current.y].parentPoint == nullptr)
        {
            cout << "Error in FindWay." << endl;
            exit(-10);
        }
        current = *map[current.x][current.y].parentPoint;
	}
    path.emplace_front(playerpos.x, playerpos.y);
//    while (!path.empty())
//    {
////        cout << "Enter while\n";
//        current = path.front();
////        cout << current.x << current.y;
//        if ((current.x == target.x)
//            && (current.y == target.y))
//            break;
//        checkedmap[current.x][current.y] = true;
////        cout << "Enter there\n";
//        mincostDirection = NULLD;
//        mincost = calculateCost(0, 0, map.size(), map.size());
//        if ((current.x > 0) && (map[current.x - 1][current.y].isRechable) && (!checkedmap[current.x - 1][current.y]))
//        {
//            if (mincost > calculateCost(current.x - 1, current.y, target.x, target.y))
//            {
//                mincost = calculateCost(current.x - 1, current.y, target.x, target.y);
//                mincostDirection = UP;
// //               cout << "1\n";
//            }
//        }
//        if ((current.y > 0) && (map[current.x][current.y - 1].isRechable) && (!checkedmap[current.x][current.y - 1]))
//        {
//            if (mincost > calculateCost(current.x, current.y - 1, target.x, target.y))
//            {
//                mincost = calculateCost(current.x, current.y - 1, target.x, target.y);
//                mincostDirection = LEFT;
// //               cout << "2\n";
//            }
//        }
//        if ((current.x < map.size() - 1) && (map[current.x + 1][current.y].isRechable) && (!checkedmap[current.x + 1][current.y]))
//        {
//            if (mincost > calculateCost(current.x + 1, current.y, target.x, target.y))
//            {
//                mincost = calculateCost(current.x + 1, current.y, target.x, target.y);
//                mincostDirection = DOWN;
// //               cout << "3\n";
//            }
//        }
//        if ((current.y < map.size() - 1) && (map[current.x][current.y + 1].isRechable) && (!checkedmap[current.x][current.y + 1]))
//        {
//            if (mincost > calculateCost(current.x, current.y + 1, target.x, target.y))
//            {
//                mincost = calculateCost(current.x, current.y + 1, target.x, target.y);
//                mincostDirection = RIGHT;
// //               cout << "4\n";
//            }
//        }
//        switch (mincostDirection)
//        {
//        case NULLD:          
// //           cout << "pop_front\n";
//            path.pop_front();
//            continue;
//            break;
//        case LEFT:
////            cout << "Choose " << current.x << "," << current.y - 1 << endl;
//            path.emplace_front(current.x, current.y - 1);
//            break;
//        case UP:
////            cout << "Choose " << current.x - 1 << "," << current.y << endl;
//            path.emplace_front(current.x - 1, current.y);
//            break;
//        case RIGHT:
////            cout << "Choose " << current.x << "," << current.y + 1 << endl;
//            path.emplace_front(current.x, current.y + 1);
//            break;
//        case DOWN:
////            cout << "Choose " << current.x + 1 << "," << current.y << endl;
//            path.emplace_front(current.x + 1, current.y);
//            break;
//        }
//    }
    if (path.size() == 1)
    {
        return seedcup::SILENT;
        if (nextStepPosition != nullptr)
            *nextStepPosition = target;
    }
    else
    {
        for(auto& i:path)
			cout << i.x << i.y;
		cout << endl;
//        cout << "pop_back\n";
        path.pop_front();
        current = path.front();
        if (nextStepPosition != nullptr)
        {
            nextStepPosition->x = current.x;
            nextStepPosition->y = current.y;
        }
        if ((current.x == player->x - 1) && (current.y == player->y))
            return seedcup::MOVE_UP;
        else if ((current.x == player->x) && (current.y == player->y - 1))
            return seedcup::MOVE_LEFT;
        else if ((current.x == player->x + 1) && (current.y == player->y))
            return seedcup::MOVE_DOWN;
        else if ((current.x == player->x) && (current.y == player->y + 1))
            return seedcup::MOVE_RIGHT;
        else
        {
            cout << "Error in FindWayPosition" << endl;
            exit(-1);
            return seedcup::SILENT;
        }
    }
    return seedcup::SILENT;
}
//
//void FindSuitableAttackPosition(vector<vector<POSITION>>& map, GameMsg& msg)
//{
//    vector<vector<double>> attackSuitMap(map.size(), vector<double>(map.size()));
//    vector<vector<POSITION>> tempMap(map.size(), vector<POSITION>(map.size()));
//    Bomb tempbomb;
//    tempbomb.bomb_id = -2;
//    tempbomb.bomb_range = msg.players[player_id]->bomb_range;
//    tempbomb.last_place_round = msg.game_round;
//    tempbomb.player_id = msg.player_id;
//
//    int bombReachArea = 0;
//    int playerReachArea = 0;
//    for (int i = 0; i < map.size(); i++)
//        for (int j = 0; j < map.size(); j++)
//        {
//            if (msg.grid[i][j].bomb_id == -1) 
//            {
//                bombReachArea = 0;
//                playerReachArea = 0;
//                tempbomb.x = i;
//                tempbomb.y = j;
//                msg.bombs.insert({ tempbomb.bomb_id,&tempbomb });
//                FindSafeRegion(tempMap, msg);
//
//
//                tempMap
//                msg.bombs.erase(tempbomb.bomb_id);
//            }
//        }
//}

int FindEnemyID(GameMsg& msg)
{
    for (auto& playertarget : msg.players)
        if (playertarget.first != msg.player_id)
        {
            return playertarget.first;
            break;
        }
    return -1;
}

POINT FindSuitableAttackPosition(vector<vector<POSITION>>& map, GameMsg& msg)
{
    auto player = msg.players[msg.player_id];
    std::shared_ptr<Player> enemy = msg.players[FindEnemyID(msg)];
    if (enemy == NULL)
        return POINT(-1, -1);
    return POINT(enemy->x, enemy->y);
}

seedcup::ActionType Attack(vector<vector<POSITION>>& map, GameMsg& msg, POINT* nextStepPosition = nullptr)
{
    auto player = msg.players[msg.player_id];
    POINT point = FindSuitableAttackPosition(map, msg);
    seedcup::ActionType action = FindWayPosition(map, msg.players[msg.player_id], point, nextStepPosition);
    if ((msg.grid[player->x][player->y].bomb_id == -1) && (player->bomb_now_num < player->bomb_max_num) &&
        (action == SILENT))
    {
        action = PLACED;
    }
    return action;
}

inline int calcDistance(vector<vector<POSITION>>& map, GameMsg& msg, int targetx, int targety, int placex, int placey)
{
    return abs(targetx - placex) + abs(targety - placey);
}

POINT FindSuitableMinePosition(vector<vector<POSITION>>& map, GameMsg& msg)
{
    auto player = msg.players[msg.player_id];
    POINT target(map.size() * 2, map.size() * 2);
    for (int i = 0; i < map.size(); i++)
        for (int j = 0; j < map.size(); j++)
        {
            if ((msg.grid[i][j].block_id != -1) && (msg.blocks[msg.grid[i][j].block_id]->removable))
            {
                if ((i > 0) && map[i - 1][j].isRechable && (!map[i - 1][j].dangernum) &&
                    (calcDistance(map,msg,target.x,target.y,player->x,player->y)
                        > calcDistance(map, msg, i-1, j,player->x, player->y)))
                {
                    target.x = i - 1;
                    target.y = j;
                }
                if ((j > 0) && map[i][j - 1].isRechable && (!map[i][j - 1].dangernum) &&
                    (calcDistance(map, msg, target.x, target.y, player->x, player->y)
                        > calcDistance(map, msg, i, j - 1, player->x, player->y)))
                {
                    target.x = i;
                    target.y = j - 1;
                }
                if ((i < map.size() - 1) && map[i + 1][j].isRechable && (!map[i + 1][j].dangernum) &&
                    (calcDistance(map, msg, target.x, target.y,player->x, player->y)
                > calcDistance(map, msg, i + 1, j, player->x, player->y)))
                {
                    target.x = i + 1;
                    target.y = j;
                }
                if ((j < map.size() - 1) && map[i][j + 1].isRechable && (!map[i][j + 1].dangernum) &&
                    (calcDistance(map, msg, target.x, target.y, player->x, player->y)
                > calcDistance(map, msg, i, j + 1, player->x, player->y)))
                {
                    target.x = i;
                    target.y = j + 1;
                }
            }
        }
    if (target.x == map.size() * 2)
        target = POINT(-1, -1);
    return target;
}

seedcup::ActionType Mine(vector<vector<POSITION>>& map, GameMsg& msg, POINT* nextStepPosition = nullptr)
{
    auto player = msg.players[msg.player_id];
    POINT point = FindSuitableMinePosition(map, msg);
    if (point.x < 0)
        return SILENT;
    seedcup::ActionType action = FindWayPosition(map, msg.players[msg.player_id], point, nextStepPosition);
    if ((msg.grid[player->x][player->y].bomb_id == -1) && (player->bomb_now_num < player->bomb_max_num) &&
        (action == SILENT))
    {
        action = PLACED;
    }
    return action;
}
int Judgecollectionnecessity(vector<vector<POSITION>>& map,GameMsg& msg)
{//该函数使用5作为视野范围，但这是否是最佳值有待考证。
    //copilot推荐5应该考虑自己的移动速度。
    //copilot太可爱了捏。
	auto player = msg.players[msg.player_id];
	int flag=0;
	for(int i=0;i<map.size();i++)
    {
	    for(int j=0;j<map.size();j++)
        {
            if ((msg.grid[i][j].item != NULLITEM) && (i >= player->x - CollectRange) && (i <= player->x + CollectRange) && (j <= player->y + CollectRange) && (j >= player->y - CollectRange))
                flag++;
		}
	}
	if(flag!=0) 
        return 1;
	else 
        return 0;
}
POINT FindSuitableCollectionPosition(vector<vector<POSITION>>& map, GameMsg& msg)
{
	auto player = msg.players[msg.player_id];
    POINT target(map.size() * 2, map.size() * 2);
    for(int i=0;i<map.size();i++){
	    for(int j=0;j<map.size();j++){
            if ((msg.grid[i][j].item != NULLITEM) && (i >= player->x - CollectRange) && (i <= player->x + CollectRange) && (j <= player->y + CollectRange) && (j >= player->y - CollectRange) &&
                (!map[i][j].dangernum) && (calcDistance(map, msg, target.x, target.y, player->x, player->y)
        > calcDistance(map, msg, i, j, player->x, player->y)))
            {
                target.x = i;
                target.y = j;
			}
		}
	}
	if (target.x == map.size() * 2)
        target = POINT(-1, -1);
    return target;
}
seedcup::ActionType Collection(vector<vector<POSITION>>& map,GameMsg& msg,POINT* nextStepPosition)
{
	    auto player =msg.players[msg.player_id];
        POINT point =FindSuitableCollectionPosition(map, msg);
        if (point.x < 0)
            return SILENT;
        seedcup::ActionType action = FindWayPosition(map, msg.players[msg.player_id], point, nextStepPosition);
        return action;
}
