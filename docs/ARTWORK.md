# Cassian artwork

The four cabinet graphics were generated with the built-in imagegen tool on 2026-10-09, then encoded as 1600-pixel-wide WebP assets at quality 92. They are included in source and embedded editor builds; no external image service is needed at runtime. The owner-provided Cassian wolf logo is retained as supplied. Crazaloth provenance markers and third-party notices remain intact. Artwork documentation is not evidence of trademark clearance; the existing release review remains separate.

Final project assets:

- `ui/src/assets/head-clean.webp` — Lumen, ivory / nickel.
- `ui/src/assets/head-crunch.webp` — Rubicon, oxblood / brass.
- `ui/src/assets/head-metal.webp` — Ferrum, graphite / steel.
- `ui/src/assets/head-classical.webp` — Aurelia, walnut / linen.

Names, logo, secondary branding and working faceplate controls are live UI elements layered on the artwork. All decorative images and SVG illustrations are hidden from assistive technology. The complete photographs scale proportionally without cropping. Workspace and pedal SVGs are original code graphics; no external icon font or trademark logo set is used.

In 1.8.1, the four graphics were edited into complete heads with blank metal control panels inside their continuous enclosures. Working knobs, channel switch and branding are HTML overlays aligned to those panels. Head-selection cards are removed. The optimized assets total approximately 836 KB.

## Complete-head edit prompt (1.8.1)

Each original cabinet image was used as its own edit reference, with this shared prompt:

> Edit the referenced Cassian amplifier cabinet photograph into a complete premium amplifier head with a real blank metal control panel built into the SAME cabinet. Keep the exact straight-on front view, 3:1 panoramic aspect ratio, full carry handle and entire head visible, black seamless background, restrained realistic materials, side corner protectors, small feet. The physical front panel MUST be enclosed by the same continuous cabinet side rails and bottom edge as the grille. Expand the currently narrow metal strip below the grille into a wide blank front control panel occupying y=65% through y=92% of the entire image, spanning x=6% to x=94%. Grille occupies about y=20% to y=63%; keep its original style but make room for the panel. No separate floating slab, no detached panel, no black gap between grille and control panel. No knobs, buttons, jacks, switches, lettering, brand names, logos or meters anywhere: a live HTML interface will add six working knobs and logo on this empty panel. Flat frontal panel, softly brushed metal with realistic machining, screws only at the edges. Ensure the blank panel is light enough for black knob controls for clean/classical, darker steel for metal, warm brass for crunch. Keep precisely rectangular mounting surfaces and coherent lighting; photorealistic expensive audio hardware, no toy, no neon.

Material instructions retained graphite leather/perforated grille/four identical tubes and electronics with dark steel for Metal; ivory/silver cloth/satin nickel for Clean; oxblood/bronze cloth/brass for Crunch; walnut/espresso/linen/champagne nickel for Classical. Only resized WebP encoding was applied after generation.

## Original cabinet reference prompt set (1.7.0)

Shared prompt, after each head-specific description:

> Use case: product-mockup. Asset type: production amplifier cabinet upper-section artwork for an interactive guitar app. Photograph straight-on, perfectly front-facing, no perspective distortion, wide panoramic 3:1 framing, entire head upper cabinet and handle visible with a small safe margin. The cabinet is wide and low, across almost the full width. Bottom edge should be a simple straight horizontal metal chassis lip where a separate interactive control faceplate will attach in the application. Do not render knobs, control panel, jacks, text, lettering, logo, badge or trademarks. Keep the center grille unobstructed. A complete protective cover/cabinet and real grille are essential, do not show exposed free-standing circuitry. Premium realistic studio product photography, precise materials, restrained light from upper left, black seamless background with no scenery, no dramatic neon, no cartoon, no toy, no huge bloomy lights. Four heads in the product family share the same perfectly frontal rectangular composition.

Head-specific descriptions:

- **Metal:** A graphite-black high-gain amplifier head. Black pebbled leather casing with substantial black corner protectors and realistic top carry handle. Recessed black perforated steel grille with angular restrained reinforcement rails; four identical glowing glass power tubes and real electronics subtly visible behind it. Powerful contemporary industrial design.
- **Clean:** An ivory cream boutique clean amplifier head. Cream textured leather casing with polished nickel corner hardware, elegant carry handle, inset tightly woven silver-gray grille cloth, subtle brushed aluminum trim. Quietly luxurious recording-studio instrument, refined precise craftsmanship.
- **Crunch:** An oxblood burgundy boutique rock amplifier head. Rich dark burgundy textured leather casing with black corner guards and top carry handle, recessed dark bronze woven grille cloth, thin warm brushed brass trim. Restrained vintage British-inspired design, own original styling.
- **Classical:** A walnut and espresso boutique acoustic/classical amplifier head. Beautiful satin dark walnut outer frame with crisp joinery, espresso leather side details and top handle, recessed finely woven warm graphite linen grille cloth, subtle brushed nickel trim. Organic refined chamber-music studio design, luxurious and believable.

An earlier exposed-interior study was generated during design but is not used or required by the app. Cabinet assets above are the selected production graphics. The original full-resolution generation outputs remain in the local imagegen output directory; only the optimized selected files are shipped.
