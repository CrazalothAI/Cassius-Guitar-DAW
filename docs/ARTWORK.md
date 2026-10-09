# Cassian artwork

The four cabinet graphics were generated with the built-in imagegen tool on 2026-10-09, then encoded as 1600-pixel-wide WebP assets at quality 92. They are included in source and embedded editor builds; no external image service is needed at runtime. The owner-provided Cassian wolf logo is retained as supplied. Crazaloth provenance markers and third-party notices remain intact. Artwork documentation is not evidence of trademark clearance; the existing release review remains separate.

Final project assets:

- `ui/src/assets/head-clean.webp` — Lumen, ivory / nickel.
- `ui/src/assets/head-crunch.webp` — Rubicon, oxblood / brass.
- `ui/src/assets/head-metal.webp` — Ferrum, graphite / steel.
- `ui/src/assets/head-classical.webp` — Aurelia, walnut / linen.

Names, logo, secondary branding and working faceplate controls are live UI elements layered around the artwork. All decorative images and SVG illustrations are hidden from assistive technology. The layout intentionally crops the photographs to the available head-window height, while preserving the covered grille and materials. Workspace and pedal SVGs are original code graphics; no external icon font or trademark logo set is used.

## Generation prompt set

Shared prompt, after each head-specific description:

> Use case: product-mockup. Asset type: production amplifier cabinet upper-section artwork for an interactive guitar app. Photograph straight-on, perfectly front-facing, no perspective distortion, wide panoramic 3:1 framing, entire head upper cabinet and handle visible with a small safe margin. The cabinet is wide and low, across almost the full width. Bottom edge should be a simple straight horizontal metal chassis lip where a separate interactive control faceplate will attach in the application. Do not render knobs, control panel, jacks, text, lettering, logo, badge or trademarks. Keep the center grille unobstructed. A complete protective cover/cabinet and real grille are essential, do not show exposed free-standing circuitry. Premium realistic studio product photography, precise materials, restrained light from upper left, black seamless background with no scenery, no dramatic neon, no cartoon, no toy, no huge bloomy lights. Four heads in the product family share the same perfectly frontal rectangular composition.

Head-specific descriptions:

- **Metal:** A graphite-black high-gain amplifier head. Black pebbled leather casing with substantial black corner protectors and realistic top carry handle. Recessed black perforated steel grille with angular restrained reinforcement rails; four identical glowing glass power tubes and real electronics subtly visible behind it. Powerful contemporary industrial design.
- **Clean:** An ivory cream boutique clean amplifier head. Cream textured leather casing with polished nickel corner hardware, elegant carry handle, inset tightly woven silver-gray grille cloth, subtle brushed aluminum trim. Quietly luxurious recording-studio instrument, refined precise craftsmanship.
- **Crunch:** An oxblood burgundy boutique rock amplifier head. Rich dark burgundy textured leather casing with black corner guards and top carry handle, recessed dark bronze woven grille cloth, thin warm brushed brass trim. Restrained vintage British-inspired design, own original styling.
- **Classical:** A walnut and espresso boutique acoustic/classical amplifier head. Beautiful satin dark walnut outer frame with crisp joinery, espresso leather side details and top handle, recessed finely woven warm graphite linen grille cloth, subtle brushed nickel trim. Organic refined chamber-music studio design, luxurious and believable.

An earlier exposed-interior study was generated during design but is not used or required by the app. Cabinet assets above are the selected production graphics. The original full-resolution generation outputs remain in the local imagegen output directory; only the optimized selected files are shipped.
