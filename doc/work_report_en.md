Data flow description

According to the simulation workflow, the main data flow is as follows:

```mermaid
graph TD
    subgraph "Input and Simulation"
        A[Input file<br/>dbreakb01.root] --> B{Geant4 simulation};
        C[BeamSimTree] --> D[beam data];
    end

    subgraph "PDC tracking detector processing"
        E(FragmentSD) --> F[FragSimData];
    end

    subgraph "Neutron detector processing"
        I(NEBULASD) --> J[NEBULAPla data];
        J --> K[Neutron analysis];
    end

    subgraph "Other"
            G{Required by track reconstruction algorithm} --> H[Output branches<br/>target_*, PDC1*, PDC2*];
    end
    B --> E & I;
```


## Figures / screenshots
The report includes in-repo screenshots (referenced by path):

![Simulation screenshot 1](assets/WORK_REPORT_WEEK/image-2.png)
![Simulation screenshot 2](assets/WORK_REPORT_WEEK/image-3.png)

![Simulation overview](assets/WORK_REPORT_WEEK/image.png)




![Reading simulation data](assets/WORK_REPORT_WEEK/image-1.png)

![Additional figure](assets/WORK_REPORT_WEEK/image-4.png)

![3D geometry image](assets/WORK_REPORT_WEEK/3d05fc0f0bee14720dc36b2a8ed1fda.jpg)

## Next steps
- Finish tuning data structures, especially those related to the target configuration.  
- Become familiar with the output data structures and the APIs used to read them.  
- PDC cathode readout simulation: define how induced charge is estimated, how positions are reconstructed, and how measurement uncertainties are modeled and applied.

---



                







