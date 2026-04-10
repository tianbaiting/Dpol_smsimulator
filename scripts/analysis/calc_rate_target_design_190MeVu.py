#!/usr/bin/env python3
from __future__ import annotations

import csv
import json
import math
from dataclasses import asdict, dataclass
from datetime import datetime, timezone
from pathlib import Path
from typing import Any


NA = 6.022_140_76e23
SIGMA_MB_TO_CM2 = 1.0e-27
MM_TO_CM = 0.1
HOUR_TO_S = 3600.0
SPEED_OF_LIGHT_M_S = 299_792_458.0
ATOMIC_MASS_UNIT_MEV = 931.494_102_42


@dataclass(frozen=True)
class SourceRecord:
    source_id: str
    category: str
    title: str
    locator: str
    role: str
    note: str


def repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def reaction_rate_hz(number_density_cm3: float, thickness_mm: float, beam_intensity_pps: float,
                     cross_section_mb: float) -> float:
    return (
        number_density_cm3
        * thickness_mm * MM_TO_CM
        * beam_intensity_pps
        * cross_section_mb * SIGMA_MB_TO_CM2
    )


def relativistic_gamma_from_kinetic_energy_per_u(kinetic_energy_mev_u: float) -> float:
    return 1.0 + kinetic_energy_mev_u / ATOMIC_MASS_UNIT_MEV


def relativistic_beta_from_kinetic_energy_per_u(kinetic_energy_mev_u: float) -> float:
    gamma_rel = relativistic_gamma_from_kinetic_energy_per_u(kinetic_energy_mev_u)
    return math.sqrt(1.0 - 1.0 / (gamma_rel * gamma_rel))


def revolution_frequency_from_extraction_radius_hz(beam_velocity_m_s: float, extraction_radius_m: float) -> float:
    orbit_circumference_m = 2.0 * math.pi * extraction_radius_m
    return beam_velocity_m_s / orbit_circumference_m


def poisson_tail_geq(mu: float, k: int) -> float:
    if k <= 0:
        return 1.0
    partial = 0.0
    for i in range(k):
        partial += math.exp(-mu) * mu ** i / math.factorial(i)
    return 1.0 - partial


def area_of_disk_cm2(diameter_mm: float) -> float:
    radius_cm = (diameter_mm / 10.0) / 2.0
    return math.pi * radius_cm * radius_cm


def thickness_from_inventory_mm(mass_g: float, density_g_cm3: float, area_cm2: float) -> float:
    return mass_g / (density_g_cm3 * area_cm2) * 10.0


def equivalent_diameter_from_inventory_mm(mass_g: float, density_g_cm3: float, thickness_mm: float) -> float:
    area_cm2 = mass_g / (density_g_cm3 * thickness_mm * MM_TO_CM)
    radius_cm = math.sqrt(area_cm2 / math.pi)
    return radius_cm * 20.0


def write_json(path: Path, payload: Any) -> None:
    path.write_text(json.dumps(payload, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")


def read_json_if_exists(path: Path) -> Any:
    if not path.exists():
        return None
    return json.loads(path.read_text(encoding="utf-8"))


def write_csv(path: Path, rows: list[dict[str, Any]]) -> None:
    if not rows:
        raise ValueError(f"no rows for {path}")
    with path.open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)


def make_sources(root: Path) -> list[SourceRecord]:
    return [
        SourceRecord(
            source_id="local_polarization_rate",
            category="local_repo",
            title="Internal IVR manuscript draft with planning-rate paragraph",
            locator=str(root / "docs/reports/qmdrawdataAna/article/arxiv/polarization.tex") + "#L340",
            role="Beam intensity, monitor counts, planning coincidence efficiency, and Sn124 planning cross section.",
            note="The same file also contains the inconsistent compact coefficient that is corrected by this script.",
        ),
        SourceRecord(
            source_id="local_polarization_rate_formula",
            category="local_repo",
            title="Internal IVR manuscript draft formula block",
            locator=str(root / "docs/reports/qmdrawdataAna/article/arxiv/polarization.tex") + "#L458",
            role="Source of sigma = 550 mb, eta_det = 4.65%, and the original n = 3.685e22 cm^-3 statement.",
            note="This block is the main local source being audited and rewritten in the planning note.",
        ),
        SourceRecord(
            source_id="local_geom_3deg_115T",
            category="local_repo",
            title="Current 3deg_1.15T geometry macro",
            locator=str(root / "configs/simulation/geometry/3deg_1.15T.mac") + "#L10",
            role="Defines the current planning geometry baseline and shows that the target macro still carries a placeholder 80x80x30 mm target with SetTarget false.",
            note="Used only to document geometry context, not to manufacture the target.",
        ),
        SourceRecord(
            source_id="local_geom_B115T",
            category="local_repo",
            title="B115T field-map geometry include",
            locator=str(root / "configs/simulation/DbeamTest/detailMag1to1.2T/geometry_B115T.mac") + "#L1",
            role="Documents the field-map and legacy PDC/NEBULA defaults included by the 3deg_1.15T macro.",
            note="Used to explain how the geometry baseline is assembled.",
        ),
        SourceRecord(
            source_id="local_acceptance_12T_5deg",
            category="local_repo",
            title="Legacy 1.2 T, 5 deg acceptance report",
            locator=str(root / "results/geo_acceptence_results/results/1.2T/acceptance_report.txt") + "#L15",
            role="Provides the nearest available PDC / NEBULA / coincidence acceptance split used as a provisional singles-rate scaling reference.",
            note="This is not the same as the current 3deg_1.15T full chain; it is kept explicitly as a legacy scaling aid.",
        ),
        SourceRecord(
            source_id="local_qmd_layout",
            category="local_repo",
            title="QMD raw-data README",
            locator=str(root / "data/qmdrawdata/qmdrawdata/readme.txt") + "#L1",
            role="Documents available targets and polarization modes in the QMD source tree.",
            note="Used to state why Sn124 and Sn112 are available in the transport input while current full-chain rate anchors are not equally mature.",
        ),
        SourceRecord(
            source_id="user_src_bunch_structure",
            category="user_input",
            title="User-provided SRC extraction radius and harmonic number",
            locator="Conversation input on 2026-04-10 with SRC screenshot",
            role="Uses harmonic number h = 6 and SRC extraction radius R = 5.36 m to derive the bunch spacing.",
            note="Replaces the earlier mistaken 11.91 kHz revolution-frequency assumption.",
        ),
        SourceRecord(
            source_id="user_beam_energy_190mevu",
            category="user_input",
            title="User-stated deuteron beam energy",
            locator="Conversation input on 2026-04-10",
            role="Beam kinetic energy 190 MeV/u used to derive relativistic beta, beam speed, and SRC revolution frequency.",
            note="The timing derivation uses gamma = 1 + T_u / (931.494 MeV) and beta = sqrt(1 - gamma^-2).",
        ),
        SourceRecord(
            source_id="assumption_density_sn",
            category="assumption",
            title="Bulk density of metallic Sn used for target stock conversion",
            locator="Engineering assumption in this calculation package",
            role="Converts 1.2 g inventory into thickness-area tradeoffs.",
            note="Only affects thickness/size conversion, not the manuscript-audit comparison itself.",
        ),
        SourceRecord(
            source_id="external_exfor",
            category="external_reference",
            title="IAEA EXFOR database",
            locator="https://nds.iaea.org/exfor/",
            role="Official database to cross-check whether comparable deuteron-target reaction data exist in the public literature.",
            note="Not used as the primary rate input in this package.",
        ),
        SourceRecord(
            source_id="external_atima",
            category="external_reference",
            title="GSI ATIMA online documentation",
            locator="https://web-docs.gsi.de/~weick/atima/",
            role="Official stopping-power and range reference for checking target energy loss and thickness feasibility.",
            note="Recommended follow-up source for replacing rough straggling assumptions with transport-backed estimates.",
        ),
        SourceRecord(
            source_id="external_geant4_prm",
            category="external_reference",
            title="Geant4 Physics Reference Manual, hadronic cross sections",
            locator="https://geant4.web.cern.ch/documentation/dev/prm_html/PhysicsReferenceManual/hadronic/Cross_Section/index.html",
            role="Official reference explaining the scope of Geant4 hadronic transport and detector-response simulation.",
            note="Used to justify why Geant4 should not replace the ImQMD event generator for production-rate inputs.",
        ),
        SourceRecord(
            source_id="external_auce_1996_sigmaR_pdf",
            category="external_reference",
            title="Auce et al. Phys. Rev. C 53 (1996) 2919 searchable PDF mirror",
            locator="https://lib.ysu.am/articles_art/5bd57cbcd47a121344e92133a487946e.pdf",
            role="Provides the public total reaction cross section table for d + 112Sn, 124Sn, and 208Pb at 37.9, 65.5, and 97.4 MeV.",
            note="Canonical paper is Phys. Rev. C 53, 2919 (1996); this mirror is used because the indexed PDF text exposes the numerical table entries needed for direct comparison.",
        ),
        SourceRecord(
            source_id="external_auce_1996_osti",
            category="external_reference",
            title="OSTI bibliographic record for Auce et al. reaction-cross-section paper",
            locator="https://www.osti.gov/biblio/284777",
            role="Canonical bibliographic anchor for the public d + Sn/Pb total reaction cross section dataset.",
            note="Used to preserve a stable bibliographic record alongside the searchable PDF mirror.",
        ),
        SourceRecord(
            source_id="external_yahiro_1984_118sn_breakup",
            category="external_reference",
            title="Yahiro et al. 118Sn breakup coincidence-cross-section study at Ed = 56 MeV",
            locator="https://kyushu-u.elsevierpure.com/en/publications/a-coupled-channel-approach-to-deuteron-projectile-breakup/",
            role="Public breakup-specific literature anchor showing that proton-neutron coincidence cross sections on a Sn target were measured and analyzed with CDCC.",
            note="Portal metadata cites DOI 10.1016/0370-2693(84)90549-5.",
        ),
        SourceRecord(
            source_id="external_iseri_1988_virtual_breakup",
            category="external_reference",
            title="Iseri et al. virtual breakup effects in polarized deuteron elastic scattering",
            locator="https://kyushu-u.elsevierpure.com/en/publications/virtual-breakup-effects-in-elastic-scattering-of-polarized-deuter/",
            role="Public heavy-target breakup literature covering 118Sn and 208Pb at Ed = 56 MeV and 208Pb at 21.5 MeV.",
            note="Portal metadata cites DOI 10.1016/0375-9474(88)90512-X.",
        ),
        SourceRecord(
            source_id="external_yahiro_1987_breakup_400",
            category="external_reference",
            title="Yahiro et al. deuteron breakup effects at 56 and 400 MeV",
            locator="https://kyushu-u.elsevierpure.com/en/publications/deuteron-breakup-effect-on-deuteron-elastic-scattering/",
            role="Public high-energy breakup context showing that breakup effects remain important even at 400 MeV total deuteron energy.",
            note="Portal metadata cites DOI 10.1016/0375-9474(87)90687-7.",
        ),
        SourceRecord(
            source_id="external_okamura_2010_breakup_208pb",
            category="external_reference",
            title="Okamura et al. complete elastic-breakup measurement at Ed = 140 MeV",
            locator="https://kyushu-u.elsevierpure.com/en/publications/study-of-breakup-mechanism-of-a-loosely-bound-projectile-in-a-reg/",
            role="Public high-energy elastic-breakup literature including 208Pb in a kinematically complete measurement.",
            note="Portal metadata cites DOI 10.1051/epjconf/20100306010.",
        ),
        SourceRecord(
            source_id="external_kalbach_2017_breakup_systematics",
            category="external_reference",
            title="Kalbach breakup systematics from a literature database",
            locator="https://www.osti.gov/servlets/purl/1346154",
            role="Provides a global database-based statement that deuteron absorptive breakup can reach 50-60% of the total reaction cross section and generally increases with target mass.",
            note="Accepted manuscript for Phys. Rev. C 95, 014606 (2017).",
        ),
    ]


def make_external_database_rows(planning_breakup_cross_section_mb: float) -> list[dict[str, Any]]:
    total_reaction_datasets = [
        ("Sn112", 37.9, 2130.0, 76.0),
        ("Sn112", 65.5, 2156.0, 47.0),
        ("Sn112", 97.4, 2212.0, 59.0),
        ("Sn124", 37.9, 2282.0, 90.0),
        ("Sn124", 65.5, 2332.0, 57.0),
        ("Sn124", 97.4, 2343.0, 59.0),
        ("Pb208", 37.9, 2844.0, 142.0),
        ("Pb208", 65.5, 3049.0, 71.0),
        ("Pb208", 97.4, 3250.0, 82.0),
    ]

    rows: list[dict[str, Any]] = []
    for target_label, beam_energy_total_mev, sigma_total_reaction_mb, sigma_uncertainty_mb in total_reaction_datasets:
        rows.append(
            {
                "dataset_group": "public_total_reaction_sigmaR",
                "entry_id": f"auce1996_{target_label.lower()}_{beam_energy_total_mev:.1f}MeV".replace(".", "p"),
                "target_label": target_label,
                "beam_energy_total_mev": beam_energy_total_mev,
                "beam_energy_mev_u": round(beam_energy_total_mev / 2.0, 3),
                "observable": "total_reaction_cross_section",
                "value_mb": sigma_total_reaction_mb,
                "uncertainty_mb": sigma_uncertainty_mb,
                "planning_breakup_cross_section_mb": planning_breakup_cross_section_mb,
                "planning_breakup_fraction_of_this_sigma": round(
                    planning_breakup_cross_section_mb / sigma_total_reaction_mb, 6
                ),
                "direct_substitute_for_190MeVu_breakup": "no",
                "source_ids": "external_auce_1996_sigmaR_pdf,external_auce_1996_osti",
                "note": "Public total reaction cross section on the same or comparison heavy target. Useful as a sanity envelope only because sigma_R != breakup sigma and the beam energy is far below 190 MeV/u.",
            }
        )

    rows.extend(
        [
            {
                "dataset_group": "public_breakup_context",
                "entry_id": "yahiro1984_118sn_56MeV_dpn",
                "target_label": "Sn118",
                "beam_energy_total_mev": 56.0,
                "beam_energy_mev_u": 28.0,
                "observable": "elastic_breakup_pn_coincidence_cross_sections_and_proton_spectrum",
                "value_mb": "",
                "uncertainty_mb": "",
                "planning_breakup_cross_section_mb": planning_breakup_cross_section_mb,
                "planning_breakup_fraction_of_this_sigma": "",
                "direct_substitute_for_190MeVu_breakup": "no",
                "source_ids": "external_yahiro_1984_118sn_breakup",
                "note": "Breakup-specific Sn-family literature anchor. The abstract states that proton-neutron coincidence cross sections in (d,pn) on 118Sn at 56 MeV were analyzed successfully with CDCC.",
            },
            {
                "dataset_group": "public_breakup_context",
                "entry_id": "iseri1988_118sn_208pb_56MeV",
                "target_label": "Sn118_and_Pb208",
                "beam_energy_total_mev": 56.0,
                "beam_energy_mev_u": 28.0,
                "observable": "elastic_scattering_with_virtual_breakup_effects",
                "value_mb": "",
                "uncertainty_mb": "",
                "planning_breakup_cross_section_mb": planning_breakup_cross_section_mb,
                "planning_breakup_fraction_of_this_sigma": "",
                "direct_substitute_for_190MeVu_breakup": "no",
                "source_ids": "external_iseri_1988_virtual_breakup",
                "note": "Abstract states that virtual breakup makes indispensable contributions to cross sections and analyzing powers for 118Sn and 208Pb at 56 MeV, with additional 208Pb information at 21.5 MeV.",
            },
            {
                "dataset_group": "public_breakup_context",
                "entry_id": "yahiro1987_pb_56_and_400MeV",
                "target_label": "Pb_targets",
                "beam_energy_total_mev": 400.0,
                "beam_energy_mev_u": 200.0,
                "observable": "elastic_scattering_breakup_effect_importance",
                "value_mb": "",
                "uncertainty_mb": "",
                "planning_breakup_cross_section_mb": planning_breakup_cross_section_mb,
                "planning_breakup_fraction_of_this_sigma": "",
                "direct_substitute_for_190MeVu_breakup": "no",
                "source_ids": "external_yahiro_1987_breakup_400",
                "note": "Abstract states that breakup effects are important especially for Pb at 56 MeV and still important even at 400 MeV total deuteron energy, which is close to the present 190 MeV/u case in total beam energy.",
            },
            {
                "dataset_group": "public_breakup_context",
                "entry_id": "okamura2010_208pb_140MeV_complete_breakup",
                "target_label": "Pb208",
                "beam_energy_total_mev": 140.0,
                "beam_energy_mev_u": 70.0,
                "observable": "kinematically_complete_elastic_breakup_measurement",
                "value_mb": "",
                "uncertainty_mb": "",
                "planning_breakup_cross_section_mb": planning_breakup_cross_section_mb,
                "planning_breakup_fraction_of_this_sigma": "",
                "direct_substitute_for_190MeVu_breakup": "no",
                "source_ids": "external_okamura_2010_breakup_208pb",
                "note": "Abstract states that a kinematically complete elastic-breakup measurement was carried out at Ed = 140 MeV on 208Pb in the Coulomb-breakup-dominant angular region 0-8 degrees.",
            },
            {
                "dataset_group": "public_breakup_systematics",
                "entry_id": "kalbach2017_absorptive_breakup_fraction",
                "target_label": "global_heavy_target_systematics",
                "beam_energy_total_mev": "",
                "beam_energy_mev_u": "",
                "observable": "absorptive_breakup_fraction_of_total_reaction",
                "value_mb": "",
                "uncertainty_mb": "",
                "planning_breakup_cross_section_mb": planning_breakup_cross_section_mb,
                "planning_breakup_fraction_of_this_sigma": "",
                "direct_substitute_for_190MeVu_breakup": "no",
                "source_ids": "external_kalbach_2017_breakup_systematics",
                "note": "Database-based systematics state that absorptive breakup can account for as much as 50-60% of the total reaction cross section and generally increases with target mass.",
            },
        ]
    )
    return rows


def main() -> None:
    root = repo_root()
    results_dir = root / "results" / "rate_target_design_190MeVu"
    results_dir.mkdir(parents=True, exist_ok=True)
    validation_dir = results_dir / "polarization_validation"
    validation_summary_path = validation_dir / "validation_summary.json"
    validation_summary = read_json_if_exists(validation_summary_path)

    sources = make_sources(root)
    if validation_summary is not None:
        sources.append(
            SourceRecord(
                source_id="local_validation_sn124_zpol_g060",
                category="local_repo",
                title="Fresh Sn124 zpol g060 Geant4 validation summary",
                locator=str(validation_summary_path),
                role="Provides the locally remeasured coincidence efficiency used to cross-check polarization.tex.",
                note="This validation uses the b/bmax weighted zpol elastic sample and fresh Geant4 outputs.",
            )
        )
    source_map = {record.source_id: asdict(record) for record in sources}

    beam_intensity_pps = 1.0e7
    beam_time_h = 16.0
    beam_energy_mev_u = 190.0
    harmonic_number = 6
    src_extraction_radius_m = 5.36
    # Derive the SRC bucket spacing from the relativistic beam speed at extraction, not from a fixed kHz guess.
    # gamma = 1 + T_u / (m_u c^2), beta = sqrt(1 - gamma^-2), v = beta c, f_rev = v / (2 pi R_ext).
    gamma_rel = relativistic_gamma_from_kinetic_energy_per_u(beam_energy_mev_u)
    beta_rel = relativistic_beta_from_kinetic_energy_per_u(beam_energy_mev_u)
    beam_velocity_m_s = beta_rel * SPEED_OF_LIGHT_M_S
    orbit_circumference_m = 2.0 * math.pi * src_extraction_radius_m
    revolution_frequency_hz = revolution_frequency_from_extraction_radius_hz(beam_velocity_m_s, src_extraction_radius_m)
    bucket_frequency_hz = harmonic_number * revolution_frequency_hz
    bucket_period_s = 1.0 / bucket_frequency_hz

    planning_cross_section_mb = 550.0
    planning_coincidence_efficiency = 0.0465
    legacy_pdc_acceptance = 0.300_933
    legacy_nebula_acceptance = 0.854_962
    legacy_coincidence_acceptance = 0.299_982
    legacy_pdc_to_coinc_ratio = legacy_pdc_acceptance / legacy_coincidence_acceptance
    legacy_nebula_to_coinc_ratio = legacy_nebula_acceptance / legacy_coincidence_acceptance

    sn_density_g_cm3 = 7.31
    sn124_molar_mass_g_mol = 124.0
    sn112_molar_mass_g_mol = 112.0
    manuscript_number_density_sn124 = 3.685e22
    recomputed_number_density_sn124 = NA * sn_density_g_cm3 / sn124_molar_mass_g_mol
    recomputed_number_density_sn112 = NA * sn_density_g_cm3 / sn112_molar_mass_g_mol
    implied_molar_mass_from_manuscript = NA * sn_density_g_cm3 / manuscript_number_density_sn124

    reference_thicknesses_mm = [0.5, 1.0, 1.5, 2.0, 3.0]
    practical_diameters_mm = [12.0, 15.0, 20.0]
    inventory_mass_g = 1.2
    validation_rate_rows: list[dict[str, Any]] = []
    external_database_rows = make_external_database_rows(planning_cross_section_mb)

    rates_rows: list[dict[str, Any]] = []
    for thickness_mm in reference_thicknesses_mm:
        reaction_rate = reaction_rate_hz(
            recomputed_number_density_sn124,
            thickness_mm,
            beam_intensity_pps,
            planning_cross_section_mb,
        )
        detected_rate = reaction_rate * planning_coincidence_efficiency
        pdc_proxy_rate = detected_rate * legacy_pdc_to_coinc_ratio
        nebula_proxy_rate = detected_rate * legacy_nebula_to_coinc_ratio
        rates_rows.append(
            {
                "scenario_group": "reference_thickness_scan",
                "scenario_id": f"thickness_{thickness_mm:.1f}mm",
                "target_reference": "Sn124 planning anchor",
                "thickness_mm": round(thickness_mm, 6),
                "target_diameter_mm": "",
                "area_cm2": "",
                "beam_intensity_pps": int(beam_intensity_pps),
                "beam_time_h": beam_time_h,
                "reaction_rate_hz": round(reaction_rate, 6),
                "detected_coincidence_rate_hz": round(detected_rate, 6),
                "detected_coincidence_events": round(detected_rate * beam_time_h * HOUR_TO_S, 6),
                "pdc_singles_proxy_rate_hz": round(pdc_proxy_rate, 6),
                "nebula_singles_proxy_rate_hz": round(nebula_proxy_rate, 6),
                "bucket_frequency_hz": round(bucket_frequency_hz, 6),
                "bucket_period_us": round(bucket_period_s * 1.0e6, 6),
                "beam_particles_per_bucket": round(beam_intensity_pps / bucket_frequency_hz, 6),
                "reaction_mean_per_bucket": round(reaction_rate / bucket_frequency_hz, 9),
                "reaction_p_ge_1": round(poisson_tail_geq(reaction_rate / bucket_frequency_hz, 1), 9),
                "reaction_p_ge_2": round(poisson_tail_geq(reaction_rate / bucket_frequency_hz, 2), 9),
                "detected_mean_per_bucket": round(detected_rate / bucket_frequency_hz, 9),
                "detected_p_ge_1": round(poisson_tail_geq(detected_rate / bucket_frequency_hz, 1), 9),
                "detected_p_ge_2": round(poisson_tail_geq(detected_rate / bucket_frequency_hz, 2), 9),
                "note": "PDC/NEBULA singles are provisional proxies scaled from the legacy 1.2T, 5deg acceptance split.",
            }
        )

    target_design_rows: list[dict[str, Any]] = []
    for diameter_mm in practical_diameters_mm:
        area_cm2 = area_of_disk_cm2(diameter_mm)
        thickness_mm = thickness_from_inventory_mm(inventory_mass_g, sn_density_g_cm3, area_cm2)
        areal_density_mg_cm2 = inventory_mass_g / area_cm2 * 1000.0
        sn124_reaction_rate = reaction_rate_hz(
            recomputed_number_density_sn124,
            thickness_mm,
            beam_intensity_pps,
            planning_cross_section_mb,
        )
        sn124_detected_rate = sn124_reaction_rate * planning_coincidence_efficiency
        sn112_same_sigma_scale = recomputed_number_density_sn112 / recomputed_number_density_sn124
        target_design_rows.append(
            {
                "inventory_target": "Sn112_or_Sn124",
                "inventory_mass_g": inventory_mass_g,
                "disk_diameter_mm": round(diameter_mm, 6),
                "area_cm2": round(area_cm2, 6),
                "areal_density_mg_cm2": round(areal_density_mg_cm2, 6),
                "bulk_thickness_mm_density731": round(thickness_mm, 6),
                "sn124_reaction_rate_hz_ref550mb": round(sn124_reaction_rate, 6),
                "sn124_detected_coincidence_rate_hz_ref550mb_eta465": round(sn124_detected_rate, 6),
                "sn124_detected_events_16h_ref": round(sn124_detected_rate * beam_time_h * HOUR_TO_S, 6),
                "sn112_same_sigma_atom_density_scale_vs_sn124": round(sn112_same_sigma_scale, 6),
                "sn112_detected_rate_hz_if_sigma_same_as_sn124": round(sn124_detected_rate * sn112_same_sigma_scale, 6),
                "note": "Sn112 rate column is only an atom-density scaling placeholder; target-specific sigma must be replaced by a dedicated ImQMD extraction.",
            }
        )

    inventory_backsolve_rows: list[dict[str, Any]] = []
    for thickness_mm in reference_thicknesses_mm:
        equivalent_diameter_mm = equivalent_diameter_from_inventory_mm(
            inventory_mass_g,
            sn_density_g_cm3,
            thickness_mm,
        )
        inventory_backsolve_rows.append(
            {
                "inventory_mass_g": inventory_mass_g,
                "chosen_thickness_mm": thickness_mm,
                "required_disk_diameter_mm": round(equivalent_diameter_mm, 6),
                "required_area_cm2": round(area_of_disk_cm2(equivalent_diameter_mm), 6),
                "note": "Shows how quickly the diameter collapses if the full 1.2 g inventory is made thicker.",
            }
        )

    nominal_3mm = next(row for row in rates_rows if row["scenario_id"] == "thickness_3.0mm")
    nominal_15mm = next(row for row in target_design_rows if math.isclose(row["disk_diameter_mm"], 15.0))
    nominal_reaction_rate_3mm = reaction_rate_hz(
        recomputed_number_density_sn124,
        3.0,
        beam_intensity_pps,
        planning_cross_section_mb,
    )

    validation_rate_rows.append(
        {
            "scenario_id": "manuscript_sigma550_eta465",
            "scope_id": "manuscript_anchor",
            "cross_section_mb": planning_cross_section_mb,
            "coincidence_efficiency": planning_coincidence_efficiency,
            "reaction_rate_hz_3mm": round(nominal_reaction_rate_3mm, 6),
            "detected_coincidence_rate_hz_3mm": round(
                nominal_reaction_rate_3mm * planning_coincidence_efficiency, 6
            ),
            "detected_coincidence_events_16h_3mm": round(
                nominal_reaction_rate_3mm * planning_coincidence_efficiency * beam_time_h * HOUR_TO_S, 6
            ),
            "source_ids": "local_polarization_rate_formula",
            "note": "Original manuscript anchor used in the existing planning note.",
        }
    )

    validation_context: dict[str, Any] | None = None
    if validation_summary is not None:
        aggregate_b_le_9 = validation_summary["aggregate"]["b_le_9"]
        local_eta = float(aggregate_b_le_9["coincidence_efficiency"])
        validation_rate_rows.append(
            {
                "scenario_id": "manuscript_sigma550_eta_localdata",
                "scope_id": "b_le_9",
                "cross_section_mb": planning_cross_section_mb,
                "coincidence_efficiency": round(local_eta, 9),
                "reaction_rate_hz_3mm": round(nominal_reaction_rate_3mm, 6),
                "detected_coincidence_rate_hz_3mm": round(
                    nominal_reaction_rate_3mm * local_eta, 6
                ),
                "detected_coincidence_events_16h_3mm": round(
                    nominal_reaction_rate_3mm * local_eta * beam_time_h * HOUR_TO_S, 6
                ),
                "source_ids": "local_polarization_rate_formula,local_validation_sn124_zpol_g060",
                "note": "Uses manuscript sigma = 550 mb and the locally remeasured coincidence efficiency.",
            }
        )
        validation_context = {
            "summary_path": str(validation_summary_path),
            "dataset_id": validation_summary["dataset_id"],
            "sigma_validation_status": validation_summary["sigma_validation_status"],
            "headline_scope": aggregate_b_le_9,
            "source_ids": ["local_validation_sn124_zpol_g060"],
        }

    inputs_payload = {
        "generated_at_utc": datetime.now(timezone.utc).isoformat(),
        "beam": {
            "beam_intensity_pps": beam_intensity_pps,
            "beam_time_h": beam_time_h,
            "beam_energy_mev_u": beam_energy_mev_u,
            "harmonic_number": harmonic_number,
            "src_extraction_radius_m": src_extraction_radius_m,
            "relativistic_gamma": gamma_rel,
            "relativistic_beta": beta_rel,
            "beam_velocity_m_s": beam_velocity_m_s,
            "orbit_circumference_m": orbit_circumference_m,
            "revolution_frequency_hz": revolution_frequency_hz,
            "bucket_frequency_hz": bucket_frequency_hz,
            "bucket_period_s": bucket_period_s,
            "timing_derivation": {
                "gamma_formula": "gamma = 1 + T_u / (m_u c^2)",
                "beta_formula": "beta = sqrt(1 - gamma^-2)",
                "velocity_formula": "v = beta c",
                "revolution_formula": "f_rev = v / (2 pi R_ext)",
                "bucket_formula": "f_bucket = h f_rev",
                "spacing_formula": "Delta t = 1 / f_bucket",
            },
            "sources": ["local_polarization_rate", "user_src_bunch_structure", "user_beam_energy_190mevu"],
        },
        "geometry_context": {
            "primary_geometry_macro": str(root / "configs/simulation/geometry/3deg_1.15T.mac"),
            "field_map_geometry_macro": str(root / "configs/simulation/DbeamTest/detailMag1to1.2T/geometry_B115T.mac"),
            "source_ids": ["local_geom_3deg_115T", "local_geom_B115T"],
        },
        "planning_physics_anchor": {
            "target_label": "Sn124",
            "cross_section_mb": planning_cross_section_mb,
            "detected_coincidence_efficiency": planning_coincidence_efficiency,
            "source_ids": ["local_polarization_rate_formula"],
        },
        "legacy_acceptance_scaling": {
            "pdc_acceptance": legacy_pdc_acceptance,
            "nebula_acceptance": legacy_nebula_acceptance,
            "coincidence_acceptance": legacy_coincidence_acceptance,
            "pdc_to_coincidence_ratio": legacy_pdc_to_coinc_ratio,
            "nebula_to_coincidence_ratio": legacy_nebula_to_coinc_ratio,
            "source_ids": ["local_acceptance_12T_5deg"],
        },
        "material_model": {
            "sn_bulk_density_g_cm3": sn_density_g_cm3,
            "sn124_molar_mass_g_mol_planning": sn124_molar_mass_g_mol,
            "sn112_molar_mass_g_mol_planning": sn112_molar_mass_g_mol,
            "sn124_number_density_cm3_recomputed": recomputed_number_density_sn124,
            "sn124_number_density_cm3_manuscript": manuscript_number_density_sn124,
            "sn112_number_density_cm3_recomputed": recomputed_number_density_sn112,
            "implied_effective_molar_mass_from_manuscript_g_mol": implied_molar_mass_from_manuscript,
            "density_source_ids": ["assumption_density_sn", "local_polarization_rate_formula"],
        },
        "inventory": {
            "sn112_mass_g": inventory_mass_g,
            "sn124_mass_g": inventory_mass_g,
            "practical_disk_diameters_mm": practical_diameters_mm,
            "reference_thicknesses_mm": reference_thicknesses_mm,
        },
        "validation_context": validation_context,
        "external_database_context": {
            "comparison_file_csv": str(results_dir / "external_database_cross_section_comparison.csv"),
            "comparison_file_json": str(results_dir / "external_database_cross_section_comparison.json"),
            "headline_public_total_reaction_comparison_targets": ["Sn112", "Sn124", "Pb208"],
            "headline_public_total_reaction_comparison_energies_mev": [37.9, 65.5, 97.4],
            "note": "Public literature provides same-system total reaction cross sections and related breakup observables, but no exact public 190 MeV/u breakup-integral anchor was located in this package.",
            "source_ids": [
                "external_auce_1996_sigmaR_pdf",
                "external_auce_1996_osti",
                "external_yahiro_1984_118sn_breakup",
                "external_iseri_1988_virtual_breakup",
                "external_yahiro_1987_breakup_400",
                "external_okamura_2010_breakup_208pb",
                "external_kalbach_2017_breakup_systematics",
            ],
        },
        "source_registry": source_map,
    }

    rates_payload = {
        "summary": {
            "nominal_3mm_reference": nominal_3mm,
            "recommended_15mm_disk_reference": nominal_15mm,
        },
        "rows": rates_rows,
        "notes": {
            "coincidence_anchor": "The full-chain detected coincidence rate is anchored to the manuscript's sigma and eta_det.",
            "legacy_singles_proxy": "PDC and NEBULA singles are provisional proxies obtained by rescaling the coincidence anchor with the legacy 1.2T, 5deg acceptance split.",
        },
    }

    target_design_payload = {
        "disk_options": target_design_rows,
        "inventory_backsolve": inventory_backsolve_rows,
        "notes": {
            "manufacturing_context": "The current geometry macros still carry an 80x80x30 mm placeholder target with SetTarget false; manufacturing should follow the inventory-constrained disk table instead.",
            "sn112_caveat": "The Sn112 planning-rate column is only an atom-density scaling placeholder until a dedicated Sn112 ImQMD + Geant4 campaign is extracted for 3deg_1.15T.",
        },
    }
    validation_rate_payload = {
        "summary": {
            "validation_available": validation_summary is not None,
            "validation_summary_path": str(validation_summary_path),
        },
        "rows": validation_rate_rows,
        "notes": {
            "manuscript_sigma_status": "The local validation package remeasures eta_det but does not independently derive sigma = 550 mb.",
        },
    }
    external_database_payload = {
        "summary": {
            "planning_breakup_cross_section_mb": planning_cross_section_mb,
            "comparison_intent": "Compare the manuscript breakup planning anchor to public total reaction datasets and breakup-specific literature anchors.",
            "direct_public_190MeVu_breakup_anchor_found": False,
        },
        "rows": external_database_rows,
        "notes": {
            "sigmaR_scope": "The Auce 1996 values are total reaction cross sections, not breakup cross sections.",
            "breakup_scope": "The breakup-specific public sources below provide qualitative or differential-observable context rather than a directly reusable integrated 190 MeV/u breakup cross section.",
        },
    }

    write_json(results_dir / "inputs.json", inputs_payload)
    write_json(results_dir / "rates.json", rates_payload)
    write_json(results_dir / "target_design.json", target_design_payload)
    write_json(results_dir / "rate_validation_comparison.json", validation_rate_payload)
    write_json(results_dir / "external_database_cross_section_comparison.json", external_database_payload)
    write_csv(results_dir / "rates.csv", rates_rows)
    write_csv(results_dir / "target_design.csv", target_design_rows)
    write_csv(results_dir / "inventory_backsolve.csv", inventory_backsolve_rows)
    write_csv(results_dir / "rate_validation_comparison.csv", validation_rate_rows)
    write_csv(results_dir / "external_database_cross_section_comparison.csv", external_database_rows)

    readme_lines = [
        "# Rate And Target Design Source Registry",
        "",
        f"Generated at UTC: {inputs_payload['generated_at_utc']}",
        "",
        "## Source Roles",
        "",
    ]
    for source in sources:
        readme_lines.append(f"### {source.source_id}")
        readme_lines.append(f"- Category: {source.category}")
        readme_lines.append(f"- Title: {source.title}")
        readme_lines.append(f"- Locator: {source.locator}")
        readme_lines.append(f"- Role: {source.role}")
        readme_lines.append(f"- Note: {source.note}")
        readme_lines.append("")
    readme_lines.extend(
        [
            "## Key Generated Files",
            "",
            f"- `inputs.json`: {results_dir / 'inputs.json'}",
            f"- `rates.csv`: {results_dir / 'rates.csv'}",
            f"- `rates.json`: {results_dir / 'rates.json'}",
            f"- `target_design.csv`: {results_dir / 'target_design.csv'}",
            f"- `target_design.json`: {results_dir / 'target_design.json'}",
            f"- `inventory_backsolve.csv`: {results_dir / 'inventory_backsolve.csv'}",
            f"- `rate_validation_comparison.csv`: {results_dir / 'rate_validation_comparison.csv'}",
            f"- `rate_validation_comparison.json`: {results_dir / 'rate_validation_comparison.json'}",
            f"- `external_database_cross_section_comparison.csv`: {results_dir / 'external_database_cross_section_comparison.csv'}",
            f"- `external_database_cross_section_comparison.json`: {results_dir / 'external_database_cross_section_comparison.json'}",
            f"- `validation_summary.json`: {validation_dir / 'validation_summary.json'}",
            f"- `imqmd_cross_section_estimates.csv`: {validation_dir / 'imqmd_cross_section_estimates.csv'}",
            f"- `imqmd_pb_profile.csv`: {validation_dir / 'imqmd_pb_profile.csv'}",
            f"- `imqmd_breakup_probability_and_cumulative_sigma.png`: {validation_dir / 'imqmd_breakup_probability_and_cumulative_sigma.png'}",
            "",
        ]
    )
    (results_dir / "README_sources.md").write_text("\n".join(readme_lines), encoding="utf-8")


if __name__ == "__main__":
    main()
