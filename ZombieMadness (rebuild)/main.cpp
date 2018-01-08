#include <iostream>
#include <allegro.h>
#include <math.h>
#include <time.h>
#include <string>
#include <sstream>
#include "PathFinder.h"
#include "maps.cpp"
#include "inventory.cpp"

#define MAPY 3000
#define MAPX 3000
#define LOGS 100
#define MAX_ZOMBIES 2000
#define MAX_BUILDINGS 500
#define MAX_LOOTS 500

#define COLOR_WHITE (color(255,255,255))
#define COLOR_GREY (color (150,150,150))
#define COLOR_BLACK (color(0,0,0))
#define COLOR_RED (color(255,0,0))
#define COLOR_GREEN (color(0,255,0))
#define COLOR_BLUE (color(0,0,255))
#define COLOR_CYAN (color(0,255,255))
#define COLOR_YELLOW (color(255,255,0))
#define COLOR_PURPLE (color (255,0,255))
#define COLOR_NONE -1

#define COLOR_FLOOR (color (40,40,40))
#define COLOR_SHADOW (color (7,7,12))

using namespace std;

enum item_type { CONSUMABLE, EQUIPTMENT, QUEST_ITEM, MATERIAL, CURRENCY };
enum quest_type { COLLECT, KILL, SAVE };
enum obj_type { DESTROYABLE, INVUNRABLE, TREE, WATER, FLOOR };
enum trigger_types { NONE, DOOR, QUEST };
enum animation_speed { VERYFAST = 10, FAST = 50, NORMAL = 100, SLOW = 150, VERYSLOW = 200 };
enum mousebuttons {LEFT = 1, RIGHT = 2, MIDDLE = 3 };
enum directions { DOWN = 3, UP = 4 };
enum bitrate {bt32 = 32, bt24 = 24, bt16 = 16, bt8 = 8 };

int solidmap[MAPY][MAPX]; // reversed for compatibility with path finder library
int color_depth, height, width,font_scale, framecounter,regulated_speed, mouse_buttons;
bool game_active,sys_fps;
int ID = 1;
int tick = 0;
double gametime; // global
double timeinc; // global

BITMAP *buffer = NULL;
BITMAP *minimap_temp = NULL;
BITMAP *minimap_streched = NULL;
BITMAP *images = NULL;

void clear() { // clear screen
    clear_bitmap(buffer);
    clear_keybuf();
}

int color(int r, int g, int b, int a = 255) { // pull RGB color
    return makeacol32(r,g,b,a);
}

int getch() { // wait for key press
    return readkey();
}

const char *c_char(char c) {
    string temp;
    temp = c;
    return temp.c_str();
}

int dist(int y0,int x0,int y1,int x1) {
    return pow(y0-y1,2)+pow(x0-x1,2);
}

bool in_range(int y0,int x0,int y1,int x1,int r) {
    return dist(y0,x0,y1,x1)<=pow(r,2);
}

string IntToString(int intValue) {
    char *myBuff;
    string strRetVal;
    myBuff = new char[100];
    memset(myBuff,'\0',100);
    itoa(intValue,myBuff,10);
    strRetVal = myBuff;
    delete[] myBuff;
    return(strRetVal);
}

const char *add_ichar(const char *add0, int add1) {
    string temp;
    temp = add0;
    temp += IntToString(add1);
    return temp.c_str();
}

bool getkey(int input) {
    return (key[input]) ? true : false;    // check if key was pressed
}

void print(int y, int x,const char *c,int foreground_color,int background_color) {
    textout_ex(buffer, font, c, x*8, y*8, foreground_color, background_color);
}

void xprint(int y, int x,const char *c,int foreground_color,int background_color) {
    textout_ex(buffer, font, c, x, y, foreground_color, background_color);
}


void refresh() { // draw bitmap
    blit(buffer,screen,0,0,0,0,width,height);
}

struct cord {
public:
    int y,x;
    cord() {
        y,x = 0;
    }
    cord(int y0, int x0) {
        y = y0;
        x = x0;
    };
    void set(int y0, int x0) {
        y = y0;
        x = x0;
    };
};

struct fadingDetails {
public:
    int y,x,col,vol;
    bool wobble;
    bool alive; // on/off switch (off is set once cycle is complete)
    string c;

    void create(string text, int y0, int x0, int xcol, bool xwobble = true) {
        // create new combat details
        y = y0;
        x = x0;
        col = xcol;
        alive = true;
        wobble = xwobble;
        c = text;
        vol = 0;
    }

    void draw() {
        int newx = x;
        int newy = y;
        if (wobble == true) {
            int dir = rand()%1;
            if (dir == 1) {
                newx-=rand()%4;
            } else {
                newx+=rand()%4;
            }
        }
        xprint(y, newx-1, c.c_str(), COLOR_WHITE, COLOR_NONE);
        xprint(y, newx+1, c.c_str(), COLOR_WHITE, COLOR_NONE);
        xprint(y-1, newx, c.c_str(), COLOR_WHITE, COLOR_NONE);
        xprint(y+1, newx, c.c_str(), COLOR_WHITE, COLOR_NONE);
        xprint(y, newx, c.c_str(), col, COLOR_NONE);
    }

    void tick() {
        if (y > 0) {
            y -= vol;
            vol++;
        } else {
            alive = false;
        }
    }
};

int MouseClicked() {
    return mouse_b;
}

bool MouseWithinLocation(int y, int x) {
    if ( ( x+1 > (mouse_x / font_scale))
            && ( x+1 < (mouse_x / font_scale)+2)
            && ( y+1 > (mouse_y / font_scale))
            && ( y+1 < (mouse_y / font_scale)+2) ) {
        return true;
    } else {
        return false;
    }
}

bool MouseWithinBounds(int start_y, int start_x, int end_y, int end_x) {
    if ( ( start_x > mouse_x+end_x) && ( start_y > mouse_y+end_y) ) {
        return true;
    } else {
        return false;
    }
}

int GetMouseX_OnScreen() {
    return mouse_x/font_scale;
}
int GetMouseY_OnScreen() {
    return mouse_y/font_scale;
}

// ========================= ZOMBIES =========================== //

class obj_zombie {
public:
    bool active, alerted, spotted;
    int hp, damage, view_r;
    int y, x, dir, col;
    int spawn_tick, respawn_rate;
    int attack_tick, attack_speed, spotted_life, max_spotted;
    obj_zombie() : active(false), hp(100), view_r(8), spawn_tick(0), respawn_rate(rand()%3000+20) {}
    void create(int y0, int x0) {
        active = true;
        col = color(rand()%255+150,rand()%255+150,rand()%255+150);
        y = y0;
        x = x0;
        hp = 100;
        damage = rand()%3+1;
        attack_tick = 0;
        attack_speed = rand()%10;
        view_r = 8;
        spawn_tick = 0;
        respawn_rate = 0;
        max_spotted = 200;
    }
};

// ========================= NPC =========================== //

class obj_npc {
public:
    string name, info;
    int ID, damage, hp, y, x;
    bool alive;
    double attack_speed, movement_speed;
    int sensor_r;
    char c;
    void create(int x_y, int x_x, string n, string i, int x_hp, int x_dmg, int sens) {
        name = n;
        info = i;
        y = x_y;
        x = x_x;
        hp = x_hp;
        damage = x_dmg;
        sensor_r = sens;
        alive = true;
    }
    bool is_dead() {
        if (hp <= 0) {
            alive = false;
            return true;
        } else {
            alive = true;
            return false;
        }
    }
};

// ========================= ITEMS =========================== //

typedef struct def_buff {
    int stat;
    bool damage_self;
    double lifetime,lifetime_tick;
    int aura_radius; // 0 skill aplies to self;
    double value;
} def_buff;

class def_skillset {
private:
    string name, info;
public:
    def_buff buff;
    void init(string xname, string xinfo, def_buff xbuff) {
        name = xname;
        info = xinfo;
        buff = xbuff;
    }
};

class obj_player {
public:
    string name, info, title;
    int race;
    int view_dist;
    inventorySystem bag;
    int hp,strength,armour,agility,stanima,sp,level;
    int location, y ,x, direction, attack_tick, knockback_tick;
    int critical,damage,experiance;
    double attack_speed,movement_speed, guarding, attacking, guard_knockback;
    bool visible, solid, spotted;
    def_skillset skills[20];
    def_buff debuff[20];
    int calc_damage() {
        srand(time(NULL));
        int i = rand()%100;
        int d = rand()%damage + strength;
        if (i < critical) {
            return d * 2;     // was critical attack
        } else {
            return d;    // wasn't critical attack
        }
    }
};

obj_player p1;

struct obj_t { // tile class
public:
    int obj_type, obj_layer, ani_position, obj_trigger, ani_tick, ani_speed, fcolor, bcolor; // indicatiors
    bool obj_solid, obj_visible, animate; // options

    int id, hp;
    cord cords;

    char c[20]; // acsi character, 20 animation frames

    void create(int layer, string xc, int xfcolor, int xbcolor, int type = NONE) {
        obj_layer = layer;
        for (int i = 0; i < 20; i++) {
            c[i]=xc[i];
        }
        fcolor = xfcolor;
        bcolor = xbcolor;
        // applie deafult settings to tile
        obj_visible = true;
        if (c[1] != 0) {
            animate = true;
        } else {
            animate = false;
        }
        ani_speed = VERYFAST;
        ani_position = 0;
        obj_solid = true;
        obj_type = type;
        hp = 100;
    }
    void ani_setframe(int frame,char xchar) {
        c[frame] = xchar;    // init tile
    }

    void spr_animate() { // shift frame position (if possible)
        if (ani_tick >= ani_speed) {
            if (c[ani_position+1] != 0) { // check if next frame exists
                ani_position++;
            } else {
                ani_position = 0;
            }
            if (ani_position > 19) { // reset if reached last frame
                ani_position = 0;
            }
            ani_tick = 0;
        } else {
            ani_tick++;
        }
    }
};

class obj_b { // main board class
public:
    obj_t tiles[MAPY+1][MAPX+1];
    obj_zombie zombies[MAX_ZOMBIES];
    obj_npc npc[200];
    obj_b() { // clear map
        int count = 0;
        for (int y = 0; y < MAPY; y++) {
            for (int x = 0; x < MAPX; x++) {
                tiles[y][x].id = count;
                tiles[y][x].c[0] = ' ';
                tiles[y][x].obj_layer = 0;
                tiles[y][x].obj_visible = false;
                tiles[y][x].obj_solid = false;
                tiles[y][x].cords.set(y,x);
                tiles[y][x].hp = 100;
                count++;
            }
        }
    }

    bool isZombieAt(int y, int x) {
        bool result = false;
        for (int i = 0; i < MAX_ZOMBIES; i++) {
            if ((zombies[i].y == y) && (zombies[i].x == x)) {
                if (zombies[i].hp > 0) {
                    result = true;
                } else {
                    break;
                }
            }
        }
        return result;
    }

    void moveZombies() {
        for (int i = 0; i < MAX_ZOMBIES; i++) {
            if ( (zombies[i].active == true) && (zombies[i].hp > 0) ) {
                if (zombies[i].spotted == true) {
                    if (zombies[i].spotted_life < zombies[i].max_spotted) {
                        zombies[i].spotted_life++;
                    } else { zombies[i].spotted = false; zombies[i].spotted_life = 0; }
                }
                if ( (in_range(zombies[i].y,zombies[i].x,p1.y,p1.x,zombies[i].view_r)) && (los(p1.y,p1.x,zombies[i].y,zombies[i].x))) {
                    // move towards player
                    int r = rand()%20; // more chance to movement
                    if (r <= 5) {
                        if ( (zombies[i].x < p1.x-1) && (tiles[zombies[i].y][zombies[i].x+1].obj_solid == false) && (isZombieAt(zombies[i].y, zombies[i].x+1) != true)) {
                            if (isZombieAt(zombies[i].y,zombies[i].x+1) == false) {
                                zombies[i].x++;
                                if (zombies[i].x > MAPX) { zombies[i].x = MAPX; }
                            }
                        }
                        if ( (zombies[i].x > p1.x+1) && (tiles[zombies[i].y][zombies[i].x-1].obj_solid == false) && (isZombieAt(zombies[i].y, zombies[i].x+1) != true)) {
                            if (isZombieAt(zombies[i].y,zombies[i].x-1) == false) {
                                zombies[i].x--;
                                if (zombies[i].x < 0) { zombies[i].x = 0; }
                            }
                        }
                        if ( (zombies[i].y < p1.y-1) && (tiles[zombies[i].y+1][zombies[i].x].obj_solid == false) && (isZombieAt(zombies[i].y, zombies[i].x+1) != true)) {
                            if (isZombieAt(zombies[i].y+1,zombies[i].x) == false) {
                                zombies[i].y++;
                                if (zombies[i].y > MAPY) { zombies[i].y = MAPY; }
                            }
                        }
                        if ( (zombies[i].y > p1.y+1) && (tiles[zombies[i].y-1][zombies[i].x].obj_solid == false) && (isZombieAt(zombies[i].y, zombies[i].x+1) != true)) {
                            if (isZombieAt(zombies[i].y-1,zombies[i].x) == false) {
                                zombies[i].y--;
                                if (zombies[i].y < 0) { zombies[i].y = 0; }
                            }
                        }
                    }
                } else {
                    // move randomly
                    int r = rand()%100; // less/aimless change of movement
                    int dx = zombies[i].x;
                    int dy = zombies[i].y;
                    switch(r) {
                    case 0:
                        break;
                    case 1:
                        dx--;
                        break;
                    case 2:
                        dy++;
                        break;
                    case 3:
                        dx++;
                    case 4:
                        dy--;
                        break;
                    }
                    if (dx > MAPX) { dx = MAPX; }
                    if (dy > MAPY) { dy = MAPY; }
                    if (dx < 0) { dx = 0; }
                    if (dy < 0) { dy = 0; }
                    if (tiles[dy][dx].obj_solid == false) {
                        zombies[i].x = dx;
                        zombies[i].y = dy;
                    }
                }
            }
        }
    }
    bool los(int x0, int y0, int x1, int y1) {
        int sx,sy, xnext, ynext, dx, dy;
        float denom, dist;
        dx = x1-x0;
        dy = y1-y0;
        if (x0 < x1)
            sx = 1;
        else
            sx = -1;
        if (y0 < y1)
            sy = 1;
        else
            sy = -1;
        xnext = x0;
        ynext = y0;
        denom = sqrt(dx * dx + dy * dy);
        while (xnext != x1 || ynext != y1) {
            if (tiles[xnext][ynext].obj_solid == true) { // or any equivalent
                return false;
            }
            if(abs(dy * (xnext - x0 + sx) - dx * (ynext - y0)) / denom < 0.5f)
                xnext += sx;
            else if(abs(dy * (xnext - x0) - dx * (ynext - y0 + sy)) / denom < 0.5f)
                ynext += sy;
            else {
                xnext += sx;
                ynext += sy;
            }
        }
        return true;
    }
    int GetMouseX_OnMap() {
        return tiles[mouse_y/font_scale][mouse_x/font_scale].cords.x;
    }
    int GetMouseY_OnMap() {
        return tiles[mouse_y/font_scale][mouse_x/font_scale].cords.y;
    }

    // =============================== MAP GEN ============================
    // draw circle
    void drawOcean(int y0, int x0, int radius, char fill_ch, int layer = 1) {
        for (int y = 0; y < MAPY-1; y++) {
            for (int x = 0; x < MAPX-1; x++) {
                if (!in_range(y0,x0,y,x,radius)) {
                    tiles[y][x].c[0] = fill_ch;
                    tiles[y][x].obj_layer = layer;
                    tiles[y][x].obj_solid = true;
                    tiles[y][x].obj_visible = true;
                    tiles[y][x].fcolor = COLOR_GREEN;
                    tiles[y][x].bcolor = COLOR_BLACK;
                    tiles[y][x].hp = 100;
                }
            }
        }
    }

    void drawCluster(int amount, int r_base, char fill_ch, bool solid, int type, int layer = 2) {
        srand(time(NULL));
        for (int i = 0; i < amount; i++) {
            int radius = rand()%r_base;
            int x_offset = rand()%MAPX;
            int y_offset = rand()%MAPY;
            for (int y = y_offset-radius; y < y_offset+radius; y++) {
                for (int x = x_offset-radius; x < x_offset+radius; x++) {
                    if (in_range(y_offset,x_offset,y,x,radius)) {
                        if ( (y > 0) && (y < MAPY) && (x > 0) && (x < MAPX) ) {
                            tiles[y][x].c[0] = fill_ch;
                            tiles[y][x].obj_layer = layer;
                            tiles[y][x].obj_type = type;
                            tiles[y][x].obj_solid = solid;
                            tiles[y][x].obj_visible = true;
                            if (type == TREE) {
                                tiles[y][x].fcolor = color(rand()%50,rand()%100+50,0);
                                tiles[y][x].animate = false;
                            } else if (type == WATER) {
                                tiles[y][x].fcolor = color(0,rand()%25,50);
                                tiles[y][x].c[1] = '-';
                                tiles[y][x].animate = true;
                                tiles[y][x].ani_speed = 50;
                            } else if (type == FLOOR) {
                                tiles[y][x].fcolor = color(rand()%25,rand()%50,rand()%20);
                                tiles[y][x].animate = false;
                            }
                            tiles[y][x].bcolor = COLOR_BLACK;
                            tiles[y][x].hp = 100;
                        }
                    }
                }
            }
        }
    }
    // draw Rect
    void drawRect(int start_x, int start_y, int end_x, int end_y, char outline, char fill, bool outline_solid, bool fill_solid,
                  int outline_col, int fill_col, int layer = 1) {
        // fill
        for (int y_ = start_y; y_ < end_y; y_++) {
            for (int x_ = start_x; x_ < end_x; x_++) {
                tiles[y_][x_].c[0] = fill;
                tiles[y_][x_].obj_layer = layer;
                tiles[y_][x_].obj_solid = fill_solid;
                tiles[y_][x_].obj_visible = true;
                tiles[y_][x_].fcolor = color(0,rand()%25,50);
                tiles[y_][x_].c[1] = '-';
                tiles[y_][x_].obj_type = WATER;
                tiles[y_][x_].animate = true;
                tiles[y_][x_].ani_speed = 50;
                tiles[y_][x_].bcolor = COLOR_BLACK;
                tiles[y_][x_].hp = 100;
            }
        }
        // outline
        for (int y_ = start_y; y_ < end_y; y_++) {
            if (outline == '-') {
                tiles[y_][start_x].c[0] = '|';
            } else {
                tiles[y_][start_x].c[0] = outline;
            }
            tiles[y_][start_x].obj_layer = layer;
            tiles[y_][start_x].obj_solid = outline_solid;
            tiles[y_][start_x].obj_visible = true;
            tiles[y_][start_x].fcolor = outline_col;
            tiles[y_][start_x].bcolor = COLOR_BLACK;
            if (outline == '-') {
                tiles[y_][end_x].c[0] = '|';
            } else {
                tiles[y_][end_x].c[0] = outline;
            }
            tiles[y_][end_x].obj_layer = layer;
            tiles[y_][end_x].obj_solid = outline_solid;
            tiles[y_][end_x].obj_visible = true;
            tiles[y_][end_x].fcolor = outline_col;
            tiles[y_][end_x].bcolor = COLOR_BLACK;
        }
        for (int x_ = start_x; x_ < end_x+1; x_++) {
            tiles[start_y][x_].c[0] = outline;
            tiles[start_y][x_].obj_layer = layer;
            tiles[start_y][x_].obj_solid = outline_solid;
            tiles[start_y][x_].obj_visible = true;
            tiles[start_y][x_].fcolor = outline_col;
            tiles[start_y][x_].bcolor = COLOR_BLACK;
            tiles[end_y][x_].c[0] = outline;
            tiles[end_y][x_].obj_layer = layer;
            tiles[end_y][x_].obj_solid = outline_solid;
            tiles[end_y][x_].obj_visible = true;
            tiles[end_y][x_].fcolor = outline_col;
            tiles[end_y][x_].bcolor = COLOR_BLACK;
        }
    }
    // forest gen
    void genForest() {
        srand(time(NULL));
        int progress = 0;
        int maxprogress = 100;
        while (progress < maxprogress) {
            clear();
            if (progress == 1) {
                // draw ground
                drawRect(0,0,MAPX,MAPY,'~','~',false,false,COLOR_WHITE,COLOR_FLOOR);
                drawCluster(100000, 20, '.', false, FLOOR);
            } else if (progress == 2) {
                // draw trees
                drawCluster(40000, 20, 'T', true, TREE);
            } else if (progress == 3) {
                drawCluster(2000, 20, '~', false, WATER);
            } else if (progress == 96) {
                int spawned = 0;
                while (spawned < MAX_LOOTS) {
                    int x0 = rand()%MAPX;
                    int y0 = rand()%MAPY;
                    if (tiles[y0][x0].obj_solid == false) {
                        p1.bag.newLoot(rand()%15+1, y0, x0);
                        spawned++;
                    }
                }
            } else if (progress == 97) {
                int spawned = 0;
                while (spawned < MAX_ZOMBIES-1) {
                    int x0 = rand()%MAPX;
                    int y0 = rand()%MAPY;
                    if (tiles[y0][x0].obj_solid == false) {
                        spawned++;
                        zombies[spawned].create(y0,x0);
                    }
                }
            } else if (progress == 98) {
                int spawned = 0;
                while (spawned < MAX_BUILDINGS-1) {
                    int x0 = rand()%MAPX;
                    int y0 = rand()%MAPY;
                    int i = rand()%12;
                    bool space_free = true;
                    if ((x0 < 50) || (x0 > MAPX-50) || (y0 < 50) || (y0 > MAPY-50)) {
                    } else {
                        for (int y = y0; y < y0+10; y++) {
                            for (int x = x0; x < x0+18; x++) {
                                if ( (tiles[y][x].obj_solid == true) || (tiles[y][x].obj_type == WATER) || (tiles[y][x].obj_type == TREE) ) { space_free = false; }
                            }
                        }
                        if (space_free == true) {
                            for (int y2 = 0; y2 < 10; y2++) {
                                for (int x2 = 0; x2 < 18; x2++) {
                                    if (Small_buildings[i][y2][x2] != '.') {
                                        if (Small_buildings[i][y2][x2] == 'D') {
                                             tiles[y2+y0][x2+x0].create(2,c_char(Small_buildings[i][y2][x2]),color(115,70,36),COLOR_BLACK, DOOR);
                                        } else {  tiles[y2+y0][x2+x0].create(2,c_char(Small_buildings[i][y2][x2]),color(30+i*2,30+i*2,16+i*2),COLOR_BLACK); }
                                    }
                                }
                            }
                            spawned++;
                        }
                    }
                }
            } else if (progress == 99) {
                p1.x = rand()%MAPX;
                p1.y = rand()%MAPY;
                for (int y = p1.y-6; y < p1.y+6; y++) {
                    for (int x = p1.x-6; x < p1.x+6; x++) {
                        if (in_range(y,x,p1.y,p1.x,6)) {
                            tiles[y][x].hp = -1;

                        }
                    }
                }
                p1.bag.newLoot(rand()%15+1, p1.y, p1.x);
                p1.bag.newLoot(rand()%15+1, p1.y, p1.x);
                p1.bag.newLoot(rand()%15+1, p1.y, p1.x);
                p1.bag.newLoot(rand()%15+1, p1.y, p1.x);
                p1.bag.newLoot(rand()%15+1, p1.y, p1.x);
                p1.bag.newLoot(rand()%15+1, p1.y, p1.x);
                p1.bag.newLoot(rand()%15+1, p1.y, p1.x);
                p1.bag.newLoot(rand()%15+1, p1.y, p1.x);
                p1.bag.newLoot(rand()%15+1, p1.y, p1.x);
                p1.bag.newLoot(rand()%15+1, p1.y, p1.x);
                p1.bag.newLoot(rand()%15+1, p1.y, p1.x);
                p1.bag.newLoot(rand()%15+1, p1.y, p1.x);
                p1.bag.newLoot(rand()%15+1, p1.y, p1.x);
                p1.bag.newLoot(rand()%15+1, p1.y, p1.x);
                p1.bag.newLoot(rand()%15+1, p1.y, p1.x);
                p1.bag.newLoot(rand()%15+1, p1.y, p1.x);
                p1.bag.newLoot(rand()%15+1, p1.y, p1.x);
                p1.bag.newLoot(rand()%15+1, p1.y, p1.x);
            }
            progress++;
            string info = "Loading... " + IntToString(progress) + "%";
            print(25,25,info.c_str(), COLOR_WHITE, COLOR_BLACK);
            refresh();
        }
    }
    // ===================== ZOMBIE FUNCTIONS ===================== //
    void playerSpotted() {
        p1.spotted = false;
        for (int i = 0; i < MAX_ZOMBIES; i++) {
            if (zombies[i].active == true) {
                if ((in_range(zombies[i].y,zombies[i].x,p1.y,p1.x,zombies[i].view_r)) && los(p1.y,p1.x,zombies[i].y,zombies[i].x) ) {
                    zombies[i].alerted = true;
                    p1.spotted = true;

                } else {
                    zombies[i].alerted = false;
                }
            }
        }
    }
    double radar_pulse;
    double radar_pause;
    bool soundWave;
    void drawHUD() {
        // ===================== SOUND RADAR ===================== //
        //circle(minimap_temp,(minimap_temp->w/2)+6,(minimap_temp->h/2)-7,10,color(140,0,0));
        if (soundWave == true) {
            for (int y = 0; y < MAPY; y++) {
                for (int x = 0; x < MAPX; x++) {
                    int tempx = p1.x - (width/2)/8 + x; // for map scrolling
                    int tempy = p1.y - (height/2)/8 + y; // for map scrolling
                    if ((tempx < 0) || (tempx > MAPX) || (tempy < 0) || (tempy > MAPY)) {
                    } else {
                        if (in_range(tempy,tempx,p1.y,p1.x,radar_pulse)) {
                            // ===================== Draw Zombies ===================== //
                            for (int i = 0; i < MAX_ZOMBIES; i++) {
                                if ( (zombies[i].y == tempy) && (zombies[i].x == tempx) && (zombies[i].active == true) ) {
                                    zombies[i].spotted = true;
                                }
                            }
                            print(y,x,tiles[tempy][tempx].c,color(50,0,0),tiles[tempy][tempx].bcolor);
                        }
                    }
                }
            }

            if (radar_pulse < p1.view_dist+20) {
                radar_pulse+=1.5;
            } else {
                radar_pulse=0;
                soundWave=false;
            }
        }

        string temp_hp,temp_sp;
        // HP
        for (int i = 4; i < (p1.hp/3); i++) {
            print(i,1,"|",COLOR_GREEN,COLOR_BLACK);
        }
        // SP
        for (int i = 4; i < (p1.sp/3); i++) {
            print(i,3,"|",color(i*i,i*i,0),COLOR_BLACK);
        }
        for (int i = 4; i < (height/8)-4; i++) {
            print(i,2,"|",color(i*4,i*4,i*4),COLOR_BLACK);
        }
        print(3,1,"-",color(65,65,65),COLOR_BLACK);
        print(3,2,"-",color(65,65,65),COLOR_BLACK);
        print(3,3,"-",color(65,65,65),COLOR_BLACK);

        print((height/8)-4,1,"-",COLOR_WHITE,COLOR_BLACK);
        print((height/8)-4,2,"-",COLOR_WHITE,COLOR_BLACK);
        print((height/8)-4,3,"-",COLOR_WHITE,COLOR_BLACK);


        // attacking hud
        print((height/8)-3, (width/8)-17,"  SPACE     X    ",COLOR_RED,COLOR_BLACK);
        print((height/8)-2, (width/8)-17,"-----------------",color(65,65,65),COLOR_BLACK);
        print((height/8)-1, (width/8)-15,"                 ",COLOR_BLACK,COLOR_BLACK);
        if ( (p1.attacking == true) || (p1.guard_knockback == true) ) {
            print((height/8)-1, (width/8)-15,"-|==>",color(0,255,255),COLOR_BLACK);
        } else {
            print((height/8)-1, (width/8)-15,"-|==>",COLOR_WHITE,COLOR_BLACK);
        }
        // guarding hud
        if ( (p1.guarding == true) || (p1.guard_knockback == true) ) {
            print((height/8)-1, (width/8)-6,"[ ]",color(0,255,255),COLOR_BLACK);
        } else {
            print((height/8)-1, (width/8)-6,"[ ]",COLOR_WHITE,COLOR_BLACK);
        }
        // actions hud
        print((height/8)-1, (width/8)-17,"|",color(65,65,65),COLOR_BLACK);
        print((height/8)-1, (width/8)-9,"|",color(65,65,65),COLOR_BLACK);
        print((height/8)-1, (width/8)-1,"|",color(65,65,65),COLOR_BLACK);

        // visuals hud
        if (p1.spotted == true) {
            print (1,(width/8)-4,"<_>",color(65,65,65),COLOR_BLACK);
            print (1,(width/8)-3, "o",COLOR_RED,COLOR_BLACK);
        } else {
            print (1,(width/8)-4,"<_>",color(65,65,65),COLOR_BLACK);
        }
        // Draw Time gague
        print(10,(width/8)-1,"O",color(100,100,150),COLOR_BLACK);
        print((height/8)-5,(width/8)-1,"*",COLOR_YELLOW,COLOR_BLACK);
        int col_grad = 0;
        for (int y = 11; y < ((int)gametime)/61; y++) {
            print(y,(width/8)-1,"|",color(col_grad,col_grad,0),COLOR_BLACK);
            col_grad+=10;
        }
    }
    // ============================ DRAWING EVERYTHING ELSE ================================== //
    void draw() {
        for (int y = 0; y < (height/8)-1; y++) { // scan y-axis
            for (int x = 0; x < (width/8)-1; x++) { // scan x-axis
                int tempx = p1.x - (width/2)/8 + x; // for map scrolling
                int tempy = p1.y - (height/2)/8 + y; // for map scrolling
                if ( (tempx < 0) || (tempx > MAPX) || (tempy < 0) || (tempy > MAPY) ) {
                    // don't print
                } else {
                    if (tiles[tempy][tempx].hp > 0) {
                        if (tiles[tempy][tempx].obj_visible == true) { // check if map obj is visible
                            if (tiles[tempy][tempx].c[tiles[tempy][tempx].ani_position] != 0) { // check if char aint empty
                                if ( (in_range(tempy,tempx,p1.y,p1.x,p1.view_dist)) && los(p1.y,p1.x,tempy,tempx) ) {
                                    print(y,x, c_char(tiles[tempy][tempx].c[tiles[tempy][tempx].ani_position]) ,tiles[tempy][tempx].fcolor,tiles[tempy][tempx].bcolor);
                                    // ===================== animate ties ===================== //
                                    if (tiles[tempy][tempx].animate == true) {
                                        tiles[tempy][tempx].spr_animate();
                                    }
                                    // ===================== system ===================== //
                                    if (sys_fps == true) {
                                        if (tiles[tempy][tempx].obj_solid == true) {
                                            print(y,x, c_char(tiles[tempy][tempx].c[tiles[tempy][tempx].ani_position]),COLOR_BLUE,tiles[tempy][tempx].bcolor);
                                        } else {
                                            print(y,x, c_char(tiles[tempy][tempx].c[tiles[tempy][tempx].ani_position]) ,tiles[tempy][tempx].fcolor,tiles[tempy][tempx].bcolor);
                                        }
                                    }
                                    // ===================== Draw Items ===================== //
                                    if (!p1.bag.loots.empty()) {
                                        for (int i = 0; i < p1.bag.loots.size(); i++) {
                                            if ( (p1.bag.loots.at(i).y == tempy) && (p1.bag.loots.at(i).x == tempx) ) {
                                                print(y,x,"X",color(0,255,255),COLOR_BLACK);
                                            }
                                        }
                                    }


                                    // ===================== Draw NPC's ===================== //
                                    //for (int i = 0; i < 2000; i++) { if (npc[i].) }
                                    // ===================== Draw Zombies ===================== //
                                    for (int i = 0; i < MAX_ZOMBIES; i++) {
                                        if ( (zombies[i].y == tempy) && (zombies[i].x == tempx) && (zombies[i].active == true) ) {
                                            if (zombies[i].alerted == true) {
                                                xprint(y*8, x*8-1, "@", COLOR_WHITE, COLOR_NONE);
                                                xprint(y*8, x*8+1, "@", COLOR_WHITE, COLOR_NONE);
                                                xprint(y*8-1, x*8, "@", COLOR_WHITE, COLOR_NONE);
                                                xprint(y*8+1, x*8, "@", COLOR_WHITE, COLOR_NONE);
                                                zombies[i].spotted = true;
                                                zombies[i].spotted_life = 0;
                                            }
                                            print(y,x,"@",zombies[i].col,COLOR_BLACK);
                                        }
                                    }
                                } else {
                                    //print(y,x,c_char(tiles[tempy][tempx].c[0]),COLOR_SHADOW,COLOR_BLACK);
                                    for (int i = 0; i < MAX_ZOMBIES; i++) {
                                        if ( (zombies[i].y == tempy) && (zombies[i].x == tempx) && (zombies[i].active == true) ) {
                                            if (zombies[i].spotted == true) {
                                                print(y,x,"x",COLOR_RED,COLOR_NONE);
                                            }
                                        }
                                    }
                                }
                                // ===================== Mouse target ===================== //
                                if (MouseWithinLocation(y,x)) {
                                    print(y,x, c_char(tiles[tempy][tempx].c[tiles[tempy][tempx].ani_position]),COLOR_RED,tiles[tempy][tempx].bcolor);
                                }

                            }
                        }
                    } else {
                        // reset destroyed objects to ground object
                        tiles[tempy][tempx].obj_visible = true;
                        tiles[tempy][tempx].c[0] = '.';
                        tiles[tempy][tempx].hp = 100;
                        tiles[tempy][tempx].fcolor = COLOR_FLOOR;
                        tiles[tempy][tempx].obj_solid = false;
                        tiles[tempy][tempx].animate = false;
                        tiles[tempy][tempx].obj_type = NONE;
                    }
                    tiles[y][x].cords.set(tempy,tempx);
                }
            }
        }
    }
};

volatile long SpeedCounter = 0;
int max_fps = 12;
volatile int fps, fpsCounter;

void FPS() {
    fps = fpsCounter;
    if (fps < max_fps-1) {
        regulated_speed-=1;
    } else if (fps > max_fps+1) {
        regulated_speed+=1;
    }
    if (regulated_speed < 0) {
        regulated_speed = 0;
    } else if (regulated_speed > 200) {
        regulated_speed = 200;
    }
    fpsCounter = 0;
}
END_OF_FUNCTION(FPS)

class sys_game {
public:
    int sel, lootCount;
    int invSel, menuSel, invCount;
    bool looting, openInv, invMenu, shift_key;
    obj_b board; // game board

    // ======================= BATTLE SYSTEM (MEELE) - Sword and shield ===================== //
    fadingDetails battleLogs[LOGS];
    void drawBattleLogs() { // draw all alive logs
        for (int i = 0; i < LOGS-1; i++) {
            if(battleLogs[i].alive == true) {
                battleLogs[i].draw();
            }
        }
    }
    void processBattleLogs() { // change in movement
        for (int i = 0; i < LOGS-1; i++) {
            if(battleLogs[i].alive == true) {
                battleLogs[i].tick();
            }
        }
    }
    void newBattleLog(string text, int y, int x, int col) { // wobble and fade are true
        for (int i = 0; i < LOGS-1; i++) {
            if (battleLogs[i].alive == false) {
                battleLogs[i].create(text, y,x,col);
                break;
            }
        }
    }
    void doZombieDamage() { // damage to player
        for (int i = 0; i < MAX_ZOMBIES; i++) {
            // zombie attack
            if ( (board.zombies[i].active == true) && (in_range(p1.y,p1.x,board.zombies[i].y,board.zombies[i].x,2)) && (p1.guarding == false) ) {
                if (board.zombies[i].attack_tick > board.zombies[i].attack_speed) {
                    int damTotal = rand()%board.zombies[i].damage;
                    p1.hp-=damTotal;
                    if (damTotal != 0) {
                        newBattleLog("-" + IntToString(damTotal),(height)/2,((width)/2)-16, color(200,20,20));
                    }
                    board.soundWave = false; // player was inturrupted
                    board.zombies[i].attack_tick = 0;
                } else {
                    board.zombies[i].attack_tick++;
                }
            }
        }
    }
    void cutTrees() {
        if (p1.attacking == true) {
            switch(p1.direction) {
            case UP:
                if ( (board.tiles[p1.y-1][p1.x].obj_type == TREE) && (board.tiles[p1.y-1][p1.x].obj_visible == true) ) {
                    board.tiles[p1.y-1][p1.x].hp -= 100;
                }
                if ( (board.tiles[p1.y-1][p1.x-1].obj_type == TREE) && (board.tiles[p1.y-1][p1.x].obj_visible == true) ) {
                    board.tiles[p1.y-1][p1.x-1].hp -= 50;
                    board.tiles[p1.y-1][p1.x-1].c[0] = 't';
                }
                if ( (board.tiles[p1.y-1][p1.x+1].obj_type == TREE) && (board.tiles[p1.y-1][p1.x].obj_visible == true) ) {
                    board.tiles[p1.y-1][p1.x+1].hp -= 50;
                    board.tiles[p1.y-1][p1.x+1].c[0] = 't';
                }
                break;

            case DOWN:
                if ( (board.tiles[p1.y+1][p1.x].obj_type == TREE) && (board.tiles[p1.y+1][p1.x].obj_visible == true) ) {
                    board.tiles[p1.y+1][p1.x].hp -= 100;
                }
                if ( (board.tiles[p1.y+1][p1.x-1].obj_type == TREE) && (board.tiles[p1.y+1][p1.x-1].obj_visible == true) ) {
                    board.tiles[p1.y+1][p1.x-1].hp -= 50;
                    board.tiles[p1.y+1][p1.x-1].c[0] = 't';
                }
                if ( (board.tiles[p1.y+1][p1.x+1].obj_type == TREE) && (board.tiles[p1.y+1][p1.x+1].obj_visible == true) ) {
                    board.tiles[p1.y+1][p1.x+1].hp -= 50;
                    board.tiles[p1.y+1][p1.x+1].c[0] = 't';
                }
                break;

            case LEFT:
                if ( (board.tiles[p1.y][p1.x-1].obj_type == TREE) && (board.tiles[p1.y][p1.x-1].obj_visible == true) ) {
                    board.tiles[p1.y][p1.x-1].hp -= 100;
                }
                if ( (board.tiles[p1.y+1][p1.x-1].obj_type == TREE) && (board.tiles[p1.y+1][p1.x-1].obj_visible == true) ) {
                    board.tiles[p1.y+1][p1.x-1].hp -= 50;
                    board.tiles[p1.y+1][p1.x-1].c[0] = 't';
                }
                if ( (board.tiles[p1.y-1][p1.x-1].obj_type == TREE) && (board.tiles[p1.y+1][p1.x-1].obj_visible == true) ) {
                    board.tiles[p1.y-1][p1.x-1].hp -= 50;
                    board.tiles[p1.y-1][p1.x-1].c[0] = 't';
                }
                break;

            case RIGHT:
                if ( (board.tiles[p1.y][p1.x+1].obj_type == TREE) && (board.tiles[p1.y][p1.x+1].obj_visible == true) ) {
                    board.tiles[p1.y][p1.x+1].hp -= 100;
                }
                if ( (board.tiles[p1.y+1][p1.x+1].obj_type == TREE) && (board.tiles[p1.y+1][p1.x+1].obj_visible == true) ) {
                    board.tiles[p1.y+1][p1.x+1].hp -= 50;
                    board.tiles[p1.y+1][p1.x+1].c[0] = 't';
                }
                if ( (board.tiles[p1.y-1][p1.x+1].obj_type == TREE) && (board.tiles[p1.y+1][p1.x+1].obj_visible == true) ) {
                    board.tiles[p1.y-1][p1.x+1].hp -= 50;
                    board.tiles[p1.y-1][p1.x+1].c[0] = 't';
                }
                break;
            }
        }
    }
    void doDamage() { // damage delt to a zombie
        for (int i = 0; i < MAX_ZOMBIES; i++) {
            // player attack
            if ( (p1.attacking == true) && (p1.guarding == false) ) {
                if (p1.direction == UP) {
                    if (board.zombies[i].active == true) {
                        if ( (board.zombies[i].x == p1.x-1) || (board.zombies[i].x == p1.x) || (board.zombies[i].x == p1.x+1) ) { // x
                            if ( (board.zombies[i].y == p1.y-1) || (board.zombies[i].y == p1.y-2) ) { // y
                                int damTotal = p1.calc_damage();
                                board.zombies[i].hp -= damTotal;
                                newBattleLog("-" + IntToString(damTotal),(height)/2,(width)/2, COLOR_BLACK);
                            }
                        }
                    }
                } else if (p1.direction == DOWN) {
                    if (board.zombies[i].active == true) {
                        if ( (board.zombies[i].x == p1.x-1) || (board.zombies[i].x == p1.x) || (board.zombies[i].x == p1.x+1) ) { // x
                            if ( (board.zombies[i].y == p1.y+1) || (board.zombies[i].y == p1.y+2) ) { // y
                                int damTotal = p1.calc_damage();
                                board.zombies[i].hp -= damTotal;
                                newBattleLog("-" + IntToString(damTotal),(height)/2,(width)/2, COLOR_BLACK);
                            }
                        }
                    }
                } else if (p1.direction == LEFT) {
                    if (board.zombies[i].active == true) {
                        if ( (board.zombies[i].x == p1.x-1) || (board.zombies[i].x == p1.x-2) ) { // x
                            if ( (board.zombies[i].y == p1.y+1) || (board.zombies[i].y == p1.y) || (board.zombies[i].y == p1.y-1) ) { // y
                                int damTotal = p1.calc_damage();
                                board.zombies[i].hp -= damTotal;
                                newBattleLog("-" + IntToString(damTotal),(height)/2,(width)/2, COLOR_BLACK);
                            }
                        }
                    }
                } else if (p1.direction == RIGHT) {
                    if (board.zombies[i].active == true) {
                        if ( (board.zombies[i].x == p1.x+1) || (board.zombies[i].x == p1.x+2) ) { // x
                            if ( (board.zombies[i].y == p1.y+1) || (board.zombies[i].y == p1.y) || (board.zombies[i].y == p1.y-1) ) { // y
                                int damTotal = p1.calc_damage();
                                board.zombies[i].hp -= damTotal;
                                newBattleLog("-" + IntToString(damTotal),(height)/2,(width)/2, COLOR_BLACK);
                            }
                        }
                    }
                }
            }
        }
    }

    void game_loop() {
        // main loop
        while(game_active == true) {
            clear();
            // process battle details
            processBattleLogs();
            // ZOMBIE MOVEMENT
            board.moveZombies();
            /* =============================== INPUT ============================
            if ( (MouseClicked() == LEFT) && (pathLocation[1] == pathLength[1]) ) {

                for (int y = 0; y < MAPY; y++) {
                    for (int x = 0; x < MAPX; x++) {
                        if (board.tiles[y][x].obj_solid == true) {
                            walkability[x][y] = 1;
                        } else {
                            solidmap[x][y] = 0;
                        }
                    }
                }
                pathStatus[1] = FindPath(1,p1.x,p1.y,board.GetMouseX_OnMap(),board.GetMouseY_OnMap());
            }
            if (pathStatus[1] == found) {
                if (pathLocation[1] != pathLength[1]) {
                    ReadPath(1,p1.x,p1.y);
                    p1.x = xPath[1];
                    p1.y = yPath[1];
                }
            } */
            if (board.soundWave == false) {
                if (getkey(KEY_ESC)) { // exit on Escape key
                    game_active = false;
                } else if (getkey(KEY_F1)) { // toggle fps
                    if (sys_fps == true) {
                        sys_fps = false;
                    } else {
                        sys_fps = true;
                    }
                }
                if (getkey(KEY_LSHIFT)) {
                    shift_key = true;
                } else { shift_key = false; }
                if ( (getkey(KEY_A)) && (!shift_key) ) {
                    if (!openInv) {
                        p1.direction = LEFT;
                        if ( (board.tiles[p1.y][p1.x-1].obj_solid == false) || (board.tiles[p1.y][p1.x-1].obj_type == DOOR) ) {
                            if (board.isZombieAt(p1.y,p1.x-1) != true) {
                                p1.x--;
                            }
                        }
                        if (p1.x < 0) {
                            p1.x = 0;
                        }
                    } else {
                        if (invMenu) { invMenu = false; } else {invMenu = true; }
                    }
                }
                if ( (getkey(KEY_D)) && (!shift_key)) {
                    if (!openInv) {
                    p1.direction = RIGHT;
                        if ( (board.tiles[p1.y][p1.x+1].obj_solid == false) || (board.tiles[p1.y][p1.x+1].obj_type == DOOR) ) {
                            if (board.isZombieAt(p1.y,p1.x+1) != true) {
                                p1.x++;
                            }
                        }
                        if (p1.x > MAPX) {
                            p1.x = MAPX;
                        }
                    } else {
                        if (invMenu) { invMenu = false; } else {invMenu = true; }
                    }
                }
                if (getkey(KEY_W)) {
                    if (!shift_key) {
                        if (!openInv) {
                            p1.direction = UP;
                            if ( (board.tiles[p1.y-1][p1.x].obj_solid == false) || (board.tiles[p1.y-1][p1.x].obj_type == DOOR) ) {
                                if (board.isZombieAt(p1.y-1,p1.x) != true) {
                                    p1.y--;
                                }
                            }
                            if (p1.y < 0) {
                                p1.y = 0;
                            }
                        } else {
                            if (invMenu) { menuSel--; } else { invSel--; }
                        }
                    } else { sel--; if (sel < 0) { sel = 0; } }
                }
                if (getkey(KEY_S)) {
                    if (!shift_key) {
                        if (!openInv) {
                            p1.direction = DOWN;
                            if ( (board.tiles[p1.y+1][p1.x].obj_solid == false) || (board.tiles[p1.y+1][p1.x].obj_type == DOOR) ) {
                                if (board.isZombieAt(p1.y+1,p1.x) != true) {
                                    p1.y++;
                                }
                            }
                            if (p1.y > MAPY) {
                                p1.y = MAPY;
                            }
                        } else {
                            if (invMenu) { menuSel++; } else { invSel++; }
                        }
                    } else { sel++; if (sel > lootCount-1) { sel = lootCount-1; } }
                }

                // attack
                if (getkey(KEY_SPACE)) {
                    if ( (!shift_key) && (!looting) ) {
                        if (!openInv) {
                            if (p1.sp >= 20) {
                                if ((p1.attacking == false) && (p1.guarding == false)) {
                                    p1.attacking = true;
                                    p1.sp-=20;
                                    doDamage();
                                    cutTrees();
                                }
                            } else if ( (p1.attacking == false) && (p1.guarding == true) ) {
                                if (p1.sp >= 10) {
                                    p1.guard_knockback = true;
                                    p1.sp-=10;
                                    p1.guarding = false;
                                }
                            }
                        }
                    }
                }
                if (getkey(KEY_E)) { if (openInv == true) { openInv = false; } else { openInv = true; invMenu = false; invCount = p1.bag.items.size(); } }
            }

            if (getkey(KEY_Q)) {
                board.soundWave = true;
            }
            // ===============================   DRAW   ===========================
            board.draw(); // draw map

            // ============================   LOOTING   ===========================

            if (openInv) {
                // inv window
                print (0,5,"|               |   |   |", COLOR_WHITE,COLOR_BLACK);
                print (1,5,"|               |   |   |", COLOR_WHITE,COLOR_BLACK);
                print (2,5,"|               |   |   |", COLOR_WHITE,COLOR_BLACK);
                print (3,5,"|               |   |   |", COLOR_WHITE,COLOR_BLACK);
                print (4,5,"|               |   |   |", COLOR_WHITE,COLOR_BLACK);
                print (5,5,"|               |   |   |", COLOR_WHITE,COLOR_BLACK);
                print (6,5,"|               |   |   |", COLOR_WHITE,COLOR_BLACK);
                print (7,5,"|               |   |   |", COLOR_WHITE,COLOR_BLACK);
                print (8,5,"|               |   |   |", COLOR_WHITE,COLOR_BLACK);
                print (9,5,"|               |   |   |", COLOR_WHITE,COLOR_BLACK);
                print(10,5,"|-----------------------|", COLOR_WHITE,COLOR_BLACK);
                print(11,5,"|               |       |", COLOR_WHITE,COLOR_BLACK);
                print(12,5,"o-----------------------o", COLOR_WHITE,COLOR_BLACK);

                if (!p1.bag.items.empty()) {
                    // draw weight info
                    p1.bag.updateCapacity();
                    std::ostringstream currentWeight;
                    currentWeight << p1.bag.currentCapacity;
                    std::ostringstream maxWeight;
                    maxWeight << p1.bag.maxCapacity;
                    string info = currentWeight.str() + "/" + maxWeight.str();
                    print(11,22,info.c_str(),COLOR_YELLOW,COLOR_BLACK);

                    if (invSel > p1.bag.items.size()-1) { invSel = p1.bag.items.size()-1; }
                    else if (invSel < 0) { invSel = 0; }
                    if (menuSel > 5) { menuSel = 5; }
                    else if (menuSel < 0) { menuSel = 0; }

                    for (int i = 0; i < 10; i++) {
                        int tempy = invSel - i; // for map scrolling
                        if (!(tempy > p1.bag.items.size()) || !(tempy < 0) ) {
                            std::ostringstream itemWeight;
                            std::ostringstream itemQuantity;
                            itemWeight << p1.bag.items.at(tempy).weight;
                            itemQuantity << p1.bag.items.at(tempy).quantity;
                            if (invSel == tempy) {
                                print(0+i,6,p1.bag.items.at(tempy).c.c_str(), COLOR_RED, COLOR_BLACK);
                                print(0+i,8,p1.bag.items.at(tempy).itemName.c_str(),COLOR_RED,COLOR_BLACK);
                                print(0+i,22,itemWeight.str().c_str(),COLOR_RED,COLOR_BLACK);
                                print(0+i,26,itemQuantity.str().c_str(),COLOR_RED,COLOR_BLACK);
                                if (invMenu) {
                                    if (menuSel == 0) {
                                        print (0+i,30,"> Drop", COLOR_RED,COLOR_BLACK);
                                    } else { print (0+i,30,"Drop", COLOR_WHITE,COLOR_BLACK); }
                                    if (menuSel == 1) {
                                        print (1+i,30,"> Use", COLOR_RED,COLOR_BLACK);
                                    } else { print (1+i,30,"Use", COLOR_WHITE,COLOR_BLACK); }

                                    if(getkey(KEY_SPACE)) {
                                        if (menuSel == 0) { // drop item
                                            p1.bag.bagToLoot(p1.bag.items.at(tempy).ID,p1.y,p1.x);
                                            invCount--;
                                        }
                                    }
                                }
                            } else {
                                print(0+i,6,p1.bag.items.at(tempy).c.c_str(), COLOR_WHITE, COLOR_BLACK);
                                print(0+i,8,p1.bag.items.at(tempy).itemName.c_str(),COLOR_WHITE,COLOR_BLACK);
                                print(0+i,22,itemWeight.str().c_str(),COLOR_WHITE,COLOR_BLACK);
                                print(0+i,26,itemQuantity.str().c_str(),COLOR_WHITE, COLOR_BLACK);
                            }

                        }
                    }
                } else {
                    print (1,5,"   Bag is empty", COLOR_WHITE,COLOR_BLACK);
                }
            }

            if (!looting) {
                lootCount = 0;
                sel = 0;
                for (int i = 0; i < p1.bag.loots.size(); i++) {
                    if (( p1.bag.loots.at(i).y == p1.y) && (p1.bag.loots.at(i).x == p1.x) ) {
                        lootCount++;
                    }
                    looting = true;
                }
            } else if ((shift_key) && (looting)) {
                int n = 0;
                for (int i = 0; i < p1.bag.loots.size(); i++) {
                    if (( p1.bag.loots.at(i).y == p1.y) && (p1.bag.loots.at(i).x == p1.x) ) {
                        if (sel == n) {
                            if (getkey(KEY_SPACE)) {
                                p1.bag.lootToBag(p1.bag.loots.at(i).y, p1.bag.loots.at(i).x, p1.bag.loots.at(i).ID);
                                clear_keybuf();
                                looting = false;
                            } else { print(25+n, 25+3, p1.bag.loots.at(i).itemName.c_str(), COLOR_YELLOW,COLOR_BLACK); }
                        } else { print(25+n, 25+3, p1.bag.loots.at(i).itemName.c_str(), COLOR_WHITE,COLOR_BLACK); }
                        n++;
                    }
                }
            } else {
                sel = 0;
                lootCount = 0;
                looting = false;
            }

            // ===============================   DRAW PLAYER  ===========================
            print((height/8)/2,(width/8)/2,"@",color(200,0,0),COLOR_BLACK); // draw player

            // draw guard
            if (p1.guarding == true) {
                if (p1.direction == LEFT) {
                    print(((height/8)/2),((width/8)/2)-1,"|",color(0,255,255),COLOR_BLACK);
                } else if (p1.direction == RIGHT) {
                    print(((height/8)/2),((width/8)/2)+1,"|",color(0,255,255),COLOR_BLACK);
                } else if (p1.direction == UP) {
                    print(((height/8)/2)-1,((width/8)/2),"-",color(0,255,255),COLOR_BLACK);
                } else if (p1.direction == DOWN) {
                    print(((height/8)/2)+1,((width/8)/2),"-",color(0,255,255),COLOR_BLACK);
                }
                p1.sp-=2;
            }
            // draw knockback animation
            if (p1.guard_knockback == true) {
                if (p1.direction == LEFT) {
                    print(((height/8)/2),((width/8)/2)-2,"|",color(0,255,255),COLOR_BLACK);
                    p1.guard_knockback = false;
                } else if (p1.direction == RIGHT) {
                    print(((height/8)/2),((width/8)/2)+2,"|",color(0,255,255),COLOR_BLACK);
                    p1.guard_knockback = false;
                } else if (p1.direction == UP) {
                    print(((height/8)/2)-2,((width/8)/2),"-",color(0,255,255),COLOR_BLACK);
                    p1.guard_knockback = false;
                } else if (p1.direction == DOWN) {
                    print(((height/8)/2)+2,((width/8)/2),"-",color(0,255,255),COLOR_BLACK);
                    p1.guard_knockback = false;
                }
            }
            // draw attacking animation
            if (p1.attacking == true) {
                if (p1.direction == LEFT) { // animation left
                    string attack_ani;
                    int x_offset;
                    if ( p1.attack_tick == 0) {
                        x_offset = 1;
                        attack_ani = "\\";
                    }
                    if ( p1.attack_tick == 1) {
                        x_offset = 2;
                        attack_ani = "-";
                    }
                    if ( p1.attack_tick == 2) {
                        x_offset = 1;
                        attack_ani = "/";
                    }
                    print( (((height/8)/2)-1) + p1.attack_tick, ((width/8)/2)-x_offset ,attack_ani.c_str(),color(0,255,255),COLOR_BLACK);
                    p1.attack_tick++;
                    if (p1.attack_tick >= 6) {
                        p1.attacking = false;
                        p1.attack_tick = 0;
                    }
                } else if (p1.direction == RIGHT) { // animation right
                    string attack_ani;
                    int x_offset;
                    if ( p1.attack_tick == 0) {
                        x_offset = 1;
                        attack_ani = "/";
                    }
                    if ( p1.attack_tick == 1) {
                        x_offset = 2;
                        attack_ani = "-";
                    }
                    if ( p1.attack_tick == 2) {
                        x_offset = 1;
                        attack_ani = "\\";
                    }
                    print( (((height/8)/2)-1) + p1.attack_tick, ((width/8)/2)+x_offset ,attack_ani.c_str(),color(0,255,255),COLOR_BLACK);
                    p1.attack_tick++;
                    if (p1.attack_tick >= 6) {
                        p1.attacking = false;
                        p1.attack_tick = 0;
                    }
                } else if (p1.direction == UP) { // animation up
                    string attack_ani;
                    int x_offset;
                    if ( p1.attack_tick == 0) {
                        x_offset = 1;
                        attack_ani = "\\";
                    }
                    if ( p1.attack_tick == 1) {
                        x_offset = 2;
                        attack_ani = "|";
                    }
                    if ( p1.attack_tick == 2) {
                        x_offset = 1;
                        attack_ani = "/";
                    }
                    print( ((height/8)/2)-x_offset , (((width/8)/2)-1)+ p1.attack_tick,attack_ani.c_str(),color(0,255,255),COLOR_BLACK);
                    p1.attack_tick++;
                    if (p1.attack_tick >= 6) {
                        p1.attacking = false;
                        p1.attack_tick = 0;
                    }
                } else if (p1.direction == DOWN) { // animation down
                    string attack_ani;
                    int x_offset;
                    if ( p1.attack_tick == 0) {
                        x_offset = 1;
                        attack_ani = "/";
                    }
                    if ( p1.attack_tick == 1) {
                        x_offset = 2;
                        attack_ani = "|";
                    }
                    if ( p1.attack_tick == 2) {
                        x_offset = 1;
                        attack_ani = "\\";
                    }
                    print( ((height/8)/2)+x_offset , (((width/8)/2)-1)+ p1.attack_tick,attack_ani.c_str(),color(0,255,255),COLOR_BLACK);
                    p1.attack_tick++;
                    if (p1.attack_tick >= 6) {
                        p1.attacking = false;
                        p1.attack_tick = 0;
                    }
                }
            }
            drawBattleLogs();
            board.drawHUD();

            // ============================ ZOMBIE CHECKS ============================= //
            doZombieDamage();
            // check if player spotted
            board.playerSpotted();
            // remove all dead zombies | Respawn
            for (int i = 0; i < MAX_ZOMBIES; i++) {
                if (board.zombies[i].hp < 0) {
                    board.zombies[i].active = false;
                    board.zombies[i].respawn_rate = rand()%300+20;
                }
                //if (board.count_zombiesAlive() < 200) {
                if (board.zombies[i].active == false) {
                    if(board.zombies[i].spawn_tick > board.zombies[i].respawn_rate) {
                        srand(time(NULL));
                        int rand_y = rand()%MAPY;
                        int rand_x = rand()%MAPX;
                        if ( (board.tiles[rand_y][rand_x].obj_solid == false) && (board.tiles[rand_y][rand_x].obj_visible == true) ) {
                            board.zombies[i].create(rand_y,rand_x);
                        }
                    } else {
                        board.zombies[i].spawn_tick+=1;
                    }
                }
                //} // 200 max per map
            }
            // add time of day
            gametime += timeinc;
            if (gametime <= 0) {
                timeinc = 0.20;
            } else if (gametime >= 2000) {
                timeinc = -0.20;
            }
            p1.view_dist = ((int)gametime/200)+6;
            p1.sp += 1.5;
            if (p1.sp > 100) {
                p1.sp = 100;
            }
            if (p1.sp < 0) {
                p1.sp = 0;
            }
            // =============================== FPS REG ==z==========================
            while(SpeedCounter>1) {
                SpeedCounter--;
                framecounter++;
            }
            fpsCounter++;

            if (sys_fps == true) { // print fps
                print(1,1,add_ichar("Frames Per Second  : ", fps),COLOR_WHITE,COLOR_BLACK);
                print(3,1,add_ichar("Regulated Speed    : ", regulated_speed),COLOR_WHITE,COLOR_BLACK);

                print(5,1,add_ichar("MOUSE X : ", mouse_x),COLOR_WHITE,COLOR_BLACK);
                print(6,1,add_ichar("MOUSE Y : ", mouse_y),COLOR_WHITE,COLOR_BLACK);

                print(9,1,add_ichar("xPath : ", xPath[1]),COLOR_WHITE,COLOR_BLACK);
                print(10,1,add_ichar("yPath : ", yPath[1]),COLOR_WHITE,COLOR_BLACK);
                print(11,1,add_ichar("MOUSE ON MAP X :", board.GetMouseX_OnMap()),COLOR_WHITE,COLOR_BLACK);
                print(12,1,add_ichar("MOUSE ON MAP Y :", board.GetMouseY_OnMap()),COLOR_WHITE,COLOR_BLACK);
                print(14,1,add_ichar("Time counter : ", gametime),COLOR_YELLOW,COLOR_BLACK);

                if (MouseClicked() == LEFT) {
                    print(8,1,"LEFT",COLOR_RED,COLOR_BLACK);
                    circlefill(buffer, mouse_x, mouse_y,
                               10, COLOR_RED);
                } else if (MouseClicked() == RIGHT) {
                    print(8,1,"RIGHT",COLOR_RED,COLOR_BLACK);
                    circlefill(buffer, mouse_x, mouse_y,
                               10, COLOR_RED);
                }
            }
            for (int i = 0; i < p1.bag.loots.size(); i++) {
                if ( (p1.bag.loots.at(i).y == p1.y) && (p1.bag.loots.at(i).x == p1.x) ) {
                    print(p1.y-2,p1.x,p1.bag.loots.at(i).itemName.c_str(), COLOR_WHITE, COLOR_BLACK);

                }
            }
            refresh();
            rest(regulated_speed);
            // seed rand function
            //srand(time(NULL));
        }
    }
};

sys_game gameloop;

int main() {
    // ===============================   SETUP   ==========================
    color_depth           =   bt32;
    width                 =   500;
    height                =   300;
    font_scale            =   8;
    max_fps               =   12;
    regulated_speed       =   15; // deafult

    // options
    sys_fps               =   false; // deafult

    // set up library
    if (allegro_init() != 0) {
        allegro_message("Initalization Error: %s", allegro_error);
        return 1;
    }
    //Set the window title when in a GUI environment
    set_window_title("Apocalypse Day");

    if (install_keyboard()) {
        allegro_message("Keyboard Error: %s", allegro_error);
        return 1;
    }
    mouse_buttons = install_mouse();
    show_os_cursor(MOUSE_CURSOR_ARROW);
    install_timer();
    if(install_sound(DIGI_AUTODETECT, MIDI_AUTODETECT, NULL)) {
        allegro_message("Sound Error: %s", allegro_error);
        return 1;
    }
    set_volume(255, 255);

    // set to unicode
    set_uformat(U_UTF8);

    // set up fps regulator
    LOCK_FUNCTION(FPS);
    LOCK_VARIABLE(fpsCounter);
    LOCK_VARIABLE(fps);
    install_int_ex(FPS,BPS_TO_TIMER(1));

    // set up buffer
    buffer = create_bitmap_ex(color_depth,width,height);
    minimap_temp = create_bitmap_ex(color_depth,50,50);
    minimap_streched = create_bitmap_ex(color_depth,200,200);

    // set drawing mode
    set_trans_blender(255, 0, 255, 255);
    set_alpha_blender();
    drawing_mode(DRAW_MODE_TRANS,NULL,0,0);
    set_color_depth(color_depth);
    // set graphics mode
    if (!set_gfx_mode(GFX_AUTODETECT_WINDOWED, width, height, 0, 0)) {
        game_active = true;
    } else {
        game_active = false;
        allegro_message("Graphics error: %s", allegro_error);
        return 1;
    }

    // ===============================   MAIN   ===========================
    // init map
    gameloop.board;
    p1.x = 1;
    p1.y = 1;
    p1.view_dist = 10;
    p1.hp = 100;
    p1.sp = 100;
    p1.damage = 50;
    p1.strength = 5;
    p1.critical = 5;
    gameloop.board.genForest();
    gametime = 2000;
    timeinc = -0.20;
    SAMPLE *son ;
    son= load_sample("data/bg_music.wav"); // or .voc
    if (son!=0) play_sample(son, 255, 0, 1000, 1);
    else allegro_message("error music");
    gameloop.game_loop();
    destroy_bitmap(buffer);
    destroy_bitmap(minimap_temp);
    destroy_bitmap(minimap_streched);
    return 0;
}
END_OF_MAIN()
