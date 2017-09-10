# CMP303 Networking

## Overview
This is a **two player** networked application utilizing a **client-server** architecture and an application-layer protocol developed on top of **TCPv4**. There is also support for **spectators**.

## "Gameplay"
The application plays like a combination of **pong** and **space invader**. Each player controls a paddle that can move only left and right. The players may also shoot bullets at each other, however hit-detection is not implemented (or any *real* gameplay for that matter).

## Techniques
The application demonstrates **client-side prediction**, **server reconciliation**, and **entity interpolation**.

## Screenshots
![alt text](https://github.com/goran2711/cmp303/blob/master/github/cmp303.png "Blue outlines show the bullets' actual positions on the client")

Blue outlines show the bullets' actual positions on the client.

## Controls
**A** - Strafe left

**D** - Strafe Right

**Space** - Shoot

**F1** - Toggle prediction

**F2** - Toggle reconciliation

**F3** - Toggle interpolation

**F4** - Toggle drawing of local client's *actual* bullet positions

## Third-party libraries
The application utilizes **SFML** for windowing, graphics and networking.
