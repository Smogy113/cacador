# SDD ledger — plan: docs/superpowers/plans/2026-11-21-personagens-e-luz.md
Pre-flight: T1 Mat4/Esqueleto/Pose → T2,T3,T4,T5 consistent; T2 VertPele.osso/peso → T3 texcoords2 consistent.
Task 1: complete (commits a634133..0e2b2a7, tests: ./build/testes → [doctest] Status: SUCCESS!)
Task 2: Ruling: normal-direction test compares each vertex with its own bone (nearest-bone heuristic wrongly picked arm bones for torso sides) and skips unused vertices (coat halves drop triangles) — none.
Task 2: complete (commits 4bc7ae3..35cb1e4, tests: ./build/testes → [doctest] Status: SUCCESS!)
Task 3: Ruling: skin shader shares the lit fragment shader; every lit uniform is now set on both programs (lit_, pele_) — one FS keeps light identical on world and characters — small CPU cost.
Task 3: Ruling: bone pair + weight packed in texcoords2 (x = bone A, y = bone B + 0.99·weight) — raylib has no free attribute for weights without its own skinning path — none.
Task 3: Ruling: open-coat shirt strip removed (smooth vertex colours blur it over the chest); 12 NPC body/outfit variants share meshes; --dist-camera screenshot flag — none.
Task 3: Web build compiles the GLSL 100 skin shader (build-web OK).
Task 3: complete (commits 35cb1e4..4effd12, tests: ./build/testes → [doctest] Status: SUCCESS!)
Task 4: Ruling: animation functions take the Esqueleto (plan signatures lacked it) so they can put the lowest foot exactly on the ground — feet never sink/float for any body size — none.
Task 4: Ruling: riding thighs at ~64° forward and 35° outward (plan test demanded >60°) — looks astride rather than chair-like — none.
Task 4: complete (commits 4effd12..9ba04f8, tests: ./build/testes → [doctest] Status: SUCCESS!)
Task 5: Ruling: horse uses 16 bones (body, neck, head, tail, 3 per leg); rider's seat follows the horse body bone (bounces with the gait); horse stride rate per gait (1.6/2.5/3.5 m per cycle) moved into horse.cpp — none.
Task 5: Ruling: one horse in three gets white socks (VisualCavalo::meias) — variety — none.
Task 5: complete (commits 9ba04f8..ff9e107, tests: ./build/testes → [doctest] Status: SUCCESS!)
Task 6: Ruling: added a sky "rim" term (fresnel-like edge light) besides bounce light — strongest cheap cue of indirect light; all levels — slight brightening of silhouettes.
Task 6: Ruling: shadow depth bias lowered (0.6 m → 0.12 m base) now that PCSS + normal offset hide acne; penumbra = 4.5 cm per metre blocker distance, 1–7 texels — none.
Task 6: complete (commits ff9e107..c0a5a7d, tests: ./build/testes → [doctest] Status: SUCCESS!)
Task 7: Ruling: SSAO is normal-aware (Alchemy-style, normal from 3-texel depth neighbours) — plain depth-difference AO greyed every flat surface; 1-texel normals made a grey band on grazing ground — none.
Task 7: Ruling: raylib near plane raised 0.01 → 0.1 m (compile definition on the raylib target) — 10× depth precision for shadows/SSAO; camera never gets closer than 0.3 m — if an object comes within 10 cm of the camera it clips.
Task 7: Ruling: no separate screen-space contact-shadow pass — SSAO + PCSS (sharp near the contact) already give the contact darkening; water already reflects sky and sun on all levels — saves a pass on High.
Task 7: Ruling: level-switch leak check done by code review (every RT created in carregar is freed in liberarEfeitos, including depth-texture scene FB), not by an automated count — needs a GL context — low.
Task 7: complete (commits c0a5a7d..3ee97bf, tests: ./build/testes → [doctest] Status: SUCCESS!)
Task 8: complete (commits 3ee97bf..f4c076a, tests: ./build/testes → [doctest] Status: SUCCESS!)
Task 8: Benchmark Low max 171k tris / 114 calls; High max 196k / 203 (characters now counted). exe + web build OK.
HD (user request "ainda tá muito low poly"): Ruling: added corpoHumanoHD (~9k tris: smoothed rings, 36-sided torso/head, sculpted face reliefs, ears, fingers, collar, lapels, belt buckle, boot soles, dented hat, optional gear) as a near-camera LOD; low model kept for distance and shadow pass; NPCs use HD within 12 m — keeps cost local to what the camera sees.
HD: Ruling: loft builder extracted to src/core/loft.{h,cpp} (shared by body.cpp and body_hd.cpp) — none.
HD: Ruling: rim light reduced (0.6 → 0.25) and limited to vertical surfaces — made faces pale and flattened distant terrain (also reviewer minor #5) — none.
HD: Ruling: --retrato screenshot flag (camera facing the player's face) — visual checks — none.
Final review: fresh reviewer subagent (opus), verdict "ready to merge with fixes".
Final: fixed web skinning (bones transposed on WebGL) — verified in headless Chromium: character renders correctly, 0 console errors (render-only; no unit test possible) — Critical.
Final: fixed elbow/knee pinching (transition rings 0.75/0.25 + thicker joint ring, thigh blended with hip) — "joelho e cotovelo não achatam correndo (modelo leve e HD)" RED→GREEN, suite 109/109.
Final: fixed off-screen characters costing poses/draws (behind-camera cull in main pass), cached 'ossos' uniform location, reused matrix buffers — render-only; benchmark Low max 186k tris / 113 calls.
Final: fixed SSAO not disabled on web (re-graded Minor→Important: Alto on web would break) — one-line, verified build.
Final: fixed rim light on all surfaces (done as part of the HD face work).
Final: minor (deferred): bounce colour is a constant green (not per terrain type).
Final: minor (deferred): uint16 index overflow has no assert in loft::tubo (HD is ~9k tris, ~6k verts: safe).
Final: minor (deferred): per-frame GetShaderLocation calls and double light sort in comecarCena.
Final: minor (deferred): minimised window (0×0) recreates post targets at 0×0 until restored.
Final: minor (deferred): unused 'mat' parameter / amp leftovers in desenharAtores.
