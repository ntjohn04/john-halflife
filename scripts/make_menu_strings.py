"""Writes the main menu's text as the WON versions had it into John Half-Life's GameUI strings.

Usage:
  python make_menu_strings.py
"""

import os
import re

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BASE = os.path.join(REPO, "config", "mod", "resource", "gameui_english.txt")

ALL = {
    "GameUI_GameMenu_ResumeGame": "Resume Game",
    "GameUI_GameMenu_PlayerList": "Player List",
    "GameUI_GameMenu_NewGame": "New Game",
    "GameUI_GameMenu_SaveGame": "Save Game",
    "GameUI_GameMenu_LoadGame": "Load Game",
    "GameUI_GameMenu_FindServers": "Find Servers",
    "GameUI_GameMenu_CreateServer": "Create Server",
    "GameUI_GameMenu_ChangeGame": "Change Game",
    "GameUI_GameMenu_Options": "Options",
    "GameUI_GameMenu_Disconnect": "Disconnect",
    "GameUI_GameMenu_LeaveGame": "Leave Game",
    "GameUI_GameMenu_Quit": "Quit",
    "GameUI_GameMenu_PlayDemo": "Play Demo",
    "GameUI_GameMenu_Multiplayer": "Multiplayer",
    "GameUI_GameMenu_Customize": "Customize",
    "GameUI_TrainingRoom": "Hazard Course",
    "GameUI_QuitConfirmationText": "Do you wish to stop playing now?",
    # Half-Life's own where WON's isn't known.
    "GameUI_MainMenu_Hint_NewGame": "Start a new game.",
    "GameUI_MainMenu_Hint_LoadGame": "Load a previously saved game.",
    "GameUI_MainMenu_Hint_ChangeGame": "Select a custom game, search the internet for custom games.",
    "GameUI_MainMenu_Hint_Configuration": "Change game settings, configure controls.",
}

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
        f.write(text.encode("utf-16"))


CAPITALS = {key: value.upper() for key, value in ALL.items() if key.startswith("GameUI_GameMenu_")}


def main():
    base = open(BASE, "rb").read().decode("utf-16")
    base = apply(base, {**ALL, **GAMES[None]})
    write(BASE, base)
    for game in ("op4", "bs"):
        write(os.path.join(REPO, "config", "expansions", game, "resource", "gameui_english.txt"),
              apply(base, {**CAPITALS, **GAMES[game]}))
    print("make_menu_strings: Half-Life's, Opposing Force's and Blue Shift's menu text written")


if __name__ == "__main__":
    main()
