# SAMURAI Workshop 2026 Talk Script

This note follows `samuraiworkshop2026.pdf` page by page.  The main talk is
pages 1--15.  Pages 16--40 are backup for questions.

## Page 1 -- Title

Open with the central question: can the isovector reorientation signal from a
y-polarized deuteron survive the realistic SAMURAI detector response and full
momentum reconstruction?

Say that the talk is not a catalog of every analysis result.  It is a
detector-level closure story for 112Sn and 124Sn at 190 MeV/u with SAMURAI,
NEBULA, and NEBULA Plus.

## Page 2 -- What Are We Trying to Establish?

Start with the answer before showing details.

The physics question is whether the y-pol IVR signal remains observable after
proton, neutron, and event-plane reconstruction.  The two targets have distinct
roles: 112Sn is the isotope reference, while 124Sn is the primary neutron-rich
sensitivity target.

Stress the scope: the observable is reconstructed, but the current event-quality
selection is still truth-assisted.  This is a detector-level closure study, not
a complete experimental uncertainty analysis.

## Page 3 -- Physical Picture: Isovector Reorientation

Explain IVR from first principles.

A polarized deuteron contains a proton and a neutron.  Near a neutron-rich
target, the two fragments feel different isovector mean-field forces.  Before
breakup this can create a polarization-dependent relative momentum bias.

Define gamma as the density-dependence parameter used in the ImQMD symmetry
energy term.  Do not describe the gamma ordering as a final soft/stiff statement
without specifying the density region.

## Page 4 -- Why Compare Two Tin Isotopes?

Use the table: both tin targets have Z = 50.  The neutron number changes from
62 in 112Sn to 74 in 124Sn.

The point is not to make two separate talks.  124Sn supplies the stronger
neutron-rich response.  112Sn supplies an isotope reference to test whether the
response follows target neutron excess rather than a generic detector or
kinematic effect.

## Page 5 -- Why Start with y Polarization?

Say that y-pol and z-pol are both physically meaningful.  The reason to start
with y-pol is experimental staging.

y-pol currently has the most mature combination of simulation, reconstruction,
detector folding, and polarization monitoring.  The left-right-up-down LRUD
strategy provides a practical p_yy monitoring path.  z-pol remains a second-stage
extension after successful commissioning.

## Page 6 -- Reconstructed IVR Observable

Define the reconstructed event plane from the reconstructed proton and neutron
transverse momenta.  Each event is rotated into that plane, and the observable
uses the sign of the reconstructed proton-neutron relative momentum
Delta p_x.

The measured object is R_x^{reco,fid}, a reconstructed fiducial ratio.  The
model prediction for comparison is the identically folded fiducial model ratio.
Say explicitly that this is not a global truth-level ratio.

## Page 7 -- From ImQMD Events to the Measured Observable

Walk through the chain from left to right:

ImQMD for 112Sn and 124Sn, then Geant4 detector response, PDC proton
reconstruction, NEBULA plus NEBULA Plus neutron reconstruction, reconstructed
event plane, px60 fiducial selection, and R_x^{reco,fid}.

The folding equation is the reason R_reco does not need to equal R_truth.
Acceptance, efficiency, resolution, migration, backgrounds, and reconstruction
are all part of the response kernel.  The correct experimental comparison is
reconstructed data against identically folded model predictions.

## Page 8 -- Defining a Controlled Neutron Fiducial Region

Explain px60 as a fiducial region:
|p_x,n^reco| < 60 MeV/c.

This is not chosen to remove all detector effects.  It selects a region where
positive and negative neutron-p_x branches both have nonzero and reasonably
balanced efficiency.  Quote the current efficiency numbers: about 0.678 and
0.644, pooled over the stated y-pol tight samples.

## Page 9 -- Where Does Detector Folding Change the Observable?

Use the R ladder to show where the shift occurs.  The largest change comes from
neutron visibility and acceptance.  Proton reconstruction is not the dominant
bottleneck, and reconstructed event-plane determination adds a smaller
correction.

The takeaway is the key logic of the talk: the absolute ratio changes, but the
gamma ordering remains.

## Page 10 -- Primary Sensitivity: 124Sn

This is the main result.

Point out the label: current closure selection means truth-assisted tight
event-quality cut plus reconstructed px60 fiducial.  The plotted reconstructed
points use reconstructed proton momentum, reconstructed neutron momentum, and
the reconstructed event plane.

The 124Sn folded observable keeps a monotonic gamma ordering.  Error bars are
current Monte Carlo statistics only.  Do not call this an experimental
significance or a truth-level asymmetry.

## Page 11 -- Isotope Reference: 112Sn versus 124Sn

This slide now shows the same-current-chain y-pol isotope comparison.

Say that 124Sn remains the primary neutron-rich sensitivity target.  Sn112 is
the isotope reference and now has an exported tight-px60 folded-reco table from
the same gamma-constraint workflow.

Read the plot qualitatively: both isotopes retain a visible y-pol gamma ordering
after folding and full momentum reconstruction.  The Sn112 high-gamma interval
is much narrower, so it is more sensitive to detector and model systematics.

Keep the caveat visible: this is still a truth-assisted tight event-quality
selection, and Sn112 planning rates currently use an atom-density scaling
placeholder.

## Page 12 -- Statistical Scale versus Experimental Systematics

Present the statistical estimate as a planning scale only.

The closest adjacent interval in the current 124Sn y-pol folded result is
gamma = 0.7 to 0.8.  The statistical-only scale for a 3-sigma separation is
about 2.8e3 usable events, while the current planning anchor is about 2.75e5
usable y-pol events.

The important message is that statistics are unlikely to be the dominant
limitation.  Detector response, neutron efficiency, polarization, backgrounds,
model dependence, and reco-only selection migration are the real priorities.

## Page 13 -- Current Status and Next Closure Step

Separate established results from required next work.

Established: the y-pol observable is reconstructable; Sn124 gamma ordering
survives detector folding; Sn112 now provides a same-current-chain isotope
reference; px60 provides a controlled fiducial region; folded model comparison
is the right strategy.

Still required: reco-only cuts, efficiency and purity studies, Sn112
target-specific rate validation, polarization uncertainty propagation, and
detector-systematic pseudo-data tests.

## Page 14 -- Summary

Close with four points.

IVR gives a polarization-dependent proton-neutron relative-momentum bias.
124Sn is the primary sensitivity target; 112Sn is the isotope reference.  The
gamma ordering survives realistic detector folding and full momentum
reconstruction in px60 for 124Sn.  The experiment should compare reconstructed
data with identically folded model predictions.

End on the bottom line: this establishes detector-level observability, not yet a
complete experimental uncertainty budget.

## Page 15 -- Thanks

Pause for questions.  If asked about the isotope comparison details, go to
Backup E.  If asked about z-pol, go to Backup F.  If asked about event-plane
reconstruction, go to Backup B.

## Page 16 -- Backup Roadmap

Use this only to orient questions.  The backup is grouped by definitions,
reconstruction, neutron acceptance, Sn124 numbers, isotope status, z-pol,
polarization monitoring, and systematics.

## Page 17 -- Backup A: Observable and Bounded Asymmetry

Show the explicit relation between R_x and the bounded asymmetry A_x.  A_x is a
stability cross-check, not the replacement for the main ratio.

## Page 18 -- Backup A: Folding Vocabulary

Use this if someone asks why truth, reconstructed, folded, and unfolded are kept
separate.  The key point is that the main result uses a reconstructed fiducial
observable.

## Page 19 -- Backup B: Proton and Neutron Residuals

Use this to answer whether the momenta are reconstructed well enough to form the
observable.  Keep the distinction between RMSE and core half-width clear.

## Page 20 -- Backup B: Event-Plane Resolution

Use this to explain why the reaction plane can be selected event by event.  The
event-plane migration is visible but not the leading change in the R ladder.

## Page 21 -- Backup B: RK vs NN Proton Cross-Check

The point is limited: proton reconstruction has been cross-checked, and it is
not the dominant source of the observable shift.

## Page 22 -- Backup B: NN Proton Backend

Use this if someone asks what the NN reconstructs.  It maps PDC hit positions to
target-frame proton momentum.

## Pages 23--28 -- Backup C: Neutron Acceptance

These slides document why px60 was selected and why a naive 1/epsilon unfolding
is not the main strategy.  Near-zero-efficiency regions make inverse corrections
unstable; forward folding is the robust comparison.

## Pages 29--32 -- Backup D: 124Sn Full Numbers

Use these for numerical questions.  The R_x values and A_x values are folded
reco-plane values for 124Sn y-pol tight px60 and carry current MC statistical
uncertainties only.

## Pages 33--34 -- Backup E: 112Sn and Isotope Comparison

Use these to explain both the earlier Sn112 reconstruction-history plots and the
current common-chain audit.  The main isotope comparison now uses the exported
tight-px60 folded-reco table for both Sn112 and Sn124.

## Pages 35--36 -- Backup F: z-pol

Say that z-pol is physically useful and remains a second-stage extension.  Do
not present it as required for the first-stage y-pol feasibility argument.

## Pages 37--38 -- Backup G: Polarization Monitoring

Use these if asked about beam polarization.  p_yy monitoring through LRUD is an
independent prerequisite for the y-pol analysis.  p_zz monitoring is more
complex, not impossible.

## Pages 39--40 -- Backup H: Systematics and Future Closure

These are the final caveat slides.  Current event-quality cuts are
truth-assisted.  The decisive next analysis is a reco-only closure test with
detector and model variations.
