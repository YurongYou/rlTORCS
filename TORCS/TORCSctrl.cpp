#include <algorithm>
#include <fstream>
#include <string>
#include <utility>
#include <vector>
#include <stdint.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/shm.h>
#include <cstring>
#include <time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <assert.h>
#include <spawn.h>


extern "C"{
    #include "lua.h"
    #include "luaT.h"
    #include "lualib.h"
    #include "lauxlib.h"
    #include "luajit.h"
    #include "TH/TH.h"
    int luaopen_TORCSctrl(lua_State *L);
}

#define image_width 640
#define image_height 480

// using std::string;
static void stackDump (lua_State *L) {
    int i;
    int top = lua_gettop(L); /* depth of the stack */
    for (i = 1; i <= top; i++) { /* repeat for each level */
        int t = lua_type(L, i);
        switch (t) {
            case LUA_TSTRING: { /* strings */
                printf("'%s'", lua_tostring(L, i));
                break;
            }
            case LUA_TBOOLEAN: { /* booleans */
                printf(lua_toboolean(L, i) ? "true" : "false");
                break;
            }
            case LUA_TNUMBER: { /* numbers */
                printf("%g", lua_tonumber(L, i));
                break;
            }
            default: { /* other values */
                printf("%s", lua_typename(L, t));
                break;
            }

        }
        printf(" ");
    /* put a separator */
    } printf("\n");
    /* end the listing */
}
//  for testing
static int sleep(lua_State *L)
{
    int m = static_cast<int> (luaL_checknumber(L,1));
    usleep(m * 1000);
    // usleep takes microseconds. This converts the parameter to milliseconds.
    // Change this as necessary.
    // Alternatively, use 'sleep()' to treat the parameter as whole seconds.
    return 0;
}

static int ctrl_wait(lua_State *L){
    int pid = static_cast<int> (luaL_checknumber(L,1));
    waitpid(pid, NULL, 0);
    return 0;
}

static int ctrl_kill(lua_State *L){
    int pid = static_cast<int> (luaL_checknumber(L,1));
    kill(pid, SIGKILL);
    return 0;
}

static int ctrl_fork(lua_State *L){
    int pid = fork();
    lua_pushinteger(L, pid);
    return 1;
}

extern char **environ;
int newGame(lua_State *L){
    bool auto_back = lua_tointeger(L, -5);
    int mkey = lua_tointeger(L, -4);
    bool isServer = lua_tointeger(L, -3);
    int display_num = lua_tointeger(L, -2);
    char config_path[120];
    strcpy(config_path, lua_tostring(L, -1));
    char key[20];
    sprintf(key, "%d", mkey);
    char *args[9];
    pid_t pid;
    if (isServer){
        args[0] = (char*)"torcs";
        args[1] = (char*)"_rgs";
        args[2] = config_path;
        args[3] = (char*)"_mkey";
        args[4] = key;
        args[5] = (char*)"_screen";
        char display_str[10];
        sprintf(display_str, "%d", display_num);
        args[6] = display_str;
        if (auto_back){
            args[7] = (char*)"_back";
            args[8] = NULL;
        } else{
            args[7] = NULL;
        }
    }
    else{
        args[0] = (char*)"torcs";
        args[1] = (char*)"_rgs";
        args[2] = config_path;
        args[3] = (char*)"_mkey";
        args[4] = key;
        if (auto_back){
            args[5] = (char*)"_back";
            args[6] = NULL;
        } else{
            args[5] = NULL;
        }
    }
    posix_spawn(&pid, "/usr/local/bin/torcs", NULL, NULL, args, environ);
    lua_settop(L,0);
    lua_pushinteger(L, pid);
    return 1;
}

struct shared_use_st
{
    int written;  //a label, if 1: available to read, if 0: available to write
    uint8_t data[image_width*image_height*3];  // image data field
    uint8_t data_remove_side[image_width*image_height*3];
    uint8_t data_remove_middle[image_width*image_height*3];
    uint8_t data_remove_car[image_width*image_height*3];
    int pid;
    int isEnd;
    double dist;

    double steerCmd;
    double accelCmd;
    double brakeCmd;
    // for reward building
    double speed;
    double angle_in_rad;
    int damage;
    double pos;
    int segtype;
    double radius;
    int frontCarNum;
    double frontDist;
};

int initializeMem(lua_State *L){
    lua_pushstring(L, "shared");
    lua_gettable(L, LUA_REGISTRYINDEX);
    shared_use_st* shared = ((shared_use_st*)lua_touserdata(L, -1));
    memset(shared, 0, sizeof(shared_use_st));
    return 0;
}

int isSetUp(lua_State *L){
    lua_pushstring(L, "isSetUp");
    lua_gettable(L, LUA_REGISTRYINDEX);
    int _isSetUp = lua_tonumber(L, -1);
    lua_settop(L,0);
    lua_pushinteger(L, _isSetUp);
    return 1;
}

int getRGBImage(lua_State *L) {
    THFloatTensor* img = ((THFloatTensor*)luaT_checkudata(L, 1, luaT_typenameid(L, "torch.FloatTensor")));
    float* data = img->storage->data;
    int choose = lua_tointeger(L, -2);

    lua_pushstring(L, "shared");
    lua_gettable(L, LUA_REGISTRYINDEX);
    shared_use_st* shared = ((shared_use_st*)lua_touserdata(L, -1));

    uint8_t* image = NULL;
    if (choose == 0) image = shared->data;
    else if (choose == 1) image = shared->data_remove_side;
    else if (choose == 2) image = shared->data_remove_middle;
    else image = shared->data_remove_car;

    lua_settop(L,0);
    for (int c = 0; c < 3; ++c)
        for (int h = 0; h < image_height; ++h)
            for (int w = 0; w < image_width; ++w)
                data[img->storageOffset + c * img->stride[0] + h * img->stride[1] + w * img->stride[2]] = image[((image_height - h - 1) * image_width + w) * 3 + c] / 255.0f;
    return 0;
}

int getGreyScale(lua_State *L){
    THFloatTensor* img = ((THFloatTensor*)luaT_checkudata(L, 1, luaT_typenameid(L, "torch.FloatTensor")));
    float* data = img->storage->data;

    lua_pushstring(L, "shared");
    lua_gettable(L, LUA_REGISTRYINDEX);
    shared_use_st* shared = ((shared_use_st*)lua_touserdata(L, -1));
    lua_settop(L,0);
    for (int h = 0; h < image_height; ++h){
        for (int w = 0; w < image_width; ++w){
            data[img->storageOffset + h * img->stride[1] + w * img->stride[2]] = (shared->data[((image_height - h - 1) * image_width + w) * 3 + 0] * 0.299 + shared->data[((image_height - h - 1) * image_width + w) * 3 + 1] * 0.587 + shared->data[((image_height - h - 1) * image_width + w) * 3 + 2] * 0.114) / 255;
        }
    }

    return 0;
}


int setSteerCmd(lua_State *L){
    double steerCmd = lua_tonumber(L, -1);
    lua_pushstring(L, "shared");
    lua_gettable(L, LUA_REGISTRYINDEX);
    shared_use_st* shared = ((shared_use_st*)lua_touserdata(L, -1));
    lua_settop(L,0);
    shared->steerCmd = steerCmd;
    return 0;
}

int setAccelCmd(lua_State *L){
    double accelCmd = lua_tonumber(L, -1);
    lua_pushstring(L, "shared");
    lua_gettable(L, LUA_REGISTRYINDEX);
    shared_use_st* shared = ((shared_use_st*)lua_touserdata(L, -1));
    lua_settop(L,0);
    shared->accelCmd = accelCmd;
    return 0;
}

int setBrakeCmd(lua_State *L){
    double breakCmd = lua_tonumber(L, -1);
    lua_pushstring(L, "shared");
    lua_gettable(L, LUA_REGISTRYINDEX);
    shared_use_st* shared = ((shared_use_st*)lua_touserdata(L, -1));
    lua_settop(L,0);
    shared->brakeCmd = breakCmd;
    return 0;
}

int setWritten(lua_State *L){
    int written = lua_tointeger(L, -1);
    // printf("setting to %d \n", written);
    lua_pushstring(L, "shared");
    lua_gettable(L, LUA_REGISTRYINDEX);
    shared_use_st* shared = ((shared_use_st*)lua_touserdata(L, -1));
    lua_settop(L,0);
    shared->written = written;
    return 0;
}


int getWritten(lua_State *L){
    lua_pushstring(L, "shared");
    lua_gettable(L, LUA_REGISTRYINDEX);
    volatile shared_use_st* volatile shared = ((shared_use_st*)lua_touserdata(L, -1));
    lua_settop(L,0);
    lua_pushinteger(L, shared->written);
    return 1;
}

int getSpeed(lua_State *L){
    lua_pushstring(L, "shared");
    lua_gettable(L, LUA_REGISTRYINDEX);
    volatile shared_use_st* volatile shared = ((shared_use_st*)lua_touserdata(L, -1));
    lua_settop(L,0);
    double speed = shared->speed;
    lua_pushnumber(L, speed);
    return 1;
}

int getDist(lua_State *L){
    lua_pushstring(L, "shared");
    lua_gettable(L, LUA_REGISTRYINDEX);
    volatile shared_use_st* volatile shared = ((shared_use_st*)lua_touserdata(L, -1));
    lua_settop(L,0);
    double dist = shared->dist;
    lua_pushnumber(L, dist);
    return 1;
}

int getAngle(lua_State *L){
    lua_pushstring(L, "shared");
    lua_gettable(L, LUA_REGISTRYINDEX);
    volatile shared_use_st* volatile shared = ((shared_use_st*)lua_touserdata(L, -1));
    lua_settop(L,0);
    double angle = shared->angle_in_rad;
    lua_pushnumber(L, angle);
    return 1;
}

int getDamage(lua_State *L){
    lua_pushstring(L, "shared");
    lua_gettable(L, LUA_REGISTRYINDEX);
    volatile shared_use_st* volatile shared = ((shared_use_st*)lua_touserdata(L, -1));
    lua_settop(L,0);
    int damage = shared->damage;
    lua_pushinteger(L, damage);
    return 1;
}

int getIsEnd(lua_State *L){
    lua_pushstring(L, "shared");
    lua_gettable(L, LUA_REGISTRYINDEX);
    volatile shared_use_st* volatile shared = ((shared_use_st*)lua_touserdata(L, -1));
    lua_settop(L,0);
    int end = shared->isEnd;
    lua_pushinteger(L, end);
    return 1;
}

int getPos(lua_State *L){
    lua_pushstring(L, "shared");
    lua_gettable(L, LUA_REGISTRYINDEX);
    volatile shared_use_st* volatile shared = ((shared_use_st*)lua_touserdata(L, -1));
    lua_settop(L,0);
    double pos = shared->pos;
    lua_pushnumber(L, pos);
    return 1;
}

int setEnd(lua_State *L){
    int value = lua_tointeger(L, -1);
    lua_pushstring(L, "shared");
    lua_gettable(L, LUA_REGISTRYINDEX);
    volatile shared_use_st* volatile shared = ((shared_use_st*)lua_touserdata(L, -1));
    lua_settop(L,0);
    shared->isEnd = value;
    return 0;
}


int getPid(lua_State *L){
    lua_pushstring(L, "shared");
    lua_gettable(L, LUA_REGISTRYINDEX);
    volatile shared_use_st* volatile shared = ((shared_use_st*)lua_touserdata(L, -1));
    lua_settop(L,0);
    int pid = shared->pid;
    while (pid == 0){
        pid = shared->pid;
        printf("pid has not been set yet\n");
    }
    lua_pushinteger(L, pid);
    return 1;
}

int setUp(lua_State *L){
    // set key
    int key = 817;
    if (lua_gettop(L) != 0){
        key = lua_tointeger(L, -1);
    }
    lua_pushstring(L, "key");
    lua_pushnumber(L, key);
    lua_settable(L, LUA_REGISTRYINDEX);

    // set up memory sharing
    int shmid = shmget((key_t)key, sizeof(struct shared_use_st), 0666|IPC_CREAT);

    lua_pushstring(L, "shmid");
    lua_pushnumber(L, shmid);
    lua_settable(L, LUA_REGISTRYINDEX);

    if(shmid == -1)
    {
        fprintf(stderr, "shmget failed\n");
        exit(EXIT_FAILURE);
    }

    // printf("\n********** Controler: Set memory sharing key to %d successfully **********\n", key);

    void *shm = NULL;
    shm = shmat(shmid, 0, 0);
    if(shm == (void*)-1)
    {
        fprintf(stderr, "shmat failed\n");
        exit(EXIT_FAILURE);
    }

    // printf("\n********** Controler: Memory sharing started, attached at %X **********\n", *((int*)(shm)));

    struct shared_use_st* shared = (struct shared_use_st*)shm;
    shared->written = 0;
    shared->pid = 0;
    shared->isEnd = 0;

    shared->steerCmd = 0.0;
    shared->accelCmd = 0.0;
    shared->brakeCmd = 0.0;

    lua_pushstring(L, "shared");
    lua_pushlightuserdata(L, (void *)shared);
    lua_settable(L, LUA_REGISTRYINDEX);

    // END set up memory sharing

    // for testing
    // for (int i = 0; i < 3 * image_width * image_height; ++i){
    //     shared->data[i] = (rand() % (255 - 0 +1));
    // }
    int _isSetUp = 1;
    lua_pushstring(L, "isSetUp");
    lua_pushnumber(L, _isSetUp);
    lua_settable(L, LUA_REGISTRYINDEX);
    return 0;
}

int cleanUp(lua_State *L){
    lua_pushstring(L, "shared");
    lua_gettable(L, LUA_REGISTRYINDEX);
    void* shm = lua_touserdata(L, -1);


    lua_pushstring(L, "shmid");
    lua_gettable(L, LUA_REGISTRYINDEX);
    int shmid = lua_tonumber(L, -1);

    if(shmdt(shm) == -1)
    {
        fprintf(stderr, "shmdt failed\n");
        lua_pushinteger(L, EXIT_FAILURE);
    }

    else if(shmctl(shmid, IPC_RMID, 0) == -1)
    {
        fprintf(stderr, "shmctl(IPC_RMID) failed\n");
        lua_pushinteger(L, EXIT_FAILURE);
    }
    else {
        // printf("********** Controler: Memory sharing stopped. Good Bye! **********\n");
        lua_pushinteger(L, EXIT_SUCCESS);
    }
    return 1;
}


int getKey(lua_State *L){
    lua_pushstring(L, "key");
    lua_gettable(L, LUA_REGISTRYINDEX);
    int key = lua_tonumber(L, -1);
    lua_settop(L,0);
    lua_pushinteger(L, key);
    return 1;
}

int getSegType(lua_State *L){
    lua_pushstring(L, "shared");
    lua_gettable(L, LUA_REGISTRYINDEX);
    volatile shared_use_st* volatile shared = ((shared_use_st*)lua_touserdata(L, -1));
    lua_settop(L,0);
    int segtype = shared->segtype;
    lua_pushinteger(L, segtype);
    return 1;
}

int getRadius(lua_State *L){
    lua_pushstring(L, "shared");
    lua_gettable(L, LUA_REGISTRYINDEX);
    volatile shared_use_st* volatile shared = ((shared_use_st*)lua_touserdata(L, -1));
    lua_settop(L,0);
    double radius = shared->radius;
    lua_pushnumber(L, radius);
    return 1;
}

int getFrontCarNum(lua_State *L){
    lua_pushstring(L, "shared");
    lua_gettable(L, LUA_REGISTRYINDEX);
    volatile shared_use_st* volatile shared = ((shared_use_st*)lua_touserdata(L, -1));
    lua_settop(L,0);
    int frontCarNum = shared->frontCarNum;
    lua_pushinteger(L, frontCarNum);
    return 1;
}

int getFrontCarDist(lua_State *L){
    lua_pushstring(L, "shared");
    lua_gettable(L, LUA_REGISTRYINDEX);
    volatile shared_use_st* volatile shared = ((shared_use_st*)lua_touserdata(L, -1));
    lua_settop(L,0);
    double frontDist = shared->frontDist;
    lua_pushnumber(L, frontDist);
    return 1;
}

static const struct luaL_Reg myLib[] =
{
    {"initializeMem", initializeMem},
    // {"getImg", getImg},
    {"cleanUp", cleanUp},
    {"sleep", sleep},
    {"ctrl_wait", ctrl_wait},
    {"ctrl_kill", ctrl_kill},
    {"ctrl_fork", ctrl_fork},
    {"setSteerCmd", setSteerCmd},
    {"setAccelCmd", setAccelCmd},
    {"setBrakeCmd", setBrakeCmd},
    {"getWritten", getWritten},
    {"setWritten", setWritten},
    {"setUp", setUp},
    {"isSetUp", isSetUp},
    {"getPid", getPid},
    {"getKey", getKey},
    {"getSpeed", getSpeed},
    {"getAngle", getAngle},
    {"getDamage", getDamage},
    {"getIsEnd", getIsEnd},
    {"setEnd", setEnd},
    {"getPos", getPos},
    {"getSegType", getSegType},
    {"getRadius", getRadius},
    {"getDist", getDist},
    {"getFrontCarNum", getFrontCarNum},
    {"getFrontCarDist", getFrontCarDist},
    {"getGreyScale", getGreyScale},
    {"getRGBImage", getRGBImage},
    {"newGame", newGame},
    {NULL, NULL},
};

int luaopen_TORCSctrl(lua_State *L){
    luaL_register(L, "TORCSctrl", myLib);
    return 1;
}



// conpile:
// g++ TORCSctrl.cpp -fPIC -shared -o TORCSctrl.so -I/Users/youyurong/torch/install/include -L/Users/youyurong/torch/install/lib -lluajit -g
// in Ubuntu
// g++ TORCSctrl.cpp -fPIC -shared -o TORCSctrl.so -I/home/ficos/torch/install/include -L/home/ficos/torch/install/lib -lluajit -g