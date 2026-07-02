# SAMURAI Workshop 2026 Talk Script

This note follows `samuraiworkshop2026.pdf` page by page.  The first part is
the main talk; the remaining pages are backup for questions.

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
bias.  Keep the language as an oriented two-body probe of the isovector field.
End on the question: can this bias be reconstructed at SAMURAI?

## Slide 3 -- How the IVR Signal Is Formed

Use the isovector reorientation schematic.  Walk the three stages: oriented
polarized deuteron, different isovector forces on p and n near the target, and
a bias in the relative momentum after breakup.  Say only that gamma changes the
density dependence of the symmetry energy in ImQMD and therefore the relative
impulse.  Keep the full symmetry-energy formula for backup A.

## Slide 4 -- Why 112Sn and 124Sn?

Both targets have Z = 50, so the Coulomb environment is closely matched rather
than identical.  N - Z changes from 12 to 24, so 124Sn gives the
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

## Slide 6 -- Reconstruction Chain for $R_x$

Use the setup figure to connect the observable to real detector elements.  The
proton goes through the SAMURAI magnet and is measured by the PDC tracking
system.  Say explicitly that the proton input is the target vertex plus the
PDC1/PDC2 track after the magnet; the NN backend gives the proton target
momentum used in $R_x$, and RK field transport is the cross-check.  The neutron
is measured by NEBULA / NEBULA Plus using hit position and ToF.  These two
reconstructed momenta define the event plane event by event.

Make the detector-folding point clear here: proton-side PDC hit resolution plus
air/window/PDC-gas multiple scattering are part of the track response; neutron
timing, hit position, threshold/acceptance, and reconstruction survival are
part of the neutron response.  Close with the chain: PDC track plus NEBULA hit
gives reconstructed proton and neutron momenta, then the reconstructed event
plane, then the sign of rotated Delta p_x and $R_x$.  Do not dwell on y/z-pol
here; the next slide does.

## Slide 7 -- Why Start with y Polarization?

Make the case for y-pol first.  z-pol is the cleaner channel: its polarization
axis lies along the beam and is therefore always inside the reaction plane, so
there is no event-by-event dilution.  But a longitudinally polarized deuteron
beam is hard to produce, hard to transport (spin precession in the beamline),
and hard to monitor ($p_{zz}$ needs absolute, two-angle normalization).  y-pol
is measurable now.  For a tensor-polarized spin-1 beam, the vertical axis
enters through a rank-2 projection onto the reaction plane, giving a
cos(2psi)-like angular dependence.  A naive average over reaction-plane angles
is diluted, but because the reaction plane is reconstructed event by event we
select a favourable psi window and recover the coherent signal.  LRUD
monitoring of $p_{yy}$ is mature and the statistics budget (backup G) easily
affords the angular selection.  Point at the z-pol inset: z-pol also carries a
strong gamma dependence, so it is a valuable later extension, not abandoned.
Close: y-pol
is the realistic first stage.

## Slide 8 -- Proton Momentum Reconstruction: RK and NN Cross-Check

Make one point: the proton momentum is reconstructed from the PDC track, and
the result is cross-checked with two independent reconstruction approaches.
Define the names before pointing at the residuals: RK means charged-particle
transport through the magnetic field, constrained by the target point and the
measured PDC hit/track; NN means a neural-network regressor trained on the same
QMD simulation.  Then say the two methods give consistent core residuals in
the same y-pol kinematic window, while NN reduces the large-tail contribution
and is therefore used for the detector-level $R_x$ result.  Do not spend time
on software architecture.

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
quantities for the observable, event plane, fiducial window, and quality cuts.
Truth remains only for the ideal reference, reconstruction diagnostics, and the
virtual-breakup veto that removes physically impossible generated events.

## Slide 11 -- Main Result: the Detector Changes the Scale, Not the Ordering

This is the key slide.  Show the before/after double panel: ideal model on the
left, final reco-defined reconstructed result on the right, with 112Sn and
124Sn in consistent colours.  State three conclusions: the detector changes the
absolute R_x, the gamma ordering remains visible, and 124Sn keeps the stronger
sensitivity while 112Sn stays a reference.  Emphasise the central result is the
survival of the ordering, not R_reco = R_truth.  Error bars are current MC
statistics only.

## Slide 12 -- Are the Statistics Sufficient?

Answer the natural question right after the main result.  The error bars shown
reflect the current MC sample (about 2300 folded events per gamma), not the
projected beamtime statistics.  Separating the closest adjacent gamma interval
(0.7 to 0.8) at 3-sigma needs about 3.7e3 usable events in the statistical-only
planning convention.  A 16 h run with the current 15 mm-diameter, 1.2 g
124Sn disk-target planning option gives about 2.75e5 usable y-pol events after
applying the current tight-px60 reco survival, roughly 70 times above the
requirement.  Conclude that event counts are not the limiting factor;
neutron acceptance, thresholds, polarization, and backgrounds are the key
systematics.

## Slide 13 -- What Is Ready for the Experiment?

Change the tone from "more simulation is needed" to "the y-pol observable is
ready from the simulation side."  Say that full SAMURAI detector simulation and
reconstruction are in place, the y-pol IVR observable is formed from
reconstructed proton, neutron, and event-plane quantities, and the gamma and
isotope ordering survive detector response.  Then pivot to experiment
preparation: polarimeter, target system, freezing the documented reco-based
analysis configuration, acceptance and background control, polarization
systematics, and commissioning.  The point is
that the next step is experimental readiness, not proving the observable again
in simulation.

## Slide 14 -- Toward the Beamtime: Preparation Timeline

Turn the experiment-preparation list into a concrete schedule.  Full detector
simulation, reconstruction, reco-defined closure, and the y-pol
physics-observable study are done now (2026-07).  The remaining hardware is the
beam polarimeter, needed to monitor the vertical tensor polarization
$p_{yy}$, and the target system --- the target holder plus the $^{112}$Sn and
$^{124}$Sn targets.  In parallel, freeze the documented reco-based analysis
configuration and the systematic budget: acceptance, backgrounds, thresholds,
and polarization uncertainty.
Offline and beam-test commissioning lead into the SAMURAI beamtime, planned for
the end of April 2027.  If asked, the intermediate phasing is a plan and can
shift; only the two anchors (simulation done now, beamtime end of April 2027)
are firm.

## Slide 15 -- Summary

Four points in non-technical language: a polarized deuteron converts an
isovector force difference into a measurable asymmetry; 124Sn is the main
sensitivity and 112Sn the reference; the detector changes the scale but the
gamma and isotope ordering survive; statistics are sufficient, so the next
focus is experimental readiness: polarimetry, targets, commissioning, and
systematic control.  Bottom line: the y-pol measurement is physics-ready from
the simulation side; the next step is preparing the experiment.

## Slide 16 -- Thanks

Pause for questions.  Route detailed questions to the backup sections:
reconstruction performance to backup C, neutron acceptance to backup D, the
truth-cut versus reco-cut closure to backup E, isotope numbers to backup F,
statistics to backup G, y-pol/z-pol and polarization monitoring to backup H, reco closure and
systematics to backup I.

## Backup

Use backup only for expert questions.  The backup keeps the full symmetry-energy
parameterization (A), the exact observable definitions (B), proton/neutron
residuals and RK-versus-NN (C), the neutron acceptance maps, px60/80/100
comparison, and selection migration (D), the truth-cut versus reco-cut closure
(E), the full 112Sn/124Sn tables and bounded asymmetry (F), the statistical
planning scale and survival flow (G), the z-pol extension status and
polarization monitoring (H), and the reco-closure and detector-systematic
checklists (I).
