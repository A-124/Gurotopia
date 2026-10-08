#include "pch.hpp"
#include "onVariant/SetClothing.hpp"
#include "onVariant/ConsoleMessage.hpp"

#include "punch.hpp"

u_char get_punch_id(u_int item_id)
{
    switch (item_id)
    {
        // Punch ID 1
        case 138: return 1; // Cyclopean Visor
        case 2976: return 1; // @note https://growtopia.fandom.com/wiki/Mods/Eye_Beam

        // Punch ID 2
        case 366: return 2; // Heartbow
        case 1464: return 2; // Golden Heartbow

        // Punch ID 3
        case 472: return 3; // Tommygun

        // Punch ID 4
        case 594: return 4; // Elvish Longbow
        case 4136: return 4; // Heatbow
        case 5424: return 4; // Winter Frost Bow
        case 5456: return 4; // Silverstar Bow
        case 10130: return 4; // Sun Shooter Bow

        // Punch ID 5
        case 768: return 5; // Sawed-Off Shotgun

        // Punch ID 6
        case 900: return 6; // Dragon Hand
        case 7758: return 6; // Prehistoric Dragon Claw
        case 7760: return 6; // Star Dragon Claw
        case 9272: return 6; // Draconic Claw

        // Punch ID 7
        case 910: return 7; // Reanimator Remote

        // Punch ID 8
        case 930: return 8; // Death Ray
        case 1010: return 8; // Destructo Ray
        case 6382: return 8; // Startopian Empire - Force Shield & Phase Blaster

        // Punch ID 9
        case 1016: return 9; // Six Shooter

        // Punch ID 10
        case 1204: return 10; // Focused Eyes

        // Punch ID 11
        case 1378: return 11; // Ice Dragon Hand
        case 1738: return 11;

        // Punch ID 12
        case 1440: return 12; // Evil Space Helmet

        // Punch ID 13
        case 1484: return 13; // Atomic Shadow Scythe

        // Punch ID 14
        case 1512: return 14; // Pet Leprechaun
        case 1648: return 14; // Unicorn Garland

        // Punch ID 15
        case 1542: return 15; // Battle Trout

        // Punch ID 16
        case 1576: return 16; // Fiesta Dragon

        // Punch ID 17
        case 1676: return 17; // Squirt Gun
        case 7504: return 17; // Super Squirt Rifle 500
        case 10288: return 17; // @note https://growtopia.fandom.com/wiki/Mods/Fire_Hose

        // Punch ID 18
        case 1570: return 18; // Mariachi Guitar
        case 1710: return 18; // Keytar
        case 1712: return 18; // Bass Guitar
        case 1714: return 18; // Tambourine
        case 4644: return 18; // Saxamaphone
        case 6044: return 18; // Fiesta Mariachi Guitar

        // Punch ID 19
        case 1748: return 19; // Flamethrower
        case 5002: return 19; // Playful Fire Sprite
        case 8006: return 19; // Hellfire Horns - Black
        case 8008: return 19; // Hellfire Horns - Blue
        case 8010: return 19; // Hellfire Horns - Orange
        case 8012: return 19; // Hellfire Horns - Ruby

        // Punch ID 20
        case 1780: return 20; // Legendbot-009

        // Punch ID 21
        case 1782: return 21; // Dragon of Legend

        // Punch ID 22
        case 1804: return 22; // Zeus' Lightning Bolt

        // Punch ID 23
        case 1868: return 23; // Violet Protodrake Leash
        case 1998: return 23; // Skeletal Dragon Claw

        // Punch ID 24
        case 1874: return 24; // Ring Of Force

        // Punch ID 25
        case 1946: return 25; // Ice Calf Leash
        case 2800: return 25; // Penguin Leash

        // Punch ID 26
        case 1952: return 26; // Owlbeard
        case 2854: return 26; // Phoenix Pacifier

        // Punch ID 27
        case 1956: return 27; // Chaos Cursed Wand

        // Punch ID 28
        case 1960: return 28; // Ecto Pack

        // Punch ID 29
        case 2636: return 29; // @note https://growtopia.fandom.com/wiki/Mods/Slasher
        case 2908: return 29; // Carrot Sword
        case 2952: return 29; // Digger's Spade
        case 3070: return 29; // @note https://growtopia.fandom.com/wiki/Mods/Slasher
        case 3108: return 29; // Chainsaw Hand
        case 3162: return 29; // Twin Swords
        case 3466: return 29; // Sushi Knife
        case 3932: return 29; // Rock Hammer
        case 3934: return 29; // Rock Chisel
        case 4166: return 29; // Death's Scythe
        case 4506: return 29; // Butcher Knife
        case 4956: return 29; // Emerald Pickaxe
        case 6312: return 29; // Phoenix Sword
        case 8554: return 29; // Caduceaxe
        case 8732: return 29; // Bamboo Sword

        // Punch ID 30
        case 1980: return 30; // Claw Glove

        // Punch ID 31
        case 2066: return 31; // Cosmic Unicorn Bracelet
        case 4150: return 31; // Eye Of Growganoth
        case 11078: return 31; // Cute Mutant Wonky
        case 11080: return 31; // Cute Mutant Plonky
        case 11082: return 31; // Cute Mutant Boogle

        // Punch ID 32
        case 2212: return 32; // Black Crystal Dragon

        // Punch ID 33
        case 2218: return 33; // Mighty Snow Rod

        // Punch ID 34
        case 2220: return 34; // Tiny Tank

        // Punch ID 35
        case 2266: return 35; // Crystal Glaive

        // Punch ID 36
        case 2386: return 36; // Heavenly Scythe

        // Punch ID 37
        case 2388: return 37; // Heartbreaker Hammer

        // Punch ID 38
        case 2450: return 38; // Diamond Dragon

        // Punch ID 39
        case 2476: return 39; // Burning Eyes
        case 4208: return 39; // Demonic Horns
        case 10336: return 39; // Black Burning Eyes

        // Punch ID 40
        case 4748: return 40; // Diamond Horns

        // Punch ID 41
        case 2512: return 41; // Marshmallow Basket

        // Punch ID 42
        case 2572: return 42; // Flame Scythe

        // Punch ID 43
        case 2592: return 43; // Legendary Katana
        case 2596: return 43; // Nacho Block
        case 9396: return 43; // Balrog's Tail

        // Punch ID 44
        case 2720: return 44; // Electric Bow

        // Punch ID 45
        case 2752: return 45; // Pineapple Launcher

        // Punch ID 46
        case 2754: return 46; // Demonic Arm

        // Punch ID 47
        case 2756: return 47; // The Gungnir

        // Punch ID 49
        case 2802: return 49; // Poseidon's Trident

        // Punch ID 50
        case 2866: return 50; // Wizard's Staff

        // Punch ID 51
        case 2876: return 51; // BLYoshi's Free Dirt

        // Punch ID 52
        case 2878: return 52; // FC Cleats
        case 2880: return 52; // Man U Cleats

        // Punch ID 53
        case 2906: return 53; // Tennis Racquet
        case 4170: return 53; // Logarithmic Wheel

        // Punch ID 54
        case 2886: return 54; // Baseball Glove

        // Punch ID 55
        case 2890: return 55; // Basketball

        // Punch ID 56
        case 2910: return 56; // Emerald Staff
        case 5006: return 56; // Playful Wood Sprite

        // Punch ID 57
        case 3066: return 57; // Fire Hose

        // Punch ID 58
        case 3124: return 58; // Soul Orb

        // Punch ID 59
        case 3168: return 59; // Strawberry Slime

        // Punch ID 60
        case 3214: return 60; // Axe Of Winter
        case 9194: return 60; // Hammer Of Winter

        // Punch ID 61
        case 3238: return 61; // Magical Carrot
        case 7408: return 61; // Abominable Snowman Suit

        // Punch ID 62
        case 3274: return 62; // T-Shirt Cannon

        // Punch ID 64
        case 3300: return 64; // Party Blaster

        // Punch ID 65
        case 3418: return 65; // Serpent Staff

        // Punch ID 66
        case 3476: return 66; // Spring Bouquet

        // Punch ID 67
        case 3596: return 67; // Pinata Pal

        // Punch ID 68
        case 3686: return 68; // Toy Lock-Bot

        // Punch ID 69
        case 3716: return 69; // Neutron Gun

        // Punch ID 71
        case 4290: return 71; // Solsascarf

        // Punch ID 72
        case 4474: return 72; // Skull Launcher

        // Punch ID 73
        case 4464: return 73; // AK-8087

        // Punch ID 75
        case 4746: return 75; // Diamond Horn

        // Punch ID 76
        case 4778: return 76; // Adventurer's Whip
        case 6026: return 76; // Whip of Truth

        // Punch ID 77
        case 3680: return 77; // Phlogiston
        case 4996: return 77; // Burning Hands

        // Punch ID 78
        case 4840: return 78; // Balloon Launcher

        // Punch ID 79
        case 5206: return 79; // Cloak of Falling Waters

        // Punch ID 80
        case 5480: return 80; // Rayman's Fist

        // Punch ID 81
        case 6110: return 81; // Pineapple Spear

        // Punch ID 82
        case 6308: return 82; // Beach Ball

        // Punch ID 83
        case 6310: return 83; // Watermelon Slice

        // Punch ID 84
        case 6298: return 84; // Smoog the Great Dragon

        // Punch ID 85
        case 6756: return 85; // Scepter of the Honor Guard

        // Punch ID 86
        case 7044: return 86; // Jade Crescent Axe

        // Punch ID 87
        case 6892: return 87; // Sorcerer's Tunic of Mystery

        // Punch ID 88
        case 6966: return 88; // Riding Bull

        // Punch ID 89
        case 7088: return 89; // Ezio's Armguards
        case 11020: return 89; // Dark Assassin's Armguards

        // Punch ID 90
        case 7098: return 90; // Apocalypse Scythe
        case 9032: return 90; // Scythe of the Underworld

        // Punch ID 91
        case 7192: return 91; // Shadow Spirit of the Underworld

        // Punch ID 92
        case 7136: return 92; // Ethereal Rainbow Dragon
        case 9738: return 92; // Staff of the Deep

        // Punch ID 93
        case 3166: return 93; // Pet Slime

        // Punch ID 94
        case 7216: return 94; // Mad Hatter

        // Punch ID 95
        case 7196: return 95; // Monarch Butterfly Wings
        case 9340: return 95; // Datemaster's Rose

        // Punch ID 96
        case 7392: return 96; // Mage's Orb

        // Punch ID 98
        case 7384: return 98; // Go-Go-Growformer!

        // Punch ID 99
        case 7414: return 99; // Chipmunk

        // Punch ID 100
        case 7402: return 100; // Snowflake Eyes with Rugged Winter Beard

        // Punch ID 101
        case 7424: return 101; // Snowfrost's Candy Cane Blade

        // Punch ID 102
        case 7470: return 102; // Narwhal Tusk Staff

        // Punch ID 103
        case 7488: return 103; // SLaminator's Boomerang

        // Punch ID 104
        case 7586: return 104; // Sonic Buster Sword
        case 7646: return 104; // Tyr's Spear

        // Punch ID 105
        case 7650: return 105; // Mjolnir

        // Punch ID 106
        case 6804: return 106; // Bubble Gun

        // Punch ID 107
        case 7568: return 107; // Hovernator Drone - White
        case 7570: return 107; // Hovernator Drone - Black
        case 7572: return 107; // Hovernator Drone - Red
        case 7574: return 107; // Hovernator Drone - Golden

        // Punch ID 108
        case 7668: return 108; // Air Horn

        // Punch ID 109
        case 7660: return 109; // Super Party Launcher
        case 9060: return 109; // Hilarious Honker

        // Punch ID 111
        case 7736: return 111; // Dragon Knight's Spear
        case 7826: return 111; // Heartstaff
        case 7828: return 111; // Golden Heartstaff
        case 7830: return 111; // Heartsword
        case 7832: return 111; // Golden Heartsword
        case 7912: return 111; // War Hammers of Darkness
        case 9116: return 111; // Red Laser Scimitar
        case 9118: return 111; // Green Laser Scimitar
        case 9120: return 111; // Blue Laser Scimitar
        case 9122: return 111; // Purple Laser Scimitar
        case 10334: return 111; // Black Balrog's Tail
        case 10498: return 111; // Candy Cane Scythe
        case 10578: return 111; // Mystic Battle Lance
        case 10626: return 111; // Rose Rifle
        case 10670: return 111; // Bow of the Rainbow
        case 10680: return 111; // Finias' Red Javelin
        case 11298: return 111; // Sakura's Revenge
        case 11312: return 111; // Spider Sniper
        case 11326: return 111; // Ferryman of the Underworld
        case 11380: return 111; // Black Bow of the Rainbow
        case 11440: return 111; // Mystic Bow
        case 11442: return 111; // Royal Mystic Bow

        // Punch ID 112
        case 7836: return 112; // Morty the Gray Elephant
        case 7838: return 112; // Morty the Orange Elephant
        case 7840: return 112; // Morty the Pink Elephant
        case 7842: return 112; // Morty the Diamond Elephant

        // Punch ID 113
        case 7950: return 113; // Ionic Pulse Cannon Tank

        // Punch ID 114
        case 8002: return 114; // Money Gun

        // Punch ID 116
        case 8022: return 116; // Junk Cannon

        // Punch ID 118
        case 8036: return 118; // Balloon Bunny

        // Punch ID 119
        case 9348: return 119; // Medusa's Crown

        // Punch ID 120
        case 8038: return 120; // Pet Egg

        // Punch ID 121
        case 8372: return 121; // Giant Eye Head

        // Punch ID 128
        case 8816: return 128; // Astro Shades - Red
        case 8818: return 128; // Astro Shades - Orange
        case 8820: return 128; // Astro Shades - Purple
        case 8822: return 128; // Astro Shades - Green

        // Punch ID 129
        case 8910: return 129; // Leaf Blower

        // Punch ID 130
        case 8942: return 130; // Dual Crescent Blade

        // Punch ID 131
        case 5276: return 131; // Celestial Lance
        case 8944: return 131; // Sun Blade

        // Punch ID 132
        case 8432: return 132; // Galactic Destructor - Green
        case 8434: return 132; // Galactic Destructor - Purple
        case 8436: return 132; // Galactic Destructor - Red
        case 8950: return 132; // Sniper Rifle

        // Punch ID 133
        case 8946: return 133; // Storm Breaker

        // Punch ID 134
        case 8960: return 134; // Spring-Loaded Fists

        // Punch ID 135
        case 9006: return 135; // Spirit of Anubis

        // Punch ID 136
        case 9058: return 136; // Cursed Katana

        // Punch ID 137
        case 9082: return 137; // Rocket-Powered Warhammer
        case 9304: return 137; // Brutal Hand Fans

        // Punch ID 138
        case 9066: return 138; // Claw Of Growganoth

        // Punch ID 139
        case 9136: return 139; // Dueling Star Fighter - Rebel Raider

        // Punch ID 140
        case 9138: return 140; // Dueling Star Fighter - Imperial Enforcer

        // Punch ID 141
        case 9172: return 141; // Armored WinterBot - Back

        // Punch ID 143
        case 9254: return 143; // Euphoric Dragon

        // Punch ID 144
        case 9256: return 144; // Party Bubble Blaster

        // Punch ID 145
        case 9236: return 145; // Ancient Shards

        // Punch ID 146
        case 9342: return 146; // Datemaster's Bling

        // Punch ID 148
        case 9378: return 148; // Lightning Gauntlets

        // Punch ID 149
        case 9376: return 149; // Mining Mech

        // Punch ID 150
        case 9410: return 150; // Radiant Doom Staff

        // Punch ID 151
        case 9462: return 151; // Boastful Brawler Hair

        // Punch ID 152
        case 9606: return 152; // Doomsday Warhammer

        // Punch ID 153
        case 9716: return 153; // Crystal Infused Sword

        // Punch ID 168
        case 10064: return 168; // Capuchin Leash

        // Punch ID 169
        case 10046: return 169; // Growboy

        // Punch ID 170
        case 10050: return 170; // Really Dangerous Pet Llama

        // Punch ID 171
        case 10128: return 171; // Mechanical Butler

        // Punch ID 172
        case 10210: return 172; // Cloak of Equilibrium

        // Punch ID 178
        case 10330: return 178; // Shadow Crown

        // Punch ID 180
        case 10388: return 180; // Swordfish Sword

        // Punch ID 184
        case 10442: return 184; // Winter Light Launcher

        // Punch ID 185
        case 10506: return 185; // Spruce Goose

        // Punch ID 188
        case 10652: return 188; // Love Charged Hands

        // Punch ID 191
        case 10676: return 191; // Shamrock Shuriken

        // Punch ID 193
        case 10694: return 193; // Mini Minokawa

        // Punch ID 194
        case 10714: return 194; // Steampunk Arm

        // Punch ID 195
        case 10724: return 195; // Lil Growpeep's Baaaa Blaster

        // Punch ID 196
        case 10722: return 196; // Bunny Ear Magnifying Glass

        // Punch ID 197
        case 10754: return 197; // Helping Hand

        // Punch ID 199
        case 10888: return 199; // Giant Pineapple Pizza Paddle

        // Punch ID 200
        case 10886: return 200; // Pineapple Chakram
        case 11308: return 200; // Royal Clam Cruiser

        // Punch ID 202
        case 10890: return 202; // Rocket Powered Pineapple Vacuum

        // Punch ID 203
        case 10922: return 203; // Pegasus Lance

        // Punch ID 205
        case 10990: return 205; // Alien Recon Wagon

        // Punch ID 206
        case 10998: return 206; // Suspicious Weather Balloon

        // Punch ID 207
        case 10952: return 207; // Space Badger

        // Punch ID 208
        case 11000: return 208; // Channel G News Van

        // Punch ID 209
        case 11006: return 209; // Soul Scythe

        // Punch ID 210
        case 11046: return 210; // Hydro Cannon

        // Punch ID 211
        case 11052: return 211; // Super Duper Ice Cream Scooper

        // Punch ID 212
        case 10960: return 212; // Space Rabbit

        // Punch ID 213
        case 10956: return 213; // Space Dog

        // Punch ID 214
        case 10958: return 214; // Space Pig

        // Punch ID 215
        case 10954: return 215; // Space Mouse

        // Punch ID 216
        case 11076: return 216; // Ambu-Lance

        // Punch ID 217
        case 11084: return 217; // Beowulf's Blade

        // Punch ID 218
        case 11118: return 218; // Sonic Buster Katana

        // Punch ID 219
        case 11120: return 219; // Primordial Jade Lance

        // Punch ID 220
        case 11116: return 220; // Crystal Crown

        // Punch ID 221
        case 11158: return 221; // Zodiac Ring

        // Punch ID 222
        case 11162: return 222; // Finger Gun

        // Punch ID 223
        case 11142: return 223; // Legendary Owl

        // Punch ID 224
        case 11232: return 224; // Ouroboros Charm

        // Punch ID 225
        case 11140: return 225; // Legendary Destroyer

        // Punch ID 226
        case 11248: return 226; // Staff of S'mores

        // Punch ID 227
        case 11240: return 227; // Seed Gatling Gun

        // Punch ID 228
        case 11250: return 228; // Harvest Jade Hat

        // Punch ID 229
        case 11284: return 229; // Paper Wasp Pet

        // Punch ID 231
        case 11292: return 231; // Lightning Umbrella

        // Punch ID 233
        case 11314: return 233; // Cauldron Cannon

        // Punch ID 234
        case 11316: return 234; // Skull of Burning Horrors

        // Punch ID 235
        case 11324: return 235; // Pharaoh's Pendant

        // Punch ID 236
        case 11354: return 236; // Turkey Float

        // Punch ID 237
        case 11438: return 237; // Digital Bow
        case 11464: return 237; // Ice Dragon Scythe
        case 11630: return 237; // Monkey Warrior's Staff
        case 11674: return 237; // Fancy Flute
        case 11716: return 237; // Mallet of Sucelles
        case 11718: return 237; // Harp of Aibell
        case 11760: return 237; // Growing Guardian Armor
        case 11786: return 237; // Soft-boiled Scepter
        case 11872: return 237; // Comida Crusher

        // Punch ID 241
        case 11814: return 241; // Rabbit Top Hat

        // Punch ID 242
        case 11548: return 242; // Guardian Armor
        case 11552: return 242; // Royal Guardian Armor

        // Punch ID 245
        case 11506: return 245; // Mask of The Dragon
        case 11508: return 245; // Royal Mask of The Dragon
        case 11562: return 245; // Pet Blood Dragon
        case 11704: return 245; // Alpha Wolf
        case 11706: return 245; // Royal Alpha Wolf
        case 11768: return 245; // Fish Tank Head
        case 11882: return 245; // Neurovision

        // Punch ID 248
        case 11818: return 248; // Equinox Scarf
        case 11876: return 248; // Serpent Shoulders

        default: return 0;
    }
}

void punch(ENetEvent& event, const std::string_view text) 
{
    const std::string id{ text.substr(sizeof("punch ")-1) };
    ::peer *pPeer = static_cast<::peer*>(event.peer->data);

    pPeer->punch_effect = (u_char)atoi(id.c_str());
    on::SetClothing(*event.peer);
}