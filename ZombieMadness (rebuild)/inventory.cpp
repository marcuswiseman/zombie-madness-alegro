#include <iostream>
#include <vector>
#include <fstream>
#include <stdlib.h>

using namespace std;

enum WEP_ACTION { STAB, SWING, THROW, SHOOT };
enum ACTION { USE, CONSUME, EQUIPT };
enum TYPE { FLAMABLE, GENRAL, TOOL };

struct item {
public:
    int y, x;
    int ID;
    int quantity;
    int rarity;
    string itemName;
    string c;
    double weight;
    double durability;
    double storeLife;
    int action[5];
    int type[5];
    int itemsRequired[5];
};

class itemDatabase {
public:
    vector<item> items;

    void init(const char* filePath) {
        fprintf(stderr, "Loading Items!\n");
        ifstream infile(filePath);
        string line;
        item newItem;

        int a = 0, b = 0, c = 0, t = 0;
        int count = 0;
        if (infile) {
            while (getline(infile, line)) {
                if (line.find("ITEM_ID=") != string::npos) {
                    line.erase(0,8);
                    newItem.ID = atoi(line.c_str());
                    a=0, b=0, c=0, t=0;
                } else if (line.find("ITEM_NAME=") != string::npos) {
                    line.erase(0,10);
                    newItem.itemName = line;
                } else if (line.find("ITEM_CHAR=") != string::npos) {
                    line.erase(0,10);
                    newItem.c = line[0];
                } else if (line.find("ITEM_WEIGHT=") != string::npos) {
                    line.erase(0,12);
                    newItem.weight = atof(line.c_str());
                } else if (line.find("ITEM_RARITY=") != string::npos) {
                    line.erase(0,12);
                    newItem.rarity = atoi(line.c_str());
                } else if (line.find("ITEM_TYPE=") != string::npos) {
                    line.erase(0,10);
                    newItem.type[t] = atoi(line.c_str());
                    t++;
                } else if (line.find("ITEM_ACTION=") != string::npos) {
                    line.erase(0,12);
                    newItem.action[c] = atoi(line.c_str());
                    c++;
                } else if (line.find("ITEM_REQUIRED=") != string::npos) {
                    line.erase(0,14);
                    newItem.itemsRequired[b] = atoi(line.c_str());
                    b++;
                } else if (line.find("END_ITEM") != string::npos) {
                    newItem.quantity = 1;
                    items.push_back(newItem);
                    count++;

                }
            }

        } else { fprintf(stderr, "Failed to find file!\n"); }
        infile.close();
    }

};

class inventorySystem {
public:
    itemDatabase itemData;
    std::vector<item> items;
    std::vector<item> loots;
    double maxCapacity;
    double currentCapacity;
    bool loaded;

    inventorySystem() {
        itemData.init("data/items.txt");
        loaded = true;
        maxCapacity = 100;
        currentCapacity = 0.0;
    }

    void updateCapacity() {
        double total;
        for (int i = 0; i < items.size(); i++) {
            total += (items.at(i).weight*items.at(i).quantity);
        }
        currentCapacity = total;
    }

    bool checkCapacity(int additionalWeight) {
        bool result = false;
        if ((currentCapacity+additionalWeight) <= maxCapacity) {
            result = true;
        }
        return result;
    }

    void newLoot(int ID, int y, int x) {
        item newLoot = getItem(ID);
        newLoot.y = y;
        newLoot.x = x;
        newLoot.quantity = 1;
        loots.push_back(newLoot);
    }

    item getItem(int ID) {
        item xitem;
        for (int i = 0; i < itemData.items.size(); i++) {
            if (itemData.items.at(i).ID == ID) {
                xitem = itemData.items.at(i);
                break;
            }
        }
        return xitem;
    }

    void bagToLoot(int ID, int y, int x) {
        for (int i = 0; i < items.size(); i++) {
            if (items.at(i).ID == ID) {
                if (items.at(i).quantity > 1) {
                    items.at(i).quantity -= 1;
                } else {
                    items.erase(items.begin() + i);
                }
                newLoot(ID,y,x);
                updateCapacity();
                break;
            }
        }
    }

    void lootToBag(int y, int x, int ID) {
        for (int i = 0; i < loots.size(); i++) {
            if (loots.at(i).y == y) {
                if (loots.at(i).x == x) {
                    // move loot over to bag if space is avalible
                    item toLoot = getItem(loots.at(i).ID);
                    if (checkCapacity(toLoot.weight)) {
                        try {
                            items.push_back(toLoot);
                            loots.erase(loots.begin() + i);
                        } catch (string e) {
                            fprintf(stderr, e.c_str());
                            throw 3;
                        }
                        break;
                    }
                    updateCapacity();
                }
            }
        }
    }

};
