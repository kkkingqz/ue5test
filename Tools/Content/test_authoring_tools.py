#!/usr/bin/env python3
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile

def run_cmd(args, check=True):
    res = subprocess.run(args, capture_output=True, text=True)
    if check and res.returncode != 0:
        raise RuntimeError(f"Command failed (exit {res.returncode}): {' '.join(args)}\nstdout: {res.stdout}\nstderr: {res.stderr}")
    return res

def main():
    if len(sys.argv) < 3:
        print(f"usage: {sys.argv[0]} <gv2-content-bin> <core-package-dir>")
        sys.exit(2)

    gv2_content = os.path.abspath(sys.argv[1])
    core_pkg = os.path.abspath(sys.argv[2])

    if not os.path.isfile(gv2_content):
        raise FileNotFoundError(f"gv2-content not found at: {gv2_content}")
    if not os.path.isdir(core_pkg):
        raise FileNotFoundError(f"core package not found at: {core_pkg}")

    print("[*] Testing 'describe' on GameData/core...")
    # 1. Test describe text
    res = run_cmd([gv2_content, "describe", core_pkg, "screen"])
    assert "definition_type: screen" in res.stdout
    assert "core:schema.definition.screen.v1" in res.stdout
    assert "title_text_id: text_id (required)" in res.stdout

    # 2. Test describe json
    res = run_cmd([gv2_content, "describe", core_pkg, "screen", "--format=json"])
    doc = json.loads(res.stdout)
    assert doc["status"] == "ok"
    assert doc["definition_type"] == "screen"
    assert doc["schema_id"] == "core:schema.definition.screen.v1"
    assert "title_text_id" in doc["fields"]
    assert doc["fields"]["title_text_id"]["required"] is True
    assert doc["fields"]["title_text_id"]["storage"] == "definition"
    assert doc["fields"]["title_text_id"]["write_policy"] == "read_only"

    # 3. Test describe unknown type
    res = run_cmd([gv2_content, "describe", core_pkg, "unknown_type"], check=False)
    assert res.returncode == 2
    assert "unknown definition type 'unknown_type'" in res.stderr

    res = run_cmd([gv2_content, "describe", core_pkg, "unknown_type", "--format=json"], check=False)
    assert res.returncode == 2
    err_doc = json.loads(res.stdout)
    assert err_doc["status"] == "error"
    assert err_doc["code"] == "unknown_definition_type"

    print("[*] Testing 'new' in temporary package workspace...")
    with tempfile.TemporaryDirectory() as tmpdir:
        test_pkg = os.path.join(tmpdir, "core")
        shutil.copytree(core_pkg, test_pkg)

        # 4. Test new actor (has no dangling refs, immediately passes validate)
        res = run_cmd([gv2_content, "new", test_pkg, "actor", "core:actor.npc.guard"])
        assert "created definition core:actor.npc.guard in definitions/actors.json5" in res.stdout

        res = run_cmd([gv2_content, "inspect", test_pkg, "core:actor.npc.guard", "--format=json"])
        actor_doc = json.loads(res.stdout)
        assert actor_doc["status"] == "ok"
        assert actor_doc["definition"]["id"] == "core:actor.npc.guard"
        assert actor_doc["definition"]["data"]["discriminator"] == "player"

        # Validate whole package immediately passes
        res = run_cmd([gv2_content, "validate", test_pkg])
        assert "ok content_hash=" in res.stdout

        # 5. Test duplicate ID rejection
        res = run_cmd([gv2_content, "new", test_pkg, "actor", "core:actor.npc.guard"], check=False)
        assert res.returncode == 2
        assert "already exists in package" in res.stderr

        res = run_cmd([gv2_content, "new", test_pkg, "actor", "core:actor.npc.guard", "--format=json"], check=False)
        assert res.returncode == 2
        err = json.loads(res.stdout)
        assert err["status"] == "error"
        assert err["code"] == "duplicate_definition_id"

        # 6. Test invalid ID grammar rejection
        res = run_cmd([gv2_content, "new", test_pkg, "item", "not-a-valid-id"], check=False)
        assert res.returncode == 2
        assert "not a valid definition id" in res.stderr

        res = run_cmd([gv2_content, "new", test_pkg, "item", "not-a-valid-id", "--format=json"], check=False)
        assert res.returncode == 2
        err = json.loads(res.stdout)
        assert err["status"] == "error"
        assert err["code"] == "invalid_definition_id"

        # 7. Test kind mismatch rejection
        res = run_cmd([gv2_content, "new", test_pkg, "item", "core:actor.hero"], check=False)
        assert res.returncode == 2
        assert "does not match definition type 'item'" in res.stderr

        res = run_cmd([gv2_content, "new", test_pkg, "item", "core:actor.hero", "--format=json"], check=False)
        assert res.returncode == 2
        err = json.loads(res.stdout)
        assert err["status"] == "error"
        assert err["code"] == "id_kind_mismatch"

        # 8. Test new definition when file doesn't exist yet
        with tempfile.TemporaryDirectory() as tmpdir2:
            test_pkg2 = os.path.join(tmpdir2, "core")
            shutil.copytree(core_pkg, test_pkg2)
            screens_file = os.path.join(test_pkg2, "definitions", "screens.json5")
            if os.path.exists(screens_file):
                os.remove(screens_file)

            res = run_cmd([gv2_content, "new", test_pkg2, "screen", "core:screen.dialog.main", "--format=json"])
            new_res = json.loads(res.stdout)
            assert new_res["status"] == "ok"
            assert new_res["definition_id"] == "core:screen.dialog.main"
            assert os.path.exists(screens_file)

            # verify the file content is valid JSON5
            with open(screens_file, "r") as f:
                content = f.read()
            assert "core:screen.dialog.main" in content

        # 9. Test creating screen and its referenced text and resource
        res = run_cmd([gv2_content, "new", test_pkg, "text", "core:text.screen.battle.title"])
        assert "created definition core:text.screen.battle.title" in res.stdout

        res = run_cmd([gv2_content, "new", test_pkg, "resource", "core:resource.screen.battle.background"])
        assert "created definition core:resource.screen.battle.background" in res.stdout

        res = run_cmd([gv2_content, "new", test_pkg, "screen", "core:screen.battle"])
        assert "created definition core:screen.battle in definitions/screens.json5" in res.stdout

        # Verify inspect finds it
        res = run_cmd([gv2_content, "inspect", test_pkg, "core:screen.battle", "--format=json"])
        screen_doc = json.loads(res.stdout)
        assert screen_doc["status"] == "ok"
        assert screen_doc["definition"]["id"] == "core:screen.battle"

        # Validate whole package succeeds
        res = run_cmd([gv2_content, "validate", test_pkg])
        assert "ok content_hash=" in res.stdout

    print("[*] Testing 'refs' on GameData/core...")
    # 10. Test refs text format on referenced text
    res = run_cmd([gv2_content, "refs", core_pkg, "core:text.screen.main.title"])
    assert "definition_id: core:text.screen.main.title" in res.stdout
    assert "references: 1" in res.stdout
    assert "definitions/screens.json5:8:24" in res.stdout
    assert "definition: core:screen.main" in res.stdout

    # 11. Test refs json format
    res = run_cmd([gv2_content, "refs", core_pkg, "core:text.screen.main.title", "--format=json"])
    refs_doc = json.loads(res.stdout)
    assert refs_doc["status"] == "ok"
    assert refs_doc["definition_id"] == "core:text.screen.main.title"
    assert refs_doc["references_count"] == 1
    assert refs_doc["references"][0]["source_file"] == "definitions/screens.json5"
    assert refs_doc["references"][0]["source_definition_id"] == "core:screen.main"
    assert refs_doc["references"][0]["line"] == 8
    assert refs_doc["references"][0]["column"] == 24
    assert refs_doc["references"][0]["json_pointer"] == "/definitions/0/data/title_text_id"

    # 12. Test refs on unreferenced ID (exit code 0, 0 references)
    res = run_cmd([gv2_content, "refs", core_pkg, "core:text.unreferenced.sample"])
    assert "definition_id: core:text.unreferenced.sample" in res.stdout
    assert "references: 0" in res.stdout

    res = run_cmd([gv2_content, "refs", core_pkg, "core:text.unreferenced.sample", "--format=json"])
    unref_doc = json.loads(res.stdout)
    assert unref_doc["status"] == "ok"
    assert unref_doc["references_count"] == 0
    assert unref_doc["references"] == []

    # 13. Test refs on invalid ID (exit code 2)
    res = run_cmd([gv2_content, "refs", core_pkg, "not-valid-id"], check=False)
    assert res.returncode == 2
    assert "not a valid definition id" in res.stderr

    res = run_cmd([gv2_content, "refs", core_pkg, "not-valid-id", "--format=json"], check=False)
    assert res.returncode == 2
    err = json.loads(res.stdout)
    assert err["status"] == "error"
    assert err["code"] == "invalid_definition_id"

    # 14. Test refs with missing arguments
    res = run_cmd([gv2_content, "refs", core_pkg], check=False)
    assert res.returncode == 2

    print("[*] Testing 'rename' in temporary package workspace...")
    with tempfile.TemporaryDirectory() as tmpdir:
        test_pkg = os.path.join(tmpdir, "core")
        shutil.copytree(core_pkg, test_pkg)

        # 15. Add comments to texts.json5 and screens.json5 to verify preservation
        texts_path = os.path.join(test_pkg, "definitions", "texts.json5")
        with open(texts_path, "r") as f:
            texts_content = f.read()
        texts_content = texts_content.replace('id: "core:text.screen.main.title",', '// Primary main screen title\n      id: "core:text.screen.main.title",')
        with open(texts_path, "w") as f:
            f.write(texts_content)

        # 16. Test rename text definition and referenced screen (text format)
        res = run_cmd([gv2_content, "rename", test_pkg, "core:text.screen.main.title", "core:text.screen.main.header"])
        assert "renamed core:text.screen.main.title -> core:text.screen.main.header" in res.stdout
        assert "files modified: 2" in res.stdout

        # Verify comment is preserved
        with open(texts_path, "r") as f:
            new_texts_content = f.read()
        assert "// Primary main screen title" in new_texts_content
        assert "core:text.screen.main.header" in new_texts_content
        assert "core:text.screen.main.title" not in new_texts_content

        # Verify screen definition was updated
        screens_path = os.path.join(test_pkg, "definitions", "screens.json5")
        with open(screens_path, "r") as f:
            screens_content = f.read()
        assert "core:text.screen.main.header" in screens_content
        assert "core:text.screen.main.title" not in screens_content

        # Validate whole package succeeds after rename
        res = run_cmd([gv2_content, "validate", test_pkg])
        assert "ok content_hash=" in res.stdout

        # 17. Test rename with JSON output
        res = run_cmd([gv2_content, "rename", test_pkg, "core:screen.inventory", "core:screen.backpack", "--format=json"])
        rename_doc = json.loads(res.stdout)
        assert rename_doc["status"] == "ok"
        assert rename_doc["old_id"] == "core:screen.inventory"
        assert rename_doc["new_id"] == "core:screen.backpack"
        assert rename_doc["files_modified_count"] >= 1

        # Validate whole package succeeds after second rename
        res = run_cmd([gv2_content, "validate", test_pkg])
        assert "ok content_hash=" in res.stdout

        # 18. Test rename nonexistent old ID (exit code 2)
        res = run_cmd([gv2_content, "rename", test_pkg, "core:text.nonexistent.id", "core:text.new.id"], check=False)
        assert res.returncode == 2
        assert "not found in package" in res.stderr

        res = run_cmd([gv2_content, "rename", test_pkg, "core:text.nonexistent.id", "core:text.new.id", "--format=json"], check=False)
        assert res.returncode == 2
        err = json.loads(res.stdout)
        assert err["status"] == "error"
        assert err["code"] == "source_definition_not_found"

        # 19. Test rename to duplicate existing new ID (exit code 2)
        res = run_cmd([gv2_content, "rename", test_pkg, "core:screen.backpack", "core:screen.main"], check=False)
        assert res.returncode == 2
        assert "already exists in package" in res.stderr

        res = run_cmd([gv2_content, "rename", test_pkg, "core:screen.backpack", "core:screen.main", "--format=json"], check=False)
        assert res.returncode == 2
        err = json.loads(res.stdout)
        assert err["status"] == "error"
        assert err["code"] == "duplicate_definition_id"

        # 20. Test rename with invalid ID grammar (exit code 2)
        res = run_cmd([gv2_content, "rename", test_pkg, "core:screen.backpack", "not-a-valid-id"], check=False)
        assert res.returncode == 2
        assert "not a valid new definition id" in res.stderr

        res = run_cmd([gv2_content, "rename", test_pkg, "not-a-valid-id", "core:screen.backpack2", "--format=json"], check=False)
        assert res.returncode == 2
        err = json.loads(res.stdout)
        assert err["status"] == "error"
        assert err["code"] == "invalid_definition_id"

        # 21. Test rename with kind mismatch (exit code 2)
        res = run_cmd([gv2_content, "rename", test_pkg, "core:screen.backpack", "core:item.backpack"], check=False)
        assert res.returncode == 2
        assert "does not match" in res.stderr

        res = run_cmd([gv2_content, "rename", test_pkg, "core:screen.backpack", "core:item.backpack", "--format=json"], check=False)
        assert res.returncode == 2
        err = json.loads(res.stdout)
        assert err["status"] == "error"
        assert err["code"] == "id_kind_mismatch"

        # 22. Test rename identical ID (no-op success)
        res = run_cmd([gv2_content, "rename", test_pkg, "core:screen.backpack", "core:screen.backpack", "--format=json"])
        noop_doc = json.loads(res.stdout)
        assert noop_doc["status"] == "ok"
        assert noop_doc["files_modified_count"] == 0

        # 23. Test rename in frozen package (exit code 2)
        # PKG-01: package_id/namespace/version are now mandatory manifest
        # fields (plan PackageSupport) — discovery must still succeed for
        # this test to reach the frozen check below.
        pkg_desc_path = os.path.join(test_pkg, "package.json5")
        with open(pkg_desc_path, "w") as f:
            f.write('{\n  package_id: "core",\n  namespace: "core",\n  version: "1.0.0",\n  frozen: true,\n}\n')

        res = run_cmd([gv2_content, "rename", test_pkg, "core:screen.backpack", "core:screen.inventory"], check=False)
        assert res.returncode == 2
        assert "package is published/frozen" in res.stderr

        res = run_cmd([gv2_content, "rename", test_pkg, "core:screen.backpack", "core:screen.inventory", "--format=json"], check=False)
        assert res.returncode == 2
        err = json.loads(res.stdout)
        assert err["status"] == "error"
        assert err["code"] == "package_frozen"

    print("[*] Testing 'validate --watch'...")
    # 24. Test single pass watch with --max-iterations=1
    res = run_cmd([gv2_content, "validate", core_pkg, "--watch", "--max-iterations=1"])
    assert "ok content_hash=" in res.stdout

    res = run_cmd([gv2_content, "validate", core_pkg, "--watch", "--max-iterations=1", "--format=json"])
    doc = json.loads(res.stdout)
    assert doc["status"] == "ok"
    assert doc["iteration"] == 1

    # 25. Test multi-iteration live watch with background file edit
    with tempfile.TemporaryDirectory() as tmpdir:
        test_pkg = os.path.join(tmpdir, "core")
        shutil.copytree(core_pkg, test_pkg)

        import subprocess
        import time

        # Start watch process with poll interval 50ms, max 3 iterations
        proc = subprocess.Popen(
            [gv2_content, "validate", test_pkg, "--watch", "--poll-interval=50", "--max-iterations=3", "--format=json"],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True
        )

        time.sleep(0.15)
        # Iteration 2: introduce a syntax error
        screens_path = os.path.join(test_pkg, "definitions", "screens.json5")
        with open(screens_path, "a") as f:
            f.write("\nthis is a syntax error\n")

        time.sleep(0.15)
        # Iteration 3: fix the error
        with open(screens_path, "r") as f:
            content = f.read()
        content = content.replace("\nthis is a syntax error\n", "")
        with open(screens_path, "w") as f:
            f.write(content)

        stdout, stderr = proc.communicate(timeout=5)
        lines = [l.strip() for l in stdout.strip().splitlines() if l.strip()]
        assert len(lines) == 3
        doc1 = json.loads(lines[0])
        assert doc1["status"] == "ok"
        assert doc1["iteration"] == 1

        doc2 = json.loads(lines[1])
        assert doc2["status"] == "invalid"
        assert doc2["iteration"] == 2

        doc3 = json.loads(lines[2])
        assert doc3["status"] == "ok"
        assert doc3["iteration"] == 3

    # 26. Test --watch rejection on other commands
    res = run_cmd([gv2_content, "inspect", core_pkg, "core:screen.main", "--watch"], check=False)
    assert res.returncode == 2
    assert "only supported for 'validate'" in res.stderr

    print("[*] Testing schema resolution from a game package's core dependency...")
    # The game package (rh) carries entities but no schemas: those belong to its core
    # dependency. 'describe' and 'new' must resolve across the package set, the way
    # 'validate' already does, or the authoring tools are unusable in the package the
    # designer actually works in.
    with tempfile.TemporaryDirectory() as tmpdir:
        shutil.copytree(core_pkg, os.path.join(tmpdir, "core"))
        game_pkg = os.path.join(tmpdir, "game")
        os.makedirs(os.path.join(game_pkg, "definitions"))
        with open(os.path.join(game_pkg, "package.json5"), "w", encoding="utf-8") as f:
            f.write(
                "{\n"
                '  package_id: "game",\n'
                '  namespace: "game",\n'
                '  version: "0.1.0",\n'
                "  dependencies: [\n"
                '    { package_id: "core", load_after: true },\n'
                "  ],\n"
                "}\n"
            )

        # describe reports the schema and names the package that actually owns it.
        res = run_cmd([gv2_content, "describe", game_pkg, "screen"])
        assert "definition_type: screen" in res.stdout
        assert "core:schema.definition.screen.v1" in res.stdout
        assert "package: core" in res.stdout
        assert "title_text_id: text_id (required)" in res.stdout

        res = run_cmd([gv2_content, "describe", game_pkg, "screen", "--format=json"])
        doc = json.loads(res.stdout)
        assert doc["status"] == "ok"
        assert doc["schema_id"] == "core:schema.definition.screen.v1"

        # new writes into the game package, using the schema owned by core.
        res = run_cmd([gv2_content, "new", game_pkg, "actor", "game:actor.npc.guard"])
        assert "created definition game:actor.npc.guard in definitions/actors.json5" in res.stdout

        res = run_cmd([gv2_content, "inspect", game_pkg, "game:actor.npc.guard", "--format=json"])
        actor_doc = json.loads(res.stdout)
        assert actor_doc["status"] == "ok"
        assert actor_doc["definition"]["id"] == "game:actor.npc.guard"

        res = run_cmd([gv2_content, "validate", game_pkg])
        assert "ok content_hash=" in res.stdout

        # A type no package in the set binds still fails, and says where it looked.
        res = run_cmd([gv2_content, "describe", game_pkg, "unknown_type"], check=False)
        assert res.returncode == 2
        assert "unknown definition type 'unknown_type'" in res.stderr
        assert "or its dependencies" in res.stderr

        res = run_cmd([gv2_content, "new", game_pkg, "unknown_type", "game:unknown_type.x"], check=False)
        assert res.returncode == 2
        assert "or its dependencies" in res.stderr

    print("[*] Testing 'index' on GameData/core...")
    # TAS-11: GameData/core is live gameplay content (not the frozen test
    # corpus, TAS-06) — assertions here must not pin its size. "active_ids:
    # N" / "[kind] (N)" counts are allowed to be anything; membership and
    # the [kind] bracket format are what's actually under test.
    # 27. Test index text format
    res = run_cmd([gv2_content, "index", core_pkg])
    assert "package_id: core" in res.stdout
    assert re.search(r"active_ids: \d+", res.stdout)
    assert re.search(r"\[screen\] \(\d+\)", res.stdout)
    assert re.search(r"\[text\] \(\d+\)", res.stdout)
    assert "core:screen.main" in res.stdout
    assert "core:screen.inventory" in res.stdout

    # 28. Test index json format
    res = run_cmd([gv2_content, "index", core_pkg, "--format=json"])
    index_doc = json.loads(res.stdout)
    assert index_doc["status"] == "ok"
    assert index_doc["package_id"] == "core"
    # The real invariant: total is exactly the sum of every kind's list —
    # not a specific number, since GameData/core grows freely (TAS-08).
    assert index_doc["total_active_ids"] == sum(len(ids) for ids in index_doc["kinds"].values())
    assert "screen" in index_doc["kinds"]
    assert "core:screen.main" in index_doc["kinds"]["screen"]
    assert "core:screen.inventory" in index_doc["kinds"]["screen"]
    assert len(index_doc["kinds"]["text"]) >= 1
    assert index_doc["redirects"] == []
    assert index_doc["tombstones"] == []

    # 29. Test index with package containing redirects and tombstones
    with tempfile.TemporaryDirectory() as tmpdir:
        test_pkg = os.path.join(tmpdir, "core")
        shutil.copytree(core_pkg, test_pkg)

        pkg_desc_path = os.path.join(test_pkg, "package.json5")
        with open(pkg_desc_path, "w") as f:
            f.write('{\n  package_id: "core",\n  namespace: "core",\n  version: "1.0.0",\n  redirects: {\n    "core:screen.old_main": "core:screen.main",\n  },\n  tombstones: [\n    "core:screen.deleted_screen",\n  ],\n}\n')

        res = run_cmd([gv2_content, "index", test_pkg, "--format=json"])
        doc = json.loads(res.stdout)
        assert doc["status"] == "ok"
        assert len(doc["redirects"]) == 1
        assert doc["redirects"][0]["source_id"] == "core:screen.old_main"
        assert doc["redirects"][0]["target_id"] == "core:screen.main"
        assert len(doc["tombstones"]) == 1
        assert doc["tombstones"][0] == "core:screen.deleted_screen"

        # Also check text format
        res = run_cmd([gv2_content, "index", test_pkg])
        assert "[redirects] (1)" in res.stdout
        assert "core:screen.old_main -> core:screen.main" in res.stdout
        assert "[tombstones] (1)" in res.stdout
        assert "core:screen.deleted_screen" in res.stdout

    # 30. Test index with missing arguments or non-existent path
    res = run_cmd([gv2_content, "index"], check=False)
    assert res.returncode == 2

    res = run_cmd([gv2_content, "index", "/nonexistent/path"], check=False)
    assert res.returncode == 2

    print("[*] Testing generate_vscode_snippets.py...")
    # 31. Test snippet generation from gv2-content index
    snippet_script = os.path.join(os.path.dirname(__file__), "..", "Editor", "generate_vscode_snippets.py")
    with tempfile.TemporaryDirectory() as tmpdir:
        out_snippets = os.path.join(tmpdir, "test.code-snippets")
        res = run_cmd([sys.executable, snippet_script, gv2_content, core_pkg, "--output", out_snippets])
        assert "Successfully generated" in res.stdout
        assert os.path.exists(out_snippets)

        with open(out_snippets, "r") as f:
            snippets_data = json.load(f)
        assert "ID: core:screen.main" in snippets_data
        assert snippets_data["ID: core:screen.main"]["body"] == ['"core:screen.main"']

    print("[*] Testing LOC-03: localization catalog does not affect package discovery, validate, index, or hash...")
    # 32. Test localization catalog isolation and hash invariance
    with tempfile.TemporaryDirectory() as tmpdir:
        tmp_pkg = os.path.join(tmpdir, "core")
        shutil.copytree(core_pkg, tmp_pkg)

        hash_before = run_cmd([gv2_content, "hash", tmp_pkg]).stdout.strip()
        validate_before = run_cmd([gv2_content, "validate", tmp_pkg]).stdout
        index_before = run_cmd([gv2_content, "index", tmp_pkg]).stdout

        loc_dir = os.path.join(tmp_pkg, "localization")
        os.makedirs(loc_dir, exist_ok=True)
        po_content = (
            'msgid ""\n'
            'msgstr ""\n'
            '"Language: ru\\n"\n'
            '"MIME-Version: 1.0\\n"\n'
            '"Content-Type: text/plain; charset=UTF-8\\n"\n'
            '\n'
            'msgctxt "core:text.screen.main.title"\n'
            'msgid "Main screen"\n'
            'msgstr "Главный экран"\n'
        )
        with open(os.path.join(loc_dir, "ru_RU.po"), "w", encoding="utf-8") as f:
            f.write(po_content)

        hash_after = run_cmd([gv2_content, "hash", tmp_pkg]).stdout.strip()
        validate_after = run_cmd([gv2_content, "validate", tmp_pkg]).stdout
        index_after = run_cmd([gv2_content, "index", tmp_pkg]).stdout

        assert hash_before == hash_after, f"Hash changed after adding localization: {hash_before} != {hash_after}"
        assert validate_before == validate_after
        assert index_before == index_after

        # Modifying PO translations also does not change hash
        with open(os.path.join(loc_dir, "ru_RU.po"), "w", encoding="utf-8") as f:
            f.write(po_content.replace("Главный экран", "Основной экран"))

        hash_modified = run_cmd([gv2_content, "hash", tmp_pkg]).stdout.strip()
        assert hash_before == hash_modified, f"Hash changed after modifying translation: {hash_before} != {hash_modified}"

    # 33. LOC-05: compile_localization tool produces deterministic String Table CSV artifacts
    print("[33] compile_localization generates deterministic String Table CSVs...")
    with tempfile.TemporaryDirectory() as tmp_out1, tempfile.TemporaryDirectory() as tmp_out2:
        compile_script = os.path.join(os.path.dirname(__file__), "compile_localization.py")
        run_cmd([sys.executable, compile_script, core_pkg, "--output-dir", tmp_out1])
        run_cmd([sys.executable, compile_script, core_pkg, "--output-dir", tmp_out2])

        csv1 = os.path.join(tmp_out1, "core_ru.csv")
        csv2 = os.path.join(tmp_out2, "core_ru.csv")
        assert os.path.exists(csv1), f"Expected {csv1} to exist"
        assert os.path.exists(csv2), f"Expected {csv2} to exist"

        content1 = open(csv1, "rb").read()
        content2 = open(csv2, "rb").read()
        assert content1 == content2, "compile_localization must be deterministic"
    # 34. LOC-08: gv2-content coverage reports missing, empty, translated, and extra keys
    print("[34] testing gv2-content coverage...")
    res = run_cmd([gv2_content, "coverage", core_pkg, "--format=json"])
    data = json.loads(res.stdout)
    assert data["package_id"] == "core"
    assert "ru" in data["locales"]
    ru_stats = data["locales"]["ru"]
    assert ru_stats["total_definitions"] == 7
    assert ru_stats["translated_count"] == 7
    assert ru_stats["empty_count"] == 0
    assert ru_stats["missing_count"] == 0
    assert ru_stats["extra_count"] == 0
    assert ru_stats["coverage_percentage"] == 100.0

    # Test in temporary package with incomplete translation
    with tempfile.TemporaryDirectory() as tmp_dir:
        tmp_pkg = os.path.join(tmp_dir, "core")
        shutil.copytree(core_pkg, tmp_pkg)

        # Write custom PO with 1 translated, 1 empty, and 1 extra key
        custom_po = (
            'msgid ""\n'
            'msgstr ""\n'
            '"Language: test\\n"\n'
            '\n'
            'msgctxt "core:text.screen.main.title"\n'
            'msgid "Main screen"\n'
            'msgstr "Экран"\n'
            '\n'
            'msgctxt "core:text.screen.inventory.title"\n'
            'msgid "Inventory"\n'
            'msgstr ""\n'
            '\n'
            'msgctxt "core:text.extra.obsolete"\n'
            'msgid "Old item"\n'
            'msgstr "Старый предмет"\n'
        )
        with open(os.path.join(tmp_pkg, "localization", "test.po"), "w", encoding="utf-8") as f:
            f.write(custom_po)

        res = run_cmd([gv2_content, "coverage", tmp_pkg, "--locale=test", "--format=json"])
        data = json.loads(res.stdout)
        stats = data["locales"]["test"]
        assert stats["total_definitions"] == 7
        assert stats["translated_count"] == 1
        assert stats["translated_keys"] == ["core:text.screen.main.title"]
        assert stats["empty_count"] == 1
        assert stats["empty_keys"] == ["core:text.screen.inventory.title"]
        assert stats["extra_count"] == 1
        assert stats["extra_keys"] == ["core:text.extra.obsolete"]
        assert stats["missing_count"] == 5

        # Validate that incomplete localization does not fail gv2-content validate
        val_res = run_cmd([gv2_content, "validate", tmp_pkg])
        assert val_res.returncode == 0

    # 35. DLA-10: Schema storage, write_policy, and operations
    print("[35] Schema storage and write_policy validation and describe...")
    with tempfile.TemporaryDirectory() as tmpdir:
        tmp_pkg = os.path.join(tmpdir, "core")
        shutil.copytree(core_pkg, tmp_pkg)

        # 35a. Test schema with managed field and operations
        schema_path = os.path.join(tmp_pkg, "schemas", "screen_v1.schema.json5")
        with open(schema_path, "r", encoding="utf-8") as f:
            content = f.read()

        # Add managed field and plain field
        content_mod = content.replace(
            'title_text_id: { kind: "text_id", required: true },',
            'title_text_id: { kind: "text_id", required: true, storage: "definition", write_policy: "read_only" },\n'
            '      gold: { kind: "int64", required: false, storage: "runtime_state", write_policy: "managed", operations: ["add_gold", "spend_gold"] },\n'
            '      target_npc: { kind: "ref_instance", target_kind: "actor", required: false },'
        )
        with open(schema_path, "w", encoding="utf-8") as f:
            f.write(content_mod)

        res = run_cmd([gv2_content, "describe", tmp_pkg, "screen", "--format=json"])
        doc = json.loads(res.stdout)
        assert doc["fields"]["gold"]["storage"] == "runtime_state"
        assert doc["fields"]["gold"]["write_policy"] == "managed"
        assert doc["fields"]["gold"]["operations"] == ["add_gold", "spend_gold"]
        assert doc["fields"]["target_npc"]["kind"] == "ref_instance"
        assert doc["fields"]["target_npc"]["storage"] == "runtime_state"

        # 35b. Invalid storage produces schema error
        with open(schema_path, "w", encoding="utf-8") as f:
            f.write(content.replace(
                'title_text_id: { kind: "text_id", required: true },',
                'title_text_id: { kind: "text_id", required: true, storage: "invalid_storage" },'
            ))
        val_res = run_cmd([gv2_content, "validate", tmp_pkg], check=False)
        assert val_res.returncode != 0
        assert "invalid_storage" in (val_res.stdout + val_res.stderr)

        # 35c. Managed write_policy without operations produces schema error
        with open(schema_path, "w", encoding="utf-8") as f:
            f.write(content.replace(
                'title_text_id: { kind: "text_id", required: true },',
                'title_text_id: { kind: "text_id", required: true, write_policy: "managed" },'
            ))
        val_res = run_cmd([gv2_content, "validate", tmp_pkg], check=False)
        assert val_res.returncode != 0
        assert "missing_operations" in (val_res.stdout + val_res.stderr)

    # 36. DLA-20: Text collector tool (collect_texts.py)
    print("[36] Text collector tool (collect_texts.py)...")
    collect_texts_py = os.path.join(os.path.dirname(__file__), "collect_texts.py")
    with tempfile.TemporaryDirectory() as tmpdir:
        tmp_pkg = os.path.join(tmpdir, "rh")
        rh_pkg = os.path.join(os.path.dirname(__file__), "..", "..", "GameData", "rh")
        shutil.copytree(rh_pkg, tmp_pkg)

        # Add a dummy lua script referencing new literal texts
        script_dir = os.path.join(tmp_pkg, "scripts", "gameplay")
        os.makedirs(script_dir, exist_ok=True)
        with open(os.path.join(script_dir, "custom_shop.lua"), "w", encoding="utf-8") as f:
            f.write(
                'local M = {}\n'
                'function M.buy()\n'
                '    local btn = button(text("custom_shop.title"), action("buy"))\n'
                '    return fail("custom_shop.out_of_stock")\n'
                'end\n'
                'return M\n'
            )

        # 36a. Dry-run reports missing texts without modifying files
        res_dry = run_cmd([sys.executable, collect_texts_py, tmp_pkg, "--dry-run"])
        assert res_dry.returncode == 0
        assert "rh:text.custom_shop.title" in res_dry.stdout
        assert "rh:text.error.custom_shop.out_of_stock" in res_dry.stdout
        assert "[dry-run] No files modified." in res_dry.stdout

        # 36b. Real run generates definition entries and PO catalog entries
        res_real = run_cmd([sys.executable, collect_texts_py, tmp_pkg])
        assert res_real.returncode == 0
        assert "Successfully updated texts" in res_real.stdout

        # Validate that the package is valid according to gv2-content validate
        real_core_pkg = os.path.join(os.path.dirname(__file__), "..", "..", "GameData", "core")
        val_res = run_cmd([gv2_content, "validate", real_core_pkg, tmp_pkg])
        assert val_res.returncode == 0

        # 36c. Second run is idempotent and modifies nothing
        res_repeat = run_cmd([sys.executable, collect_texts_py, tmp_pkg])
        assert res_repeat.returncode == 0
        assert "texts found in Lua scripts are already defined and present in PO catalogs." in res_repeat.stdout

    print("[*] All authoring tools tests passed successfully!")

if __name__ == "__main__":
    main()






