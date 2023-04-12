// TODO: Storing location data somehow rather than re-generating encounters for unique rooms
// TODO: NPCs and conversation using a topic-based system (you know things, NPC knows things, you get a list of the overlap)
// TODO: Add a pub where you can buy drinks and get drunk (status effect) - this reduces stress and increases morale
// TODO: Improve skills by studying at the library and spending your experience points. The more education you have, the cheaper.
// TODO: Strength and dexterity requirements for weapons. Failing either of these results in only being able to use the weapon at 50% of your skill points.
//       Small weapons tend to have lower requirements; dexterity requirements are more common.
// TODO: Smart use items; they are only picked up if you can't use them right now
// TODO: Weapon system that uses your skills. Probably when you equip a weapon, it modifies your attack by your firearms skill
// TODO: Add "flinch", which causes you or the enemy to miss a turn when hit sometimes

#include "raylib.h"
#include <stdlib.h>

#if defined(PLATFORM_WEB)
    #include <emscripten/emscripten.h>
#endif

typedef struct Dice {
    int num;
    int sides;
    int mod;
} Dice;

// ENTITIES

typedef enum {
    ENT_MONSTERSTART,
    ENT_WOLF, ENT_VIPER, ENT_RAT_THING, ENT_SPAWN, ENT_ZOMBIE,
    ENT_GUARD, ENT_TROOPER, ENT_OFFICER,
    ENT_OGRE, ENT_GHOUL, ENT_GHAST, ENT_WIGHT, ENT_LICH, ENT_DEMILICH,
    ENT_BYAKHEE, ENT_CHTHONIAN, ENT_COLOUR, ENT_DARK_YOUNG, ENT_DEEP_ONE, ENT_SHAMBLER, ENT_POLYP, ENT_HORROR,

    ENT_SPAWNERSTART,
    ENT_CELLARSPAWNER1, ENT_ITEMSPAWNER1, ENT_FEATURESPAWNER1,
    
    ENT_FEATURESTART,
    ENT_DEBRIS, ENT_SAFE, ENT_TABLE, ENT_EXPLODING_BARREL, ENT_WELL,
    ENT_TOUCHPLATE, ENT_PUSHWALL, ENT_SWITCH, ENT_LOCKED_DOOR, ENT_BOOKSHOP, ENT_GENERAL_STORE,
    ENT_GALLERY, ENT_BED,
    
    ENT_ITEMSTART,
    ENT_PISTOL, ENT_RIFLE, ENT_FLARE_GUN, ENT_DYNAMITE, ENT_SMG, ENT_FLAMETHROWER, ENT_HMG, ENT_CHAINGUN,
    ENT_NERVE_GAS_LAUNCHER, ENT_ROCKET_LAUNCHER, ENT_FIREBOMB, ENT_LIGHTNING_GUN,
    ENT_9MM, ENT_50CAL, ENT_FGAS, ENT_FRAG_GRENADE, ENT_NGAS, ENT_ROCKET, ENT_BATTERY,
    ENT_STIMPACK, ENT_MEDIKIT, ENT_BANDAGE, ENT_MORPHINE, ENT_SERUM, ENT_SEDATIVE,
    ENT_SCROLL_TELEPORT, ENT_SCROLL_CREATE_ZOMBIE, ENT_SCROLL_WITHER, ENT_SCROLL_SHRINK, ENT_SCROLL_HOLD,
    ENT_SCROLL_POISON, ENT_SCROLL_PARALYSE,
    ENT_KEY,
    
    ENT_PLOTSTART,
    ENT_LETTER, ENT_HUMPY_FRONT_DOOR, ENT_HUMPY_CELLAR_DOOR, ENT_HUMPY_CELLAR_KEY, ENT_HUMPY_BODY,
    ENT_END
} EntityId;

typedef enum ActorStates {
    STATE_IDLE, STATE_ALERTED, STATE_WANDER, STATE_HIDDEN, STATE_HEARD_SOMETHING
} ActorState;

typedef enum SpellId {
    SPELL_HOLD, SPELL_ROT, SPELL_POISON, SPELL_PARALYSE, SPELL_TELEPORT, SPELL_RAISE_ZOMBIE, SPELL_SHRINK,
    SPELL_MAX
} SpellId;

typedef struct Entity {
    char name[32];
    char examine[256];
    int hp;
    int portable;
    int attack; // increases chance to hit
    int defence; // decreases enemy chance to hit
    Dice damage;
    int fear;
    SpellId spells[32];
    EntityId ammoType;
    int stack; // default stack size when item is picked up - used mostly for ammo
    // autopopulated, not in data
    EntityId id;
    int hpMax;
    int created;
    ActorState state;
    bool actedThisTurn;
    bool seen;
    bool canHideIn;
} Entity;

typedef enum Attribute {
    ATTR_STR, ATTR_CON, ATTR_EDU, ATTR_INT, ATTR_DEX, ATTR_POW, ATTR_CHA,
    ATTR_MAX
} Attribute;

typedef enum Skill {
    SKILL_CLIMBING, 
    // TODO: A number of crafting skills to create consumable items. CRAFT_AUTOMATON is based on Arcanum, CRAFT_TRAP is based on the Assassin from Diablo 2
    SKILL_CRAFT_AUTOMATON, SKILL_CRAFT_TRAP, 
    SKILL_DODGE,
    SKILL_FIREARM_AUTOMATICS, SKILL_FIREARM_ARTIILLERY, SKILL_FIREARM_HANDGUNS, SKILL_FIREARM_RIFLES, SKILL_FIREARM_SHOTGUNS,
    SKILL_FIRST_AID, SKILL_LOCKSMITH,
    // TODO: Each school of magic has its own skill, and there's a general spellcasting system; DCSS-inspired
    SKILL_MAGIC_ALTERATION, SKILL_MAGIC_CONJURATION, SKILL_MAGIC_ILLUSION, SKILL_MAGIC_SPELLCASTING, SKILL_MAGIC_SUMMONING,
    SKILL_SPOT_HIDDEN, SKILL_STEALTH, SKILL_THROW,
    SKILL_MAX
} Skill;

typedef struct SkillData {
    char name[32];
} SkillData;

SkillData skillData[SKILL_MAX] = {
    {"Climbing"},
    {"Craft Automaton"},
    {"Craft Trap"},
    {"Dodge"},
    {"Automatics"},
    {"Artillery"},
    {"Handguns"},
    {"Rifles"},
    {"Shotguns"},
    {"First aid"},
    {"Locksmithing"},
    {"Alteration Magic"},
    {"Conjuration Magic"},
    {"Illusion Magic"},
    {"Spellcasting"},
    {"Summoning Magic"},
    {"Spot Hidden"},
    {"Stealth"},
    {"Throwing"}
};

typedef struct SpellData {
    SpellId id;
    char name[32];
    int cost;
    Skill school;
} SpellData;

SpellData spellData[SPELL_MAX] = {
    (SpellData) {SPELL_HOLD, "Grasping Tentacles", 10, SKILL_MAGIC_SUMMONING},
    (SpellData) {SPELL_ROT, "Word of Withering", 10, SKILL_MAGIC_ALTERATION},
    (SpellData) {SPELL_POISON, "Serpent Sting", 10, SKILL_MAGIC_SUMMONING},
    (SpellData) {SPELL_PARALYSE, "Paralysing Fog", 10, SKILL_MAGIC_CONJURATION},
    (SpellData) {SPELL_TELEPORT, "Teleport", 10, SKILL_MAGIC_ALTERATION},
    (SpellData) {SPELL_RAISE_ZOMBIE, "Raise Zombie", 10, SKILL_MAGIC_SUMMONING},
    (SpellData) {SPELL_SHRINK, "Diminuation Ray", 10, SKILL_MAGIC_ALTERATION}
};

int Skill_calculateXP(int currentSkill, int education) {
    int cost = 0;
    if (currentSkill < 50) {
        cost = 100 - currentSkill - education;
    } else {
        int incLevel = currentSkill - 50;
        cost = 50 + incLevel*incLevel - education;
    }
    if (cost < 1) {
        cost = 1;
    }
    return cost;
}

bool Skill_test(int currentSkill, int difficultyMod) {
    int roll = GetRandomValue(1, 100) + difficultyMod;
    return roll <= currentSkill;
}

typedef struct PlayerStats {
    int stress;
    int stressRecovery;
    int stressResistance;
    int morale;
    int satiation;
    int rest;
    int actions;
    int actionsMax;
    EntityId weapon; 
    Attribute attributes[ATTR_MAX];
    Skill skills[SKILL_MAX]; 
    int xp;
    int level;
    int sp; // Skill points can be spent by studying, modified by EDU.
    int spellSlotsLeft;
    int spellsKnown;
    int mp;
    int mpMax;
} PlayerStats;

PlayerStats playerStats = (PlayerStats) {
    .stressRecovery=10,
    .morale=4,
    .satiation=4,
    .rest=4,
    .attributes={10,10,10,10,10,10,10},
    .skills={10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10},
    .spellSlotsLeft=1,
    .mp=20,
    .mpMax=20
};
Entity playerEntity = (Entity) {"Player", "It's you.", 100, 0, 4, 4, {1, 3, 0}};

// TODO: Load from external file (compiled CSV file)
Entity entities[ENT_END] = {
    (Entity) {"= MONSTER START ="},
    (Entity) {"Wolf", "A wolf, snarling and ready to attack!", 25, 0, 5, 5, {1, 6, 0}, 5},
    (Entity) {"Viper", "Snakes... why did it have to be snakes?", 8, 0, 8, 10, 10},
    (Entity) {"Rat Thing", "A squat, deformed hybrid of rat and man, standing about 3 feet tall.", 10, 0, 3, 8, {1, 4, 0}, 10},
    (Entity) {"Formless Spawn", "A pulsing aboeba-like thing that flows more than moves.", 25, 0, 5, 5, {1, 6, 0}, 15},
    (Entity) {"Zombie", "A rotting corpse that has somehow been re-animated.", 50, 0, 3, 3, {1, 8, 2}, 15},
    (Entity) {"Guard", "Dressed in a brown uniform.", 25, 8, 6},
    (Entity) {"Trooper", "A bulky soldier in a blue uniform.", 100},
    (Entity) {"Officer", "Resplendent in his white uniform.", 75},
    (Entity) {"Ogre", "A bloated, gassy abomination, covered in weeping sores.", 200},
    (Entity) {"Goul", "This foul, grey, skeletal creature looks at you with insatiable hunger.", 100},
    (Entity) {"Ghast", "Rubbery skin and a gaping maw; long of limb and grasping claw.", 125},
    (Entity) {"Wight", "A deathly cold emanates from this shadowy figure that always seems just out of focus.", 150},
    (Entity) {"Lich", "The skeletal remains of a mighty sorceror, dressed in tattered rags.", 300},
    (Entity) {"Demilich", "A floating skull, crackling with arcane energies.", 500},
    (Entity) {"Byakhee", "A misshapen monstrosity somewhere between a bat and a faceless dog.", 75},
    (Entity) {"Chthonian", "Utterly alien to behold, a cylinder of green flesh and tentacle.", 400},
    (Entity) {"Colour out of Space", "A strange and unearthly hue suffuses the area.", 100},
    (Entity) {"Dark Young", "Thorny and tentacled, the nightmarish spawn of some blasphemous outer God.", 150},
    (Entity) {"Deep One", "Repulsive, staring fish-eyes and a vacant mouth; a lithe physique.", 75},
    (Entity) {"Shambler", "Seeming to shimmer half in and half out of existence; grinning and voracious.", 250},
    (Entity) {"Polyp", "This flying cone, surrounded in waving filaments, seems to move impossibly.", 125},
    (Entity) {"Hunting Horror", "A bony, skeletal nightmare, all tooth and claw.", 200},

    (Entity) {"= SPAWNER START = "},
    (Entity) {"Monster Spawner 1"},
    (Entity) {"Item Spawner 1"},
    (Entity) {"Feature Spawner 1"},

    (Entity) {"= FEATURE START ="},
    (Entity) {"Pile of debris", "A pile of random bits and pieces.", 1000, .canHideIn=true},
    (Entity) {"Safe", "A sturdy cabinet, locked with a combination lock.", 1000},
    (Entity) {"Table", "A four-legged wooden surface with papers on it.", 1000, .canHideIn=true},
    (Entity) {"Exploding Barrel", "This red barrel is marked with stark warning signs.", 25},
    (Entity) {"Well", "Long and dark and deep is the drop. Who knows what's down there?", 1000},
    (Entity) {"Touchplate", "The floor here is just slightly offset. It's a trap!", 1000},
    (Entity) {"Pushwall", "Part of the wall here appears to be slightly offset.", 1000},
    (Entity) {"Switch", "A lever, set into the floor.", 1000},
    (Entity) {"Locked Door", "The door is locked with a key. Maybe you have it, maybe you don't.", 1000},
    (Entity) {"Book shop", "A book shop, filled with interesting tomes. Improve your skills here.", 1000},
    (Entity) {"Counter", "A bored-looking assistant waits to take your order.", 1000},
    (Entity) {"Ticket office", "An attendant waits behind a desk, selling tickets to the gallery.", 1000},
    (Entity) {"Bed", "A bed. Use it to sleep and recover rest and HP (up to 50 percent of max)", 1000, .canHideIn=true},

    (Entity) {"= ITEM START ="},
    (Entity) {"Pistol", "A compact hand-held weapon. Fires 9mm rounds.", 1000, 1, .attack=8, .damage={1, 6, 0}, .ammoType=ENT_9MM},
    (Entity) {"Rifle", "A long, shoulder-supported firearm. Fires .50 calibre rounds.", 1000, 1, .attack=15, .damage={1, 8, 0}, .ammoType=ENT_50CAL},
    (Entity) {"Flare Gun", "A pistol for shooting flares. Can lead to a fiery demise.", 1000, 1},
    (Entity) {"Dynamite", "A bundle of red sticks. They tend to go boom.", 1000, 1},
    (Entity) {"Sub Machine Gun", "Rapidly fires 9mm rounds, depeting your ammo and also the enemy's life.", 1000, 1},
    (Entity) {"Flamethrower", "A tube with a fuel canister attached. It will burn things.", 1000, 1},
    (Entity) {"Heavy Machine Gun", "Large, heavy weapon that rapidly fires .50 cal rounds.", 1000, 1},
    (Entity) {"Chaingun", "Experimental multi-barrelled gatling gun. Shoots .50 cal rounds at a ludicrous rate.", 1000, 1},
    (Entity) {"Nerve Gas Launcher", "A large tube with a drum magazine that fires shells of nerve gas.", 1000, 1},
    (Entity) {"Rocket Launcher", "Shoulder-mounted cylinder; it fires rockets, as one would expect.", 1000, 1},
    (Entity) {"Firebomb", "This large and scary-looking weapon lobs incendiary shells, with great devastation.", 1000, 1},
    (Entity) {"Lightning Gun", "A strange alien weapon which sends bolts of electrity coursing through the air.", 1000, 1},
    (Entity) {"9mm rounds", "A box if 9mm rounds, 16 in all.", 1000, 1, .stack=32},
    (Entity) {".50 cal rounds", "A box of .50 cal rounds, 32 in all.", 1000, 1},
    (Entity) {"Fuel", "A fuel canister, which you can use with a flamethrower.", 1000, 1},
    (Entity) {"Frag Grenade", "Pull pin, throw, big boom.", 1000, 1, .damage={3, 10, 10}},
    (Entity) {"Nerve Gas", "Canister of 4 nerve gas shells, to be used with the nerve gas launcher.", 1000, 1},
    (Entity) {"Rocket", "A rocket. Launch it from a rocket launcher.", 1000, 1},
    (Entity) {"Battery", "A large battery that can be connected to electrical devices to power them.", 1000, 1},
    (Entity) {"Stimpack", "A small one-use medical kit. Heals 10 points of damage.", 1000, 1},
    (Entity) {"Medikit", "A full medikit. Heals 25 points of damage.", 1000, 1},
    (Entity) {"Bandage", "Can be used to staunch bleeding in a pinch.", 1000, 1},
    (Entity) {"Morphine", "Prevents morale damage from being wounded.", 1000, 1},
    (Entity) {"Serum", "A strange blue liquid. When consumed, it will send you into a frenzy.", 1000, 1},
    (Entity) {"Sedative", "Calms you down, but be careful about overuse.", 1000, 1},
    (Entity) {"Scoll of Teleport", "Transdimensional energies instantly move you elsewhere, but you cannot control it precisely.", 1000, 1},
    (Entity) {"Scroll of Raise Zombie", "This dark magic creates a zombie to fight with you.", 1000, 1},
    (Entity) {"Scroll of Word of Wither", "This terrible spell will drain the life from all living targets, inflicting massive damage.", 1000, 1},
    (Entity) {"Scroll of Diminuation Ray", "A green ray of unearthly energies shrinks and enfeebles those it touches.", 1000, 1},
    (Entity) {"Scroll of Grasping Tentacles", "Tentacles burst from the floor and bind the target, immobilising it.", 1000, 1},
    (Entity) {"Scroll of Serpent's Sting", "A spectral black viper materialises and lunges at the target, injecting poison.", 1000, 1},
    (Entity) {"Scroll of Paralysing Fog", "A strange golden mist fills the air; those that breathe it fall, unable to move.", 1000, 1},
    (Entity) {"Key", "Used to unlock a locked door.", 1000, 1},

    (Entity) {"= PLOT FEATURE START ="},
    (Entity) {"Letter", ":introLetter", 1000, 1},
    (Entity) {"Front door", "This is the front door of Humpy's house.\nIt's closed but not locked.", 1000},
    (Entity) {"Cellar door", "The door to the cellar. It seems to be locked.\nThere's a strange smell coming from here...", 1000},
    (Entity) {"Iron key", "It's a large old iron key, rough to the touch.", 1000, 1},
    (Entity) {"Humpy's corpse", "The dead body of your friend.\n"
        "He is covered in what looks like claw marks.\n"
        "Blood still flows sluggishly from the wounds - he hasn't been"
        "\ndead long, evidently.", 1000
    }
};

Entity Entity_create(EntityId eid) {
    Entity e = entities[eid];
    int roll = 0;
    switch (eid) {
        case ENT_CELLARSPAWNER1:
            roll = GetRandomValue(ENT_RAT_THING, ENT_RAT_THING + 2);
            break;
        case ENT_FEATURESPAWNER1:
            roll = GetRandomValue(ENT_FEATURESTART + 1, ENT_ITEMSTART - 1);
            break;
        case ENT_ITEMSPAWNER1:
            roll = GetRandomValue(ENT_ITEMSTART + 1, ENT_PLOTSTART - 1);
            break;
        default:
            break;
    }
    if (roll) {
        e = entities[roll];
    }
    if (eid != ENT_MONSTERSTART && eid != ENT_SPAWNERSTART && eid != ENT_FEATURESTART && eid != ENT_ITEMSTART) {
        e.created = 1;
        e.hpMax = e.hp;
    }
    e.id = eid;
    return e;
}

bool Entity_isMob(Entity* e) {
    return e->created && e->hp > 0 && e->attack && e->defence;
}

// ENCOUNTERS

#define ENCOUNTER_MAX_ENTITY 8
#define LOCATION_MAX_ENCOUNTERS 128
#define ENCOUNTER_TEXT_LENGTH 512
#define ENCOUNTER_MAX_ROOM 64

typedef enum Direction {
    DIR_NORTH, DIR_EAST, DIR_SOUTH, DIR_WEST, DIR_MAX
} Direction;

char dirStrings[DIR_MAX][8] = {
    "North", "East", "South", "West"
};

typedef struct Room Room;
struct Room {
    char description[ENCOUNTER_TEXT_LENGTH];
    EntityId spawnList[ENCOUNTER_MAX_ENTITY];
    Room* exits[DIR_MAX];
    Entity entities[ENCOUNTER_MAX_ENTITY];
    int x;
    int y;
    bool created;
    bool custom;
};

typedef struct Encounter {
    Room rooms[ENCOUNTER_MAX_ROOM];
    Room* startingRoom;
} Encounter;

typedef struct EncounterTemplate {
    char name[32];
    void (*constructor)(Encounter*);
    int unique;
    int visited;
} EncounterTemplate;

typedef struct Location {
    EncounterTemplate encounters[LOCATION_MAX_ENCOUNTERS];
    int maxEncounters;
    int lastVisited;
} Location;

Room* Encounter_addRoom(Encounter* enc, int x, int y, Room room) {
    int pos = x * 8 + y;
    enc->rooms[pos] = room;
    for (int i = 0; i < ENCOUNTER_MAX_ENTITY; i++) {
        enc->rooms[pos].entities[i] = Entity_create(enc->rooms[pos].spawnList[i]);
    }
    enc->rooms[pos].x = x;
    enc->rooms[pos].y = y;
    enc->rooms[pos].created = true;
    return &enc->rooms[pos];
}

void Encounter_linkRooms(Room* from, Room* to, Direction dir) {
    from->exits[dir] = to;
    if (dir <= DIR_EAST) {
        to->exits[dir+2] = from;
    } else {
        to->exits[dir-2] = from;
    }
}

void Encounter_generateBasicMap(Encounter* enc, int minSize, int maxSize) {
    int createdRooms = 0;
    int roomNumber = GetRandomValue(minSize, maxSize);
    Vector2 newRoomPos = {GetRandomValue(0, 7), GetRandomValue(0, 7)};
    Vector2 lastRoomPos = newRoomPos;
    Room* lastRoom = Encounter_addRoom(enc, newRoomPos.x, newRoomPos.y, (Room) {"A room with a random description."});
    enc->startingRoom = lastRoom;
    int panic = 500;
    while (createdRooms < roomNumber && panic > 0) {
        Direction dir = (Direction) GetRandomValue(DIR_NORTH, DIR_MAX - 1);
        switch (dir) {
            case DIR_NORTH:
                newRoomPos.y--;
                break;
            case DIR_EAST:
                newRoomPos.x++;
                break;
            case DIR_SOUTH:
                newRoomPos.y++;
                break;
            case DIR_WEST:
                newRoomPos.x--;
                break;
            default:
                break;
        }
        if (newRoomPos.y > 7 || newRoomPos.y < 0 || newRoomPos.x > 7 || newRoomPos.x < 0) {
            newRoomPos = lastRoomPos;
            panic--;
            continue;
        }
        int roomIndex = newRoomPos.x * 8 + newRoomPos.y;
        if (enc->rooms[roomIndex].created) {
            newRoomPos = lastRoomPos;
            panic--;
            continue;
        }
        Room* newRoom = Encounter_addRoom(enc, newRoomPos.x, newRoomPos.y, (Room) {"A room with a random description."});
        Encounter_linkRooms(lastRoom, newRoom, dir);
        lastRoom = newRoom;
        lastRoomPos = newRoomPos;
        panic = 500;
        createdRooms++;
    }
}

void Encounter_createExtraDoors(Encounter* enc, int maxExtraDoors) {
    int doorsAdded = 0;
    int panic = 500;
    while (doorsAdded < maxExtraDoors && panic > 0) {
        int roomIndex = GetRandomValue(0, 63);
        if (!enc->rooms[roomIndex].created) {
            continue;
        }
        Direction newDir = (Direction) GetRandomValue(DIR_NORTH, DIR_MAX - 1);
        switch (newDir) {
            case DIR_NORTH:
                if (roomIndex - 1 >= 0 && enc->rooms[roomIndex - 1].created) {
                    Encounter_linkRooms(&enc->rooms[roomIndex], &enc->rooms[roomIndex - 1], newDir);
                    doorsAdded++;
                }
                break;
            case DIR_EAST:
                if (roomIndex + 8 < ENCOUNTER_MAX_ROOM && enc->rooms[roomIndex + 8].created) {
                    Encounter_linkRooms(&enc->rooms[roomIndex], &enc->rooms[roomIndex + 8], newDir);
                    doorsAdded++;
                }
                break;
            case DIR_SOUTH:
                if (roomIndex + 1 < ENCOUNTER_MAX_ROOM && enc->rooms[roomIndex + 1].created) {
                    Encounter_linkRooms(&enc->rooms[roomIndex], &enc->rooms[roomIndex + 1], newDir);
                    doorsAdded++;
                }
                break;
            case DIR_WEST:
                if (roomIndex - 8 >= 0 && enc->rooms[roomIndex - 8].created) {
                    Encounter_linkRooms(&enc->rooms[roomIndex], &enc->rooms[roomIndex - 8], newDir);
                    doorsAdded++;
                }
                break;
            default:
                break;
        }
        panic--;
    }
}

Room* Encounter_populateCustomRoom(Encounter* enc, Room customRoom) {
    while (true) {
        int roomIndex = GetRandomValue(0, 63);
        if (!enc->rooms[roomIndex].created || enc->rooms[roomIndex].custom) {
            continue;
        }
        TextCopy(enc->rooms[roomIndex].description, customRoom.description);
        enc->rooms[roomIndex].custom = true;
        for (int i = 0; i < ENCOUNTER_MAX_ENTITY; i++) {
            if (!customRoom.spawnList[i]) {
                return &enc->rooms[roomIndex];
            }
            enc->rooms[roomIndex].entities[i] = Entity_create(customRoom.spawnList[i]);
        }
        return &enc->rooms[roomIndex];
    }
}

void EncounterTemplate_humpyHouse(Encounter* enc) {
    Encounter_generateBasicMap(enc, 10, 14);
    enc->startingRoom = Encounter_populateCustomRoom(enc, (Room) {"You are in the hallway of Humpy's house.", {ENT_HUMPY_FRONT_DOOR}});
    Encounter_populateCustomRoom(enc, (Room) {"You enter Humpy's kitchen; the reek hits you like a shovel.", {ENT_RAT_THING, ENT_RAT_THING, ENT_HUMPY_CELLAR_DOOR}});
    Encounter_populateCustomRoom(enc, (Room) {"You are on the landing.", {ENT_SCROLL_PARALYSE}});
    Encounter_populateCustomRoom(enc, (Room) {"You are in the master bedroom. It is in disarray; papers\nare strewn everywhere.", {ENT_BED, ENT_HUMPY_CELLAR_KEY}});
    Encounter_createExtraDoors(enc, 6);
}

Location arkham = {
    (EncounterTemplate) {"Humpy's House", &EncounterTemplate_humpyHouse, 1}
};

void Encounter_create(Encounter* enc, EncounterTemplate et) {
    (*et.constructor)(enc);
}

void Encounter_drawFromLocation(Encounter* enc, Location* loc) {
    int roll;
    do {
        roll = GetRandomValue(0, loc->maxEncounters);
    } while (loc->encounters[roll].unique && loc->encounters[roll].visited);
    Encounter_create(enc, loc->encounters[roll]);
    loc->encounters[roll].visited = 1;
    loc->lastVisited = roll;
}

// DISPLAY

char roomDescription[256] = "";
char popupBox[512] = "";
char statusMessage[128] = "";
Encounter currentEncounter = {};
Room* currentRoom;
Location* currentLocation;
Font fontTtf;
Color textColour = BLACK;
Color bgColour = LIGHTGRAY;
Color statusMsgColour = MAROON;
int fontSize = 16;

// MENU

typedef struct UIElement {
    char label[32];
    int action;
    int key;
    int width;
    int height;
    int visible;
    int fontSize;
    Color fontColour;
} UIElement;

typedef struct Menu {
    int id;
    UIElement items[32];
    int nextItem;
    int x;
    int y;
} Menu;

void Menu_draw(Menu m) {
    int line = m.y;
    for (int i = 0; i < m.nextItem; i++) {
        UIElement* el = &m.items[i];
        if (el->visible) {
            DrawTextEx(fontTtf, el->label, (Vector2) {m.x, line}, fontSize, 0, el->fontColour);
            line += el->height;
        }
    }
}

typedef struct {
    int key;
    int mouseX;
    int mouseY;
    int mouseButton;
} UserAction;

// Detects which menu item was triggered
int Menu_action(Menu m, UserAction action) {
    for (int i = 0; i < m.nextItem; i++) {
        UIElement* el = &m.items[i];
        if (action.key && action.key == el->key) {
            return el->action;
        }
        // TODO: mouse support
    }
    return -1;
}

Menu actionMenu;

// INVENTORY

#define INV_MAX 9

Entity inventory[INV_MAX] = {};
int nextInv = 0;

void Inventory_add(Entity e) {
    if (nextInv == INV_MAX) {
        return;
    }
    inventory[nextInv] = e;
    nextInv++;
}

int Inventory_has(EntityId eid) {
    for (int i = 0; i < nextInv; i++) {
        if (inventory[i].id == eid) {
            return 1;
        }
    }
    return 0;
}

Entity* Inventory_getByEntityId(EntityId eid) {
    for (int i = 0; i < nextInv; i++) {
        if (inventory[i].id == eid) {
            return &inventory[i];
        }
    }
    return 0;
}

Entity* Inventory_getByIndex(int index) {
    if (index < 0 || index >= INV_MAX) {
        return 0;
    }
    return &inventory[index];
}

void Inventory_remove(EntityId eid) {
    int shuffle = 0;
    for (int i = 0; i < nextInv; i++) {
        if (inventory[i].id == eid) {
            shuffle = 1;
        }
        if (shuffle && i < nextInv - 1) {
            inventory[i] = inventory[i + 1];
        }
    }
    if (shuffle) {
        nextInv--;
    }
}

void RunScript(char* script) {
    if (TextIsEqual(script, ":introLetter")) {
        TextCopy(popupBox, "My friend, please come visit me as soon as possible.\n\nI've discovered something terrible and we must talk.\n\nRegards, Humpy");
    }
}

void Verb_examine(Entity* e) {
    // When a colon is in the examine field, run a "script", which is code.
    if (e->examine[0] == ':') {
        RunScript(e->examine);
    } else {
        TextCopy(popupBox, e->examine);
    }
}

void ChangeLocation(Location* newloc) {
    currentLocation = newloc;
    currentEncounter = (Encounter) {};
    Encounter_create(&currentEncounter, currentLocation->encounters[currentLocation->lastVisited]);
    currentLocation->encounters[currentLocation->lastVisited].visited = 1;
    currentRoom = currentEncounter.startingRoom;
    TextCopy(roomDescription, currentRoom->description);
}

int RollDice(Dice d) {
    int total = 0;
    for (int r = 0; r < d.num; r++) {
        total += GetRandomValue(1, d.sides);
    }
    return total;
}

void GainExp(int xp) {
    playerStats.xp += xp;
    TextCopy(statusMessage, TextFormat("You gain %d experience point.", xp));
}

void CombatReward(Entity* e) {
    if (e->hp <= 0) {
        playerStats.stress = playerStats.stress > e->fear ? playerStats.stress - e->fear / 2 : 0;
    }
}

typedef enum {
    MENU_VERB, MENU_ATTACK, MENU_CAST, MENU_HIDE, MENU_PICKUP, MENU_EXAMINE, MENU_EXPLORE, MENU_USE,
    MENU_INVENTORY, MENU_WAIT, MENU_SKILLS, MENU_CHANGE_LOCATION
} MenuIds;

void SetVerbMenu() {
    actionMenu.id = MENU_VERB;
    actionMenu.items[0] = (UIElement) {"(A)ttack", MENU_ATTACK, KEY_A, 128, 16, 1, 16, textColour};
    actionMenu.items[1] = (UIElement) {"(C)ast", MENU_CAST, KEY_C, 128, 16, 1, 16, textColour};
    actionMenu.items[2] = (UIElement) {"(H)ide", MENU_HIDE, KEY_H, 128, 16, 1, 16, textColour};
    actionMenu.items[3] = (UIElement) {"(P)ick up", MENU_PICKUP, KEY_P, 128, 16, 1, 16, textColour};
    actionMenu.items[4] = (UIElement) {"E(x)amine", MENU_EXAMINE, KEY_X, 128, 16, 1, 16, textColour};
    actionMenu.items[5] = (UIElement) {"(U)se", MENU_USE, KEY_U, 128, 16, 1, 16, textColour};
    actionMenu.items[6] = (UIElement) {"(I)nventory", MENU_INVENTORY, KEY_I, 128, 16, 1, 16, textColour};
    actionMenu.items[7] = (UIElement) {"(W)ait", MENU_WAIT, KEY_W, 128, 16, 1, 16, textColour};
    actionMenu.items[8] = (UIElement) {"(L)eave", MENU_EXPLORE, KEY_L, 128, 16, 1, 16, textColour};
    actionMenu.nextItem = 9;
}

void SetEntityMenu(int id) {
    actionMenu.id = id;
    int key = 49;
    for (int i = 0; i < ENCOUNTER_MAX_ENTITY; i++) {
        if (currentRoom->entities[i].created && currentRoom->entities[i].hp > 0) {
            actionMenu.items[key-49] = (UIElement) {"", i, key, 128, 16, 1, 16, textColour};
            TextCopy(actionMenu.items[key-49].label, TextFormat("%i) %s", key-48, currentRoom->entities[i].name));
            key++;
        }
    }
    actionMenu.items[key-49] = (UIElement) {"0) Back", -1, 48, 128, 16, 1, 16, textColour};
    actionMenu.nextItem = key-48;
}

void SetHideMenu(int id) {
    actionMenu.id = id;
    int key = 49;
    for (int i = 0; i < ENCOUNTER_MAX_ENTITY; i++) {
        if (currentRoom->entities[i].created && currentRoom->entities[i].hp > 0 && currentRoom->entities[i].canHideIn) {
            actionMenu.items[key-49] = (UIElement) {"", i, key, 128, 16, 1, 16, textColour};
            TextCopy(actionMenu.items[key-49].label, TextFormat("%i) %s", key-48, currentRoom->entities[i].name));
            key++;
        }
    }
    actionMenu.items[key-49] = (UIElement) {"0) Back", -1, 48, 128, 16, 1, 16, textColour};
    actionMenu.nextItem = key-48;
}

void SetRoomMenu(int id) {
    actionMenu.id = id;
    int key = 0;
    if (currentRoom->exits[DIR_NORTH]) {
        actionMenu.items[key++] = (UIElement) {"(N)orth", DIR_NORTH, KEY_N, 128, 16, 1, 16, textColour};
    }
    if (currentRoom->exits[DIR_EAST]) {
        actionMenu.items[key++] = (UIElement) {"(E)ast", DIR_EAST, KEY_E, 128, 16, 1, 16, textColour};
    }
    if (currentRoom->exits[DIR_SOUTH]) {
        actionMenu.items[key++] = (UIElement) {"(S)outh", DIR_SOUTH, KEY_S, 128, 16, 1, 16, textColour};
    }
    if (currentRoom->exits[DIR_WEST]) {
        actionMenu.items[key++] = (UIElement) {"(W)est", DIR_WEST, KEY_W, 128, 16, 1, 16, textColour};
    }
    actionMenu.items[key++] = (UIElement) {"0) Back", -1, 48, 128, 16, 1, 16, textColour};
    actionMenu.nextItem = key;
}

void SetKnownLocationMenu() {
    actionMenu.id = MENU_CHANGE_LOCATION;
    int key = 49;
    int encountersLeft = currentLocation->maxEncounters + 1;
    for (int i = 0; i <= currentLocation->maxEncounters; i++) {
        EncounterTemplate* et = &currentLocation->encounters[i];
        if (et->visited && et->unique) {
            actionMenu.items[key-49] = (UIElement) {"", i, key, 128, 16, 1, 16, textColour};
            TextCopy(actionMenu.items[key-49].label, TextFormat("%d) %s", key-48, et->name));
            encountersLeft--;
            key++;
        }
    }
    if (encountersLeft) {
        actionMenu.items[key-49] = (UIElement) {"(E)xplore", KEY_E, KEY_E, 128, 16, 1, 16, textColour};
        key++;
    }
    actionMenu.items[key-49] = (UIElement) {"0) Go back", -1, 48, 128, 16, 1, 16, textColour};
    actionMenu.nextItem = key-48;
}

void SetInventoryMenu() {
    actionMenu.id = MENU_INVENTORY;
    int key = 49;
    for (int i = 0; i < nextInv; i++) {
        actionMenu.items[key-49] = (UIElement) {"", i, key, 128, 16, 1, 16, textColour};
        if (inventory[i].stack) {
            TextCopy(actionMenu.items[key-49].label, TextFormat("%i) %s (%d)", key-48, inventory[i].name, inventory[i].stack));
        } else {
            TextCopy(actionMenu.items[key-49].label, TextFormat("%i) %s", key-48, inventory[i].name));
        }
        key++;
    }
    actionMenu.items[key-49] = (UIElement) {"0) Back", -1, 48, 128, 16, 1, 16, textColour};
    actionMenu.nextItem = key-48;
}

void SetSkillMenu() {
    actionMenu.id = MENU_SKILLS;
    int key = 49;
    // Randomly generate a list of skills.
    // TODO: Probably each location should store these rather than randomising them?
    int num = GetRandomValue(3, 6);
    Skill alreadyChosen[6] = {};
    int selected = 0;
    while (selected < num) {
        Skill skillToChoose = (Skill) GetRandomValue(0, SKILL_MAX - 1);
        for (int j = 0; j < selected; j++) {
            if (alreadyChosen[j] == skillToChoose) {
                skillToChoose = SKILL_MAX;
                break;
            }
        }
        if (skillToChoose != SKILL_MAX) {
            alreadyChosen[selected] = skillToChoose;
            selected++;
        }
    }
    for (int i = 0; i < selected; i++) {
        actionMenu.items[key-49] = (UIElement) {"", alreadyChosen[i], key, 128, 16, 1, 16, textColour};
        int xpCost = Skill_calculateXP(playerStats.skills[alreadyChosen[i]], playerStats.attributes[ATTR_EDU]);
        TextCopy(actionMenu.items[key-49].label, TextFormat("%d) %s (%dxp)", key-48, skillData[alreadyChosen[i]].name, xpCost));
        key++;
    }
    actionMenu.items[key-49] = (UIElement) {"0) Back", -1, 48, 128, 16, 1, 16, textColour};
    actionMenu.nextItem = key-48;
}

void SetSpellMenu() {
    actionMenu.id = MENU_CAST;
    for (int i = 0; i < playerStats.spellsKnown; i++) {
        SpellData spell = spellData[playerEntity.spells[i]];
        actionMenu.items[i] = (UIElement) {"", spell.id, i+49, 128, 16, 1, 16, textColour};
        TextCopy(actionMenu.items[i].label, TextFormat("%d) %s", i+1, spell.name));
    }
    actionMenu.items[playerStats.spellsKnown] = (UIElement) {"0) Back", -1, 48, 128, 16, 1, 16, textColour};
    actionMenu.nextItem = playerStats.spellsKnown + 1;
}

void AlertAllEnemiesInRoom(Room* r) {
    for (int i = 0; i < ENCOUNTER_MAX_ENTITY; i++) {
        if (Entity_isMob(&r->entities[i])) {
            r->entities[i].state = STATE_HEARD_SOMETHING;
        }
    }
}

void MakeNoise(int potentialLoudness) {
    // Check the player's stealth skill
    bool wasQuiet = Skill_test(playerStats.skills[SKILL_STEALTH], potentialLoudness);
    if (wasQuiet) {
        return;
    }
    // Alert enemies adjacent to the player
    for (Direction d = 0; d < DIR_MAX; d++) {
        if (currentRoom->exits[d]) {
            AlertAllEnemiesInRoom(currentRoom->exits[d]);
        }
    }
}

void Fight(Entity* attacker, Entity* defender) {
    // Rule out armour and weapons from fighting; only things with both attack and defense are monsters
    if (!attacker->attack || !attacker->defence) {
        return;
    }
    int chanceToHit;
    // Attacking from a hidden position always hits, is twice as strong, and skips the defender's turn too
    if (attacker->state == STATE_HIDDEN) {
        defender->hp -= RollDice(playerEntity.damage) * 2;
        defender->actedThisTurn = true;
        return;
    }
    if (!defender->defence) {
        chanceToHit = 100;
    } else {
        chanceToHit = attacker->attack * 50 / defender->defence;
    }
    if (GetRandomValue(1, 100) < chanceToHit) {
        defender->hp -= RollDice(playerEntity.damage);
    }
    attacker->actedThisTurn = true;
    MakeNoise(15);
}

void ReadScroll(SpellId spellId, EntityId eid) {
    if (playerStats.spellSlotsLeft <= 0) {
        TextCopy(statusMessage, "No spell slots left to learn spell!");
        return;
    }
    playerEntity.spells[playerStats.spellsKnown] = spellId;
    playerStats.spellSlotsLeft--;
    playerStats.spellsKnown++;
    Inventory_remove(eid);
    return;
}


void Verb_use(Entity* e) {
    playerEntity.state = STATE_IDLE;
    switch (e->id) {
        case ENT_HUMPY_FRONT_DOOR:
            SetKnownLocationMenu();
            break;
        case ENT_BOOKSHOP:
            SetSkillMenu();
            break;
        case ENT_GALLERY:
         if (playerStats.morale == 4) {
                TextCopy(statusMessage, "You feel no need for entertainment at this time.");
                return;
            }
            TextCopy(popupBox, "You spend a pleasant few hours viewing the paintings hanging here.\n\nHere it will descibe something fun.");
            if (playerStats.morale < 4) {
                playerStats.morale = 4;
            }
            GainExp(1);
            break;
        case ENT_BED:
            if (playerStats.rest == 4) {
                TextCopy(statusMessage, "You are not tired!");
                return;
            }
            if (playerEntity.hp < playerEntity.hpMax / 2) {
                playerEntity.hp = playerEntity.hpMax / 2;
            }
            playerStats.rest = 4;
            TextCopy(statusMessage, "You sleep for a few hours and recover somewhat.");
            break;
        default:
            break;
    }
}

void Verb_inventoryUse(Entity* e) {
    if (e->id >= ENT_PISTOL && e->id <= ENT_LIGHTNING_GUN) {
        // If the player has a weapon, unequip current weapon
        if (playerStats.weapon) {
            Entity* curWeapon = &entities[playerStats.weapon];
            playerEntity.attack -= curWeapon->attack;
            Inventory_add(*e);
        }
        playerStats.weapon = e->id;
        playerEntity.attack += e->attack;
        playerEntity.damage = e->damage;
        Inventory_remove(e->id);
        return;
    }
    switch (e->id) {
        case ENT_STIMPACK:
            playerEntity.hp = playerEntity.hp < playerEntity.hpMax - 10 ? playerEntity.hp + 10 : playerEntity.hpMax;
            Inventory_remove(e->id);
            break;
        case ENT_SEDATIVE:
            playerStats.stress -= playerStats.stress >= 15 ? 15 : 0;
            Inventory_remove(e->id);
            break;
        case ENT_FRAG_GRENADE:
            for (int i = 0; i < ENCOUNTER_MAX_ENTITY; i++) {
                currentRoom->entities[i].hp -= RollDice(e->damage);
                CombatReward(&currentRoom->entities[i]);
            }
            Inventory_remove(e->id);
            MakeNoise(100);
            break;
        case ENT_SCROLL_PARALYSE:
            ReadScroll(SPELL_PARALYSE, e->id);
            break;
        default:
            break;
    }
}

void Verb_attack(Entity* e) {
    if (playerStats.weapon) {
        // Do we have the correct ammo type?
        Entity* ammo = Inventory_getByEntityId(entities[playerStats.weapon].ammoType);
        if (ammo) {
            ammo->stack--;
            Fight(&playerEntity, e);
            if (ammo->stack <= 0) {
                Inventory_remove(entities[playerStats.weapon].ammoType);
            }
        }
    } else {
        // No weapon? Just punch
        Fight(&playerEntity, e);
    }
    CombatReward(e);
    playerEntity.state = STATE_IDLE;
}

void Verb_pickup(Entity* e) {
    playerEntity.state = STATE_IDLE;
    if (!e->portable) {
        return;
    }
    Inventory_add(*e);
    e->hp = 0;
}

void Verb_cast(SpellData spell) {
    if (playerStats.mp <= 0) {
        return;
    }
    // TODO: No, spells have targeters, but this is just whatever for now
    Entity* targets[ENCOUNTER_MAX_ENTITY];
    int t = 0;
    for (int i = 0; i < ENCOUNTER_MAX_ENTITY; i++) {
        if (Entity_isMob(&currentRoom->entities[i])) {
            targets[t++] = &currentRoom->entities[i];
        }
    }
    if (t == 0) {
        return;
    }
    switch (spell.id) {
        case SPELL_PARALYSE:
            // TODO: A correct effect!
            Entity* target = targets[GetRandomValue(0, t-1)];
            target->hp = 0;
            break;
        default:
            break;
    }
    playerStats.mp -= spell.cost;
}

void MoveEntity(Entity* e, Room* to) {
    for (int i = 0; i < ENCOUNTER_MAX_ENTITY; i++) {
        if (to->entities[i].created == 0) {
            Entity copy = *e;
            e->created = 0;
            e->attack = 0;
            e->id = 0;
            e->defence = 0;
            to->entities[i] = copy;
            to->entities[i].actedThisTurn = true;
            return;
        }
    }
}

void HandleGameTurn() {
    for (int j = 0; j < ENCOUNTER_MAX_ROOM; j++) {
        Room* r = &currentEncounter.rooms[j];
        if (r->created) {
            for (int i = 0; i < ENCOUNTER_MAX_ENTITY; i++) {
                Entity* e = &r->entities[i];
                if (!e->actedThisTurn && Entity_isMob(e)) {
                    // The state machine is "interrupted" if the monster is currently with the player - regardless of state, they attack.
                    // However if the player is hidden we continue with the normal fallback behaviour.
                    if (r == currentRoom) {                 
                        if (!e->seen) {
                            playerStats.stress += (e->fear - playerStats.stressResistance > 1) ? e->fear - playerStats.stressResistance : 1;
                            e->seen = true;
                        }
                        // If the player isn't hiding, monsters do bad things
                        if (playerEntity.state != STATE_HIDDEN) {
                            if (e->state == STATE_ALERTED) {
                                Fight(e, &playerEntity);
                            }
                            e->state = STATE_ALERTED;
                        } else {
                            // When the player is hidden the monster goes into the wandering state
                            e->state = STATE_WANDER;
                        }
                    }
                    if (e->actedThisTurn) {
                        continue;
                    }
                    // If the monster isn't interacting with the player, it should fall through to here
                    if (e->state == STATE_HEARD_SOMETHING) {
                        e->state = STATE_ALERTED;
                    } else if (e->state == STATE_ALERTED) {
                        // If an alerted enemy doesn't find the player in one turn, they may go into wandering state instead
                        // TODO: Possibly a monster property?
                        if (GetRandomValue(1, 3) == 1) {
                            e->state = STATE_WANDER;
                        }
                        // Otherwise entities wander about, find the player, etc. etc.
                        if (currentRoom->y < r->y && r->exits[DIR_NORTH]) {
                            MoveEntity(e, r->exits[DIR_NORTH]);
                        } else if (currentRoom->x > r->x && r->exits[DIR_EAST]) {
                            MoveEntity(e, r->exits[DIR_EAST]);
                        } else if (currentRoom->y > r->y && r->exits[DIR_SOUTH]) {
                            MoveEntity(e, r->exits[DIR_SOUTH]);
                        } else if (currentRoom->x < r->x && r->exits[DIR_WEST]) {
                            MoveEntity(e, r->exits[DIR_WEST]);
                        }                        
                    } else if (e->state == STATE_WANDER) {
                        // Randomly choose an exit and have a look there; chance to return to idle.
                        bool exitFound = false;
                        // TODO: Possibly a monster property?
                        if (GetRandomValue(1, 3) == 1) {
                            e->state = STATE_IDLE;
                        }
                        while (!exitFound) {
                            Direction d = (Direction) GetRandomValue(DIR_NORTH, DIR_WEST);
                            if (r->exits[d]) {
                                MoveEntity(e, r->exits[d]);
                                exitFound = true;
                            }
                        }
                    }
                }
            }
        }
    }
    // Set all the acted flags back to false. Not really happy with this second loop.
    for (int j = 0; j < ENCOUNTER_MAX_ROOM; j++) {
        Room* r = &currentEncounter.rooms[j];
        if (r->created) {
            for (int i = 0; i < ENCOUNTER_MAX_ENTITY; i++) {
                r->entities[i].actedThisTurn = false;
            }
        }
    }
    for (int i = 0; i < DIR_MAX; i++) {
        if (currentRoom->exits[i]) {
            for (int f = 0; f < ENCOUNTER_MAX_ENTITY; f++) {
                if (Entity_isMob(&currentRoom->exits[i]->entities[f])) {
                    break;
                }
            }
        }
    }
}

/**
 * Check for player input; if there is, handle it and tick the world.
*/
void HandlePlayerTurn() {
    int key = GetKeyPressed();
    if (key) {
        TextCopy(statusMessage, "");
        // If the popup box is here, any key clears it and does nothing else
        if (TextLength(popupBox)) {
            TextCopy(popupBox, "");
            return;
        }
        int menuAction = Menu_action(actionMenu, (UserAction) {key});
        switch (actionMenu.id) {
            case MENU_VERB:
                switch (menuAction) {
                    case MENU_ATTACK:
                        SetEntityMenu(menuAction);
                        // Auto attack if there's only one monster here (2 because back and the monster are the options)
                        if (actionMenu.nextItem == 2) {
                            Verb_attack(&currentRoom->entities[actionMenu.items[0].action]);
                        }
                        return;
                    case MENU_CAST:
                        SetSpellMenu();
                        return;
                    case MENU_HIDE:
                        for (int i = 0; i < ENCOUNTER_MAX_ENTITY; i++) {
                            if (Entity_isMob(&currentRoom->entities[i])) {
                                return;
                            }
                        }
                        SetHideMenu(menuAction);
                        return;
                    case MENU_PICKUP:
                    case MENU_EXAMINE:
                    case MENU_USE:
                        SetEntityMenu(menuAction);
                        return;
                    case MENU_INVENTORY:
                        SetInventoryMenu();
                        return;
                    case MENU_WAIT:
                        HandleGameTurn();
                        TextCopy(statusMessage, "Time passes.");
                        return;
                    case MENU_EXPLORE:
                        // Player cannot leave if a) there are enemies and b) stress is 100% or more
                        if (playerStats.stress >= 100) {
                            for (int i = 0; i < ENCOUNTER_MAX_ENTITY; i++) {
                                if (Entity_isMob(&currentRoom->entities[i])) {
                                    return;
                                }
                            }
                            TextCopy(statusMessage, "You must destroy these creatures or go mad!");
                        }                        
                        SetRoomMenu(menuAction);
                        //SetKnownLocationMenu(menuAction);
                        return;             
                    default:
                        return;
                }
                break;
            case MENU_EXPLORE:
                if (menuAction == -1) {
                    TextCopy(roomDescription, currentRoom->description);
                    SetVerbMenu();
                    return;
                }
                playerEntity.state = STATE_IDLE;
                currentRoom = currentRoom->exits[menuAction];
                MakeNoise(-25);
                HandleGameTurn();
                TextCopy(roomDescription, currentRoom->description);
                SetVerbMenu();
                return;
            case MENU_INVENTORY:
            {
                int iid = Menu_action(actionMenu, (UserAction) {key});
                if (iid == -1) {
                    TextCopy(roomDescription, currentRoom->description);
                    SetVerbMenu();
                    return;
                }
                Entity* e = Inventory_getByIndex(iid);
                Verb_inventoryUse(e);
                SetVerbMenu();
                HandleGameTurn();
                return;
            }
            case MENU_SKILLS:
            {
                int sk = Menu_action(actionMenu, (UserAction) {key});
                if (sk == -1) {
                    SetVerbMenu();
                    return;
                }
                int xpCost = Skill_calculateXP(playerStats.skills[sk], playerStats.attributes[ATTR_EDU]);
                if (playerStats.xp < xpCost) {
                    TextCopy(statusMessage, "You don't have enough XP to raise this skill.");
                    return;
                }
                playerStats.skills[sk]++;
                playerStats.xp -= xpCost;
                SetSkillMenu();
                return;
            }
            case MENU_CHANGE_LOCATION:
            {
                if (menuAction >= 0 && menuAction <= currentLocation->maxEncounters) {
                    EncounterTemplate* et = &currentLocation->encounters[menuAction];
                    if (et->visited && et->unique) {
                        // TODO: Do not recreate the encounter! We need to store these.
                        currentEncounter = (Encounter) {};
                        Encounter_create(&currentEncounter, *et);
                        currentRoom = currentEncounter.startingRoom;
                        et->visited = 1;
                        TextCopy(roomDescription, currentRoom->description);
                        currentLocation->lastVisited = menuAction;
                        SetVerbMenu();
                    }
                } else if (key == KEY_E) {
                    currentEncounter = (Encounter) {};
                    Encounter_drawFromLocation(&currentEncounter, currentLocation);
                    currentRoom = currentEncounter.startingRoom;
                    TextCopy(roomDescription, currentRoom->description);
                    SetVerbMenu();
                    GainExp(1);
                }
                playerEntity.state = STATE_IDLE;
                HandleGameTurn();
                // Reduce survival attributes
                playerStats.actions--;
                if (playerStats.actions == 0) {
                    playerStats.actions = playerStats.actionsMax;
                    playerStats.rest--;
                    playerStats.morale--;
                    playerStats.satiation--;
                    if (playerStats.rest == 0 || playerStats.rest == 0 || playerStats.satiation == 0) {
                        // You die.
                        playerEntity.hp = 0;            
                    }
                }
                return;
            }
            case MENU_CAST:
            {
                int sid = Menu_action(actionMenu, (UserAction) {key});
                if (sid == -1) {
                    SetVerbMenu();
                    return;
                }
                SpellData spell = spellData[sid];
                Verb_cast(spell);
                HandleGameTurn();
                return;
            }
            default:
            {
                int eid = Menu_action(actionMenu, (UserAction) {key});
                if (eid == -1) {
                    TextCopy(roomDescription, currentRoom->description);
                    SetVerbMenu();
                    return;
                }
                Entity* e = &currentRoom->entities[eid];
                bool refreshMenu = true;
                if (e->created) {
                    switch (actionMenu.id) {
                        case MENU_ATTACK:
                            Verb_attack(e);
                            break;
                        case MENU_PICKUP:
                            Verb_pickup(e);
                            break;
                        case MENU_EXAMINE:
                            Verb_examine(e);
                            break;
                        case MENU_USE:
                            Verb_use(e);
                            refreshMenu = false;
                            break;
                        case MENU_HIDE:
                            playerEntity.state = STATE_HIDDEN;
                            refreshMenu = false;
                            SetVerbMenu();
                            break;
                        default:
                            break;
                    }
                    if (refreshMenu) {
                        SetEntityMenu(actionMenu.id);
                        // Automatically go back if there's one or fewer items left
                        if (actionMenu.nextItem <= 1) {
                            SetVerbMenu();
                        }
                    }
                }
                HandleGameTurn();
                return;
            }
        }
    }
}

static void UpdateDrawFrame(void);

// Main loop
int main() {
    const int screenWidth = 800;
    const int screenHeight = 600;
    InitWindow(screenWidth, screenHeight, "raylib");
    SetTargetFPS(60);

    // Load graphics
    fontTtf = LoadFontEx("resources/PixAntiqua.ttf", 16, 0, 250);
    
    // Set up player character
    playerEntity.hpMax = playerEntity.hp;
    playerStats.actions = playerStats.attributes[ATTR_CON] * 2;
    playerStats.actionsMax = playerStats.actions;

    // Set starting location (default &arkham)
    ChangeLocation(&arkham);

    // Set up initial UI elements
    actionMenu.x = 600;
    actionMenu.y = 16;
    SetVerbMenu();

    // Main game loop
    while (!WindowShouldClose() && playerEntity.hp > 0) {
        HandlePlayerTurn();
        UpdateDrawFrame();
    }
    UnloadFont(fontTtf); 
    CloseWindow();
    return 0;
}

void DrawAreaMap() {
    int startX = 300;
    int startY = 200;
    for (int x = 0; x < 8; x++) {
        for (int y = 0; y < 8; y++) {
            int roomIndex = x * 8 + y;
            if (!currentEncounter.rooms[roomIndex].created) {
                continue;
            }
            DrawRectangleLines(startX + (x*50), startY + (y*50), 50, 50, BLACK);
            int xoffs, yoffs = 0;
            for (int exitIndex = 0; exitIndex < DIR_MAX; exitIndex++) {
                    switch ((Direction) exitIndex) {
                        case DIR_NORTH:
                            xoffs = 20;
                            yoffs = -5;
                            break;
                        case DIR_EAST:
                            xoffs = 45;
                            yoffs = 20;
                            break;
                        case DIR_SOUTH:
                            xoffs = 20;
                            yoffs = 45;
                            break;
                        case DIR_WEST:
                            xoffs = -5;
                            yoffs = 20;
                            break;
                        default:
                            break;
                    }
                    if (currentEncounter.rooms[roomIndex].exits[exitIndex]) {
                        DrawRectangle(startX + (x*50) + xoffs, startY + (y*50) + yoffs, 10, 10, BLACK);
                    }
                }
            if (currentRoom == &currentEncounter.rooms[roomIndex]) {
                DrawText("@", startX + (x*50) + 15, startY + (y*50) + 15, 8, BLACK);
            }
            for (int i = 0; i < ENCOUNTER_MAX_ENTITY; i++) {
                if (Entity_isMob(&currentEncounter.rooms[roomIndex].entities[i])) {
                    switch (currentEncounter.rooms[roomIndex].entities[i].state) {
                        case STATE_IDLE:
                            DrawText(":", startX + (x*50) + 5, startY + (y*50) + 5, 8, BLACK);
                            break;
                        case STATE_ALERTED:
                            DrawText("!", startX + (x*50) + 5, startY + (y*50) + 5, 8, BLACK);
                            break;
                        case STATE_WANDER:
                        case STATE_HEARD_SOMETHING:
                            DrawText("?", startX + (x*50) + 5, startY + (y*50) + 5, 8, BLACK);
                            break;
                        case STATE_HIDDEN:
                            break;
                    }
                }
            }
        }
    }
}

// Update and draw game frame
static void UpdateDrawFrame(void) {
    BeginDrawing();
        ClearBackground(bgColour);
        // TODO: This is inefficient and should ideally be done only when the items list is updated (we can append to roomDescription there)
        char itemsHere[256] = "";
        for (int i = 0; i < ENCOUNTER_MAX_ENTITY; i++) {
            if (currentRoom->entities[i].created && currentRoom->entities[i].hp > 0) {
                TextCopy(
                    itemsHere, 
                    TextFormat(
                        "%s\nYou see a %s. (%d/%d)",
                        itemsHere,
                        currentRoom->entities[i].name,
                        currentRoom->entities[i].hp,
                        currentRoom->entities[i].hpMax
                    )
                );
            }
        }
        // Show information about any mobs in adjacent rooms
        // TODO: Reset this each time the game turn moves and set a flag on the room?
        for (int i = 0; i < DIR_MAX; i++) {
            if (currentRoom->exits[i]) {
                for (int j = 0; j < ENCOUNTER_MAX_ENTITY; j++) {
                    if (Entity_isMob(&currentRoom->exits[i]->entities[j])) {
                        TextCopy(
                            itemsHere,
                            TextFormat("%s\nYou hear something moving to the %s...", itemsHere, dirStrings[i])
                        );
                        break;
                    }
                }
            }
        }
        DrawTextEx(fontTtf, TextFormat("%s\n%s", roomDescription, itemsHere), (Vector2) {10, 10}, fontSize, 0, textColour);
        Menu_draw(actionMenu);
        // Status bar
        DrawTextEx(fontTtf, statusMessage, (Vector2) {10, 378}, fontSize, 0, statusMsgColour);
        DrawTextEx(fontTtf, TextFormat("HP: %d/%d", playerEntity.hp, playerEntity.hpMax), (Vector2) {10, 400}, fontSize, 0, DARKGREEN);
        DrawTextEx(fontTtf, TextFormat("Stress: %d%%", playerStats.stress), (Vector2) {10, 416}, fontSize, 0, DARKGREEN);
        DrawTextEx(fontTtf, TextFormat("Actions: %d", playerStats.actions), (Vector2) {10, 432}, fontSize, 0, DARKGREEN);
        DrawTextEx(fontTtf, TextFormat("Satiation: %d", playerStats.satiation), (Vector2) {10, 448}, fontSize, 0, DARKGREEN);
        DrawTextEx(fontTtf, TextFormat("Rest: %d", playerStats.rest), (Vector2) {10, 464}, fontSize, 0, DARKGREEN);
        DrawTextEx(fontTtf, TextFormat("Morale: %d", playerStats.morale), (Vector2) {10, 480}, fontSize, 0, DARKGREEN);
        DrawTextEx(fontTtf, TextFormat("MP: %d/%d", playerStats.mp, playerStats.mpMax), (Vector2) {10, 496}, fontSize, 0, DARKGREEN);
        if (playerStats.weapon) {
            DrawTextEx(fontTtf, TextFormat("WPN: %s", entities[playerStats.weapon].name), (Vector2) {210, 400}, fontSize, 0, textColour);
        } else {
            DrawTextEx(fontTtf, "WPN: Fists", (Vector2) {210, 400}, fontSize, 0, textColour);
        }
        if (playerEntity.state == STATE_HIDDEN) {
            DrawTextEx(fontTtf, "Hidden", (Vector2) {210, 416}, fontSize, 0, MAROON);
        }
        DrawAreaMap();
        // Popup box for messages
        if (TextLength(popupBox)) {
            DrawRectangle(100, 50, 600, 500, bgColour);
            DrawRectangleLines(100, 50, 600, 500, textColour);
            DrawTextEx(fontTtf, popupBox, (Vector2) {108, 58}, fontSize, 0, textColour);
        }
    EndDrawing();
}