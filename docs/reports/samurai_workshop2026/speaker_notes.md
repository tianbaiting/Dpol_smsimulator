# SAMURAI Workshop 2026 Talk Script

This note follows `samuraiworkshop2026.pdf` page by page.  The main talk is
the title plus 10 content slides and a thanks page.  The remaining pages are
backup for questions.

The deck is now organised around one question: can a polarized deuteron turn a
proton--neutron isovector force difference into a measurable breakup momentum
asymmetry, and does this signal survive the realistic SAMURAI detector?

## Slide 1 -- Title

Open with the central question in plain language: can SAMURAI observe isovector
reorientation?  This is a detector-level feasibility study with y-polarized
deuterons on 112Sn and 124Sn.  Do not mention reconstruction, px60, or folding
on the title page.

## Slide 2 -- A Polarized Deuteron as an Isovector Probe

Build intuition only.  A deuteron is a bound proton--neutron pair; polarization
gives it a preferred orientation; near a neutron-rich target the two fragments
feel different isovector forces; after breakup this becomes a relative-momentum
bias.  Use the "femtoscopic two-particle compass" line sparingly.  End on the
question: can this bias be reconstructed at SAMURAI?

## Slide 3 -- How the IVR Signal Is Formed

Use the isovector reorientation schematic.  Walk the three stages: oriented
polarized deuteron, different isovector forces on p and n near the target, and
a bias in the relative momentum after breakup.  Say only that gamma changes the
density dependence of the symmetry energy in ImQMD and therefore the relative
impulse.  Keep the full symmetry-energy formula for backup A.

## Slide 4 -- Why 112Sn and 124Sn?

Both targets have Z = 50.  N - Z changes from 12 to 24, so 124Sn gives the
stronger neutron-rich response and 112Sn is the isotope reference.  Show the
ideal (generator-level) prediction: a clear gamma and isotope ordering.  State
explicitly that the rest of the talk asks whether this ideal ordering survives
a realistic measurement.

## Slide 5 -- What Do We Measure?

Define the observable in words: define a reaction plane from the proton and
neutron directions, rotate into it, and count whether the proton or neutron
gets the larger momentum component along the selected axis.  Show only the two
formulae for Delta p_x and R_x.  Stress that we do not measure the force event
by event.  Say the proton momentum, neutron momentum, and event plane are all
reconstructed; keep the atan2 and rotation details for backup B.

## Slide 6 -- Can SAMURAI Measure the Required Information?

Use the setup figure to connect the observable to real detector elements.  The
proton goes through the SAMURAI magnet and is measured by the PDC tracking
system; the neutron is measured by NEBULA / NEBULA Plus using hit position and
ToF.  These two reconstructed momenta define the event plane event by event.
Close with the chain: proton track plus neutron hit gives reconstructed momenta
and event plane, then the sign of rotated Delta p_x gives R_x.  Do not dwell on
y/z-pol here; the next slide does.

## Slide 7 -- Why Start with y Polarization?

Make the case for y-pol first.  z-pol is the cleaner channel: its polarization
axis lies along the beam and is therefore always inside the reaction plane, so
there is no event-by-event dilution.  But a longitudinally polarized deuteron
beam is hard to produce, hard to transport (spin precession in the beamline),
and hard to monitor ($p_{zz}$ needs absolute, two-angle normalization).  y-pol
is measurable now: its vertical axis makes the signal modulate as cos(2psi)
with the angle psi to the reaction plane, which dilutes a naive average, but
because the reaction plane is reconstructed event by event we select a
favourable psi window and recover the coherent signal.  LRUD monitoring of
$p_{yy}$ is mature and the statistics budget (backup G) easily affords the
angular selection.  Point at the z-pol inset: z-pol also carries a strong gamma
dependence, so it is a valuable later extension, not abandoned.  Close: y-pol
is the realistic first stage.

## Slide 8 -- Proton Momentum Reconstruction: RK and NN Cross-Check

Make one point: the proton momentum is reconstructed from the PDC track, and
the result is cross-checked with two independent reconstruction approaches.
Use the residual figure rather than backend details.  Say RK and NN have
consistent core residuals in the same QMD kinematic window; the NN backend is
used for the detector-level $R_x$ result because it reduces the large-tail
contribution.  Do not spend time on architecture.

## Slide 9 -- Neutron Momentum Reconstruction

Make one point: the neutron momentum is reconstructed from NEBULA / NEBULA Plus
hit position and ToF.  Use the one-dimensional residual distributions, not the
2D scatter plot.  Avoid saying the intrinsic detection efficiency changes
strongly.  Say the usable phase space is set by geometry, hit finding, ToF, and
reconstruction survival.

## Slide 10 -- Event-Plane Reconstruction

Make one point: the event plane is built from reconstructed proton and neutron
transverse momenta.  Reaction-plane migration broadens the observable but does
not erase the gamma ordering.  State the comparison strategy: the ideal
reference uses QMD truth quantities; the detector result uses reconstructed
quantities for the observable and fiducial momentum window.  Keep the caveat
explicit: the event-quality class is still truth-defined in this version, with
reco-defined closure remaining.

## Slide 11 -- Main Result: the Detector Changes the Scale, Not the Ordering

This is the key slide.  Show the before/after double panel: ideal model on the
left, folded reconstructed on the right, with 112Sn and 124Sn in consistent
colours.  State three conclusions: the detector changes the absolute R_x, the
gamma ordering remains visible, and 124Sn keeps the stronger sensitivity while
112Sn stays a reference.  Emphasise the central result is the survival of the
ordering, not R_reco = R_truth.  Error bars are current MC statistics only.

## Slide 12 -- Are the Statistics Sufficient?

Answer the natural question right after the main result.  The error bars shown
are limited by the MC sample (about 2300 folded events per gamma), not by
physics.  Separating the closest adjacent gamma interval (0.7 to 0.8) at
3-sigma needs about 2.8e3 usable events.  A 16 h beamtime on a 15 mm 124Sn
target delivers about 2.75e5 usable y-pol events --- roughly a factor of 100
above the requirement, so that interval is measured at the ~30-sigma level and
the real-data error bars will be about an order of magnitude smaller than those
on the slide.  Conclude that statistics are comfortable; the real limitation is
systematics --- neutron acceptance, thresholds, polarization, and backgrounds ---
not event counts.  Be explicit that this is a statistical-only estimate.

## Slide 13 -- What Is Established and What Is Not Yet?

Separate established results from required next work.  Established: the
observable is reconstructed, folding changes the absolute ratio, the gamma and
isotope ordering survive, 124Sn is the primary target.  Still required:
reconstructed event-quality cuts, efficiency/purity/migration, 112Sn rate
validation, polarization uncertainty, neutron threshold and backgrounds, and
pseudo-data closure.  Give the honest scope once, clearly: the observable and
event plane are reconstructed but the quality class is still truth-defined, so
this is a detector-level closure study.

## Slide 14 -- Toward the Beamtime: Preparation Timeline

Turn the "still required" list into a concrete schedule.  Full simulation and
the detector-level closure are done now (2026-07).  The remaining hardware is
the beam polarimeter, needed to monitor the vertical tensor polarization
$p_{yy}$, and the target system --- the target holder plus the $^{112}$Sn and
$^{124}$Sn targets.  In parallel, run the reco-defined selection closure and
the detector-systematic studies.  Offline and beam-test commissioning lead
into the SAMURAI beamtime, planned for the end of April 2027.  If asked, the
intermediate phasing is a plan and can shift; only the two anchors (simulation
done now, beamtime end of April 2027) are firm.

## Slide 15 -- Summary

Four points in non-technical language: a polarized deuteron converts an
isovector force difference into a measurable asymmetry; 124Sn is the main
sensitivity and 112Sn the reference; the detector changes the scale but the
gamma and isotope ordering survive; the next step is a fully reconstructed
closure and detector-systematic validation.  Bottom line: a y-polarized
deuteron is a realistic first-stage path to testing IVR at SAMURAI.

## Slide 16 -- Thanks

Pause for questions.  Route detailed questions to the backup sections:
reconstruction performance to backup C, neutron acceptance to backup D, the
folding ladder to backup E, isotope numbers to backup F, statistics to backup G,
y-pol/z-pol and polarization monitoring to backup H, remaining closure and
systematics to backup I.

## Backup

Use backup only for expert questions.  The backup keeps the full symmetry-energy
parameterization (A), the exact observable definitions (B), proton/neutron
residuals and RK-versus-NN (C), the neutron acceptance maps and px60/80/100
comparison (D), the folding ladder and matrix equation (E), the full 112Sn/124Sn
tables and bounded asymmetry (F), the statistical planning scale and survival
flow (G), the z-pol folded results and polarization monitoring (H), and the
truth-assisted-cut and detector-systematic checklists (I).
