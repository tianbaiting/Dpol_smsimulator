# QMD Rawdata Columns

## Scope

This note documents the plain-text QMD rawdata layout used under:

- `data/qmdrawdata/qmdrawdata/allevent/`

It is intended for tasks that read or convert campaign directories such as:

- `d+Sn124-ypolE190g050ynp/`
- `d+Sn124-zpolE190g100zpn/`

## Directory Layout

Each extracted campaign directory contains paired files:

- `dbreakb01.dat` ... `dbreakb10.dat`
- `geminioutb01.dat` ... `geminioutb10.dat`

The `bXX` suffix is the impact-parameter bucket. In the shipped files:

- `b01` corresponds to `BIMP = 1.0 fm`
- `b02` corresponds to `BIMP = 2.0 fm`
- ...
- `b10` corresponds to `BIMP = 10.0 fm`

This mapping is explicit in the file contents, for example:

- `dbreakb02.dat` header includes `b= 2.000fm`
- `geminioutb02.dat` data rows use `BIMP = 2.0000`

## `dbreak*.dat`

### Meaning

`dbreak*.dat` stores proton and neutron momentum for elastic deuteron-breakup events only.

Not every simulated event appears in this file. The file contains only the subset of events where breakup happened.

### Layout

- line 1: annotation string
- line 2: column names
- line 3 onward: one breakup event per line

### Columns

1. `No.`
- breakup event number

2. `pxp(MeV/c)`
- proton momentum x component

3. `pyp(MeV/c)`
- proton momentum y component

4. `pzp(MeV/c)`
- proton momentum z component

5. `pxn(MeV/c)`
- neutron momentum x component

6. `pyn(MeV/c)`
- neutron momentum y component

7. `pzn(MeV/c)`
- neutron momentum z component

## `geminiout*.dat`

### Meaning

`geminiout*.dat` stores the QMD + GEMINI output particles or fragments for each simulated event.

### Read Rules

- line 1 is an annotation and should be skipped
- middle lines are whitespace-separated numeric records
- the all-zero line marks end of data
- the final line is a legend and should not be parsed as data

Typical footer:

```text
0 0 0 0 0 0 0 0 0 0 0
Z N P1 P2 P3 J M WEIGHT BIMP ISIM IFRG
```

### Columns

1. `Z`
- charge number of the emitted particle or fragment

2. `N`
- neutron number of the emitted particle or fragment

3. `P1`
- momentum per nucleon in x, unit `MeV/c`

4. `P2`
- momentum per nucleon in y, unit `MeV/c`

5. `P3`
- momentum per nucleon in z, unit `MeV/c`

6. `J`
- not used by this campaign
- keep the column for format compatibility
- in GEMINI naming this is commonly the spin-like field, but downstream code in this repo should treat it as unused unless a task proves otherwise

7. `M`
- not used by this campaign
- keep the column for format compatibility

8. `WEIGHT`
- event weight
- the campaign README marks it as not used

9. `BIMP`
- impact parameter, effectively in `fm`

10. `ISIM`
- simulated event number

11. `IFRG`
- parent fragment number within the same event

### Important Interpretation

`P1`, `P2`, and `P3` are per-nucleon momenta, not total fragment momentum.

For a fragment with:

- `A = Z + N`

the total fragment momentum should be reconstructed as:

- `Px = A * P1`
- `Py = A * P2`
- `Pz = A * P3`

### Event Grouping

- rows with the same `ISIM` belong to the same simulated event
- rows with the same `ISIM` and `IFRG` belong to the same parent-fragment lineage within that event

This grouping is important when converting QMD/GEMINI text output into ROOT event records.

## Naming Notes

The directory names contain tags such as:

- `E190`
- `ypol`, `zpol`
- `g050`, `g100`
- `ynp`, `ypn`, `znp`, `zpn`

Only `E190` and the `bXX <-> BIMP` mapping are explicitly evidenced by the shipped text files.

The remaining tags appear to encode beam/polarization or campaign-condition labels, but they are not formally documented in the dataset README. Treat those tags as campaign metadata labels unless another project source of truth defines them more precisely.
