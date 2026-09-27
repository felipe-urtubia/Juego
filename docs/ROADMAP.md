# Roadmap

## Current Refactor Goals

1. Complete the repository migration to `src/` and `include/` only.
2. Remove remaining business logic output from simulation and career orchestration.
3. Split large modules into focused systems:
   - match engine
   - tactical engine
   - transfer market
   - finance system
   - competition scheduler
   - save manager
4. Expand CMake support and keep Windows batch tooling as a convenience wrapper.

## Gameplay Growth Areas

- More explicit match phases and minute-level event generation.
- Stronger tactical cause/effect in chance creation, transitions and fatigue.
- Better rival AI rotation, tactical adaptation and squad planning.
- Richer transfer negotiation flow with clearer multi-step offers and promises.
- Deeper dressing room dynamics and long-term player development systems.
- Future option: interactive penalty shootout mini-game integrated into the Match Center; not planned for the current implementation cycle.

## Open Source Priorities

- Improve automated validation coverage.
- Add focused tests for transfer logic, competition rules and match engine math.
- Keep documentation aligned with the real implementation status.
- Continue reducing root-level clutter and generated artifacts from the repository surface.
