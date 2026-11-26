# Vendored dependencies needing fixes WxLens can't make upstream directly carry them as tracked
# patches under external/patches/, applied at configure time so external/ stays pristine in git.
# Used by maplibre-native-qt.cmake (ADR 0004) and legacy-supercell-wx.cmake (ADR 0002).
find_package(Git REQUIRED)

# Applies an ordered patch series to sourceDir, idempotently. The LAST patch doubles as the
# series' completion marker: later fixes intentionally touch lines introduced by earlier ones, so
# reverse-testing an earlier patch in isolation stops being valid once a later one is present. A
# clean checkout cannot contain the final patch without the whole series having applied first.
function(wxlens_apply_patch_series label sourceDir)
    set(patches ${ARGN})
    list(GET patches -1 finalPatch)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" apply --check --reverse "${finalPatch}"
        WORKING_DIRECTORY "${sourceDir}"
        RESULT_VARIABLE alreadyApplied
        OUTPUT_QUIET ERROR_QUIET)
    if (alreadyApplied EQUAL 0)
        message(STATUS "${label} patch series already applied")
        return()
    endif()
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" apply --check ${patches}
        WORKING_DIRECTORY "${sourceDir}"
        RESULT_VARIABLE applicable
        OUTPUT_QUIET ERROR_QUIET)
    if (NOT applicable EQUAL 0)
        message(FATAL_ERROR "${label} patch series is neither cleanly applied nor applicable to "
                            "${sourceDir} - reconcile it against the new upstream source "
                            "(AGENTS.md, Vendored-dependency patches)")
    endif()
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" apply ${patches}
        WORKING_DIRECTORY "${sourceDir}"
        RESULT_VARIABLE applyResult)
    if (NOT applyResult EQUAL 0)
        message(FATAL_ERROR "Failed to apply ${label} patch series to ${sourceDir}")
    endif()
    # Stated outright so a CI log proves the series went on. The guards above make a silent no-op
    # impossible, but "configure succeeded" is indirect evidence and a build log should not need
    # that inference to establish which vendored fixes are actually in the binary.
    list(LENGTH patches patchCount)
    message(STATUS "${label}: applied ${patchCount} patch(es) to ${sourceDir}")
endfunction()
