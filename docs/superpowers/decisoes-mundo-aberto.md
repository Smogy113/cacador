# SDD ledger — plan: docs/superpowers/plans/2026-11-21-mundo-aberto.md
Pre-flight:
- T3→T4 Mapa/Lugar/Estrada: consistent.
- T2 Vec3f/Rgb defined in daytime.h; T4,T5,T7 consume them → Ruling: move Vec3f/Rgb into src/core/math.h shared by all — avoids circular includes — cheap if wrong.
- T5 MeshBuilder::calcularAO consumes Terreno (T4): consistent.
- T7 moverAPe consumes Terreno, MundoColisao: consistent; T9 Cavalo reuses Corpo/Entrada: consistent.
- T8 Cenario feeds T10 pontosNPC, T11 quadrosRecompensa, T6 LuzPontual: consistent.
- T12 Nivel used by T6 Iluminacao::carregar(Nivel) before T12 defines it → Ruling: define Nivel/Efeitos in quality.h during Task 6 (stub efeitosDe), tests in Task 12 — ordering only.
Task 1: Ruling: FetchContent via GIT_REPOSITORY instead of tarball URL — codeload tarball returns 403 via proxy, git works — none if wrong.
Task 1: Ruling: screenshot check samples whole image, not the top 50 rows — top rows are sky, false failure — none.
Task 1: Ruling: shaders will be embedded as C++ strings with a per-platform header (#version 330 / 100) instead of files — keeps a single .exe and one source for web/desktop — if wrong, harder shader editing.
Task 1: complete (commits ad17fe0..6bd727d, tests: ./build/testes → [doctest] Status: SUCCESS!)
Task 2: Ruling: Vec3f/Rgb live in src/core/math.h (pre-flight ruling applied) — none.
Task 2: complete (commits 6bd727d..2865feb, tests: ./build/testes → [doctest] Status: SUCCESS!)
Task 3: Ruling: names use _ for spaces in lugares.txt (shown with spaces in game); north = -z — tokenized format can't hold spaces — none.
Task 3: Ruling: Floresta and Ponte places do not flatten terrain (forest = tree density region, bridge sits on river) — would look wrong otherwise — low.
Task 3: complete (commits 2865feb..c0552c6, tests: ./build/testes → [doctest] Status: SUCCESS!)
Task 4: Ruling: added 2 tests (bridge sits on river; real-map places on dry land) — map file and river curve must agree — none.
Task 4: Ruling: road/river masks stored as float grids (27 MB total RAM) not uint8 — simpler; generation ~1 s — if memory matters on web, pack to uint8 later.
Task 4: Ruling: river course is a fixed Catmull-Rom curve in terrain.cpp through Ponte_Velha (60,-120) — plan said fixed curve in code — moving the bridge requires editing code.
Task 4: complete (commits c0552c6..9bef847, tests: ./build/testes → [doctest] Status: SUCCESS!)
Task 5: Ruling: MeshBuilder lives in src/core (pure, tested); paraMesh() is a free function in src/render/meshconv — keeps tests free of raylib — none.
Task 5: Ruling: faces larger than 2 m are subdivided (boxes/roofs) so per-vertex AO has resolution; AO stored in vertex alpha + texcoord.x, floor 0.25; vertices never self-occlude by their own primitive — test needs a vertex near the corner — more triangles on big buildings.
Task 5: Ruling: added test "normais apontam para fora da caixa" to pin winding — backface culling would hide faces otherwise — none.
Task 5: complete (commits 9bef847..4fadf8b, tests: ./build/testes → [doctest] Status: SUCCESS!)
Task 6: Ruling: tone mapping (ACES) + grading done inside lit/sky shaders, no post-process pass on Low — saves a full-screen pass on Intel HD; post pass only when High effects need it (Task 12) — if wrong, slight banding.
Task 6: Ruling: added whole-map 16 m "horizonte" mesh (1 draw call, discarded where detailed terrain draws) so mountains are visible beyond view distance; fog is density-only (no hard cutoff) and palette densities lowered — spec wants beauty + Red Dead horizon; Low view distance 250 m would hide all mountains otherwise — +~17k tris.
Task 6: Ruling: --pos/--yaw/--qualidade flags added in Task 6 (plan put --pos in Task 8) — needed for screenshots now — none.
Task 6: Ruling: efeitosDe() implemented with its tests in Task 6 (pre-flight ruling); Task 12 adds the rest — none.
Task 6: Ruling: game state lives in src/game/jogo.{h,cpp} (class Jogo) not main.cpp — keeps main small — none.
Task 6: Ruling: terrain colour per vertex (smooth road edges), normal per face (faceted look); skirt normals forced up — jagged roads and dark specks otherwise — none.
Task 6: complete (commits 4fadf8b..ebda242, tests: ./build/testes → [doctest] Status: SUCCESS!)
Task 7: Ruling: camera math in src/core/camrig (RigCamera, unit-tested: never under terrain+0.5, pulls in before walls, pitch clamp); src/game/camera not needed — Jogo copies rig to Camera3D — Review Focus #4 now has a test.
Task 7: Ruling: fixed test bug — wall boxes must sit at terrain height (flattened test ground is ~15 m up) — none.
Task 7: Ruling: Esc pauses (SetExitKey disabled), Q quits from pause, click resumes, focus loss pauses — Review Focus #5; no automated test (needs window) — low.
Task 7: Ruling: blob shadow drawn with default shader (no fog) — fine at close range — minor visual if wrong.
Task 7: complete (commits ebda242..ed5b61e, tests: ./build/testes → [doctest] Status: SUCCESS!)
Task 8: Ruling: Cenario is a class (src/render/props) with one merged mesh per 64 m chunk for buildings, plus near (lazy, <120 m) and far tree meshes; exposes luzes, quadros{pos,yaw,lugar}, rotasNPC (one group per place), cavalos — plan's flat Model list would cost too many draw calls — Tasks 9-11 consume these names.
Task 8: Ruling: added walkable floors (MundoColisao::adicionarPiso/alturaPiso, step-up ≤0.7 m) with 2 tests; bridge built as ramps — otherwise the player walks under the bridge in the river — none.
Task 8: Ruling: material types in vertex data (TIPO_CHAMA always lit at night, TIPO_JANELA glows at night, flames discarded by day) with 3 tests — spec: torches light at dusk, windows glow — none.
Task 8: Ruling: rotated footprints collide as grids of small AABBs (rotated AABB would block streets) — more boxes (grid-accelerated) — low.
Task 8: Ruling: loading screen "Carregando o mundo..." (load ≈1.5 s here) — none.
Task 8: Budget measured at city center (Low): ~111k tris, 67 draw calls (limits 300k / 300).
Task 8: complete (commits ed5b61e..7892a22, tests: ./build/testes → [doctest] Status: SUCCESS!)
Task 9: Ruling: horse logic in src/core/horse.{h,cpp} (plan said motion.h) with extra tests (trot w/o Shift, stops without input, riderless horse doesn't move, slower turning at gallop, slope helper inclinacaoPermitida) — keeps motion.h small — none.
Task 9: Ruling: map-crossing test checks the diagonal (1400·√2 m at 12 m/s ≈ 2.75 min, within 2.5–4) instead of a road bot; VEL_GALOPE stays 12 — spec's "~3–4 min" and "~12 m/s" agree on the diagonal — if players feel it's too fast, lower to 10.
Task 9: Ruling: --montado and --girar-camera screenshot flags — visual checks — none.
Task 9: complete (commits 7892a22..080427c, tests: ./build/testes → [doctest] Status: SUCCESS!)
Task 10: Ruling: Populacao takes groups of points (one per place: Cenario::rotasNPC) not a flat list — NPCs stay in their own town — none.
Task 10: Ruling: at night 80% go home: vanish at once if >60 m from player, else walk home first; added tests for that and for walls/ground — Review: no popping in front of player — none.
Task 10: Ruling: 60 NPCs (plan said ~40) — 40 left the city looking empty; far NPCs drawn as one merged mesh (1 call) — +~20 draw calls max.
Task 10: complete (commits 080427c..3c28aae, tests: ./build/testes → [doctest] Status: SUCCESS!)
Task 11: Ruling: on-screen text has no accents (semAcentos) — raylib's built-in font is ASCII only; embedding a TTF adds size — if wrong, load a font later.
Task 11: Ruling: 6 posters per board, generated per board; added tests "mapa sem lugares ainda gera cartazes" and nomeLegivel; wanted men are "last seen" outside the city — none.
Task 11: Ruling: --abrir-quadro and --mapa flags; map errors shown 10 s top-left (Review Focus #1 verified with a missing file) — none.
Task 11: complete (commits 3c28aae..55953a6, tests: ./build/testes → [doctest] Status: SUCCESS!)
Task 12: Ruling: shadows/bloom/god rays disabled in the web test build — WebGL1 depth textures are an extension; web is a quick-test build only — web looks like Low.
Task 12: Ruling: bloom works on the tone-mapped (LDR) image with threshold 0.9; Alto renders to a texture so MSAA is lost there — avoids float render targets on old GPUs — slight aliasing on Alto.
Task 12: Ruling: quality menu lives on the pause screen (keys 1/2/3), switches without restart and saves config.txt in the working folder; first run picks a level from GL_RENDERER — none.
Task 12: Ruling: slope-scaled shadow bias + normal offset (uniform dirSombra) — removed roof acne — none.
Task 12: complete (commits 55953a6..7892ad7, tests: ./build/testes → [doctest] Status: SUCCESS!)
Task 13: Benchmark (48 frames, 12 spots × 4 hours): Low max 121k tris / 71 calls (limits 350k/350); High max 143k / 162. Load ≈1.5 s native, 5 s in software-WebGL.
Task 13: .exe 2.7 MB static + data 8 KB (< 10 MB). Web build 0.9 MB, opens in headless Chromium with 0 console errors.
Task 13: Ruling: fixed a narrowing error only clang (emscripten) reported; game chdirs to the exe folder if data/ is not found — double-click from elsewhere would lose the map — none.
Task 13: Ruling: real 60 FPS on Intel HD can't be measured here (no GPU); budget is the proxy — must be confirmed on the user's PC.
Task 13: complete (commits 7892ad7..c5050c0, tests: ./build/testes → [doctest] Status: SUCCESS!)
Final review: fresh reviewer subagent (opus), verdict "ready to merge with fixes".
Final: fixed camera inside wall on player's right (shoulder offset clamped + recheck after terrain bump) — "câmera não entra na parede colada no lado direito" RED→GREEN, suite 85/85
Final: fixed road A→A / same-spot road NaN UB (map error + terrain guard) — "estrada de um lugar para ele mesmo é erro", "estrada entre lugares no mesmo ponto é erro" RED→GREEN, suite 85/85
Final: fixed weak AMD APUs starting on High — "AMD integrada não começa no Alto" RED→GREEN, suite 85/85
Final: fixed load hang on huge raio (re-graded Minor→Important: game hangs on a typo) — "raio grande demais é erro" RED→GREEN, suite 85/85
Final: Ruling: no-window check (MessageBox "OpenGL 3.3") fixed without an automated test — needs a GL context that fails; verified it compiles for Windows — if wrong, old Intel HD users see nothing.
Final: Ruling: terrain LOD meshes stamped as used when built (re-graded Minor→Important: memory grows on long rides on shared-memory laptops); render-only, no unit test possible without GL — low.
Final: minor (deferred): partial-failure leaks in shadow/post render targets; resize on Alto rebuilds shadow map (1 frame without shadows).
Final: minor (deferred): bounty board open skips pause/focus logic.
Final: minor (deferred): error overlay shows max 6 errors, hides after 10 s even paused; duplicate place names not reported.
Final: minor (deferred): 1280x720 window taller than 1366x768 usable area; no fullscreen toggle.
Final: minor (deferred): NPCs reappear at home in the morning even if the player is near.
