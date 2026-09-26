#ifndef _RANDOM_NAMES_H_
#define _RANDOM_NAMES_H_

#include <string>
#include <cstdlib>
#include "time.h"
#include "Application/Model/Config.h"

// Song-title style names: a mood word + a place/thing, e.g. NeonTide,
// JadeRiver, MidnightRun. Every pair fits MAX_NAME_LENGTH (12).
static const char *nameMoods[] = {
    "Neon",   "Jade",   "Velvet", "Silver", "Golden", "Crystal",
    "Amber",  "Cobalt", "Violet", "Scarlet","Ivory",  "Onyx",
    "Lunar",  "Solar",  "Astral", "Cosmic", "Stellar","Polar",
    "Misty",  "Rainy",  "Stormy", "Sunny",  "Frozen", "Hazy",
    "Quiet",  "Lonely", "Distant","Hidden", "Lost",   "Faded",
    "Electric","Analog","Pixel",  "Digital","Chrome", "Retro",
    "Midnight","Twilight","Dawn", "Dusk",   "Summer", "Winter",
    "Wild",   "Secret", "Dream",  "Echo",   "Ghost",  "Paper",
};
static const char *nameThings[] = {
    "Tide",   "River",  "Ocean",  "Rain",   "Storm",  "Wave",
    "Sky",    "Moon",   "Star",   "Comet",  "Orbit",  "Nova",
    "City",   "Street", "Avenue", "Harbor", "Garden", "Temple",
    "Forest", "Valley", "Canyon", "Desert", "Island", "Summit",
    "Drive",  "Run",    "Flight", "Voyage", "Escape", "Drift",
    "Dream",  "Memory", "Signal", "Pulse",  "Groove", "Rhythm",
    "Lights", "Fire",   "Glow",   "Shadow", "Mirror", "Window",
    "Sword",  "Lotus",  "Lantern","Petal",  "Echo",   "Heart",
};

static bool randomNamesSeeded = false;

std::string getRandomName() {
    if (!randomNamesSeeded) {
        // The simulator can ask for the same names every run
        // (-RGNANOSIM_NAMESEED=n), so scripted screenshots don't change
        const char *seed = Config::GetInstance()->GetValue("RGNANOSIM_NAMESEED");
        srand(seed ? (unsigned)atoi(seed) : (unsigned)time(NULL));
        randomNamesSeeded = true;
    }
    const int moods = sizeof(nameMoods) / sizeof(nameMoods[0]);
    const int things = sizeof(nameThings) / sizeof(nameThings[0]);
    std::string name;
    do {
        std::string mood = nameMoods[rand() % moods];
        std::string thing = nameThings[rand() % things];
        if (mood == thing)
            continue;
        name = mood + thing;
    } while (name.empty() || name.length() > MAX_NAME_LENGTH);
    return name;
}

#endif //_RANDOM_NAMES_H_
