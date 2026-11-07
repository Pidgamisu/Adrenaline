# The itch.io page: text to paste in

Drafted 2026-11-03. Each heading below is a field on the itch.io project edit page. Nothing here is
final - it is a first draft to argue with, and the taglines are deliberately offered as a choice
because that one is taste.

Controls and the piece list are taken from `Plugins/GetawayMovement/README.md`, not invented.

---

## Title

    Adrenaline

## Short description / tagline

Shown under the title and in listings. Pick one:

1. `Draft a piece. Place it. Race your friends. Nobody scores if everybody finishes.`
2. `Build the course, then race it. Wallrun, slide and blow holes in everyone else's plans.`
3. `A first-person party platformer about making the course hard for everyone but you.`

Number 1 states the hook and the scoring rule in one line, which is the thing that makes people
understand the game. Number 2 sells the movement. Number 3 is the shortest.

## Classification

- **Kind of project:** Downloadable
- **Classification:** Game
- **Genre:** Platformer
- **Release status:** In development

## Description

> **You and your friends build the course you are about to race down.**
>
> Every round deals you one piece from the box - a ramp, a breakable panel, a bridge that gives way
> under you, a bed of spikes, a stick of C4 - and you drop it anywhere in the level. Then everyone
> runs it at once.
>
> Here is the catch: **nobody scores if everybody finishes.** So place a piece that stops the others
> and not you. That spike pit is only worth putting down if you are sure you can clear it. Get
> greedy and you will build something nobody gets past, including yourself.
>
> ### Movement is the whole game
>
> This is not a game about blocking the path. It is about changing what the path *is*. The movement
> is Source-style and it goes deep:
>
> - **Bunny hop** without losing speed - hold jump and it auto-hops for you
> - **Air-strafe** to gain speed and curve in mid-air
> - **Slide** by crouching at speed, and go faster downhill
> - **Wallrun** any wall beside you, then kick off it with jump
> - **Double jump** that redirects your momentum where you are holding, without costing you speed
> - **Mantle** a ledge mid-jump and keep three quarters of your run-up
>
> Every piece is chosen to open a new line rather than close one. A wall is a wallrun surface. A
> ramp is a launch. A crate is a step up *and* something to smash through at speed. C4 is a rocket
> jump if you aim it at your own feet.
>
> ### Eleven pieces
>
> Floor, wall, ledge, breakable panel, crate, ramp, collapsing bridge, C4 pickup, spikes, jump pad,
> and an anti-gravity vortex that lifts you for as long as you stay inside it.
>
> ### Playing with other people
>
> One player hosts and the others join by typing the host's address, which the menu shows the host
> so they can read it out. On a home network that is all it takes.
>
> **Playing with someone far away, there are two ways.** The easy one: put both computers on a mesh
> VPN - [Tailscale](https://tailscale.com) is free and takes a few minutes - and the menu will show
> the host a `100.x` address labelled *Tailscale*, which is the one to read out. The other way is to
> forward **UDP port 7777** through the host's router. There is no matchmaking and no server list;
> the game cannot punch through a router on its own yet.
>
> **Being straight with you: this has been played across two computers in two different states,
> over Tailscale, and it held up - smooth movement, and the deaths, the breakables, the C4 and the
> scoring all stayed in step. It has not been tested over a forwarded port.** If you try it with a
> friend, I would genuinely like to hear whether it worked.
>
> ### This is an early build
>
> - **One course.** The drafting, the build clock, the race and the scoring all work - there is just
>   a single level to build in so far.
> - **No sound at all.** That is not a bug, it has not been made yet. Bring your own music.
> - **The look is deliberate.** Unlit pink on black, every surface see-through with a solid band
>   round its edge. Greybox as a style rather than a placeholder.
>
> Feedback on how the movement *feels* is the most useful thing you can give me.

## Install instructions

> Unzip anywhere and run **Getaway.exe**.
>
> If it refuses to start, run `Engine\Extras\Redist\en-us\vc_redist.x64.exe` from inside the folder
> - that is the Microsoft runtime Unreal needs, and some machines do not have it yet.
>
> Windows only for now.

## Controls

itch.io has no controls field, so this goes at the end of the description or in install
instructions.

> **While you are building**
>
> | Key | Action |
> |---|---|
> | W A S D, Space / C | Fly the camera |
> | Left Shift | Fly faster |
> | Mouse wheel | Choose which piece from your hand |
> | R | Turn the piece 90° |
> | Left click | Place it |
> | Enter | Done building - everyone starts running |
>
> **While you are running**
>
> | Key | Action |
> |---|---|
> | W A S D | Move |
> | Space, or mouse wheel | Jump. Hold to auto-hop. Press again in the air to double jump |
> | C, Left Shift or Left Ctrl | Crouch. At speed it becomes a slide |
> | A / D + mouse, in the air | Air-strafe for speed and curve |
> | Space, on a wall beside you | Kick off a wallrun |
> | W into a ledge, mid-jump | Mantle |
> | Left click | Use what you are carrying. C4: throw it, click again to set it off. Grapple: throw the hook, click again to let go |
> | Escape | Menu |
>
> **Controller** - swap between pad and mouse mid-round, nothing to set up.
>
> | Control | Action |
> |---|---|
> | Left stick | Move, or fly the camera while building |
> | Right stick | Look |
> | Triggers, while building | Camera up and down |
> | A | Jump, or place the piece |
> | X | Turn the piece |
> | LB or B | Crouch and slide |
> | RB, then RT | Throw C4, then detonate |
> | Start | Done building |
> | D-pad Down | Restart |
>
> You carry two C4 charges at most, and a thrown charge is spent for good - run through a C4 pickup
> to restock.

## Tags

    first-person, parkour, movement, multiplayer, local-multiplayer, platformer, party-game,
    fps, physics, speedrun

itch.io allows up to ten. `parkour` and `movement` are the ones that find the audience who care
about this kind of game; `party-game` sets the expectation that it is meant to be played with
people.

## Pricing

**No payment / free.** For a one-map, silent early build this is the honest option, and it is also
the one that actually gets played - a price tag on an unknown first release stops almost everybody.
A "name your own price" donation can be turned on later without changing the page.

## Visibility

Keep it on **Draft** until the build has been looked at on screen and the page reads right. Draft is
private to you. **Restricted** is the useful middle setting if you want a few people to try it
before it is public.

## Still to make

Not text, and the page is weaker without them:

- **Cover image**, 630x500. Listings crop it to 315x250 from the centre, so keep anything that
  matters away from the edges.
- **Four or five screenshots.**
- **A short gif or video of movement at speed.** This game's whole appeal is motion, and a still
  screenshot cannot show a wallrun into a slide into a C4 jump. This is the single highest-value
  asset for the page.
