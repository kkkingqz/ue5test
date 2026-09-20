cmake_minimum_required(VERSION 3.25)
project(GV2Headless LANGUAGES C CXX)

include(CTest)

# CFC-03: the File API query must exist before configure starts; creating it here is too late
# for CMake to emit a reply for this configure pass.
if(BUILD_TESTING AND NOT EXISTS "${CMAKE_BINARY_DIR}/.cmake/api/v1/query/codemodel-v2")
    message(FATAL_ERROR
        "Missing pre-configure CMake File API query. Run: "
        "mkdir -p '${CMAKE_BINARY_DIR}/.cmake/api/v1/query' && "
        "touch '${CMAKE_BINARY_DIR}/.cmake/api/v1/query/codemodel-v2', then configure again."
    )
endif()

# Sanitizer build mode for the portable targets. ASan+UBSan were executed once as
# CFC-13 acceptance evidence, but nothing in the tree could reproduce that run by a
# command, so the result was not repeatable. Configure with
# -DGV2_SANITIZE=address,undefined to get it back.
set(GV2_SANITIZE "" CACHE STRING
    "Comma-separated sanitizer list for portable targets (e.g. address,undefined). Empty disables sanitizers."
)
if(GV2_SANITIZE)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
        message(FATAL_ERROR
            "GV2_SANITIZE requires Clang or GCC, got '${CMAKE_CXX_COMPILER_ID}'."
        )
    endif()
    add_compile_options(-fsanitize=${GV2_SANITIZE} -fno-omit-frame-pointer -g)
    add_link_options(-fsanitize=${GV2_SANITIZE})
    message(STATUS "GV2: sanitizers enabled for portable targets: ${GV2_SANITIZE}")
endif()

include("${CMAKE_CURRENT_SOURCE_DIR}/Tools/Testing/GV2CliContractTests.cmake")

set(GV2_PCC_FIXTURE_ROOT
    "${PROJECT_SOURCE_DIR}/Tests/Fixtures/PortableContentCore"
    CACHE INTERNAL "Canonical shared PortableContentCore fixture root"
)

add_subdirectory(Source)
add_subdirectory(Tools/Content)
add_subdirectory(Headless)

if(BUILD_TESTING)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    add_test(
        NAME ctest_expected_failure_contract
        COMMAND
            "${CMAKE_COMMAND}"
            "-DPROJECT_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
            -P "${PROJECT_SOURCE_DIR}/Tools/Testing/ValidateNoWillFail.cmake"
    )
    add_test(
        NAME ctest_process_contract_self_test
        COMMAND
            "${CMAKE_COMMAND}"
            "-DPROJECT_SOURCE_DIR=${PROJECT_SOURCE_DIR}"
            -P "${PROJECT_SOURCE_DIR}/Tools/Testing/AssertProcessSelfTest.cmake"
    )
    add_test(
        NAME ctest_headless_json_contract_self_test
        COMMAND
            "${CMAKE_COMMAND}"
            "-DGV2_HEADLESS_PROGRAM=$<TARGET_FILE:gv2-headless>"
            "-DGV2_GOLDEN_DIGEST=${PROJECT_SOURCE_DIR}/Tests/Fixtures/GoldenRuns/golden_headless_10_seed_42.digest.json5"
            "-DGV2_FIXTURE_ROOT=${GV2_PCC_FIXTURE_ROOT}"
            "-DGV2_WORKING_DIRECTORY=${PROJECT_SOURCE_DIR}"
            -P "${PROJECT_SOURCE_DIR}/Tools/Testing/AssertHeadlessJsonSelfTest.cmake"
    )
    add_test(
        NAME documentation_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Documentation/validate_docs.py"
    )
    add_test(
        NAME documentation_contract_self_test
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Documentation/validate_docs.py"
            "--self-test"
    )
    add_test(
        NAME pcc_shared_fixture_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Content/validate_pcc_fixtures.py"
            "${GV2_PCC_FIXTURE_ROOT}"
    )
    add_test(
        NAME host_conformance_parity_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Content/validate_host_conformance_parity.py"
    )
    add_test(
        NAME core_decoupling_gate_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Content/validate_core_decoupling.py"
    )
    add_test(
        NAME core_decoupling_gate_negative_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Content/validate_core_decoupling.py"
            "--self-test"
    )
    add_test(
        NAME core_boundary_gate_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Content/validate_core_boundary.py"
    )
    # SAS-06: the package manifest is a generated artifact. Nothing prevents it
    # drifting from the sources by hand, so the committed manifest is verified
    # against what the generator produces — the same shape as the mods.lock check.
    add_test(
        NAME rh_manifest_up_to_date_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Content/generate_manifest.py"
            "${PROJECT_SOURCE_DIR}/GameData/rh"
            "--check"
    )
    add_test(
        NAME core_boundary_gate_negative_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Content/validate_core_boundary.py"
            "--self-test"
    )
    add_test(
        NAME authoring_metadata_gate_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Content/validate_authoring_metadata.py"
    )
    add_test(
        NAME authoring_metadata_gate_negative_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Content/validate_authoring_metadata.py"
            "--self-test"
    )
    # UPP-11: monotonic decrease gate for the Universal UI Property Pipeline migration --
    # schema-specific Prepare/Build functions, FGV2ScreenFieldValue payload members, and
    # schema-specific DTOs in GV2BridgeTypes.h must never grow after M2.
    add_test(
        NAME ui_pipeline_legacy_gate_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Content/validate_ui_pipeline_legacy_gate.py"
    )
    add_test(
        NAME ui_pipeline_legacy_gate_negative_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Content/validate_ui_pipeline_legacy_gate.py"
            "--self-test"
    )
    # GBF-01: derive every fallible Engine AddChild call in the Game Shell attach
    # path from source and require its result to reach the transaction as failure.
    add_test(
        NAME shell_attach_failure_consumption_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Testing/validate_shell_attach_failure_consumption.py"
    )
    add_test(
        NAME shell_attach_failure_consumption_negative_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Testing/validate_shell_attach_failure_consumption.py"
            "--self-test"
    )
    # PSC-14 / PSC-AF-02: keyed GameShell rebuilds create fresh panel slots.
    # Derive every reconcile/restore call from production and require the shared
    # shell-owned layout policy on both the success and rollback paths.
    add_test(
        NAME game_shell_slot_policy_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Testing/validate_game_shell_slot_policy.py"
    )
    add_test(
        NAME game_shell_slot_policy_negative_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Testing/validate_game_shell_slot_policy.py"
            "--self-test"
    )
    # GBF-03: enumerate capability struct members from the source declaration and
    # compare them with the independent classification table; do not use ABI layout
    # as a surrogate for semantic completeness.
    add_test(
        NAME ui_capability_member_inventory_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Testing/validate_ui_capability_member_inventory.py"
    )
    add_test(
        NAME ui_capability_member_inventory_negative_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Testing/validate_ui_capability_member_inventory.py"
            "--self-test"
    )
    # GBF-07: derive cancellable Commit roots from production UI source and require
    # each independent sequence boundary to have an enum-backed recovery decision.
    add_test(
        NAME ui_rollback_boundary_inventory_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Testing/validate_ui_rollback_boundaries.py"
    )
    add_test(
        NAME ui_rollback_boundary_inventory_negative_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Testing/validate_ui_rollback_boundaries.py"
            "--self-test"
    )
    # DCA-03: a class element hardcoded behind LoadClass/FindObject with a fixed /Game/
    # path is a hidden, backwards dependency of code on content layout. Scans Source/
    # itself rather than a fixed file list, so the pattern is caught wherever it recurs.
    add_test(
        NAME no_hardcoded_asset_paths_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Testing/validate_no_hardcoded_asset_paths.py"
    )
    add_test(
        NAME no_hardcoded_asset_paths_negative_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Testing/validate_no_hardcoded_asset_paths.py"
            "--self-test"
    )
    # TSR-02 (ADR-0046): ratchets test suite content coupling boundaries and file size limits.
    # Prevents introducing new content couplings into Source/**/Tests and enforces line ceilings.
    add_test(
        NAME test_content_coupling_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Testing/validate_test_content_coupling.py"
    )
    add_test(
        NAME test_content_coupling_negative_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Testing/validate_test_content_coupling.py"
            "--self-test"
    )
    # PAH-08 (ADR-0042, INV-P5): structural half of the two-part gate. Every
    # presentation-authority access is phase-classified, and no authority is
    # reachable from an application-phase root. The observational half is a
    # runtime counter (GV2.Runtime.UI.PresentationAuthorityPhaseContract).
    add_test(
        NAME presentation_authority_phase_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Testing/validate_presentation_authority_phase.py"
    )
    add_test(
        NAME presentation_authority_phase_negative_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Testing/validate_presentation_authority_phase.py"
            "--self-test"
    )
    # PAH-02 (ADR-0042, preamble): a permitted object (a resolved screen) must not be
    # obtainable except through the check that permits it (UGV2ScreenRegistry::Resolve).
    # Naming the raw FGV2ScreenRegistryEntry row outside the registry would let production
    # code read an unvalidated Layer/WidgetClass and bypass that check.
    add_test(
        NAME screen_registry_entry_encapsulation_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Testing/validate_screen_registry_entry_encapsulation.py"
    )
    add_test(
        NAME screen_registry_entry_encapsulation_negative_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Testing/validate_screen_registry_entry_encapsulation.py"
            "--self-test"
    )
    # CFC-05: Registry lifecycle and sealing ownership (plan SessionLifecycle, M1).
    # Ensures only registry_lifecycle.lua installs registries and owns DESCRIPTOR in contract order;
    # forbids ad hoc game.<slot> assignments and unlisted registry factories in Scripts/.
    add_test(
        NAME registry_lifecycle_ownership_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Testing/validate_registry_lifecycle_ownership.py"
    )
    add_test(
        NAME registry_lifecycle_ownership_negative_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Testing/validate_registry_lifecycle_ownership.py"
            "--self-test"
    )
    # CFC-05A: Canonical state composition ownership (plan PortableCorrectness, M2).
    # Ensures only state_composition.lua owns default state composition and contribution merging;
    # forbids schema-shaped state keys and native merge logic in C++ sources.
    add_test(
        NAME state_composition_ownership_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Testing/validate_state_composition_ownership.py"
    )
    add_test(
        NAME state_composition_ownership_negative_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Testing/validate_state_composition_ownership.py"
            "--self-test"
    )
    # PAH-04A (ADR-0042, INV-P1): after a session reaches Ready, presentation code
    # must not discover content. Every content-discovery call site in production
    # source must be classified pre-Ready or explicitly deferred to a named task.
    add_test(
        NAME pre_ready_content_discovery_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Testing/validate_pre_ready_content_discovery.py"
    )
    add_test(
        NAME pre_ready_content_discovery_negative_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Testing/validate_pre_ready_content_discovery.py"
            "--self-test"
    )
    # SAC-02: package.json5 has one typed read owned by artifact discovery. The
    # companion unittest mutates the source shape with a second and direct read.
    add_test(
        NAME manifest_single_read_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Testing/validate_manifest_single_read.py"
    )
    add_test(
        NAME manifest_single_read_negative_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Testing/test_manifest_single_read_gate.py"
    )
    # PSC-02 (ADR-0043 D1/D5): FResolvedPackageSet may be constructed only by the two
    # blessed host-bootstrap factories -- a second, independently declared factory
    # anywhere in the tree would be a second package-set authority (PAH-R3).
    add_test(
        NAME package_set_factory_inventory_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Testing/validate_package_set_factory_inventory.py"
    )
    add_test(
        NAME package_set_factory_inventory_negative_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Testing/validate_package_set_factory_inventory.py"
            "--self-test"
    )
    # PSC-04 (ADR-0043 D1): every FGV2SessionContentSnapshot member has an explicit role
    # classification, derived from the struct's own declaration -- a new/removed member
    # cannot silently pass by leaving the independent classification table stale.
    add_test(
        NAME session_content_snapshot_field_inventory_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Testing/validate_session_content_snapshot_field_inventory.py"
    )
    add_test(
        NAME session_content_snapshot_field_inventory_negative_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Testing/validate_session_content_snapshot_field_inventory.py"
            "--self-test"
    )
    # PSC-06 (ADR-0043 D1): every declaration receiving a FGV2PresentationPrepareContext
    # parameter/field has an explicit classification, derived from the declarations
    # themselves -- a new/removed site cannot silently pass by leaving the table stale.
    add_test(
        NAME prepare_context_inventory_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Testing/validate_prepare_context_inventory.py"
    )
    add_test(
        NAME prepare_context_inventory_negative_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Testing/validate_prepare_context_inventory.py"
            "--self-test"
    )
    # CFC-04 (ADR-0043 D1, BootstrapAndSessionLifecycle.md): the snapshot is the sole
    # UI schema authority -- cache construction outside FGV2SessionContentCandidate::Build
    # and materializer calls without explicit PrepareContext are prohibited.
    add_test(
        NAME ui_schema_authority_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Testing/validate_ui_schema_authority.py"
    )
    add_test(
        NAME ui_schema_authority_negative_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Testing/validate_ui_schema_authority.py"
            "--self-test"
    )
    # PSC-09A (ADR-0043 D2/D4): GV2PresentationApply's own Build.cs dependency list IS
    # the primary guarantee physical Apply cannot reach a content/authority type -- this
    # gate reads that declaration directly and requires GV2's forward edge onto it exist.
    add_test(
        NAME presentation_apply_module_graph_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Testing/validate_presentation_apply_module_graph.py"
            "--reply-dir"
            "${CMAKE_BINARY_DIR}/.cmake/api/v1/reply"
    )
    add_test(
        NAME presentation_apply_module_graph_negative_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Testing/validate_presentation_apply_module_graph.py"
            "--self-test"
            "--reply-dir"
            "${CMAKE_BINARY_DIR}/.cmake/api/v1/reply"
    )
    # PSC-09B (ADR-0043 D2/D3): every prepared-operation field is a plain, self-contained
    # value -- never a function/callback, a PrepareContext/repository/registry/theme
    # pointer, or a generic untyped service handle. Second, denylist-independent rubric
    # on top of the module-graph gate above.
    add_test(
        NAME presentation_apply_field_inventory_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Testing/validate_presentation_apply_field_inventory.py"
    )
    add_test(
        NAME presentation_apply_field_inventory_negative_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Testing/validate_presentation_apply_field_inventory.py"
            "--self-test"
    )
    # PSC-10B (ADR-0043 D3): derive the style-target set from actual interface
    # implementations, require matching Prepare/Apply branches, and reject all retired
    # pull-style accessors, late hover resolution, and value-sink calls outside the
    # transaction façade (except IsDesignTime-only serialized preview writes).
    # PSC-11 (ADR-0043 D2): what the Apply module exports and what it may do. The exported
    # surface is derived from its own Public/ declarations, and the capability scan walks its
    # whole source tree -- secondary to the module graph, as ADR-0043 D4 states.
    # PSC-11: the include closure PSC-12 will have to carry, classified before the move and
    # ratcheted so it can shrink but not grow.
    add_test(
        NAME apply_move_closure_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Testing/validate_apply_move_closure.py"
    )
    add_test(
        NAME apply_move_closure_negative_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Testing/validate_apply_move_closure.py"
            "--self-test"
    )
    add_test(
        NAME presentation_apply_surface_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Testing/validate_presentation_apply_surface.py"
    )
    add_test(
        NAME presentation_apply_surface_negative_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Testing/validate_presentation_apply_surface.py"
            "--self-test"
    )
    add_test(
        NAME central_style_runtime_boundary_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Testing/validate_central_style_runtime_boundary.py"
    )
    add_test(
        NAME central_style_runtime_boundary_negative_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Testing/validate_central_style_runtime_boundary.py"
            "--self-test"
    )
    # PSC-14: derive the viewport-dependent widget set from canonical production scaling
    # calls and require each class to implement the prepared geometry-refresh role. The
    # matching Unreal automation test exercises the actual process-wide viewport event.
    add_test(
        NAME viewport_refresh_coverage_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Testing/validate_viewport_refresh_coverage.py"
    )
    add_test(
        NAME viewport_refresh_coverage_negative_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Testing/validate_viewport_refresh_coverage.py"
            "--self-test"
    )
    # PSC-09B (ADR-0043 D2/D3): the set of IGV2PropertyConsumer kinds is derived from
    # GV2PropertyConsumers.h's own class declarations, not a task-authored list; every one
    # of them must route its own Commit through the transaction protocol.
    add_test(
        NAME property_consumer_transaction_coverage_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Testing/validate_property_consumer_transaction_coverage.py"
    )
    add_test(
        NAME property_consumer_transaction_coverage_negative_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Testing/validate_property_consumer_transaction_coverage.py"
            "--self-test"
    )
    # CFC-02: fail-closed UE test report validator, transport, and runner contracts
    add_test(
        NAME ue_test_report_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Testing/test_ue_test_report.py"
    )
    add_test(
        NAME mcp_transport_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Testing/test_mcp_transport.py"
    )
    add_test(
        NAME ue_acceptance_runner_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Testing/run_ue_acceptance.py"
            "--self-test"
    )
    # AEP-06: pure comparison logic for the two-producer equivalence proof
    # (the proof itself needs an installed engine and is run manually — see
    # Tools/Testing/prove_acceptance_producer_equivalence.py).
    add_test(
        NAME acceptance_producer_equivalence_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Testing/test_prove_acceptance_producer_equivalence.py"
    )
    # CFC-02A: automation fixture lifetime isolation and test ownership contract
    add_test(
        NAME test_fixture_ownership_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Testing/validate_test_fixture_ownership.py"
    )
    add_test(
        NAME test_fixture_ownership_negative_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Testing/validate_test_fixture_ownership.py"
            "--self-test"
    )
    # CFC-03A: headless run manifest and digest hash-field inventory and validator gate
    add_test(
        NAME headless_hash_field_inventory_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Testing/validate_headless_hash_fields.py"
    )
    add_test(
        NAME headless_hash_field_inventory_negative_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Testing/validate_headless_hash_fields.py"
            "--self-test"
    )
    # CFC-04A: session content snapshot ownership and Screen Registry value-isolation contract
    add_test(
        NAME session_snapshot_ownership_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Testing/validate_session_snapshot_ownership.py"
    )
    add_test(
        NAME session_snapshot_ownership_negative_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Testing/validate_session_snapshot_ownership.py"
            "--self-test"
    )
    # CFC-06: session replacement token, owner routines, and projection publication/teardown contract
    add_test(
        NAME session_replacement_ownership_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Testing/validate_session_replacement_ownership.py"
    )
    add_test(
        NAME session_replacement_ownership_negative_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Testing/validate_session_replacement_ownership.py"
            "--self-test"
    )
    # CFC-07: session transition policy and single Lua VM invariant contract
    add_test(
        NAME session_transition_ownership_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Testing/validate_session_transition_ownership.py"
    )
    add_test(
        NAME session_transition_ownership_negative_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Testing/validate_session_transition_ownership.py"
            "--self-test"
    )
    # SAC-06: session operation profile measurement and retention limit contract
    add_test(
        NAME session_operation_profile_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Testing/measure_session_operation_profile.py"
            "--check"
    )
    add_test(
        NAME session_operation_profile_negative_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Testing/measure_session_operation_profile.py"
            "--self-test"
    )
    # CFC-08: save slot crash matrix and process concurrency contract
    add_test(
        NAME save_slot_crash_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Testing/test_save_slot_crash.py"
            "--helper"
            "$<TARGET_FILE:gv2_save_slot_crash_helper>"
    )
    add_test(
        NAME save_slot_crash_negative_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Testing/test_save_slot_crash.py"
            "--self-test"
    )
    # Baseline gate: the named regression checks the accepted C++ foundation was
    # verified on must stay registered, and CFC-introduced native enums must keep
    # closed production dispatch plus a test inventory.
    add_test(
        NAME cpp_foundation_closure_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Testing/validate_cpp_foundation_closure.py"
    )
    add_test(
        NAME cpp_foundation_closure_negative_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Testing/validate_cpp_foundation_closure.py"
            "--self-test"
    )
    add_test(
        NAME cpp_foundation_closure_python_contract
        COMMAND
            "${Python3_EXECUTABLE}"
            "${PROJECT_SOURCE_DIR}/Tools/Testing/test_cpp_foundation_closure.py"
    )
endif()
