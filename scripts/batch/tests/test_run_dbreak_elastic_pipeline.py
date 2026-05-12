from pathlib import PurePosixPath

from run_dbreak_elastic_pipeline import (
    CONFIG, plan_ypol_symlinks, build_rsync_cmd, build_geninput_cmd,
    plan_filtered_tree,
)


def _cfg(**overrides):
    cfg = dict(CONFIG)
    cfg.update(overrides)
    return cfg


def test_plan_ypol_symlinks_emits_one_per_isotope_gamma_direction():
    cfg = _cfg(isotopes=["Sn112"], gammas=["g050", "g060"],
               ypol_directions=["ynp", "ypn"])
    plan = plan_ypol_symlinks(cfg)

    assert len(plan) == 4  # 1 iso * 2 gammas * 2 dirs
    src0, dst0 = plan[0]
    assert src0 == PurePosixPath(
        "data/qmdrawdata/qmdrawdata/20260413ypol/d+Sn112E190/"
        "d+Sn112E190g050ynp-RP360/dbreak.dat"
    )
    assert dst0 == PurePosixPath(
        "data/qmdrawdata/qmdrawdata/y_pol/phi_random/"
        "d+Sn112E190g050ynp/dbreak.dat"
    )


def test_plan_ypol_symlinks_default_config_has_16_entries():
    plan = plan_ypol_symlinks(CONFIG)
    assert len(plan) == 2 * 4 * 2  # 2 iso * 4 gammas * 2 dirs

    # All sources point under 20260413ypol; all dests under y_pol/phi_random
    for src, dst in plan:
        assert "20260413ypol" in str(src)
        assert "y_pol/phi_random" in str(dst)
        assert src.name == "dbreak.dat"
        assert dst.name == "dbreak.dat"


def test_build_rsync_cmd_only_includes_dbreak_dat():
    cfg = _cfg()
    cmd = build_rsync_cmd(cfg, dry_run=False)
    # Must be a list of args, not a shell string
    assert isinstance(cmd, list)
    assert cmd[0] == "rsync"
    assert "-avz" in cmd
    # Filter pattern: only dbreak.dat files come through
    assert "--include=*/" in cmd
    assert "--include=dbreak.dat" in cmd
    assert "--exclude=*" in cmd
    # Source ends with trailing slash (rsync semantic for dir contents)
    src = next(a for a in cmd if a.endswith("/20260413ypol/"))
    assert "/home/tian/workspace/dpol/smsimulator5.5/data/qmdrawdata/qmdrawdata/20260413ypol/" == src
    # Destination uses ssh alias
    dst = cmd[-1]
    assert dst.startswith("spana03:")
    assert dst.endswith("/20260413ypol/")


def test_build_rsync_cmd_dry_run_adds_flag():
    cfg = _cfg()
    cmd = build_rsync_cmd(cfg, dry_run=True)
    assert "--dry-run" in cmd


def test_build_geninput_cmd_ypol_sn112():
    cfg = _cfg()
    cmd = build_geninput_cmd(cfg, mode="ypol", isotope="Sn112")
    # Returns a single shell string suitable for run_bash()
    assert isinstance(cmd, str)
    assert "source setup.sh" in cmd
    assert "build/bin/GenInputRoot_qmdrawdata" in cmd
    assert "--mode ypol" in cmd
    assert "--source elastic" in cmd
    assert "--target-filter Sn112" in cmd
    assert "--cut-unphysical on" in cmd
    assert "--cut-ypol-axis-limit 150.0" in cmd
    assert "--cut-zpol-axis-limit 150.0" in cmd
    assert "--randomize-ypol off" in cmd
    assert "--randomize-zpol on" in cmd
    assert "--rotation-seed 0" in cmd


def test_build_geninput_cmd_zpol_sn124():
    cmd = build_geninput_cmd(_cfg(), mode="zpol", isotope="Sn124")
    assert "--mode zpol" in cmd
    assert "--target-filter Sn124" in cmd


def test_plan_filtered_tree_ypol_picks_target_isotope_gamma_dirs():
    cfg = _cfg(isotopes=["Sn112"], gammas=["g050"],
               ypol_directions=["ynp"])
    plan = plan_filtered_tree(cfg, pol="ypol")
    # 1 iso * 1 gamma * 1 dir * 1 dbreak.root file
    assert len(plan) == 1
    src, dst = plan[0]
    assert str(src) == ("data/simulation/g4input/20260413ypol/d+Sn112E190/"
                        "d+Sn112E190g050ynp-RP360/dbreak.root")
    assert str(dst) == ("logs/dbreak_elastic_pipeline/g4input_filtered_ypol/"
                        "d+Sn112E190g050ynp/dbreak.root")


def test_plan_filtered_tree_zpol_emits_10_b_slices_per_dir():
    cfg = _cfg(isotopes=["Sn112"], gammas=["g050"],
               zpol_directions=["znp"])
    plan = plan_filtered_tree(cfg, pol="zpol")
    # 1 iso * 1 gamma * 1 dir * 10 b-slices
    assert len(plan) == 10
    assert all("dbreakb" in str(src) for src, _ in plan)
    # b01..b10 distinct
    bnums = sorted({str(src).split("dbreakb")[1].split(".")[0] for src, _ in plan})
    assert bnums == [f"{i:02d}" for i in range(1, 11)]


def test_plan_filtered_tree_default_config_total_176():
    plan_y = plan_filtered_tree(CONFIG, pol="ypol")
    plan_z = plan_filtered_tree(CONFIG, pol="zpol")
    assert len(plan_y) == 16     # 2 iso * 4 gamma * 2 dir * 1 file
    assert len(plan_z) == 160    # 2 iso * 4 gamma * 2 dir * 10 files
    assert len(plan_y) + len(plan_z) == 176
