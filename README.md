AI Player EcoSys - 2D Survival (SDL2/OpenGL + C++ & Python)

A digital survival simulation with 25 AI players roaming a 2D world.
Each agent learns by trial and error to grab coins, buy food, recharge energy, avoid collisions, and survive longer. Nothing is hard coded, behavior evolves as they experience rewards and penalties.

You can watch learning happen live, hover any agent to see their full HUD, and use the neon stats panel to track the whole populations progress.

Overview (what youll see)

Agents compete and adapt in a 2048x2048 world.

Coins you drop are key resources, agents collect them to buy food in the STORE zone (top left).

Low energy? They head to the RECHARGE zone (top right).

If agents fail to eat or recharge, they collapse and respawn weaker (death counter increments).

Over time they shift strategies, As runs go longer youll see them stockpile more, "realizing" a larger buffer helps survival.

Mystery Crates (purple) add variety. They can grant +3 coins, +1 food, speed boost (8s), or +30 health. Spawn them with S or wait for periodic spawns.

Live Stats & Legend

Hover your mouse over an agent to see their per agent HUD (name, full stats, behavior).
The left stats panel (toggle F1) shows a ranked, neon green table of everyone.

Legend:

H = Health

E = Energy

C = Coins

F = Food (carried units)

IQ = Intelligence level (learning progress proxy)

P = Performance points (overall score)

D = Deaths (times theyve died and respawned)

Zones are labeled STORE and RECHARGE directly on world.

Key Mechanics (why it feels alive)

Reinforcement signals (C++ -> Python)
Agents receive small rewards/penalties for outcomes like collecting coins, buying/eating food, keeping safe distance, recharging, or getting too close to others.

IQ based stamina
As IQ increases, energy drains 12% slower at high IQ, translating to roughly +3 seconds longer before needing recharge. Smarter agents recharge less often.

Food economics
Food costs 5 coins at the STORE. Agents stockpile a small reserve (based on IQ) and only eat when low (H ≤ 70 or E ≤ 60). Eating restores +25 health and +20 energy.

Separation steering (no pileups)
Agents apply repulsive steering when too close, reducing collisions and clustering.

Deaths & respawn
Ignoring survival rules leads to collapse and respawn with penalty; the D counter exposes how tough the world is.

Mystery Crates
Random boons (+3 coins, +1 food, speed boost for 8s, or +30 health) inject exploration pressure and strategy shifts.

Controls

Left Click - Drop a coin at the mouse position

S - Spawn a Mystery Crate at the mouse position

Right drag - Pan camera

Mouse Wheel "+" and "-" for Zoom in/out (zoom centers on cursor)

0 - Reset camera

F1 - Toggle the left stats panel

ESC / Q - Quit

Hover an agent to reveal their personal HUD.

Build &Run

Dependencies: SDL2, SDL2_image, SDL2_ttf, OpenGL (GLEW), Python 3.13 (headers & lib).

Files expected in your working folder:

brain.py
main.cpp
DejaVuSans.ttf
images/
  player.png   (70 x 120)
  coin.png     (35 x 35)


Compile e.g:
g++ -std=c++17 -Wall -Wextra -pedantic main.cpp -o app \
  $(sdl2-config --cflags --libs) -lSDL2_image -lSDL2_ttf -lGL -lGLEW \
  -I/usr/include/python3.13 -L/usr/lib -lpython3.13 -ldl -lm

Run: ./app

You do not need to run brain.py yourself.
The C++ app embeds Python and imports brain.py directly just keep brain.py in the same folder.

How learning works (at a glance)

Each frame, C++ sends a compact world state to brain.py.

The Python "brain" returns velocities and a HUD string per agent.

C++ computes outcomes (coin pickups, eating, recharging, safe spacing, deaths) and sends rewards back to the brain.

The brain updates memory, saves state to disk, and gradually adapts decisions.
Over longer runs youll notice strategy evolution (e.g., bigger coin buffers, saner shopping/eating, less panic recharging).

Tuning highlights (already in code)

Larger world (2048 x 2048) for room to roam.

Hover only HUD above agents to reduce clutter.

Neon green stats panel with black outline for readability.

Food reserve logic so F finally climbs and doesn’t auto zero.

IQ -> stamina scaling for fewer recharge trips as agents improve.

Tips for a good demo

Start by dropping clusters of coins away from zones to see gathering and travel decisions.

Spawn a few Mystery Crates (S) to shake up priorities.

Zoom into the STORE/RECHARGE zones to watch transactional behaviors.

Let it run for a while, strategic shifts become clearer after many ticks.

Troubleshooting

Blank window / missing textures
Ensure images/player.png, images/coin.png, and DejaVuSans.ttf are present.

Python load errors
Keep brain.py in the same folder as the executable (or add that folder to PYTHONPATH).

Slow performance
Reduce window size or zoom out less, ensure GPU drivers are active.

Compilation warnings
The provided main.cpp resolves typical warning hot spots, use the compile string above.

Roadmap Ideas

Expose crates to brain.py so agents can deliberately seek them.

Add alliances and betrayal signals to reward shaping.

Introduce terrain costs or obstacles to encourage path planning.

Persistent lineage/genetics for inter-run evolution.

Credits

Rendering/UI: SDL2, SDL2_image, SDL2_ttf, OpenGL (GLEW)

Learning & memory: Python 3.13 (brain.py embedded)

License

Personal/experimental use by default. Adapt as you see fit for your lab/demo (:

