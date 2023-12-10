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

const int CollectStepMax = 15;

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

typedef struct tagPOSITION {
    POINT pos;

    //dangernum=6 or 7 means it will bomb next round!!!
    int dangernum = 0;//n means bomb be placed before n

    bool isBomb = 0;
    bool isRechable = 0;
    bool isEdgeBomb = false;
    POINT* parentPoint = nullptr;
    int minstep = 1000;//minstep means the min step to the current position (didn't consider round i.e. speed)
    int currentCost = -1;
    //bool isItem = 0;
    //bool isRemovableBlock = 0;
    //bool AvailableDirection[4] = { 0 };
    //bool isNearable = 0;
    //std::shared_ptr<Area> pArea = NULL;
}POSITION;

void printMap(const GameMsg& msg, int mode = 0, vector<vector<POSITION>>* positionmap = nullptr);
void SpreadBomb(vector<vector<POSITION>>& array, GameMsg& msg, std::shared_ptr<Bomb> bombptr);
void FindSafeRegion(vector<vector<POSITION>>& array, GameMsg& msg);
void FindRechableRegion(vector<vector<POSITION>>& array, GameMsg& msg);
inline int calculateCost(vector<vector<POSITION>>& array, GameMsg& msg, int currentx, int currenty, int targetx, int targety);
void CalcCostMap(vector<vector<POSITION>>& array, GameMsg& msg, int mode = 0);
POINT FindHiddenPos(vector<vector<POSITION>>& map, std::shared_ptr<seedcup::Player>player);
inline int HamiltonDist(int x1, int y1, int x2, int y2);
seedcup::ActionType FindWayPosition(vector<vector<POSITION>>& map, std::shared_ptr<seedcup::Player>player, POINT& target, POINT* nextStepPosition = nullptr);
int FindEnemyID(GameMsg& msg);
inline int calcDistance(vector<vector<POSITION>>& map, GameMsg& msg, int targetx, int targety, int placex, int placey);
POINT FindSuitableAttackPosition(vector<vector<POSITION>>& map, GameMsg& msg);
seedcup::ActionType Attack(vector<vector<POSITION>>& map, GameMsg& msg,int*pushnum, POINT* nextStepPosition = nullptr);
seedcup::ActionType RangeAttack(int* pushnum, GameMsg& msg);
POINT FindSuitableMinePosition(vector<vector<POSITION>>& map, GameMsg& msg);
seedcup::ActionType Mine(vector<vector<POSITION>>& map, GameMsg& msg, POINT* nextStepPosition = nullptr);
int Judgecollectionnecessity(vector<vector<POSITION>>& map,GameMsg& msg);
int JudgeStrictCollectionnecessity(vector<vector<POSITION>>& map, GameMsg& msg);
POINT FindSuitableCollectionPosition(vector<vector<POSITION>>& map, GameMsg& msg);
seedcup::ActionType Collection(vector<vector<POSITION>>& map,GameMsg& msg,POINT* nextStepPosition = nullptr);
seedcup::ActionType MakeDecision(vector<vector<POSITION>>& map, GameMsg& msg, int currentNum);
bool IsSafeIfPlaceBomb(vector<vector<POSITION>>& map, GameMsg& msg, POINT& target, int currentNum, bool renew = false);
bool IsAbleToFindSafe(vector<vector<POSITION>>& map, GameMsg& msg, int currentNum);
bool IsStrictAbleToFindSafe(vector<vector<POSITION>>& map, GameMsg& msg, int currentNum);
seedcup::ActionType FindDirectionOnBomb(vector<vector<POSITION>>& map, GameMsg& msg, int currentNum, POINT* nextStepPosition=nullptr);
int CalcRechableArea(vector<vector<POSITION>>& map);
bool FindWayBombPush(vector<vector<POSITION>>& map, GameMsg& msg, POINT bombtarget, POINT& goplace);
POINT FindPlacePushEscape(vector<vector<POSITION>>& map, GameMsg& msg, POINT& targetbomb);
seedcup::ActionType PushEscape(vector<vector<POSITION>>& map, GameMsg& msg, POINT* nextStepPosition);


ActionType act(GameMsg& msg, SeedCup& server, int currentNum)
{
#if !ON_DEBUG
    std::cout.setstate(std::ios_base::failbit);
#endif
    vector<vector<POSITION>> positionmap(msg.grid.size(), vector<POSITION>(msg.grid.size()));

    for(int i=0;i<positionmap.size();i++)
        for (int j = 0; j < positionmap.size(); j++)
        {
            positionmap[i][j].pos = POINT(i, j);
		}
    FindRechableRegion(positionmap, msg);
    FindSafeRegion(positionmap, msg);//由于实现原理，请放置在findreachableregion函数之后
    CalcCostMap(positionmap, msg);

#if ON_DEBUG
    printMap(msg, 1, &positionmap);
#endif

    ActionType returnnum= MakeDecision(positionmap, msg, currentNum);
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
    // 设置随机操作
    return returnnum;
}

seedcup::ActionType MakeDecision(vector<vector<POSITION>>& positionmap, GameMsg& msg, int currentNum)
{
    ActionType returnnum = (ActionType)0;
    auto& player = msg.players[msg.player_id];
    POINT playerpos = POINT(player->x, player->y);
    POINT nextStepPosition;
    static int prestate = 0;//1 Attack 2 Mine 3 Run away 4 collect
    static int count = 0;//counting
    static int pushnum = 0;
    bool token = 1;
    bool isPushtoEscape = false;
    //if (positionmap[player->x][player->y].dangernum <= 2||pushnum!=0)
    //{
    //    if (pushnum != 0) {
    //        std::cout << "Try to RangeAttack";
    //        returnnum = RangeAttack(&pushnum, msg);
    //        std::cout << "pushnum is" << pushnum << endl;
    //        return returnnum;
    //    }
    //    if (positionmap[msg.players[FindEnemyID(msg)]->x][msg.players[FindEnemyID(msg)]->y].isRechable
    //        && (msg.players[FindEnemyID(msg)]->invincible_time == 0)
    //        &&(!JudgeStrictCollectionnecessity(positionmap, msg)))
    //    {
    //        returnnum = Attack(positionmap, msg,&pushnum, &nextStepPosition);
    //        if (prestate == 1)
    //            count++;
    //        else
    //        {
    //            prestate = 1;
    //            count = 0;
    //        }
    //        std::cout << "TRY TO Attack\n";
    //        if (pushnum != 0)return returnnum;
    //    }
    //    else if (Judgecollectionnecessity(positionmap, msg))
    //    {
    //        returnnum = Collection(positionmap, msg, &nextStepPosition);
    //        if (prestate == 4)
    //            count++;
    //        else
    //        {
    //            prestate = 4;
    //            count = 0;
    //        }
    //        cout << "TRY TO Collect\n";
    //        cout << "Collect at" << nextStepPosition.x << " " << nextStepPosition.y << endl;
    //    }
    //    else if (player->bomb_now_num < player->bomb_max_num)
    //    {
    //        returnnum = Mine(positionmap, msg, &nextStepPosition);
    //        if (prestate == 2)
    //            count++;
    //        else
    //        {
    //            prestate = 2;
    //            count = 0;
    //        }
    //        cout << "TRY TO Mine\n";
    //        cout << "Mine at" << nextStepPosition.x << " " << nextStepPosition.y << endl;
    //    }
    //    else
    //    {
    //        cout << "TRY TO MOVE\n";
    //        POINT hiddenPosition = FindHiddenPos(positionmap, player);
    //        if (prestate == 3)
    //            count++;
    //        else
    //        {
    //            prestate = 3;
    //            count = 0;
    //        }
    //        if (hiddenPosition.x < 0)
    //        {
    //            cout << "Sorry,point error\n";
    //            returnnum = SILENT;
    //        }
    //        else
    //        {
    //            returnnum = FindWayPosition(positionmap, player, hiddenPosition, &nextStepPosition);
    //            if (positionmap[nextStepPosition.x][nextStepPosition.y].dangernum >= 3)
    //                returnnum = SILENT;
    //        }
    //    }

    if (positionmap[player->x][player->y].dangernum <= 2)
    {
        if (positionmap[msg.players[FindEnemyID(msg)]->x][msg.players[FindEnemyID(msg)]->y].isRechable
            && (msg.players[FindEnemyID(msg)]->invincible_time == 0)
            && (!JudgeStrictCollectionnecessity(positionmap, msg)))
        {
            returnnum = Attack(positionmap, msg, &pushnum, &nextStepPosition);
            token = 0;
            if (prestate == 1)
                count++;
            else
            {
                prestate = 1;
                count = 0;
            }
            std::cout << "TRY TO Attack\n";
            //if (pushnum != 0)return returnnum;
        }
        else if (Judgecollectionnecessity(positionmap, msg))
        {
            returnnum = Collection(positionmap, msg, &nextStepPosition);
            if (returnnum == SILENT)//Invalid
            {
                cout << "TRY TO Collect but FAILED\n";
                token = 1;
            }
            else
            {
                token = 0;
                if (prestate == 4)
                    count++;
                else
                {
                    prestate = 4;
                    count = 0;
                }
                cout << "TRY TO Collect\n";
                cout << "Collect at" << nextStepPosition.x << " " << nextStepPosition.y << endl;
            }
        }
        if ((player->bomb_now_num < player->bomb_max_num) && token)
        {
            returnnum = Mine(positionmap, msg, &nextStepPosition);
            if (returnnum == SILENT)//Invalid
            {
                cout << "TRY TO Mine but FAILED\n";
                token = 1;
            }
            else
            {
                token = 0;
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
        }
        if(token)
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
                if (positionmap[nextStepPosition.x][nextStepPosition.y].dangernum >= 5)
                    returnnum = SILENT;
            }
        }
        if (positionmap[nextStepPosition.x][nextStepPosition.y].dangernum >= 3)
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
            cout << "Hidden position is(After forcing out)" << hiddenPosition.x << hiddenPosition.y << endl;
            returnnum = FindWayPosition(positionmap, player, hiddenPosition, &nextStepPosition);
            if (positionmap[nextStepPosition.x][nextStepPosition.y].dangernum >= 3)
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
            cout << "Try to push bomb now\n";
            returnnum = PushEscape(positionmap, msg, &nextStepPosition);
            if (returnnum == SILENT)
            {
                cout << "Find FAILED\n";
                returnnum = SILENT;
            }
            else
            {
                isPushtoEscape = true;
            }
        }
        else
        {
            returnnum = FindWayPosition(positionmap, player, hiddenPosition, &nextStepPosition);
            if ((positionmap[nextStepPosition.x][nextStepPosition.y].dangernum >= 5)
                && (positionmap[nextStepPosition.x][nextStepPosition.y].dangernum > positionmap[player->x][player->y].dangernum))
                returnnum = SILENT;
            //if (positionmap[nextStepPosition.x][nextStepPsition.y].dangernum > positionmap[player->x][player->y].dangernum)
            //    returnnum = SILENT;
        }
    }

    //if (count > 40)
    //    returnnum = PLACED;

    if ((msg.grid[player->x][player->y].bomb_id != -1) && (!isPushtoEscape))
    {
        cout << "On a bomb,start avoid function.\n";
        returnnum = FindDirectionOnBomb(positionmap, msg, currentNum);
        token = 0;
    }
    if (returnnum == PLACED)
        if (IsSafeIfPlaceBomb(positionmap, msg, playerpos, true, currentNum))
			returnnum = PLACED;
        else
        {
            cout<< "Bomb is not safe\n";
            returnnum = SILENT;
        }



    cout << "count is" << count << endl;
    return returnnum;
}

void printMap(const GameMsg& msg, int mode, vector<vector<POSITION>>* positionmap)
{
    if ((mode != 0) && (positionmap == nullptr))
    {
        printf("Error:positionmap is nullptr\n");
        mode = 0;
    }
    printf("\x1b[H\x1b[2J");
    // 打印自己的id
    std::cout << "selfID:" << msg.player_id<<std::endl;
    std::cout << "HP:" << msg.players.find(msg.player_id)->second->hp;
    std::cout << "\t\t";
    std::cout << "BombNum:" << msg.players.find(msg.player_id)->second->bomb_now_num
        <<"/"<< msg.players.find(msg.player_id)->second->bomb_max_num;
    std::cout << "\t\t";
    std::cout << "BombRange:" << msg.players.find(msg.player_id)->second->bomb_range;
    std::cout << "\t\t";
    std::cout << "Speed:" << msg.players.find(msg.player_id)->second->speed;
    std::cout << std::endl;

    std::cout << "Shield time:" << msg.players.find(msg.player_id)->second->shield_time;
    std::cout << "\t\t";
    std::cout << ((msg.players.find(msg.player_id)->second->invincible_time>0)?"Invince!   ":"Not invince");
    std::cout << "\t\t";
    std::cout << (msg.players.find(msg.player_id)->second->has_gloves ? "Has Glove" : "No glove");
    std::cout << std::endl;

    std::cout << "X:" << msg.players.find(msg.player_id)->second->x;
    std::cout << "\t\t";
    std::cout << "Y:" << msg.players.find(msg.player_id)->second->y;
    std::cout << std::endl;

    // 打印地图
    //cout << endl << endl;
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
                case seedcup::INVINCIBLE:
                    cout << "🗽";
                    break;
                case seedcup::SHIELD:
                    cout << "🔰";
                    break;
                case seedcup::HP:
                    cout << "💖";
                    break;
                case seedcup::SPEED:
                    cout << "🛼";
                    break;
                case seedcup::GLOVES:
                    cout << "🧤";
                    break;
                default:
                    cout << "?";
                    break;
                }
            }
            else {
                printf("⬛");
            }
        }
        if (mode == 1)
        {
            printf("\t\t");
            for (int j = 0; j < msg.grid.size(); j++)
                printf("%2d", (*positionmap)[i][j].dangernum);
            printf("\t\t");
            for (int j = 0; j < msg.grid.size(); j++)
                printf("%2d", ((*positionmap)[i][j].minstep >= 99 ? 0 : (*positionmap)[i][j].minstep));
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
    if (enemy->invincible_time != 0)
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
							array[x][y].dangernum = array[x][y].dangernum >= 99 ? array[x][y].dangernum : 99;
							cout << "Update dangernum" << endl;
						}
            }
    }
}

void FindRechableRegion(vector<vector<POSITION>>& array, GameMsg& msg)
{//Attention: Bomb place under your place
    for (int i = 0; i < array.size(); i++)
        for (int j = 0; j < array.size(); j++)
        {
            array[i][j].isRechable = false;
            array[i][j].isEdgeBomb = false;
        }

    auto& map = msg.grid;
    POINT target;
    std::deque<POINT> waitedarr;
    vector<vector<bool>> checkedmap(map.size(), std::vector<bool>(map.size()));
    bool findEnemyInvincible = false;

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
            if (map[target.x][target.y].bomb_id != -1)
                array[target.x][target.y].isEdgeBomb = true;
            continue;
        }
        else if (!map[target.x][target.y].player_ids.empty())
        {
            findEnemyInvincible = false;
            for (auto& it : map[target.x][target.y].player_ids)
                if ((it != msg.player_id) && (msg.players[it]->invincible_time > 0))
				{
                    findEnemyInvincible = true;
					break;
				}
            if (findEnemyInvincible)
            {
                array[target.x][target.y].isRechable = true;//To make the findsafe function work.
				continue;
            }
            else
            {
				array[target.x][target.y].isRechable = true;
			}
            
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
    array[msg.players[msg.player_id]->x][msg.players[msg.player_id]->y].isRechable = true;
}

void CalcCostMap(vector<vector<POSITION>>& array, GameMsg& msg, int mode)
{
    auto &player = msg.players[msg.player_id];
    std::function<bool(POINT&, POINT&)> cmp =
        [&array](POINT& a, POINT& b) {return array[a.x][a.y].currentCost > array[b.x][b.y].currentCost; };
    priority_queue<POINT, std::deque<POINT>, decltype(cmp)> frontier(cmp);
    frontier.push(POINT(player->x, player->y));
    array[player->x][player->y].currentCost = 0;
    array[player->x][player->y].minstep = 0;
    vector<vector<bool> > checkedmap(array.size(), std::vector<bool>(array.size(), false));
    bool findover = false;
    POINT next;
    POINT current;
    int newcost = 0;
    int currentstep = 0;
    int i, j = 0;
    //cout << "Enter costcalc"<< endl;
    if (mode == 0)
        while (!frontier.empty())
        {
            current = frontier.top();
            checkedmap[current.x][current.y] = true;
            frontier.pop();
            //cout << "Enter costcalc while" << current.x << current.y << endl;
            for (i = -1; i <= 1; i++)
            {
                for (j = -1; j <= 1; j++)
                {
                    if (i * j != 0)continue;
                    if (i == 0 && j == 0)continue;
                    next = POINT(current.x + i, current.y + j);
                    if (next.x >= 0 && next.x < array.size() && next.y >= 0 && next.y < array.size()
                        && array[next.x][next.y].isRechable)
                    {
                        newcost = array[current.x][current.y].currentCost + calculateCost(array, msg, current.x, current.y, next.x, next.y);
                        currentstep = array[current.x][current.y].minstep + 1;
                        if (!checkedmap[next.x][next.y] || newcost < array[next.x][next.y].currentCost)//default value is -1
                        {
                            array[next.x][next.y].minstep = currentstep;
                            array[next.x][next.y].currentCost = newcost;
                            frontier.push(next);
                            array[next.x][next.y].parentPoint = &array[current.x][current.y].pos;
                            //cout << "add" << next.x << next.y << endl;
                        }
                    }
                }
            }
        }
    else if (mode == 1)
        while (!frontier.empty())
        {
            current = frontier.top();
            checkedmap[current.x][current.y] = true;
            frontier.pop();
            //cout << "Enter costcalc while" << current.x << current.y << endl;
            for (i = -1; i <= 1; i++)
            {
                for (j = -1; j <= 1; j++)
                {
                    if (i * j != 0)continue;
                    if (i == 0 && j == 0)continue;
                    next = POINT(current.x + i, current.y + j);
                    if (next.x >= 0 && next.x < array.size() && next.y >= 0 && next.y < array.size()
                        && array[next.x][next.y].isRechable)
                    {
                        newcost = array[current.x][current.y].currentCost + 1;
                        currentstep = array[current.x][current.y].minstep + 1;
                        if (!checkedmap[next.x][next.y] || newcost < array[next.x][next.y].currentCost)//default value is -1
                        {
                            array[next.x][next.y].minstep = currentstep;
                            array[next.x][next.y].currentCost = newcost;
                            frontier.push(next);
                            array[next.x][next.y].parentPoint = &array[current.x][current.y].pos;
                            //cout << "add" << next.x << next.y << endl;
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

inline int calculateCost(vector<vector<POSITION>>& array, GameMsg& msg, int currentx, int currenty, int targetx, int targety)
{
    //dijkstra Cost Function
    //Attention: must be non-negative
    //Assume that the dangernum max is 7
 //   int dangerCost = 0;
 //   std::shared_ptr<Player> enemy = msg.players[FindEnemyID(msg)];
 //   if (array[currentx][currenty].dangernum == 0)
 //       if (array[targetx][targety].dangernum > array[currentx][currenty].dangernum)
 //           dangerCost = (array[targetx][targety].dangernum - array[currentx][currenty].dangernum) * 10;
 //       else
 //           dangerCost = 0;
 //   else if (array[targetx][targety].dangernum > array[currentx][currenty].dangernum)
 //       dangerCost = (array[targetx][targety].dangernum - array[currentx][currenty].dangernum) * 2;
 //   else
 //       dangerCost = -10;
 //   int enemycost = 0;
 //   if ((enemy->invincible_time == 0) && array[enemy->x][enemy->y].isRechable)
 //       enemycost = HamiltonDist(currentx, currenty, enemy->x, enemy->y) / 5;
	//else
	//	enemycost = 0;
 //   int itemcost = 0;
 //   switch (msg.grid[targetx][targety].item)
 //   {
 //       case ItemType::NULLITEM: itemcost = 0;
 //           break;
 //       case ItemType::BOMB_RANGE:
 //           itemcost = -10;
 //           break;
 //       case ItemType::BOMB_NUM:
 //           itemcost = -10;
 //           break;
 //       case ItemType::HP:
 //           itemcost = -100;
 //           break;
 //       case ItemType::INVINCIBLE:
 //           itemcost = -100;
 //           break;
 //       case ItemType::SHIELD:
 //           itemcost=-100;
 //           break;
 //   }
 //   int finalcost = dangerCost + enemycost + itemcost + (abs(currentx - targetx) + abs(currenty - targety)) * 2;

 //   return finalcost > 0 ? finalcost : 0;
    if (array[targetx][targety].dangernum > 4)
        return 3;
    else if (array[targetx][targety].dangernum > 0)
        return 2;
	else
		return 1;
}

seedcup::ActionType FindWayPosition(vector<vector<POSITION>>& map, std::shared_ptr<seedcup::Player>player, POINT& target, POINT* nextStepPosition)
{
    POINT current = target;
    POINT playerpos = POINT(player->x, player->y);
    std::deque<POINT> path;
    Direction mincostDirection;
    cout << "target is" << target.x << target.y;
    if ((target.x > map.size()) || (target.x < 0) || (!map[target.x][target.y].isRechable))
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

    int minsteparr[5] = {100,100,100,100,100};//up,left,center(not use),right,down (CONSIDER THAT: need to add center or not?)

    for (int i = -1; i <= 1; i++)
        for (int j = -1; j <= 1; j++)
        {
			if (i * j != 0)continue;
			if (i == 0 && j == 0)continue;
            if ((enemy->x + i >= 0) && (enemy->x + i < map.size()) && (enemy->y + j >= 0) && (enemy->y + j < map.size()))
                if (map[enemy->x + i][enemy->y + j].isRechable)
                    minsteparr[i * 2 + j + 2] = map[enemy->x + i][enemy->y + j].minstep;
		}
    //minsteparr[2] = 100;

    int minway = 0;
    for (int i = 1; i < 5; i++)
        if ((minsteparr[i] < minsteparr[minway]) && (minsteparr[i] != -1) || (minsteparr[minway] == -1))
			minway = i;
    return POINT(enemy->x + (minway % 2 == 0 ? (minway - 2) / 2 : 0), enemy->y + (minway % 2 == 1 ? (minway - 2) : 0));
}

seedcup::ActionType Attack(vector<vector<POSITION>>& map, GameMsg& msg,int*pushnum, POINT* nextStepPosition)
{
    auto player = msg.players[msg.player_id];
    std::shared_ptr<Player> enemy = msg.players[FindEnemyID(msg)];
    POINT point1(0, 0);
    POINT point2(0, map.size()-1);
    POINT point3(map.size()-1, 0);
    POINT point4(map.size()-1, map.size()-1);
    POINT pointnow(player->x, player->y);
    //if (player->has_gloves && (((player->x == enemy->x) && (abs(player->y - enemy->y) > player->bomb_range)) || ((player->y == enemy->y)
    //    && (abs(player->x - enemy->x) > player->bomb_range))) && pointnow != point1
    //    && pointnow != point2 && pointnow != point3 && pointnow != point4) 
    //{
    //    *pushnum = -1;
    //    ActionType returnnum = (ActionType)0;
    //    returnnum = RangeAttack(pushnum, msg);
    //    if (*pushnum != 0)
    //        return PLACED;
    //}
    POINT point = FindSuitableAttackPosition(map, msg);
    seedcup::ActionType action = FindWayPosition(map, msg.players[msg.player_id], point, nextStepPosition);
    if ((msg.grid[player->x][player->y].bomb_id == -1) && (player->bomb_now_num < player->bomb_max_num) &&
        ((point.x == player->x) && (point.y == player->y)))
    {
        action = PLACED;
    }
    return action;


    //auto player = msg.players[msg.player_id];
    //POINT point = FindSuitableAttackPosition(map, msg);
    //seedcup::ActionType action = FindWayPosition(map, msg.players[msg.player_id], point, nextStepPosition);
    //if ((msg.grid[player->x][player->y].bomb_id == -1) && (player->bomb_now_num < player->bomb_max_num) &&
    //    (action == SILENT))
    //{
    //    action = PLACED;
    //}
    //return action;
}
seedcup::ActionType RangeAttack(int* pushnum, GameMsg& msg) {
    auto player = msg.players[msg.player_id];
    std::shared_ptr<Player> enemy = msg.players[FindEnemyID(msg)];
    if (*pushnum == -1) {
        if (enemy->y == player->y && enemy->x < player->x && msg.grid[player->x + 1][player->y].block_id == -1) {
            *pushnum = 1;
            return PLACED;
        }
        else if (enemy->y == player->y && enemy->x > player->x && msg.grid[player->x - 1][player->y].block_id == -1) {
            *pushnum = 1;
            return PLACED;
        }
        else if (enemy->x == player->x && enemy->y < player->y && msg.grid[player->x][player->y + 1].block_id == -1) {
            *pushnum = 1;
            return PLACED;
        }
        else if (enemy->x == player->x && enemy->y > player->y && msg.grid[player->x][player->y - 1].block_id == -1) {
            *pushnum = 1;
            return PLACED;
        }
        else {
            *pushnum = 0;
            return SILENT;
        }
    }
    else if (*pushnum == 1) {
        if (enemy->y == player->y && enemy->x < player->x && msg.grid[player->x + 1][player->y].block_id == -1) {
            *pushnum = 2;//2 Attack up
            return MOVE_DOWN;
        }
        else if (enemy->y == player->y && enemy->x > player->x && msg.grid[player->x - 1][player->y].block_id == -1) {
            *pushnum = 3;//3 Attack DOWN
            return MOVE_UP;
        }
        else if (enemy->x == player->x && enemy->y < player->y && msg.grid[player->x][player->y + 1].block_id == -1) {
            *pushnum = 4;//4 Attack LEFT
            return MOVE_RIGHT;
        }
        else if (enemy->x == player->x && enemy->y > player->y && msg.grid[player->x][player->y - 1].block_id == -1) {
            *pushnum = 5;//5 Attack RIGHT
            return MOVE_LEFT;
        }
        else {
            *pushnum = 0;
            return SILENT;
        }
    }
    if (*pushnum > 1) {
        if (*pushnum == 2) {
            *pushnum = 0;
            return MOVE_UP;
        }
        if (*pushnum == 3) {
            *pushnum = 0;
            return MOVE_DOWN;
        }
        if (*pushnum == 4) {
            *pushnum = 0;
            return MOVE_LEFT;
        }
        if (*pushnum == 5) {
            *pushnum = 0;
            return MOVE_RIGHT;
        }
    }
    return SILENT;
}


//inline int calcDistance(vector<vector<POSITION>>& map, GameMsg& msg, int targetx, int targety, int placex, int placey)
//{
//    return abs(targetx - placex) + abs(targety - placey);
//}

POINT FindSuitableMinePosition(vector<vector<POSITION>>& map, GameMsg& msg)
{
    auto player = msg.players[msg.player_id];
    POINT target(map.size() * 2, map.size() * 2);
    bool flag = 0;
    for (int i = 0; i < map.size(); i++)
        for (int j = 0; j < map.size(); j++)
        {
            if ((msg.grid[i][j].block_id != -1) && (msg.blocks[msg.grid[i][j].block_id]->removable) && (map[i][j].dangernum == 0))
            {
                if (target.x == map.size() * 2)
                {
                    flag = 1;
                }
                if ((i > 0) && map[i - 1][j].isRechable && (!map[i - 1][j].dangernum) &&
                    (flag||map[target.x][target.y].minstep > map[i - 1][j].minstep))
                {
                    flag = 0;
                    target.x = i - 1;
                    target.y = j;
                }
                if ((j > 0) && map[i][j - 1].isRechable && (!map[i][j - 1].dangernum) &&
                    (flag || map[target.x][target.y].minstep > map[i][j - 1].minstep))
                {
                    flag = 0;
                    target.x = i;
                    target.y = j - 1;
                }
                if ((i < map.size() - 1) && map[i + 1][j].isRechable && (!map[i + 1][j].dangernum) &&
                    (flag || map[target.x][target.y].minstep > map[i + 1][j].minstep))
                {
                    flag = 0;
                    target.x = i + 1;
                    target.y = j;
                }
                if ((j < map.size() - 1) && map[i][j + 1].isRechable && (!map[i][j + 1].dangernum) &&
                    (flag || map[target.x][target.y].minstep > map[i][j + 1].minstep))
                {
                    flag = 0;
                    target.x = i;
                    target.y = j + 1;
                }
            }
        }
    if (target.x == map.size() * 2)
        target = POINT(-1, -1);
    return target;
}

seedcup::ActionType Mine(vector<vector<POSITION>>& map, GameMsg& msg, POINT* nextStepPosition)
{
    auto player = msg.players[msg.player_id];
    POINT point = FindSuitableMinePosition(map, msg);
    if (point.x < 0)
        return SILENT;
    seedcup::ActionType action = FindWayPosition(map, msg.players[msg.player_id], point, nextStepPosition);
    if ((msg.grid[player->x][player->y].bomb_id == -1) && (player->bomb_now_num < player->bomb_max_num) &&
        ((point.x == player->x) && (point.y == player->y)))
    {
        action = PLACED;
    }
    return action;
}
int Judgecollectionnecessity(vector<vector<POSITION>>& map, GameMsg& msg)
{
    auto player = msg.players[msg.player_id];
    int flag = 0;
    for (int i = 0; i < map.size(); i++)
    {
        for (int j = 0; j < map.size(); j++)
        {
            if ((msg.grid[i][j].item != NULLITEM) && (map[i][j].isRechable) && (map[i][j].minstep < CollectStepMax))
            {
                if ((!player->has_gloves) || (msg.grid[i][j].item != GLOVES))
                    flag++;
            }
        }
    }
    if (flag != 0)
        return 1;
    else
        return 0;
}
int JudgeStrictCollectionnecessity(vector<vector<POSITION>>& map, GameMsg& msg)
{
    auto player = msg.players[msg.player_id];
    int flag = 0;
    for (int i = 0; i < map.size(); i++)
    {
        for (int j = 0; j < map.size(); j++)
        {
            if ((msg.grid[i][j].item != NULLITEM) && (map[i][j].isRechable) && (map[i][j].minstep < CollectStepMax))
            {
                if ((!player->has_gloves) || msg.grid[i][j].item != GLOVES)
                    flag++;
                if ((msg.grid[i][j].item == SPEED) && map[i][j].minstep < 7)
					flag += 10;
                if ((msg.grid[i][j].item == SHIELD) && map[i][j].minstep < 7)
                    flag += 10;
                if ((msg.grid[i][j].item == HP) && map[i][j].minstep < 7)
                    flag += 10;
                if (msg.grid[i][j].item == INVINCIBLE && map[i][j].minstep < 7)
                    flag += 10;
            }
        }
    }
    if (flag >= 10)
        return 1;
    else
        return 0;
}
POINT FindSuitableCollectionPosition(vector<vector<POSITION>>& map, GameMsg& msg)
{
    auto player = msg.players[msg.player_id];
    POINT target(map.size() * 2, map.size() * 2);
    for (int i = 0; i < map.size(); i++) {
        for (int j = 0; j < map.size(); j++) {
            if (target.x == map.size() * 2) {
                if ((msg.grid[i][j].item != NULLITEM) && (map[i][j].isRechable) && (map[i][j].minstep < CollectStepMax) &&
                    ((!player->has_gloves) || (msg.grid[i][j].item != GLOVES)) &&
                    ((map[i][j].dangernum < 3) || (2 * map[i][j].minstep < CollectStepMax)))
                {
                    target.x = i;
                    target.y = j;
                }
            }
            else {
                if ((msg.grid[i][j].item != NULLITEM) && (map[i][j].isRechable) && (map[i][j].minstep < CollectStepMax) &&
					((!player->has_gloves) || (msg.grid[i][j].item != GLOVES)) &&
                    ((map[i][j].dangernum < 3) || (2 * map[i][j].minstep < CollectStepMax)))
                {
                    if (!player->has_gloves) {
                        if (msg.grid[i][j].item > msg.grid[target.x][target.y].item ||
                            ((msg.grid[i][j].item == msg.grid[target.x][target.y].item) && (map[i][j].minstep < map[target.x][target.y].minstep)))
                        {
                            target.x = i;
                            target.y = j;
                        }
                    }
                    else {
                        if ((msg.grid[i][j].item > msg.grid[target.x][target.y].item && msg.grid[i][j].item != GLOVES) ||
							((msg.grid[i][j].item == msg.grid[target.x][target.y].item) && (map[i][j].minstep < map[target.x][target.y].minstep))
                            ) {
                            target.x = i;
                            target.y = j;
                        }
                    }
                }
            }
        }
    }
    if (target.x == map.size() * 2)
        target = POINT(-1, -1);
    return target;
	//auto player = msg.players[msg.player_id];
 //   POINT target(map.size() * 2, map.size() * 2);
 //   for(int i=0;i<map.size();i++){
	//    for(int j=0;j<map.size();j++){
 //           if ((msg.grid[i][j].item != NULLITEM) && (i >= player->x - CollectRange) && (i <= player->x + CollectRange) && (j <= player->y + CollectRange) && (j >= player->y - CollectRange) &&
 //               (!map[i][j].dangernum) && (calcDistance(map, msg, target.x, target.y, player->x, player->y)
 //       > calcDistance(map, msg, i, j, player->x, player->y)))
 //           {
 //               target.x = i;
 //               target.y = j;
	//		}
	//	}
	//}
	//if (target.x == map.size() * 2)
 //       target = POINT(-1, -1);
 //   return target;
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

int CalcRechableArea(vector<vector<POSITION>>& map)
{
    int count = 0;
    for(int i=0;i<map.size();i++)
        for (int j = 0; j < map.size(); j++)
        {
            if (map[i][j].isRechable)
            {
                count++;
            }
        }
    return count;
}

//Virtual Bomb id start by -50
bool IsSafeIfPlaceBomb(vector<vector<POSITION>>& map, GameMsg& msg, POINT& target, int currentNum, bool renew)
{//Attention: may change msg value in this function tempory
    static int count = 0;
    if (renew)
    {
        count = 0;
    }
    if (count > 20)//Attetion:  temperory way to avoid bomb id conflict
    {
        count = 0;
    }
    auto &player = msg.players[msg.player_id];
    if ((msg.grid[player->x][player->y].bomb_id == -1) && (player->bomb_now_num < player->bomb_max_num))
    {
        count++;

        std::shared_ptr<Bomb> bombtemp = std::make_shared<Bomb>();
        bombtemp->bomb_id = -50 + count;
        bombtemp->last_place_round = msg.game_round - 1;//ATTENTION:  WARNING this 
        bombtemp->x = target.x;
        bombtemp->y = target.y;
        bombtemp->player_id = player->player_id;
        bombtemp->bomb_range = player->bomb_range;
        player->bomb_now_num++;
        msg.grid[player->x][player->y].bomb_id = bombtemp->bomb_id;
        msg.bombs.insert({ bombtemp->bomb_id, bombtemp });



        FindRechableRegion(map, msg);
        FindSafeRegion(map, msg);//由于实现原理，请放置在findreachableregion函数之后
        CalcCostMap(map, msg);
        for(int i=0;i<map.size();i++)
		    for(int j=0;j<map.size();j++)
                if (map[i][j].isRechable && (!map[i][j].dangernum))
                {
                    if (map[i][j].minstep <= player->speed * 3 - currentNum)
                    {
                        msg.grid[player->x][player->y].bomb_id = -1;
                        msg.bombs.erase(-50 + count);
                        player->bomb_now_num--;
                        FindRechableRegion(map, msg);
                        FindSafeRegion(map, msg);//由于实现原理，请放置在findreachableregion函数之后
                        CalcCostMap(map, msg);
                        return true;
                    }
                    else
                        continue;
			    }
		msg.grid[player->x][player->y].bomb_id = -1;
		msg.bombs.erase(-50 + count);
		player->bomb_now_num--;
		return false;
        FindRechableRegion(map, msg);
        FindSafeRegion(map, msg);//由于实现原理，请放置在findreachableregion函数之后
        CalcCostMap(map, msg);
    }
    return false;
}

seedcup::ActionType FindDirectionOnBomb(vector<vector<POSITION>>& map, GameMsg& msg, int currentNum, POINT* nextStepPosition)
{
    vector<vector<POSITION>> tempmap(msg.grid.size(), vector<POSITION>(msg.grid.size()));
    for (int i = 0; i < tempmap.size(); i++)
        for (int j = 0; j < tempmap.size(); j++)
        {
            tempmap[i][j].pos = POINT(i, j);
        }
    auto &player = msg.players[msg.player_id];
    int playertruex = player->x;
    int playertruey = player->y;
    int currentRechableArea = 0;
    int currentDirection = -1;
    for (int i = -1; i <= 1; i++)
        for (int j = -1; j <= 1; j++)
        {
            if (i * j != 0)continue;
            if (i == 0 && j == 0)continue;
            if ((player->x + i >= 0) && (player->x + i < map.size()) && (player->y + j >= 0) && (player->y + j < map.size()))
                if (map[player->x + i][player->y + j].isRechable)
                {
                    msg.grid[player->x][player->y].player_ids.erase(player->player_id);
                    player->x = player->x + i;
                    player->y = player->y + j;
                    msg.grid[player->x][player->y].player_ids.insert(player->player_id);
                    FindRechableRegion(tempmap, msg);
                    FindSafeRegion(tempmap, msg);//由于实现原理，请放置在findreachableregion函数之后
                    CalcCostMap(tempmap, msg);
                    if (IsStrictAbleToFindSafe(tempmap, msg, currentNum))
                    {
                        if ((currentDirection == -1) || ((CalcRechableArea(tempmap) > currentRechableArea)))
                        {
                            currentDirection = 2 * i + j + 2;
                            currentRechableArea = CalcRechableArea(tempmap);
                        }
                    }
                    msg.grid[player->x][player->y].player_ids.erase(player->player_id);
                    player->x = playertruex;
                    player->y = playertruey;
                    msg.grid[player->x][player->y].player_ids.insert(player->player_id);
                }
        }


    switch (currentDirection)
    {
    case -1:return seedcup::SILENT;
    case 0:return seedcup::MOVE_UP;
    case 4:return seedcup::MOVE_DOWN;
    case 1:return seedcup::MOVE_LEFT;
    case 3:return seedcup::MOVE_RIGHT;
    }
    return seedcup::SILENT;
}

bool IsAbleToFindSafe(vector<vector<POSITION>>& map, GameMsg& msg,int currentNum)
{
    auto& player = msg.players[msg.player_id];
    for (int i = 0; i < map.size(); i++)
        for (int j = 0; j < map.size(); j++)
            if (map[i][j].isRechable && (!map[i][j].dangernum))
            {
                if (map[i][j].minstep <= player->speed * 3 - currentNum)
                {
                    //printf("Safe point is");
                    return true;
                }
                else
                    continue;
            }
    return false;
}

bool IsStrictAbleToFindSafe(vector<vector<POSITION>>& map, GameMsg& msg, int currentNum)
{
    auto& player = msg.players[msg.player_id];
    for (int i = 0; i < map.size(); i++)
        for (int j = 0; j < map.size(); j++)
            if (map[i][j].isRechable && (map[i][j].dangernum<5))
            {
                if (map[i][j].minstep <= player->speed  - currentNum)
                {
                    //printf("Safe point is");
                    return true;
                }
                else
                    continue;
            }
    return false;
}

//This function have not completed
int FindEscapeWay(vector<vector<POSITION>>& array, GameMsg& msg, int currentNum)
{
    vector<vector<POSITION>>& newmap = array;
    auto& player = msg.players[msg.player_id];
    CalcCostMap(newmap, msg, 1);
    return 1;
}

bool FindWayBombPush(vector<vector<POSITION>>& map, GameMsg& msg,POINT bombtarget,POINT& goplace)
{
    auto& player = msg.players[msg.player_id];
    int fromx, fromy;
    int targetx, targety;
    POINT bestfrom(-1, -1);
    for (int i = -1; i <= 1; i++)
        for (int j = -1; j <= 1; j++)
        {
            if (i * j != 0)continue;
            if (i == 0 && j == 0)continue;
            fromx = bombtarget.x + i;
            fromy = bombtarget.y + j;
            targetx= bombtarget.x - i;
            targety = bombtarget.y - j;
            if ((fromx >= 0) && (fromx < map.size()) && (fromy >= 0) && (fromy < map.size())
                && (targetx >= 0) && (targetx < map.size()) && (targety >= 0) && (targety < map.size()))
                    if(map[fromx][fromy].isRechable)
                        if((msg.grid[targetx][targety].bomb_id==-1) 
                            && (msg.grid[targetx][targety].block_id == -1)
                            && (msg.grid[targetx][targety].item == NULLITEM)
                            && (msg.grid[targetx][targety].player_ids.empty()))
                        {
                            if (bestfrom.x < 0)
                            {
                                bestfrom.x = fromx;
                                bestfrom.y = fromy;
                            }
                            else if (map[bestfrom.x][bestfrom.y].minstep > map[fromx][fromy].minstep)
                            {
                                bestfrom.x = fromx;
                                bestfrom.y = fromy;
                            }
                        }
        }
    if (bestfrom.x < 0)
    {
        goplace.x = -1;
        goplace.y = -1;
        return false;
    }
    else
    {
        goplace = bestfrom;
        return true;
    }
}

//bool IsNeedToPushEscape(vector<vector<POSITION>>& map, GameMsg& msg)
//{
//    if ((!IsAbleToFindSafe(map, msg, 0)) && (CalcRechableArea(map) <= 5))
//    {
//
//    }
//}

POINT FindPlacePushEscape(vector<vector<POSITION>>& map, GameMsg& msg, POINT& targetbomb)
{
    int rech = CalcRechableArea(map);
    POINT fromtarget(-1, -1);
    POINT besttarget(-1, -1);
    POINT besttarget_bomb(-1, -1);
    if ((!IsAbleToFindSafe(map, msg, 0)) && (rech <= 5))
    {
        for(int i=0;i<map.size();i++)
            for (int j = 0; j < map.size(); j++)
            {
                if (map[i][j].isEdgeBomb)
                {
#if ON_DEBUG
                    printf("Checked bomb %d,%d\n", i, j);
#endif
                    if (FindWayBombPush(map, msg, POINT(i, j), fromtarget))
                    {
#if ON_DEBUG
                        printf("Wait target is %d,%d\n", fromtarget.x, fromtarget.y);
#endif
                        if (besttarget.x < 0)
                        {
                            besttarget = fromtarget;
                            besttarget_bomb = POINT(i, j);
                        }
                        else if (map[besttarget.x][besttarget.y].minstep > map[fromtarget.x][fromtarget.y].minstep)
                        {
                            besttarget = fromtarget;
                            besttarget_bomb = POINT(i, j);
                        }
                    }
                }
            }
    }
    targetbomb = besttarget_bomb;
    if (besttarget.x < 0)
        return POINT(-1, -1);
    else
        return besttarget;
}

seedcup::ActionType PushEscape(vector<vector<POSITION>>& map, GameMsg& msg, POINT* nextStepPosition)
{
    auto& player = msg.players[msg.player_id];
    POINT target, targetbomb;
    seedcup::ActionType action;
    target = FindPlacePushEscape(map, msg, targetbomb);
    if (target.x < 0)
        return SILENT;
    if ((target.x == player->x) && (target.x == player->x))
    {
        if (target.x > targetbomb.x)
            action = seedcup::MOVE_UP;
        else if (target.x < targetbomb.x)
            action = seedcup::MOVE_DOWN;
        else if (target.y < targetbomb.y)
            action = seedcup::MOVE_RIGHT;
        else
            action = seedcup::MOVE_LEFT;
        if (nextStepPosition != nullptr)
            *nextStepPosition = target;
    }
    else
         action= FindWayPosition(map, player, target, nextStepPosition);

    return action;
}

