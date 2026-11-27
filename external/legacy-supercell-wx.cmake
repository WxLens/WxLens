# wxdata (external/legacy-supercell-wx/wxdata) is linked from the top-level CMakeLists.txt, but
# its tracked patches go on here, before that add_subdirectory - see ADR 0002's amendment for when
# a wxdata patch is acceptable instead of waiting on an upstream fix.
#
# Patch paths are relative to the legacy repo's root (wxdata/...), so they apply from there.
set(LEGACY_SCWX_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/legacy-supercell-wx")

# Ar2vFile decompressed a Level 2 volume's ~50-100 independent bzip2 LDM records one at a time,
# which was most of the cost of loading a volume. Decompresses them in parallel and parses them in
# file order as before, so the decoded volume is byte-identical. Measured 1.6-2.9x faster
# LoadData on real 2026-11-26 volumes (docs/performance-baseline.md, 2026-11-26).
wxlens_apply_patch_series("wxdata" "${LEGACY_SCWX_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/patches/0016-wxdata-parallel-ldm-decompress.patch")
