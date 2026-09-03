# "WxLens" name collision screening — 2026-09-03

Practical collision screening for the Phase 1 gate in [ROADMAP.md](ROADMAP.md)
("**WxLens**" name and trademark due diligence). The project owner has stated they are **not**
pursuing trademark registration, so this is a "is anyone already using this name" check, not a
registrability opinion — and it is **not legal advice**. If distribution ever becomes material, or
registration is reconsidered, get counsel; a web/database scan is not a clearance search.

## What was searched, 2026-09-03

| Scope | Query | Result |
|---|---|---|
| General software | `"WxLens" software` | No product of this name. Nearest: WinLens (optical design, Excelitas), wxBasic, WXP (Weather Processor) |
| Weather/radar apps | `"WxLens" OR "Wx Lens" weather radar app` | No product of this name. Nearest in the same niche: **Supercell Wx**, **wX** (joshuatee), WeatherWX, Wx for iPad, WPEC WX |
| Trademarks (via web index) | `"WxLens" trademark` | No `WxLens` mark surfaced. Nearest `WX`-prefixed marks: **WXNOW** (reg. 2020), **WXVANE**, **WXDAT** (Optical Detection Systems Inc — weather data collection/distribution), **WX WILEY X** (eyewear), **WAVELENS** (optical apparatus, filed 2011) |
| Domains / code hosting | `wxlens.com OR wxlens.app OR wxlens github` | Nothing under this name; results were all `wxWidgets`-family projects and the unrelated Kubernetes "Lens"/OpenLens/FreeLens tools |

Databases **not** directly queried (web index only, so treat the trademark row as indicative, not
authoritative): USPTO TESS/TSDR, WIPO Global Brand Database, EUIPO. Jurisdictions considered: US
(primary), none others checked.

## Findings

- **No direct collision found** for `WxLens` as a software product, app, domain, or registered mark.
- The `Wx` prefix is heavily used in this exact niche (weather) and by an unrelated but large
  software family (`wxWidgets`, `wxPython`, `wxPHP`). That is a **discoverability** risk, not a
  legal one: searches for "wx lens" tend to surface wxWidgets material and optical-lens software.
- `WXVANE` / `WXDAT` (Optical Detection Systems Inc) are the closest marks conceptually — same
  `WX`-for-weather construction in a weather-data field. Worth re-checking directly in USPTO if the
  project ever distributes commercially.
- One nearby product is the upstream this project vendors from — **Supercell Wx** (`supercellwx.net`,
  `dpaulat/supercell-wx`), whose `wxdata` library WxLens builds on (ADR 0002). No name overlap, but
  attribution obligations are separate and already tracked in ACKNOWLEDGEMENTS.md.

## Decision

Proceed with the name **WxLens**. Re-run this screening — and query USPTO/WIPO directly — before
registering a domain, publishing to an app store, or any material commercial distribution.

## Sources

- [Supercell Wx](https://supercellwx.net/) · [dpaulat/supercell-wx](https://github.com/dpaulat/supercell-wx)
- [wX (Google Play)](https://play.google.com/store/apps/details?id=joshuatee.wx&hl=en_US)
- [WXNOW trademark detail](https://trademark.justia.com/884/73/wxnow-88473258.html)
- [Optical Detection Systems Inc trademarks](https://trademarks.justia.com/owners/optical-detection-systems-inc-1798318)
- [WAVELENS trademarks](https://trademark.justia.com/owners/wavelens-2975334)
- [USPTO trademarks](https://www.uspto.gov/trademarks) · [WIPO trademarks](https://www.wipo.int/en/web/trademarks)
- [wxWidgets](https://github.com/wxWidgets/wxWidgets) · [WXP (software)](https://en.wikipedia.org/wiki/WXP_(software))
