# SAMURAI Workshop 2026 Speak Memo

Use this as a rehearsal script, not as written proceedings.  The goal is to sound calm, direct, and technically mature.

## Memory Spine

Memorize these five sentences first.  They are the backbone of the talk.

1. The question is whether the IVR signal survives the realistic SAMURAI detector and reconstruction chain.
2. The detector-level observable is fully reconstructed: proton, neutron, event plane, and reco-defined cuts.
3. The detector changes the absolute scale of $R_x$, but it does not erase the gamma and isotope ordering.
4. The y-pol configuration is experimentally realistic now, and the statistics are sufficient after the angular selection.
5. The remaining work is beamtime preparation and systematic control, not proving the observable again in simulation.

## Main Script

### Slide 1 -- Title

Cue: state the question.

Good afternoon.  I want to answer one concrete question: after the full SAMURAI detector response and reconstruction are included, can we still observe the isovector reorientation signal from polarized deuteron breakup?

My answer is yes, for the y-polarized configuration.  The full chain has been exercised up to the detector-level observable.  The remaining task is no longer to prove that the observable exists in simulation.  The remaining task is to prepare the experiment and control the systematics.

### Slide 2 -- A Polarized Deuteron as an Isovector Probe

Cue: build intuition.

A deuteron is a proton-neutron two-body system.  When it is polarized, that two-body system has a preferred orientation.  Near a neutron-rich target, the proton and neutron parts do not feel exactly the same nuclear environment.  That difference is the isovector part of the interaction.

After breakup, the difference can appear as a bias in the relative momentum of the proton and neutron.  So the polarized deuteron is an oriented proton-neutron probe of the isovector field.

### Slide 3 -- How the IVR Signal Is Formed

Cue: three-step mechanism.

This slide shows the mechanism in three steps.  First, the incoming deuteron has a preferred spin alignment.  Second, near the target, the proton and neutron receive different isovector impulses.  Third, after breakup, that difference becomes a momentum asymmetry.

In QMD, changing the symmetry-energy parameter changes the density dependence of the isovector field.  Therefore it changes the relative impulse, and finally the measured proton-neutron momentum asymmetry.  We do not measure the force event by event.  We measure a statistical asymmetry and ask whether its gamma dependence survives the detector.

### Slide 4 -- What Do We Measure?

Cue: define $R_x$ in words.

The observable is a counting ratio.  For each event, we reconstruct the proton momentum and neutron momentum.  These two momenta define the event plane.  Then we rotate into that plane and look at the relative momentum along the selected axis.

We count events where the proton side is positive and events where it is negative, and form $R_x$.  This is useful because the final observable is not a complicated fit to individual forces.  It is a reconstructed event-by-event momentum-asymmetry count.

### Slide 5 -- Why 112Sn and 124Sn?

Cue: isotope control.

The two tin targets give a clean comparison.  They have the same charge, $Z=50$, so the Coulomb environment is closely matched.  But their neutron excess is different: $N-Z=12$ for $^{112}$Sn and $N-Z=24$ for $^{124}$Sn.

This makes $^{124}$Sn the main neutron-rich sensitivity target, while $^{112}$Sn is the isotope reference.  The ideal generator-level result shows the expected gamma and isotope ordering.  The real question is whether this ordering remains after SAMURAI acceptance and reconstruction.


### Slide 6 -- Why Start with y Polarization?

Cue: y-pol is practical.

In an ideal geometry, z-pol is cleaner because the polarization axis is approximately along the beam direction.  But experimentally it is harder to produce, transport, and monitor.  In particular, $p_{zz}$ monitoring requires a more demanding absolute normalization.

y-pol is the practical first measurement.  The vertical tensor axis gives a rank-2 projection onto the event plane, so the angular dependence is $\cos(2\psi)$-like.  A naive average over all reaction-plane angles would dilute the signal.

But we reconstruct the reaction plane event by event.  That lets us select a favorable $\psi$ window.  The projected statistics can afford this selection, and LRUD monitoring of $p_{yy}$ is mature.  So y-pol is not the most idealized case, but it is the experimentally realistic one now.

### Slide 7 -- Reconstruction Chain for $R_x$

Cue: map observable to detectors.

The required information maps directly onto SAMURAI.  The proton is bent by the SAMURAI magnet and measured by the PDC tracking system.  The input is the known target vertex plus the PDC1 and PDC2 track after the magnet.  From that I reconstruct the target-frame proton momentum.  The detector-level observable uses the NN proton backend, with RK field transport as the independent cross-check.

The neutron is reconstructed from NEBULA and NEBULA Plus hit position and time of flight.  These two reconstructed momenta define the event plane event by event.

The important point for this slide is that this is a detector-folded chain.  On the proton side, PDC hit resolution and air, window, and PDC-gas multiple scattering are included in the simulated track response.  On the neutron side, timing, hit position, thresholds, acceptance, and reconstruction survival are included.  The detector-level $R_x$ is therefore not evaluated with hidden truth information; it uses reconstructed proton momentum, reconstructed neutron momentum, reconstructed event plane, and reco-defined fiducial and quality cuts.  Truth remains only for the ideal reference, diagnostics, and the virtual-breakup veto.


### Slide 8 -- Proton Momentum Reconstruction

Cue: two independent proton reconstructions.

The proton momentum is constrained by the target point and the measured PDC hit or track after the magnet.  I use two independent reconstruction approaches.

RK means charged-particle transport through the magnetic field, solving for the target momentum consistent with the PDC measurement.  NN means a neural-network regression trained within the same simulation framework.

The important point is the cross-check.  RK and NN agree in the core residuals in the y-pol kinematic window.  NN reduces the large tails, so it is used for the detector-level $R_x$.  RK remains an independent field-transport validation.

### Slide 9 -- Neutron Momentum Reconstruction

Cue: neutron limitation is acceptance, not arbitrary efficiency.

The neutron momentum is reconstructed from NEBULA and NEBULA Plus hit position and time of flight.  The residuals show that the core reconstruction is narrow enough for the event-plane and momentum-asymmetry analysis.

The limitation should be described correctly.  It is mainly usable phase space: geometry, thresholds, hit finding, time of flight, and reconstruction survival.  The acceptance changes strongly with neutron transverse momentum.  That changes the absolute scale of $R_x$, but it does not by itself destroy the physics ordering.

### Slide 10 -- Event-Plane Reconstruction

Cue: migration is included.

The event plane is built from reconstructed proton and neutron transverse momenta.  Because both momenta have finite resolution, the reconstructed plane migrates relative to the generator-level plane.

This migration broadens the observable and reduces contrast.  But the important test is whether it randomizes the physics signal.  It does not.  The detector-level result uses reconstructed quantities and reco-defined cuts, and the gamma ordering remains visible.

### Slide 11 -- Main Result

Cue: detector changes scale, not ordering.

This is the central result.

The detector does not leave $R_x$ unchanged, and we should not expect it to.  Acceptance, thresholds, reconstruction resolution, neutron phase space, and event-plane migration all change the absolute scale.

The decisive question is different: does the physics ordering survive?

It does.  The gamma dependence remains visible, and $^{124}$Sn remains the stronger neutron-rich response compared with $^{112}$Sn.  If the detector killed the sensitivity, these reconstructed points would collapse into a featureless distribution.  That is not what we see.

### Slide 12 -- Are the Statistics Sufficient?

Cue: not statistics-limited.

The error bars on the previous slide are current Monte Carlo statistics, not the projected beamtime statistics.

For planning, the closest adjacent gamma separation needs about $3.7\times10^3$ usable events for a three-sigma statistical separation.  With a 16-hour run on the 15 mm-diameter, 1.2 g $^{124}$Sn disk target option, the projected y-pol sample after the current reconstructed survival is about $2.75\times10^5$ usable events.

That is roughly 70 times above the statistical requirement.  So I would not describe the y-pol measurement as statistics-limited.  The real work is systematic control: neutron acceptance, thresholds, backgrounds, and polarization monitoring.

### Slide 13 -- What Is Ready for the Experiment?

Cue: simulation side is ready.

Let me separate what is done from what remains.  What is done is the detector-level physics chain: QMD input, SAMURAI detector response, proton reconstruction, neutron reconstruction, event-plane reconstruction, reco-defined cuts, and the final $R_x$ observable.

The result is not only generator-level.  The y-pol observable has been formed after reconstruction, and the gamma and isotope ordering survive.  Therefore, from the simulation side, the y-pol observable is established.

What remains is experimental preparation: $p_{yy}$ polarimetry, the $^{112}$Sn and $^{124}$Sn target system, the frozen analysis configuration, and the systematic budget.

### Slide 14 -- Toward the Beamtime

Cue: this is experiment readiness.

This timeline is not a plan to finish the simulation.  That part is done: full detector simulation, reconstruction, reco-defined closure, and the y-pol observable study.

The remaining work is the normal preparation for a real beamtime.  We need the beam polarimeter for $p_{yy}$, the tin target system, commissioning, and a systematic budget for acceptance, thresholds, backgrounds, and polarization.  In other words, the project has moved from feasibility simulation to experiment readiness.

### Slide 15 -- Summary

Cue: close with four points.

Let me close with four points.

First, a polarized deuteron converts an isovector proton-neutron force difference into a measurable breakup momentum asymmetry.  Second, $^{124}$Sn provides the main neutron-rich sensitivity, with $^{112}$Sn as the isotope reference.  Third, after the full SAMURAI detector simulation and reconstruction, the absolute scale of $R_x$ changes, but the gamma and isotope ordering survive.  Fourth, y-pol statistics are sufficient; the limiting issues are systematic and experimental.

So the conclusion is simple.  The y-pol measurement is physics-ready from the simulation side.  The next step is to prepare the experiment carefully enough to make the measurement convincing.

Thank you.

## Short Version

If time is short, compress the conclusion to this:

The detector does not leave the observable unchanged.  The important result is that the ordering that carries the physics information survives the realistic detector and reconstruction chain.  The y-pol statistics are sufficient, so the next task is not more proof-of-principle simulation.  It is polarimetry, targets, commissioning, and systematic control.

## Q&A Bank

### What do you mean by full simulation?

The full detector-level chain: QMD input, SAMURAI detector response, proton reconstruction, neutron reconstruction, event-plane reconstruction, reco-defined fiducial and quality cuts, and the final $R_x$ observable.  The detector result is not generator truth.  Truth is used for the ideal reference, diagnostics, and the virtual-breakup veto.

### Why y-pol first?

z-pol is geometrically cleaner, but harder as a near-term beam configuration because production, spin transport, and $p_{zz}$ monitoring are more demanding.  y-pol is available now.  Its tensor signal has a reaction-plane angular dependence, but the event plane is reconstructed event by event, and the projected statistics can afford the favorable angular selection.

### Where does $\cos(2\psi)$ come from?

It comes from tensor polarization.  A spin-1 tensor polarization is a rank-2 object.  A fixed vertical tensor axis projected onto the event plane naturally gives a second-harmonic, $\cos(2\psi)$-like angular dependence.  The exact sign depends on the angle convention; the important point is the rank-2 twofold structure.

### Is the NN reconstruction a black box?

The NN is a regression backend for proton momentum reconstruction.  It is validated against RK field transport, which uses charged-particle motion through the magnetic field and the PDC measurement.  RK and NN agree in the core residuals; NN gives smaller large-tail contributions in the selected y-pol window.

### Is neutron efficiency the main problem?

I would call it neutron acceptance and usable phase space.  The intrinsic counter efficiency is not the point.  Geometry, thresholds, hit finding, ToF, and reconstruction survival vary across neutron transverse momentum.  This changes the absolute $R_x$ scale, but the reconstructed study shows that the gamma and isotope ordering survive.

### Why not put the target outside the magnet at 0 degrees?

The 0-degree target-outside option does not give a common detector working point.  On the proton side, weak fields leave the tracks too close to the exit direction and force the PDC far off axis, while strong fields over-bend the tracks and split the useful $p_x$ range.  On the neutron side, the old geometric scan gave about 69% NEBULA acceptance at 0 degrees compared with about 85% near the 5-degree reference.  Moving the PDC cannot fix the neutron acceptance, because it is set mainly by the beam/target angle.

### What configuration do we use?

The baseline is the target-inside SAMURAI configuration: 1.15 T field, target at $(-12,0,-1069)$ mm, beam axis rotated by 3 degrees, one exit window removed, and the polarimeter placed before the magnet.  This is the common working point used for the detector-level reconstruction and acceptance studies.

### Are statistics enough?

Yes for the current planning estimate.  The closest gamma separation needs about $2.8\times10^3$ usable events for a three-sigma statistical separation, while the 16-hour $^{124}$Sn y-pol estimate gives about $2.75\times10^5$ usable events.  The measurement is therefore systematic-limited rather than statistics-limited.

### What is still missing?

Not the basic simulation proof, and not reco closure.  What remains is experiment preparation: $p_{yy}$ polarimetry, target readiness, commissioning, neutron acceptance and threshold systematics, background estimates, and the final systematic budget.
