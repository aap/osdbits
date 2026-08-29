# permuter findings (2026-08-29)

- flare.c / flare_21A6D8: CLOSED (221/221 + pool byte-exact).  The
  increments go in address order [0],[1],[2].  A first revert based on
  inferring statement order from .lit4 pool order was wrong - pool
  emission tracks scheduled load order, not statement order.

Confirmed plateaus (do not re-grind with statement moves):
- DrawSCEText 109/111: in-block sched tie (8-statement FULL perm floor
  is worse than baseline).
- DrawExtraBuf2 141/160, DrawIllegalText 89/108: register-name
  permutations, no statement/declaration order effect (1601 + 900
  variants).
- InitFog 175/176: sched1 slot, no statement order effect.
