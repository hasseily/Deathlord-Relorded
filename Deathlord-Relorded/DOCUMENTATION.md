# DEATHLORD RELORDED

Thank you for downloading Deathlord Relorded!

This guide accompanies the 3.0 test build for Windows, Linux, and macOS. Read
the included `README.md` for installation, platform requirements, and save-file
handling. The game runs from the included hard disk image; no disk swapping is
required. `Ctrl-P` pitches camp, and `Alt-P` pauses or resumes the emulator.

Deathlord was an RPG released in 1987 for the Apple 2 and C64 platforms, highly influenced by the likes of Ultima, Wizardry and Bard's Tale. It was a lot less successful than these 3 franchises, partly due to a couple of significant errors:

- About 5 weeks before release, Electronic Arts requested of the developers David Wong and Al Escudero to change the setting of the game from Nordic to Japanese, in order to capitalize on the wave of JRPGs during that time (although Deathlord is certainly not a JRPG).
- During development, there was a strong quality assurance team that would consistently play the development builds of the game. They became very good at it, and they would solve the riddles faster and faster, so the devs kept bumping up the difficulty with more devious traps.
  
These mistakes made for an immensely difficult game to pick up for any new player, and it had some weird issues like names of cities that would sometimes switch from pseudo-Japanese to more Western. And having class names like "Ansatsusha" (assassin) didn't really help either.
Well here we are, 35 years later. Deathlord is, with all its flaws, an incredible RPG with unique puzzles that deserves to be played, but not in its original incarnation with perma-death, tons of reboots, loss of gold when you pool it over the limit, etc...
And this is why you now are in possession of Deathlord Relorded (DLRL), which aims to let you enjoy Deathlord with all its tricks without the frustration.

## HOW TO PLAY

Download the original documentation from:

<https://archive.org/download/Deathlord/Deathlord.pdf>

It will provide the critical list of commands, as well as races, classes and spell descriptions. That is all you need from the original documentation. Note that minimums and maximums of race attributes are wrong, and so are some other details so don't bother reading too much into the tables. There are also some textual mistakes, but overall, the documentation does a decent job at explaining what's going on.

**Do read the original documentation or your experience will be problematic, to say the least!**

For the rest, take a look below. Much has changed. For the better, we hope.

## CHARACTER CREATION

When you first launch the game, you will be greeted by the original interface, complete with graphical artifacts from the period. There's also a (hopefully) helpful line below the original game area that explains what can and should be pressed.

Press `C` to go into character options. In general, press `ESC` to go back and exit whatever screen you're on, if you need to.

**DLRL 3.0 includes the boot and scenario data in one hard disk image. There are no separate boot or scenario disks to insert.**

Using the arrow keys and `Enter`, select "Conjure a New Character". Select a race, and it will ask you if you're happy with the attributes. Press `A` when the Relorded hint offers automatic rolling. It stops when Strength, Constitution, Intelligence, and Dexterity are all at least one below the maximum for that race. Almost any key cancels automatic rolling.

### RACES AND CLASSES FEATURES IN DEATHLORD RELORDED

Speaking of races, you can find the minimum and maximum acceptable attributes for each race here:

<https://github.com/hasseily/Deathlord-Relorded/wiki/Player-Characters>

The original documentation is wrong on those. DLRL doesn't change this data. However, DLRL does add many features to races and classes that are missing in the original. Why, you ask? Good question, fellow gamer. You see, if/when you ever bother to read the few excellent FAQs on Deathlord, every single one of them will tell you that there's no point in using any race other than human. And as for classes, you'll probably never play a ranger or peasant, for example. So DLRL upgrades all of these.

Of special mention are the gnome's incredible dark-vision in dungeons, and the peasant's extreme resilience. It is up to you to decide if you would have an interest in taking advantage of these unique abilities.

#### Toshi and Nintoshi (Elf and Half-Elf)

- Doesn't get damaged by swamp or cactus tiles

#### Troll and Ogre (Orc and Half-Orc)

- Doesn't get damaged by swamp or cactus tiles

#### Gnome

- In dungeons and towers, if the leader, the Gnome will see at the maximum distance in the dark

#### Obake (Halfling)

- Identifies automatically hidden doors, unique trees and tombstones
- If the leader, identifies pits automatically

#### Ryoshi (Ranger)

- Doesn't get damaged by swamp or cactus tiles
- On the overland, a ranger will forage for food for the whole party
- Identifies automatically poisoned and bonus pools, as well as hidden doors and unique trees and tombstones
- If the leader, identifies pits automatically

#### Yabanjin (Barbarian)

- Doesn't get damaged by swamp or cactus tiles
- Identifies automatically poisoned and bonus pools

#### Yakuza (Thief)

- Identifies automatically weapon and armor chests, hidden doors and unique trees and tombstones
- If the leader, identifies pits automatically

#### Shizen (Druid)

- Doesn't get damaged by swamp or cactus tiles
- Identifies automatically poisoned and bonus pools

#### Genkai (Illusionist)

- Identifies automatically teleporters and chutes, illusiory walls and rocks

#### Kosaku (Peasant)

- On the overland, a peasant will forage for food for the whole party
- A peasant is very resilient and never gets damaged by any environment tile (swamp, fire, etc...)
- Every 5th level, the peasant gets one random attribute point increase. There is no maximum.

## CREATING A GROUP OF ADVENTURERS

Once you've created 6 characters, it is time to assemble a group. First, make sure you select "Disperse Existing Group" before assembling a new group.

**DEATHLORD RELORDED ONLY SUPPORTS A FULL PARTY OF 6 CHARACTERS!!!!**

Return to the main menu and choose **Play Game** (`P`). Press `Space` when the Relorded startup screen prompts you to continue.

## Special features of DLRL in-game

- If you do nothing, time will pass but not a turn. Pause the game via the "Emulator" menu to completely freeze everything.
- Visibility is much improved compared to the original game. You can see up to 16 tiles away in the overland or in town.
- All character info is available at all times.
- Many bugs have been fixed, such as gold pool loss and ninja/monk armor class reset at level 32.
- Some items can be used by more classes, such as the crossbow being available to the peasant.
- Ranged weapons can be used from any position in the party, not just the front line.
- Much more information is available in battle, such as turns disabled or XP gained.
- Brand new inventory screen accessible via `INS` or the "Relorded" menu.
- Original interface accessible via `F11` or the "Relorded" menu. The new rules still apply. If you want to play the original game, just load up the images in the "Images" directory in your favorite Apple 2 emulator. But we have no idea why you'd want to do this unless you're a masochist.
- Toggle with `F10` between the pseudo-Japanese words and classic English language D&D. I took the liberty to significantly change the meaning to some of them in order to better align them with their stats.
- Cheat-sheet spell window accessible via `Alt-S` or the "Relorded" menu.

## Relorded changes and the Hacking menu

Open **Relorded > Relorded Changes** to enable or disable the changes to the base game described below. For direct character, health, status, and inventory edits, use **Hacking > Party Editor...** with an active party. The Hacking menu also provides **Invincible** and **Automatic Battle Success**; both default to off.

- "Distribute Food": Automatically distributes food so that when the active character buys food, they can buy the maximum amound (100) in one shot.
- "Distribute Gold": Automatically distributes gold when it's acquired, so that it isn't wasted if the active character has reached the 10,000 limit.
- "Expanded Weapon Use": Changes weapon restrictions to conform to the Relorded file `DLRLAssets/InventoryList.csv`.
- "Extra Race And Class Bonuses": See the section on races and classes.
- "Exit Pit By Moving": Exit a pit by moving in addition to using the '^' key.
- "Fix Gold Pooling": When pooling gold, ensure that any gold above the 10,000 limit is redistributed to others and not lost.
- "Freeze Time When Idle": Do not pass a turn when doing nothing.
- "Keep Extra XP On Levelup": When leveling up, don't discard the extra gained XP.
- "Magic Water Increases Stats": Magic water always increases stats instead of any random effect (including negative!).
- "No Autosave After Death": Stops the game from autosaving upon the death of a character (perma-death).
- "No HP Loss From Starvation": You don't lose HP when starving, but you won't heal when resting if you don't have food.
- "No Level Drain": I hate level drain.
- "No Stats Limit": Stats can increase without a limit. The original limits you to starting maximums.
- "Ranged Attack For Rear Line": Rear line can used ranged weapons.
- "Search Always Succeeds": In the original, search success is between 29% and 68% depending on the race of the party leader. See <https://github.com/hasseily/Deathlord-Relorded/wiki/Logic-for-random-checks#searching>.
- "XP Reallocation": After battle, properly allocate XP to the whole party. The original favors the leader and doesn't give out XP to those who don't act.

**Relorded > Original Interface Darkness** controls the curtain behind the original interface (`F11`). `Alt-Enter` switches between full screen and windowed mode. **File > Export active party...** and **Import party...** manage party backups; use these before trying party edits or other gameplay tools.

Thank you for playing!

Please let us know your feedback on itch.io or github.

<https://rikkles.itch.io/deathlord-relorded>

<https://github.com/hasseily/Deathlord-Relorded>

Rikkles, 2025.
