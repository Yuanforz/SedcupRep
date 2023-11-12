#include "include/elements/parse.h"
#include "include/seedcup.h"
#include "function.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <deque>
#include <unordered_map>
#include <vector>
using namespace std;
using namespace seedcup;

#include <termio.h>
#include <stdio.h>
#include <unistd.h>

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
    int  x;
    int  y;
};

typedef struct tagPOSITION {
    int x = -1;
    int y = -1;
    int dangernum = 0;//n means bomb be placed befor n
    bool isBomb = 0;
    bool isRechable = 0;
    bool isItem = 0;
    bool isRemovableBlock = 0;
    //bool AvailableDirection[4] = { 0 };
    //bool isNearable = 0;
    std::shared_ptr<Area> pArea = NULL;

}POSITION;

void printMap(const GameMsg& msg);
int SetState(const GameMsg& msg, int prestate, POINT& orientPoint);
void SpreadBomb(vector<vector<POSITION>>& array, GameMsg& msg, std::shared_ptr<Bomb> bombptr);
void FindSafeRegion(vector<vector<POSITION>>& array, GameMsg& msg);
void FindRechableRegion(vector<vector<POSITION>>& array, GameMsg& msg);
POINT FindHiddenPos(vector<vector<POSITION>>& map, std::shared_ptr<seedcup::Player>player);
inline int calculateCost(int currentx, int currenty, int targetx, int targety);
seedcup::ActionType FindWayPosition(vector<vector<POSITION>>& map, std::shared_ptr<seedcup::Player>player, POINT& target);

ActionType act(GameMsg& msg, SeedCup& server, int currentNum)
{
    auto& player = msg.players[msg.player_id];
    auto& map = msg.grid;
    //printMap(msg);
    
    //static int prestate = 0;
    //int state = SetState(msg, prestate);
    vector<vector<POSITION>> positionmap(msg.grid.size(), vector<POSITION>(msg.grid.size()));
    ActionType returnnum = (ActionType)0;
    //vector<vector<int>> safemap(msg.grid.size(), vector<int>(msg.grid.size(), 0));
    FindSafeRegion(positionmap, msg);
    FindRechableRegion(positionmap, msg);
    for (int i = 0; i < msg.grid.size(); i++)
    {
        for (int j = 0; j < msg.grid.size(); j++)
            cout << positionmap[i][j].dangernum * (positionmap[i][j].isBomb ? -1 : 1);
            //cout << positionmap[i][j].isRechable;
        cout << endl;
    }
    POINT hiddenPosition = FindHiddenPos(positionmap, player);;
    if (positionmap[player->x][player->y].dangernum != 0)
    {
        cout << "Danger!\n";
    }
    if (hiddenPosition.x < 0)
        cout << "Sorry,point error\n";
    else
    {
        returnnum = FindWayPosition(positionmap, player, hiddenPosition);
    }

    
    // �����������
    return returnnum;
}

void printMap(const GameMsg& msg)
{

    // ��ӡ�Լ���id
    std::cout << "self:" << msg.player_id << std::endl;
    // ��ӡ��ͼ
    //cout << endl << endl;
    printf("\n\n");
    auto& grid = msg.grid;
    for (int i = 0; i < msg.grid.size(); i++) {
        for (int j = 0; j < msg.grid.size(); j++) {
            auto& area = grid[i][j];
            if (grid[i][j].block_id != -1) {
                if (msg.blocks.find(grid[i][j].block_id)->second->removable) {
                    printf("0 ");
                }
                else {
                    printf("N ");
                }
            }
            else if (area.player_ids.size() != 0) {
                if ((msg.players.find(msg.player_id)->second->x == i) &&
                    (msg.players.find(msg.player_id)->second->y == j))
                    printf("M ");
                else
                    printf("1 ");
            }
            else if (area.bomb_id != -1) {
                printf("8 ");
            }
            else if (area.item != seedcup::NULLITEM) {
                switch (area.item) {
                case seedcup::BOMB_NUM:
                    cout << "a ";
                    break;
                case seedcup::BOMB_RANGE:
                    cout << "b ";
                    break;
                case seedcup::INVENCIBLE:
                    cout << "c ";
                    break;
                case seedcup::SHIELD:
                    cout << "d ";
                    break;
                case seedcup::HP:
                    cout << "e ";
                    break;
                default:
                    cout << "**";
                    break;
                }
            }
            else {
                printf("__");
            }
        }
        printf("\n");
    }
}

int SetState(const GameMsg& msg, int prestate, POINT& orientPoint)
{
    int state = 0;
    switch (prestate)
    {
    case 0://silent
        state = 2;
        break;
    case 1://avoid attack
        if (msg.bombs.size() != 0)
            state = 1;//imcomplete isnecceray
        else
            state = 2;
        break;
    case 2://place bomb to get item and score
        state = 2;//imcomplete
        break;
    case 3://get item
        state = 2;
        break;
    case 4://attack
        state = 2;
        break;
    defalut:
        state = 0;
        break;
    }
    return state;
}

void SpreadBomb(vector<vector<POSITION>>& array, GameMsg& msg, std::shared_ptr<Bomb> bombptr)
{
    //Attention:may change msg value (bombs.lastplacetime)
    int targetnum = msg.game_round - bombptr->last_place_round;
    POINT startpoint = { bombptr->x, bombptr->y };
    array[startpoint.x][startpoint.y].dangernum = targetnum;
    array[startpoint.x][startpoint.y].isBomb = true;
    for (int yleft = startpoint.y - 1; yleft >= 0; yleft--)//left
    {
        if (startpoint.y - yleft > bombptr->bomb_range)
            break;
        if (msg.grid[startpoint.x][yleft].block_id != -1)//exist block
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
        if (msg.grid[startpoint.x][yright].block_id != -1)//exist block
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
        if (msg.grid[xup][startpoint.y].block_id != -1)//exist block
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
        if (msg.grid[xdown][startpoint.y].block_id != -1)//exist block
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

inline int calculateCost(int currentx, int currenty, int targetx, int targety)
{//A star Cost Function
    return abs(currentx - targetx) + abs(currenty - targety) + 1;
}

seedcup::ActionType FindWayPosition(vector<vector<POSITION>>& map, std::shared_ptr<seedcup::Player>player, POINT& target)
{
    POINT current;
    std::deque<POINT> path;
    Direction mincostDirection;
    int mincost;
    vector<vector<bool>> checkedmap(map.size(), std::vector<bool>(map.size(), false));
    path.emplace_front(player->x, player->y);
    cout << "Enter Way found\n";
    cout << "target is" << target.x << target.y;
    while (!path.empty())
    {
        cout << "Enter while\n";
        current = path.front();
        cout << current.x << current.y;
        if ((current.x == target.x)
            && (current.y == target.y))
            break;
        checkedmap[current.x][current.y] = true;
        cout << "Enter there\n";
        mincostDirection = NULLD;
        mincost = calculateCost(0, 0, map.size(), map.size()) + 1;
        if ((current.x > 0) && (map[current.x - 1][current.y].isRechable) && (!checkedmap[current.x - 1][current.y]))
        {
            if (mincost > calculateCost(current.x - 1, current.y, target.x, target.y))
            {
                mincost = calculateCost(current.x - 1, current.y, target.x, target.y);
                mincostDirection = UP;
                cout << "1\n";
            }
        }
        if ((current.y > 0) && (map[current.x][current.y - 1].isRechable) && (!checkedmap[current.x][current.y - 1]))
        {
            if (mincost > calculateCost(current.x, current.y - 1, target.x, target.y))
            {
                mincost = calculateCost(current.x, current.y - 1, target.x, target.y);
                mincostDirection = LEFT;
                cout << "2\n";
            }
        }
        if ((current.x < map.size() - 1) && (map[current.x + 1][current.y].isRechable) && (!checkedmap[current.x + 1][current.y]))
        {
            if (mincost > calculateCost(current.x + 1, current.y, target.x, target.y))
            {
                mincost = calculateCost(current.x + 1, current.y, target.x, target.y);
                mincostDirection = DOWN;
                cout << "3\n";
            }
        }
        if ((current.y < map.size() - 1) && (map[current.x][current.y + 1].isRechable) && (!checkedmap[current.x][current.y + 1]))
        {
            if (mincost > calculateCost(current.x, current.y + 1, target.x, target.y))
            {
                mincost = calculateCost(current.x, current.y + 1, target.x, target.y);
                mincostDirection = RIGHT;
                cout << "4\n";
            }
        }
        switch (mincostDirection)
        {
        case NULLD:          
            printf("Pop front\n");
            path.pop_front();
            continue;
            break;
        case LEFT:
            printf("Choose %d,%d\n", current.x, current.y - 1);
            path.emplace_front(current.x, current.y - 1);
            break;
        case UP:
            printf("Choose %d,%d\n", current.x-1, current.y );
            path.emplace_front(current.x - 1, current.y);
            break;
        case RIGHT:
            printf("Choose %d,%d\n", current.x, current.y + 1);
            path.emplace_front(current.x, current.y + 1);
            break;
        case DOWN:
            printf("Choose %d,%d\n", current.x+1, current.y );
            path.emplace_front(current.x + 1, current.y);
            break;
        }
    }
    if (path.size() == 1)
        return seedcup::SILENT;
    else
    {
        cout << "pop_back\n";
        path.pop_back();
        current = path.back();
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
            exit(-1);
            return seedcup::SILENT;
        }
    }
}

//int cmpDistOfBomb(std::shared_ptr<Bomb> bomb1, std::shared_ptr<Bomb> bomb2, Player &me)//1 for 1>2 and 0 for otherwise
//{
//    
//}
//std::shared_ptr<Bomb> FindThreatenBomb(const GameMsg& msg)
//{
//    int nearestid = msg.bombs.begin();
//    for (auto& bomb : msg.bombs)
//        if (cmpDistOfBomb(msg.bombs[nearestid], bomb.second.get(), msg.players[msg.player_id]))
//            nearestid = bomb.second.get()->bomb_id;
//    return msg.bombs[nearestid];
//}
//
//void FindPlacetoBomb(const GameMsg& msg, POINT& orientPoint)
//{
//
//}
//
//void DecideOrient(const GameMsg& msg, int state, POINT& orientPoint)
//{
//
//    
//    auto& me = msg.players[msg.player_id];
//    if (state == 1)
//    {
//        if (msg.bombs.empty())
//            return;
//        auto targetbomb = FindThreatenBomb(msg);
//        FindHiddenPos(msg, targetbomb, orientPoint);
//    }
//    else if (state == 2)
//    {
//        FindHiddenPos(msg, orientPoint);
//    }
//    else if (state == 3)
//    {
//
//    }
//}