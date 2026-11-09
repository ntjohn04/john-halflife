"""Writes the main menu's text as the WON versions had it into John Half-Life's GameUI strings: config/mod's (Half-Life,
and Uplink, which take it) in place, and Opposing Force's and Blue Shift's from it (config/expansions/<game>, installed
over config/mod's). Running it again changes nothing.

As WON had it, in all three: the menu's labels in capitals (OPTIONS kept, more readable than WON's CONFIGURATION), a
hint beside each button (the first start keeps Half-Life's GameMenu.res's hints, jhl_setup.cpp), and Hazard Course
for the Training Room. Opposing
Force's drill sergeant: its hints, its quit question and its Boot Camp. From the community's restorations of the WON
menus (GameBanana's "Opposing Force WON Menu restored", "More Accurate WON Menu", "Blue Shift WON Menu Restored!") and
PCGamingWiki's menu text corrections, which agree. Labels for buttons John Half-Life's menu doesn't have stay as they are.

Usage:
  python make_menu_strings.py
"""

import os
import re

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BASE = os.path.join(REPO, "config", "mod", "resource", "gameui_english.txt")

ALL = {
    "GameUI_GameMenu_ResumeGame": "RESUME GAME",
    "GameUI_GameMenu_PlayerList": "PLAYER LIST",
    "GameUI_GameMenu_NewGame": "NEW GAME",
    "GameUI_GameMenu_SaveGame": "SAVE GAME",
    "GameUI_GameMenu_LoadGame": "LOAD GAME",
    "GameUI_GameMenu_FindServers": "FIND SERVERS",
    "GameUI_GameMenu_CreateServer": "CREATE SERVER",
    "GameUI_GameMenu_ChangeGame": "CHANGE GAME",
    "GameUI_GameMenu_Options": "OPTIONS",
    "GameUI_GameMenu_Disconnect": "DISCONNECT",
    "GameUI_GameMenu_LeaveGame": "LEAVE GAME",
    "GameUI_GameMenu_Quit": "QUIT",
    "GameUI_GameMenu_PlayDemo": "PLAY DEMO",
    "GameUI_GameMenu_Multiplayer": "MULTIPLAYER",
    "GameUI_GameMenu_Customize": "CUSTOMIZE",
    "GameUI_TrainingRoom": "Hazard Course",
    "GameUI_QuitConfirmationText": "Do you wish to stop playing now?",
    # Half-Life's own where WON's isn't known.
    "GameUI_MainMenu_Hint_NewGame": "Start a new game.",
    "GameUI_MainMenu_Hint_LoadGame": "Load a previously saved game.",
    "GameUI_MainMenu_Hint_ChangeGame": "Select a custom game, search the internet for custom games.",
    "GameUI_MainMenu_Hint_Configuration": "Change game settings, configure controls.",
}

# The menu draws each hint beside its button, unwrapped, so with the labels in capitals a long one can run off the
# screen at 640x480, the smallest mode and the one to check: FIND SERVERS with Half-Life's own hint ("Search for online
# multiplayer servers.") and WON's ("View a list of Half-Life game servers and join the one of your choice.") did, so
# Half-Life's and Blue Shift's server hints are short; Opposing Force's, and every other, fit.
GAMES = {
    # Half-Life (config/mod).
    None: {
        "GameUI_PlayGame": "Play Half-Life",
        "GameUI_MainMenu_Hint_FindServer": "Join a Half-Life game online.",
        "GameUI_MainMenu_Hint_CreateServer": "Host a Half-Life game online.",
        "GameUI_MainMenu_Hint_QuitGame": "Quit playing Half-Life.",
    },
    "op4": {
        "GameUI_PlayGame": "Play Opposing Force",
        "GameUI_TrainingRoom": "Boot Camp",
        "GameUI_QuitConfirmationText": "You going AWOL on me, son?",
        "GameUI_MainMenu_Hint_NewGame": "Begin a new mission, soldier.",
        "GameUI_MainMenu_Hint_LoadGame": "Load a saved mission.",
        # Not WON's: its main menu had no such buttons (Multiplayer led to Internet and LAN games, its manual says), so
        # in the drill sergeant's voice as the rest.
        "GameUI_MainMenu_Hint_FindServer": "Prepare for combat. This is not an exercise!",
        "GameUI_MainMenu_Hint_CreateServer": "Start your own adventure with your buddies.",
        "GameUI_MainMenu_Hint_ChangeGame": "Select a custom game, maggot.",
        "GameUI_MainMenu_Hint_Configuration": "Looking to tweak out your setup, dirtbag?",
        "GameUI_MainMenu_Hint_QuitGame": "A good soldier never leaves his squad behind.",
    },
    # Blue Shift had no multiplayer: Half-Life's own hints there.
    "bs": {
        "GameUI_PlayGame": "Play Blue Shift",
        "GameUI_MainMenu_Hint_FindServer": "Search for online servers.",
        "GameUI_MainMenu_Hint_CreateServer": "Host a server for others.",
        "GameUI_MainMenu_Hint_QuitGame": "Quit playing Blue Shift.",
    },
}


def apply(text, values):
    for key, value in values.items():
        pattern = re.compile(r'^(\s*"' + re.escape(key) + r'"\s+)"[^"]*"', re.M)
        text, count = pattern.subn(lambda m: m.group(1) + '"' + value + '"', text)
        if count != 1:
            raise SystemExit(f"make_menu_strings: {key} found {count} times")
    return text


def write(path, text):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "wb") as f:
        f.write(text.encode("utf-16"))  # little-endian with its byte order mark, as GameUI's files


def main():
    base = open(BASE, "rb").read().decode("utf-16")
    base = apply(base, {**ALL, **GAMES[None]})
    write(BASE, base)
    for game in ("op4", "bs"):
        write(os.path.join(REPO, "config", "expansions", game, "resource", "gameui_english.txt"), apply(base, GAMES[game]))
    print("make_menu_strings: Half-Life's, Opposing Force's and Blue Shift's menu text written")


if __name__ == "__main__":
    main()
