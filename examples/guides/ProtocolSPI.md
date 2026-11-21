# Lab 28 — SPI Protocol — Master Transactions

**Level:** Intermediate\
**Category:** Digital Instruments\
**Pages:** SPI Protocol

## What you will build or observe

Configure an AD3 SPI master and inspect a command/response transfer.

## Before you run

Use Simulation for structure; physical use needs a voltage-compatible SPI target, a shared ground and valid wiring.

## Follow along

1. Press CONFIGURE.
2. Try TRANSFER 0x9F.
3. If a compatible SPI flash is connected, try FLASH READ ID.
4. Press RELEASE PINS.

## Follow the JavaScript

`spiCfg` → `onSpiCfgClicked()` → `Protocol1.SPI.configure({clock:0,mosi:1,miso:2,select:3,...})`.

All entries below come from **saved widget Properties → Events** bindings in this `.wfpanel` (not from manual `ui.on()` calls). The script contains the named functions.

| Page | Control (ID) | Event | JavaScript function |
|---|---|---|---|
| SPI Protocol | `spiCfg` (button) | `clicked` | `onSpiCfgClicked()` |
| SPI Protocol | `spiXfer` (button) | `clicked` | `onSpiXferClicked()` |
| SPI Protocol | `spiReadId` (button) | `clicked` | `onSpiReadIdClicked()` |
| SPI Protocol | `spiStop` (button) | `clicked` | `onSpiStopClicked()` |

## Make one small change

Change the configured SPI mode only when your connected target supports it.

To modify the example in the app, choose **Stop / Design**, open the Script Editor, find the named function or constant, make one change, and run the Panel again. When you like it, use **Save As** to keep your own project.

## Why this works

Clock, MOSI, MISO and chip-select are separate signals; receiving data requires a responding target.

## Safety and scope

Check target voltage, pin direction and chip-select wiring before driving physical pins.

## If something does not work

Simulation output is marked simulated; it is not proof that a flash device exists.

## Related files

- **Editable project:** `examples/panels/ProtocolSPI.wfpanel`
- **Readable JavaScript:** `examples/source/ProtocolSPI.js`
- The `.wfpanel` embeds the script, Pages and event assignments; opening it in WFScript does not require the `.js` file alongside it.
- In the complete source tree run `python tools/build_examples.py` after modifying the readable `.js` version.
