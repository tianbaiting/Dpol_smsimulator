# SAMURAI Workshop 2026 Rehearsal Script

This is a spoken English script for the main talk.  The intended message is:

> The full detector simulation and reconstruction chain has been completed.  The y-polarized deuteron measurement is physics-ready from the simulation side.  The next work is experiment preparation and systematic control.

## Slide 1 -- Title

Good morning.  In this talk I want to address one very concrete question: can SAMURAI measure the isovector reorientation signal from polarized deuteron breakup, after the realistic detector response and reconstruction are included?

The point of the talk is not only that the QMD generator predicts a signal.  That is the starting point.  The real question is whether the signal survives a realistic SAMURAI measurement: proton reconstruction, neutron reconstruction, event-plane reconstruction, acceptance, and statistics.  My conclusion is that, for the y-polarized configuration, the full simulation and reconstruction study is already in place, and the observable remains measurable.  The remaining task is to prepare the experiment and control systematics.

## Slide 2 -- A Polarized Deuteron as an Isovector Probe

Let me first state the physics idea in simple terms.  A deuteron is a proton-neutron two-body system.  If the deuteron is polarized, this two-body system has a preferred orientation.  When it passes close to a neutron-rich target, the proton and neutron do not feel exactly the same nuclear environment.  The difference is the isovector part of the interaction.

After breakup, that difference can appear as a bias in the relative momentum of the proton and neutron.  So the polarized deuteron is not just a projectile.  It is an oriented proton-neutron probe of the isovector field.  The experimental question is whether this small relative-momentum bias can be reconstructed with SAMURAI.

## Slide 3 -- How the IVR Signal Is Formed

This schematic shows the chain of the effect.  Before the interaction, the deuteron has a preferred spin alignment.  Near the target, the proton and neutron components experience different isovector impulses.  After breakup, the two fragments carry a correlated momentum asymmetry.

In the QMD calculation, changing the symmetry-energy parameter changes the density dependence of the isovector field.  That changes the relative impulse and therefore changes the final proton-neutron momentum asymmetry.  This is what I call the isovector reorientation signal.

I want to emphasize that we are not measuring the force event by event.  We measure a statistical momentum asymmetry after breakup, and then ask whether its isotope and gamma dependence survives the detector.

## Slide 4 -- Why 112Sn and 124Sn?

The isotope choice is important.  The two tin targets have the same charge, Z equals 50, so the Coulomb environment is closely matched.  But the neutron excess is very different: 112Sn has N minus Z equal to 12, while 124Sn has N minus Z equal to 24.

That gives us a useful pair.  124Sn should have the stronger neutron-rich isovector response, while 112Sn provides the isotope reference.  On this slide, the ideal generator-level calculation shows the expected ordering with gamma and with isotope.

This ideal result is not the final claim.  It is the benchmark.  The rest of the talk asks a stricter question: after the SAMURAI detector response and reconstruction, does this ordering remain visible?

## Slide 5 -- What Do We Measure?

The observable is intentionally simple.  For each event, we reconstruct the proton momentum and the neutron momentum.  From them, we define the event plane.  Then we rotate the momenta into that event-plane frame and look at the relative momentum component along the selected axis.

In practice, we count whether the proton side or the neutron side wins in this rotated momentum component.  That gives a ratio, R_x.  It is a counting observable, which is useful because it is robust against some absolute normalization effects.

The important point for this talk is that the observable is defined from reconstructed quantities.  We need the proton momentum, the neutron momentum, and the reaction plane.  If any one of those fails, the measurement fails.  So the next slides show that these ingredients have been reconstructed and checked.

## Slide 6 -- Can SAMURAI Measure the Required Information?

Here is the experimental mapping.  The charged proton goes through the SAMURAI magnetic field and is measured by the PDC tracking system.  The neutron is measured by NEBULA and NEBULA Plus, using hit position and time of flight.  Together, the reconstructed proton and neutron momenta define the event plane.

So the analysis chain is direct: proton track plus neutron hit gives reconstructed momenta; reconstructed momenta give the event plane; the event plane gives the rotated relative momentum; and the sign of that relative momentum gives R_x.

This is the first key message.  The observable is not being evaluated with invisible truth information in the detector result.  The detector-level result is built from the reconstructed proton, neutron, and event-plane quantities.

## Slide 7 -- Why Start with y Polarization?

Now let me explain why I focus on y polarization.  In an ideal mathematical sense, z polarization is cleaner.  The polarization axis is approximately along the beam direction, and therefore it is naturally related to the reaction plane.  But experimentally it is harder: a longitudinally polarized deuteron beam is harder to produce, harder to transport because of spin precession, and harder to monitor because p_zz requires an absolute, two-angle normalization.

The y-polarized configuration is the realistic first stage.  The price is that the vertical polarization axis is not fixed relative to the event-by-event reaction plane.  For a tensor-polarized spin-1 beam, the relevant projection is rank two, so the angular dependence is cos(2 psi)-like.  If we simply average over all reaction-plane angles, the signal is diluted.

But this is not a fatal problem, because we reconstruct the reaction plane event by event.  We can select a favorable psi window where the tensor projection keeps the signal.  The statistics are large enough to afford this selection, and LRUD monitoring of p_yy is a mature method.  So y-pol is not the most idealized configuration, but it is the configuration that is experimentally ready.

## Slide 8 -- Proton Momentum Reconstruction: RK and NN Cross-Check

The proton reconstruction is based on the target point and the measured PDC hit or track after the magnet.  We use two independent reconstruction approaches.

The first is RK, which means charged-particle transport through the measured magnetic field.  In other words, we propagate trajectories in the magnetic field and solve for the target momentum that is consistent with the PDC measurement.

The second is NN, a neural-network regression trained on the same QMD-based simulation sample.  The purpose of the NN is not to replace physics with a black box.  The purpose is to provide a stable event-by-event reconstruction in the detector phase space.

The important cross-check is that RK and NN give consistent core residuals in the same y-pol kinematic window.  The NN reconstruction reduces the large-tail contribution, so I use it for the detector-level R_x result.  The RK result is an independent field-transport validation of the proton momentum scale and resolution.

## Slide 9 -- Neutron Momentum Reconstruction

The neutron reconstruction uses NEBULA and NEBULA Plus hit information, especially position and time of flight.  This slide shows the neutron residuals directly.  The reconstruction is not perfect, but the core resolution is narrow enough for the event-plane and momentum-asymmetry analysis.

Here I want to be precise about the limitation.  The issue is not that the intrinsic neutron detection efficiency suddenly changes in an arbitrary way.  The main limitation is the usable detector phase space: geometry, thresholds, hit finding, time of flight, and reconstruction survival.  In particular, the acceptance changes strongly with the neutron transverse momentum.

That is why I do not claim that the detector preserves the absolute scale of R_x.  It does not.  What I claim is that after this neutron reconstruction and acceptance folding, the gamma and isotope ordering of the signal remains visible.

## Slide 10 -- Event-Plane Reconstruction

The reaction plane is reconstructed from the proton and neutron transverse momenta.  Because both momenta have finite resolution, the reconstructed event plane is not exactly the truth plane.  This migration broadens the observable and reduces the contrast.

But the crucial question is whether the event-plane reconstruction randomizes the signal.  The answer from the full reconstruction study is no.  The event-plane migration is present and must be included, but it does not erase the ordering.

In the main analysis, the ideal reference is evaluated at generator level, while the detector-level result uses reconstructed quantities and the same reconstructed fiducial selection.  This is the correct comparison for the experiment: it tells us what SAMURAI would actually see after reconstruction.

## Slide 11 -- Main Result: the Detector Changes the Scale, Not the Ordering

This is the central result of the talk.  The left panel shows the ideal generator-level expectation.  The right panel shows the detector-folded and reconstructed result.

The detector clearly changes the absolute scale.  That is expected, because acceptance, thresholds, reconstruction resolution, and event-plane migration all enter.  But the important point is that the ordering is not destroyed.  The gamma dependence remains visible, and the 124Sn response remains stronger than the 112Sn reference.

If the detector response killed the physics sensitivity, the reconstructed points would collapse into a featureless distribution, or the isotope and gamma ordering would disappear.  That is not what we see.  The signal is reduced, but it survives.  This is the main evidence that the y-pol measurement is feasible with SAMURAI.

The error bars here are current Monte Carlo statistics.  They are not the projected beamtime statistics.  I will address the real statistical scale on the next slide.

## Slide 12 -- Are the Statistics Sufficient?

The natural question is whether the remaining separation is statistically useful.  The current reconstructed points are based on the available Monte Carlo sample, roughly a few thousand folded events per gamma point.  That is why the plotted MC error bars are not the final experimental expectation.

For planning, the closest adjacent gamma interval requires about 2.8 times 10^3 usable events for a three-sigma statistical separation in this convention.  With a 16-hour run on the current 15 mm diameter, 1.2 g 124Sn disk target option, the projected usable y-pol sample after the current tight px60 reconstructed survival is about 2.75 times 10^5 events.

That is about two orders of magnitude above the statistical requirement.  Therefore, I would not describe this measurement as statistics-limited.  The real work is systematic control: neutron acceptance, thresholds, background, polarization monitoring, and the final analysis cuts.

This is an important change in emphasis.  We do not need to keep proving that a signal exists in simulation.  The simulation and reconstruction show that it survives.  The next question is how well we control the experimental systematics.

## Slide 13 -- What Is Ready for the Experiment?

Let me summarize what has already been done.  We have the QMD input.  We have the SAMURAI detector simulation.  We have proton reconstruction, neutron reconstruction, and event-plane reconstruction.  We have the detector-folded y-pol observable, built from reconstructed quantities.  And we have checked that the gamma and isotope ordering survives the detector response.

So, from the simulation side, the y-pol observable is ready.  I do not mean that there are no systematic uncertainties.  Of course there are.  I mean that the full detector and reconstruction chain has been exercised up to the physics observable, and the observable remains sensitive.

The next step is therefore experimental preparation: the beam polarimeter, the target system, the final analysis selection, the acceptance and background model, and the polarization systematic budget.  These are experiment-readiness tasks, not missing proof that the observable can be reconstructed.

## Slide 14 -- Toward the Beamtime: Preparation Timeline

This timeline reflects that distinction.  The full detector simulation, reconstruction, and y-pol physics-observable study are done now.  The remaining tasks are the hardware and systematic-control tasks needed for beamtime.

The beam polarimeter is needed for reliable p_yy monitoring.  The target system must be finalized for both 112Sn and 124Sn.  In parallel, we should freeze the fiducial cuts and define the systematic budget for acceptance, backgrounds, thresholds, and polarization uncertainty.

Then the offline and beam-test commissioning should lead into the SAMURAI beamtime.  The message is simple: the project is moving from feasibility simulation into experiment preparation.

## Slide 15 -- Summary

Let me close with four points.

First, a polarized deuteron provides an oriented proton-neutron probe of the isovector field.  Second, 124Sn gives the main neutron-rich sensitivity, while 112Sn provides an isotope reference with closely matched charge.  Third, after the full SAMURAI detector simulation and reconstruction, the absolute scale of R_x changes, but the gamma and isotope ordering survive.  Fourth, the projected y-pol statistics are sufficient; the next focus is systematic control and beamtime preparation.

So my conclusion is this: the y-polarized measurement is physics-ready from the simulation side.  The full simulation and reconstruction chain has been completed to the final observable.  The task now is not to restart the simulation argument, but to prepare the experiment carefully enough to make the measurement convincing.

Thank you.  I am happy to discuss the reconstruction, acceptance, and systematic checks in the backup slides.

## Short Closing Version

If time is short, use this closing paragraph:

The main result is not that the detector leaves the observable unchanged.  It does not.  The main result is that after realistic detector response and reconstruction, the ordering that carries the physics information remains visible.  With the projected y-pol statistics, the measurement is not limited by event count.  It is now an experiment-preparation problem: polarimetry, targets, acceptance, backgrounds, and systematic control.

## Backup Answers for Likely Questions

### If asked: "What do you mean by full simulation?"

I mean the complete chain needed for the detector-level observable: QMD event input, SAMURAI detector response, proton reconstruction through the magnetic spectrometer, neutron reconstruction with NEBULA and NEBULA Plus, event-plane reconstruction, reconstructed fiducial cuts, and the final R_x observable.  The conclusion is based on the observable after this chain, not on generator-level truth alone.

### If asked: "Why y-pol instead of z-pol?"

z-pol is cleaner geometrically, but it is not the most practical near-term beam configuration because production, spin transport, and p_zz monitoring are harder.  y-pol is experimentally accessible now.  Its signal has an event-plane angular dependence, but the reaction plane is reconstructed event by event, so we can select the favorable angular window.  The projected statistics are large enough to pay that cost.

### If asked: "Where does the cos(2 psi) dependence come from?"

It comes from tensor polarization.  A spin-1 tensor polarization is a rank-2 object, so the projection of a fixed polarization axis onto the event plane has a second-harmonic angular structure, schematically cos(2 psi).  The exact sign or phase depends on the angle convention, but the important point is the rank-2, twofold angular dependence.  A naive average over all reaction-plane angles dilutes it, while event-by-event reaction-plane reconstruction lets us select the useful region.

### If asked: "Is the NN reconstruction a black box?"

The NN is used as a regression backend for stable event-by-event proton momentum reconstruction.  It is trained and evaluated in the same simulation framework and kinematic window.  The important validation is that it is cross-checked against RK field transport, which is an independent reconstruction based on charged-particle motion through the measured magnetic field.  The two agree in the core residuals, while NN reduces large tails in the selected y-pol window.

### If asked: "Is neutron efficiency the problem?"

I would phrase it as neutron acceptance and usable phase space, not as a changing intrinsic efficiency.  NEBULA and NEBULA Plus reconstruct the neutron from hit position and time of flight.  The limitation is that geometry, thresholds, hit finding, and reconstruction survival depend strongly on neutron transverse momentum.  That changes the absolute R_x scale, but the reconstructed study shows that the physics ordering survives.

### If asked: "Are the statistics enough after the psi selection?"

Yes, in the current planning estimate.  The closest gamma separation needs about 2.8e3 usable events for a three-sigma statistical separation in the statistical-only convention.  The projected 16-hour 124Sn y-pol run gives about 2.75e5 usable events after the current tight px60 reconstructed survival.  So statistics are not the dominant concern; systematic control is.

### If asked: "What is still missing?"

What remains is not the basic simulation proof.  What remains is experimental preparation: p_yy polarimetry, target system readiness, final fiducial-cut freeze, neutron acceptance and threshold systematics, background estimates, and commissioning.  These are exactly the tasks expected before a beamtime.
