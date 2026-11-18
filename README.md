# Horse Whistle

An SKSE plugin for Skyrim SE/AE. It adds a **Horse Whistle**, a reusable item with the vanilla flute's model that calls your horse to you.

## Buying one

Talk to any of the five stablemasters and choose **"I'd like to buy a horse whistle. (25 gold)"**. The line only appears if you have at least 25 gold. The stablemasters are Skulvar Sable-Hilt (Whiterun), Geimund (Solitude), Ulundil (Windhelm), Hofgrir Horse-Crusher (Riften) and Cedran (Markarth).

The stablemaster's reply has no voice recording, so it flashes past. [Fuz Ro D-oh](https://www.nexusmods.com/skyrimspecialedition/mods/15109) keeps silent lines on screen if you want to read it.

## Using it

Use the whistle from your inventory (it's listed with potions). It's never used up. If you own a horse that's alive, it appears a few steps in front of you, facing you.

The horse called is the last horse you rode, if you own it. Otherwise it's the last horse you owned and rode, which the mod remembers in your save. Stolen horses aren't called.

It won't work:
- **Indoors.** Horses can't enter interiors.
- **While you're riding.**
- **If your horse is dead,** or you've never ridden a horse you own.

## Requirements

- SKSE64 for your game version
- Address Library for SKSE Plugins ("All in one (Anniversary Edition)" for AE)

## Install

Install the build zip with your mod manager and enable `HorseWhistle.esp`. It's a light (ESL) plugin, so it doesn't use a load-order slot.

## Log

`Documents/My Games/Skyrim Special Edition/SKSE/HorseWhistle.log`

## Building

`.github/workflows/build.yml` builds the DLL on GitHub Actions. `tools/EspGen` regenerates the `.esp` with Mutagen: `dotnet run --project tools/EspGen -- data/HorseWhistle.esp`.
