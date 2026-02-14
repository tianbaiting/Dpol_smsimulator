# ---
# jupyter:
#   jupytext:
#     cell_metadata_filter: -all
#     formats: ipynb,py:percent
#     notebook_metadata_filter: kernelspec,language_info,jupytext
#     text_representation:
#       extension: .py
#       format_name: percent
#       format_version: '1.3'
#       jupytext_version: 1.19.1
#   kernelspec:
#     display_name: .venv
#     language: python
#     name: python3
#   language_info:
#     codemirror_mode:
#       name: ipython
#       version: 3
#     file_extension: .py
#     mimetype: text/x-python
#     name: python
#     nbconvert_exporter: python
#     pygments_lexer: ipython3
#     version: 3.13.5
# ---

# %% [markdown]
# ## Same cut as get ratio
#
# Display pxp - pxn plots for different gamma values

# %%
import os
import numpy as np
import math
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d import Axes3D
import plotly.graph_objects as go
import plotly.express as px
from plotly.subplots import make_subplots
import plotly.offline as pyo

# %%
# Install necessary packages if not already installed
import subprocess
import sys

def install_package(package):
    try:
        __import__(package)
    except ImportError:
        print(f"Installing {package}...")
        subprocess.check_call([sys.executable, "-m", "pip", "install", package])

# Check and install necessary packages
install_package("plotly")
install_package("nbformat")
install_package("ipywidgets")

# Set plotly renderer to static HTML to avoid Jupyter display issues
import plotly.io as pio
pio.renderers.default = "browser"

# %%
# Get SMSIMDIR environment variable
smdir = os.getenv('SMSIMDIR')


# %% [markdown]
# Calculate y polarization first

# %%
pols = ["ynp", "ypn"]
count_px_pbiggger = 0
count_px_nbiggger = 0
bmin = 5
bmax = 10
targets = ["Pb208", "Xe130"]  # Only Pb208 and Xe130 targets
R_list = []
gammas = ["050", "060", "070", "080"]

# Initialize data storage dictionaries
pxp_values = {gamma: [] for gamma in gammas}
pxn_values = {gamma: [] for gamma in gammas}
phi_values = {gamma: [] for gamma in gammas}
rpphi_values = {gamma: [] for gamma in gammas}

# Store 3D momentum data
momentum_data = {
    'proton': {target: {gamma: {'px': [], 'py': [], 'pz': []} for gamma in gammas} for target in targets},
    'neutron': {target: {gamma: {'px': [], 'py': [], 'pz': []} for gamma in gammas} for target in targets}
}

# Total 3D momentum data (all targets and gammas)
total_momentum_data = {
    'proton': {'px': [], 'py': [], 'pz': []},
    'neutron': {'px': [], 'py': [], 'pz': []}
}

for target in targets:
    for gamma in gammas:
        for p in pols:
            # Build file path relative to SMDIR
            folder = os.path.join(smdir, f"QMDdata/rawdata/y_pol/phi_random/d+{target}E190g{gamma}{p}/")
            filename = f"dbreak.dat"
            filepath = os.path.join(folder, filename)

            # Check if file exists and open
            if not os.path.isfile(filepath):
                print(f"Warning: File not found: {filepath}")
                continue  # Skip non-existent files

            with open(filepath, 'r') as inputfile:
                # Read file header
                info = inputfile.readline().strip()
                header = inputfile.readline().strip()

                # Read data
                for line in inputfile:
                    data = line.split()
                    if len(data) < 7:
                        continue  # Skip incorrectly formatted lines
                    eventNo = int(data[0])

                    pxp_orig, pyp_orig, pzp_orig = map(float, data[1:4])
                    pxn_orig, pyn_orig, pzn_orig = map(float, data[4:7])

                    b, rpphi_deg = map(float, data[7:9])

                    # Create initial 3D vectors using numpy arrays
                    pp_orig = np.array([pxp_orig, pyp_orig, pzp_orig], dtype=float)
                    pn_orig = np.array([pxn_orig, pyn_orig, pzn_orig], dtype=float)

                    # Calculate azimuthal angle (phi) of sum momentum
                    vec_sum_orig = pp_orig + pn_orig
                    phi_for_rotation = np.arctan2(vec_sum_orig[1], vec_sum_orig[0])

                    # First condition check
                    condition1_part1 = abs(pyp_orig - pyn_orig) < 150
                    condition1_part2 = (vec_sum_orig[0]**2 + vec_sum_orig[1]**2) > 2500
                    
                    if condition1_part1 and condition1_part2:
                        # Rotation operation
                        angle_rad = -phi_for_rotation
                        cos_a = np.cos(angle_rad)
                        sin_a = np.sin(angle_rad)
                        
                        R_z = np.array([
                            [cos_a, -sin_a, 0],
                            [sin_a,  cos_a, 0],
                            [0,      0,     1]
                        ])

                        # Rotate original momentum vectors
                        pp_rotated = R_z @ pp_orig
                        pn_rotated = R_z @ pn_orig

                        # Update momentum components
                        pxp, pyp, pzp = pp_rotated[0], pp_rotated[1], pp_rotated[2]
                        pxn, pyn, pzn = pn_rotated[0], pn_rotated[1], pn_rotated[2]

                        # Second condition check
                        condition2_part1 = (pxp + pxn) < 200
                        condition2_part2 = (np.pi - abs(phi_for_rotation)) < 0.2
                        
                        if condition2_part1 and condition2_part2:
                            # Store rotated momentum data (for Pb208 compatibility)
                            if target == "Pb208":
                                vector_total_rotated = np.array([pxp + pxn, pyp + pyn, pzp + pzn])
                                phi_to_store = np.arctan2(vector_total_rotated[1], vector_total_rotated[0])
                                
                                phi_values[gamma].append(phi_to_store)
                                rpphi_values[gamma].append(rpphi_deg * np.pi / 180.0)
                                pxp_values[gamma].append(pxp)
                                pxn_values[gamma].append(pxn)
                            
                            # Store 3D momentum data (all targets)
                            momentum_data['proton'][target][gamma]['px'].append(pxp)
                            momentum_data['proton'][target][gamma]['py'].append(pyp)
                            momentum_data['proton'][target][gamma]['pz'].append(pzp)
                            
                            momentum_data['neutron'][target][gamma]['px'].append(pxn)
                            momentum_data['neutron'][target][gamma]['py'].append(pyn)
                            momentum_data['neutron'][target][gamma]['pz'].append(pzn)
                            
                            # Store total 3D momentum data
                            total_momentum_data['proton']['px'].append(pxp)
                            total_momentum_data['proton']['py'].append(pyp)
                            total_momentum_data['proton']['pz'].append(pzp)
                            
                            total_momentum_data['neutron']['px'].append(pxn)
                            total_momentum_data['neutron']['py'].append(pyn)
                            total_momentum_data['neutron']['pz'].append(pzn)

# %%
# Create output directory if not exists
import os
if not os.path.exists('output'):
    os.makedirs('output')
    print("Created output directory")

print(f"Data processing completed!")

# %%
gamma_label = [0.5, 0.6, 0.7, 0.8]

print(gamma_label[0])
print(gamma_label[1])


# %%
# Use Plotly to create interactive 3D momentum distributions
def plot_interactive_3d_momentum():
    """Plot interactive 3D momentum distributions with Plotly."""
    
    # Color settings
    colors = ['blue', 'red', 'green', 'orange']
    gamma_labels = [0.5, 0.6, 0.7, 0.8]
    
    # Create subplots: 2 rows x 2 cols, each is 3D
    fig = make_subplots(
        rows=2, cols=2,
        subplot_titles=['Proton - Pb208', 'Neutron - Pb208', 
                       'Proton - Xe130', 'Neutron - Xe130'],
        specs=[[{'type': 'scatter3d'}, {'type': 'scatter3d'}],
               [{'type': 'scatter3d'}, {'type': 'scatter3d'}]],
        vertical_spacing=0.1,
        horizontal_spacing=0.1
    )
    
    # For each target and particle type
    subplot_positions = [(1, 1), (1, 2), (2, 1), (2, 2)]
    target_particle_combinations = [
        ('Pb208', 'proton'), ('Pb208', 'neutron'),
        ('Xe130', 'proton'), ('Xe130', 'neutron')
    ]
    
    for idx, ((target, particle_type), (row, col)) in enumerate(zip(target_particle_combinations, subplot_positions)):
        for gamma_idx, gamma in enumerate(gammas):
            px_data = momentum_data[particle_type][target][gamma]['px']
            py_data = momentum_data[particle_type][target][gamma]['py']
            pz_data = momentum_data[particle_type][target][gamma]['pz']
            
            if len(px_data) > 0:
                # Sample data if too many points
                sample_size = min(1500, len(px_data))
                if len(px_data) > sample_size:
                    indices = np.random.choice(len(px_data), sample_size, replace=False)
                    px_sample = [px_data[i] for i in indices]
                    py_sample = [py_data[i] for i in indices]
                    pz_sample = [pz_data[i] for i in indices]
                else:
                    px_sample, py_sample, pz_sample = px_data, py_data, pz_data
                
                # Add scatter trace
                fig.add_trace(
                    go.Scatter3d(
                        x=px_sample,
                        y=py_sample,
                        z=pz_sample,
                        mode='markers',
                        marker=dict(
                            size=3,
                            color=colors[gamma_idx],
                            opacity=0.7
                        ),
                        name=f'{target}-{particle_type}-gamma={gamma_labels[gamma_idx]}',
                        showlegend=True if idx == 0 else False  # show legend only in first subplot
                    ),
                    row=row, col=col
                )
    
    # Update layout (titles and legend)
    fig.update_layout(
        title={
            'text': 'Interactive 3D Momentum Distribution (rotatable & zoomable)',
            'x': 0.5,
            'xanchor': 'center',
            'font': {'size': 20}
        },
        height=800,
        width=1200,
        legend=dict(
            x=1.02,
            y=1,
            traceorder="normal",
            font=dict(size=10)
        )
    )
    
    # Update axis labels for each 3D subplot
    for i in range(1, 5):
        fig.update_scenes(
            xaxis_title="Px (MeV/c)",
            yaxis_title="Py (MeV/c)", 
            zaxis_title="Pz (MeV/c)",
            selector=dict(type="scene")
        )
    
    # Show and save
    fig.show()
    
    # Save as HTML file
    fig.write_html("output/interactive_3d_momentum_by_target.html")
    print("Interactive 3D plot saved to: output/interactive_3d_momentum_by_target.html")
    
    return fig

# Execute interactive plotting
interactive_fig = plot_interactive_3d_momentum()


# %%
# Create total interactive 3D momentum distribution plot
def plot_total_interactive_3d_momentum():
    """Plot total proton and neutron interactive 3D momentum distribution"""
    
    # Create subplots: 1 row x 2 cols
    fig = make_subplots(
        rows=1, cols=2,
        subplot_titles=['Total Proton 3D Momentum', 'Total Neutron 3D Momentum'],
        specs=[[{'type': 'scatter3d'}, {'type': 'scatter3d'}]],
        horizontal_spacing=0.1
    )
    
    # Proton data
    px_p = total_momentum_data['proton']['px']
    py_p = total_momentum_data['proton']['py']
    pz_p = total_momentum_data['proton']['pz']
    
    if len(px_p) > 0:
        # Sample data if too many
        sample_size = min(3000, len(px_p))
        if len(px_p) > sample_size:
            indices = np.random.choice(len(px_p), sample_size, replace=False)
            px_sample = [px_p[i] for i in indices]
            py_sample = [py_p[i] for i in indices]
            pz_sample = [pz_p[i] for i in indices]
        else:
            px_sample, py_sample, pz_sample = px_p, py_p, pz_p
        
        # Calculate momentum magnitude for coloring
        momentum_magnitude = np.sqrt(np.array(px_sample)**2 + np.array(py_sample)**2 + np.array(pz_sample)**2)
        
        # Add proton scatter plot
        fig.add_trace(
            go.Scatter3d(
                x=px_sample,
                y=py_sample,
                z=pz_sample,
                mode='markers',
                marker=dict(
                    size=4,
                    color=momentum_magnitude,
                    colorscale='Viridis',
                    opacity=0.7,
                    colorbar=dict(
                        title="Momentum (MeV/c)",
                        x=0.45,
                        len=0.8
                    )
                ),
                name='Proton',
                hovertemplate='<b>Proton</b><br>' +
                            'Px: %{x:.1f} MeV/c<br>' +
                            'Py: %{y:.1f} MeV/c<br>' +
                            'Pz: %{z:.1f} MeV/c<br>' +
                            '|P|: %{marker.color:.1f} MeV/c<extra></extra>'
            ),
            row=1, col=1
        )
    
    # Neutron data
    px_n = total_momentum_data['neutron']['px']
    py_n = total_momentum_data['neutron']['py']
    pz_n = total_momentum_data['neutron']['pz']
    
    if len(px_n) > 0:
        # Sample data if too many
        sample_size = min(3000, len(px_n))
        if len(px_n) > sample_size:
            indices = np.random.choice(len(px_n), sample_size, replace=False)
            px_sample = [px_n[i] for i in indices]
            py_sample = [py_n[i] for i in indices]
            pz_sample = [pz_n[i] for i in indices]
        else:
            px_sample, py_sample, pz_sample = px_n, py_n, pz_n
        
        # Calculate momentum magnitude for coloring
        momentum_magnitude = np.sqrt(np.array(px_sample)**2 + np.array(py_sample)**2 + np.array(pz_sample)**2)
        
        # Add neutron scatter plot
        fig.add_trace(
            go.Scatter3d(
                x=px_sample,
                y=py_sample,
                z=pz_sample,
                mode='markers',
                marker=dict(
                    size=4,
                    color=momentum_magnitude,
                    colorscale='Plasma',
                    opacity=0.7,
                    colorbar=dict(
                        title="Momentum (MeV/c)",
                        x=1.02,
                        len=0.8
                    )
                ),
                name='Neutron',
                hovertemplate='<b>Neutron</b><br>' +
                            'Px: %{x:.1f} MeV/c<br>' +
                            'Py: %{y:.1f} MeV/c<br>' +
                            'Pz: %{z:.1f} MeV/c<br>' +
                            '|P|: %{marker.color:.1f} MeV/c<extra></extra>'
            ),
            row=1, col=2
        )
    
    # Update layout
    fig.update_layout(
        title={
            'text': 'Total Interactive 3D Momentum Distribution - Y Polarization (colored by magnitude)',
            'x': 0.5,
            'xanchor': 'center',
            'font': {'size': 20}
        },
        height=600,
        width=1400,
        showlegend=True
    )
    
    # Update 3D scene settings
    fig.update_scenes(
        xaxis_title="Px (MeV/c)",
        yaxis_title="Py (MeV/c)",
        zaxis_title="Pz (MeV/c)",
        camera=dict(
            eye=dict(x=1.2, y=1.2, z=1.2)
        )
    )
    
    # Show figure
    fig.show()
    
    # Save as HTML file
    fig.write_html("output/total_interactive_3d_momentum.html")
    print("Total interactive 3D plot saved to: output/total_interactive_3d_momentum.html")
    
    return fig

# Execute total interactive plotting
total_interactive_fig = plot_total_interactive_3d_momentum()


# %%
# Create animated 3D plot by gamma parameter
def create_animated_3d_momentum():
    """Create animated 3D momentum distribution plot with varying gamma"""
    
    frames = []
    gamma_labels = [0.5, 0.6, 0.7, 0.8]
    
    # Create a frame for each gamma
    for gamma_idx, gamma in enumerate(gammas):
        frame_data = []
        
        # Add data for each target and particle type
        for target in targets:
            for particle_type in ['proton', 'neutron']:
                px_data = momentum_data[particle_type][target][gamma]['px']
                py_data = momentum_data[particle_type][target][gamma]['py']
                pz_data = momentum_data[particle_type][target][gamma]['pz']
                
                if len(px_data) > 0:
                    # Sample data
                    sample_size = min(800, len(px_data))
                    if len(px_data) > sample_size:
                        indices = np.random.choice(len(px_data), sample_size, replace=False)
                        px_sample = [px_data[i] for i in indices]
                        py_sample = [py_data[i] for i in indices]
                        pz_sample = [pz_data[i] for i in indices]
                    else:
                        px_sample, py_sample, pz_sample = px_data, py_data, pz_data
                    
                    # Set color and symbol
                    color = 'red' if particle_type == 'proton' else 'blue'
                    symbol = 'circle' if target == 'Pb208' else 'diamond'
                    
                    frame_data.append(
                        go.Scatter3d(
                            x=px_sample,
                            y=py_sample,
                            z=pz_sample,
                            mode='markers',
                            marker=dict(
                                size=4,
                                color=color,
                                opacity=0.7,
                                symbol=symbol
                            ),
                            name=f'{target}-{particle_type}',
                            hovertemplate=f'<b>{target} {particle_type}</b><br>' +
                                        'Px: %{x:.1f} MeV/c<br>' +
                                        'Py: %{y:.1f} MeV/c<br>' +
                                        'Pz: %{z:.1f} MeV/c<extra></extra>'
                        )
                    )
        
        frames.append(go.Frame(data=frame_data, name=str(gamma_labels[gamma_idx])))
    
    # Create initial figure (using first gamma data)
    initial_data = frames[0].data if frames else []
    
    fig = go.Figure(
        data=initial_data,
        frames=frames
    )
    
    # Add play/pause buttons
    fig.update_layout(
        title={
            'text': 'Animated 3D Momentum Distribution by Gamma - Y Pol<br><sub>Red=Proton, Blue=Neutron; Circle=Pb208, Diamond=Xe130</sub>',
            'x': 0.5,
            'xanchor': 'center',
            'font': {'size': 18}
        },
        scene=dict(
            xaxis_title="Px (MeV/c)",
            yaxis_title="Py (MeV/c)",
            zaxis_title="Pz (MeV/c)",
            camera=dict(
                eye=dict(x=1.5, y=1.5, z=1.5)
            )
        ),
        updatemenus=[{
            "buttons": [
                {
                    "args": [None, {"frame": {"duration": 1000, "redraw": True},
                                  "fromcurrent": True, "transition": {"duration": 300}}],
                    "label": "Play",
                    "method": "animate"
                },
                {
                    "args": [[None], {"frame": {"duration": 0, "redraw": True},
                                    "mode": "immediate", "transition": {"duration": 0}}],
                    "label": "Pause",
                    "method": "animate"
                }
            ],
            "direction": "left",
            "pad": {"r": 10, "t": 87},
            "showactive": False,
            "type": "buttons",
            "x": 0.1,
            "xanchor": "right",
            "y": 0,
            "yanchor": "top"
        }],
        sliders=[{
            "active": 0,
            "yanchor": "top",
            "xanchor": "left",
            "currentvalue": {
                "font": {"size": 20},
                "prefix": "gamma = ",
                "visible": True,
                "xanchor": "right"
            },
            "transition": {"duration": 300, "easing": "cubic-in-out"},
            "pad": {"b": 10, "t": 50},
            "len": 0.9,
            "x": 0.1,
            "y": 0,
            "steps": [
                {
                    "args": [[f.name], {"frame": {"duration": 300, "redraw": True},
                                      "mode": "immediate", "transition": {"duration": 300}}],
                    "label": f.name,
                    "method": "animate"
                } for f in frames
            ]
        }],
        width=1000,
        height=700
    )
    
    # Show figure
    fig.show()
    
    # Save as HTML file
    fig.write_html("output/animated_3d_momentum_gamma.html")
    print("Animated 3D plot saved to: output/animated_3d_momentum_gamma.html")
    
    return fig

# Create animated 3D plot
animated_fig = create_animated_3d_momentum()

# %%
# Print data statistics
print("=== Data Statistics ===")
print(f"Total proton data points: {len(total_momentum_data['proton']['px'])}")
print(f"Total neutron data points: {len(total_momentum_data['neutron']['px'])}")
print()

# Define gamma labels
gamma_labels = [0.5, 0.6, 0.7, 0.8]

# Statistics by target and gamma
for target in targets:
    print(f"Target: {target}")
    for gamma_idx, gamma in enumerate(gammas):
        proton_count = len(momentum_data['proton'][target][gamma]['px'])
        neutron_count = len(momentum_data['neutron'][target][gamma]['px'])
        print(f"  gamma={gamma_labels[gamma_idx]}: {proton_count} proton points, {neutron_count} neutron points")
    print()

# Momentum range statistics
if len(total_momentum_data['proton']['px']) > 0:
    print("=== Momentum Range Statistics ===")
    print("Proton momentum range:")
    print(f"  Px: {min(total_momentum_data['proton']['px']):.1f} ~ {max(total_momentum_data['proton']['px']):.1f} MeV/c")
    print(f"  Py: {min(total_momentum_data['proton']['py']):.1f} ~ {max(total_momentum_data['proton']['py']):.1f} MeV/c")
    print(f"  Pz: {min(total_momentum_data['proton']['pz']):.1f} ~ {max(total_momentum_data['proton']['pz']):.1f} MeV/c")
    
    print("Neutron momentum range:")
    print(f"  Px: {min(total_momentum_data['neutron']['px']):.1f} ~ {max(total_momentum_data['neutron']['px']):.1f} MeV/c")
    print(f"  Py: {min(total_momentum_data['neutron']['py']):.1f} ~ {max(total_momentum_data['neutron']['py']):.1f} MeV/c")
    print(f"  Pz: {min(total_momentum_data['neutron']['pz']):.1f} ~ {max(total_momentum_data['neutron']['pz']):.1f} MeV/c")

# %%
import matplotlib.pyplot as plt
# Plot 3D momentum distribution for different targets and gammas
def plot_3d_momentum_by_target_gamma():
    """Plot 3D momentum distribution for proton and neutron with different targets and gammas"""
    
    # Create large figure with all target and gamma subplots
    fig = plt.figure(figsize=(20, 15))
    fig.suptitle('3D Momentum Distribution for Different Targets and Gammas - Y Polarization', fontsize=16)
    
    # Set color mapping
    colors = plt.cm.Set1(np.linspace(0, 1, len(gammas)))
    gamma_labels = [0.5, 0.6, 0.7, 0.8]
    
    plot_idx = 1
    
    for target_idx, target in enumerate(targets):
        # Create proton and neutron 3D plots for each target
        
        # Proton 3D plot
        ax_p = fig.add_subplot(len(targets), 2, plot_idx, projection='3d')
        for gamma_idx, gamma in enumerate(gammas):
            px_data = momentum_data['proton'][target][gamma]['px']
            py_data = momentum_data['proton'][target][gamma]['py']
            pz_data = momentum_data['proton'][target][gamma]['pz']
            
            if len(px_data) > 0:
                # Random sampling to reduce points (if too many)
                if len(px_data) > 1000:
                    indices = np.random.choice(len(px_data), 1000, replace=False)
                    px_sample = [px_data[i] for i in indices]
                    py_sample = [py_data[i] for i in indices]
                    pz_sample = [pz_data[i] for i in indices]
                else:
                    px_sample, py_sample, pz_sample = px_data, py_data, pz_data
                
                ax_p.scatter(px_sample, py_sample, pz_sample, 
                           c=[colors[gamma_idx]], alpha=0.6, s=20,
                           label=f'gamma={gamma_labels[gamma_idx]}')
        
        ax_p.set_xlabel('Px (MeV/c)')
        ax_p.set_ylabel('Py (MeV/c)')
        ax_p.set_zlabel('Pz (MeV/c)')
        ax_p.set_title(f'Proton - {target}')
        ax_p.legend()
        plot_idx += 1
        
        # Neutron 3D plot
        ax_n = fig.add_subplot(len(targets), 2, plot_idx, projection='3d')
        for gamma_idx, gamma in enumerate(gammas):
            px_data = momentum_data['neutron'][target][gamma]['px']
            py_data = momentum_data['neutron'][target][gamma]['py']
            pz_data = momentum_data['neutron'][target][gamma]['pz']
            
            if len(px_data) > 0:
                # Random sampling to reduce points (if too many)
                if len(px_data) > 1000:
                    indices = np.random.choice(len(px_data), 1000, replace=False)
                    px_sample = [px_data[i] for i in indices]
                    py_sample = [py_data[i] for i in indices]
                    pz_sample = [pz_data[i] for i in indices]
                else:
                    px_sample, py_sample, pz_sample = px_data, py_data, pz_data
                
                ax_n.scatter(px_sample, py_sample, pz_sample, 
                           c=[colors[gamma_idx]], alpha=0.6, s=20,
                           label=f'gamma={gamma_labels[gamma_idx]}')
        
        ax_n.set_xlabel('Px (MeV/c)')
        ax_n.set_ylabel('Py (MeV/c)')
        ax_n.set_zlabel('Pz (MeV/c)')
        ax_n.set_title(f'Neutron - {target}')
        ax_n.legend()
        plot_idx += 1
    
    plt.tight_layout()
    plt.savefig('output/3d_momentum_by_target_gamma.pdf', bbox_inches='tight', dpi=300)
    plt.savefig('output/3d_momentum_by_target_gamma.png', bbox_inches='tight', dpi=300)
    plt.show()

# Execute plotting
plot_3d_momentum_by_target_gamma()

# %% [markdown]
# ## Save data to ROOT file using PyROOT (Compatible with SMSimulator)
#
# Use PyROOT to create ROOT files with TBeamSimData objects, exactly like GenInputRoot_np_atime.cc

# %%
import subprocess
import os

# Output directory configuration
output_root_dir_native = os.path.join(smdir, "d_work/rootfiles/ypol_native/")

# Particle masses (MeV)
Mp = 938.27  # proton rest mass
Mn = 939.57  # neutron rest mass

def save_to_root_using_native_root(output_dir=output_root_dir_native, filename_prefix='ypol_np'):
    """
    Save processed momentum data to ROOT file by calling ROOT macro
    Uses GenInputRoot_np_atime.cc logic but generates temporary data files
    and calls ROOT to compile and run the macro
    """
    
    # Create output directory if not exists
    if not os.path.exists(output_dir):
        os.makedirs(output_dir)
        print(f"Created directory: {output_dir}")
    
    # Create temporary directory for intermediate .dat files
    temp_dir = os.path.join(smdir, "d_work/temp_dat_files/")
    if not os.path.exists(temp_dir):
        os.makedirs(temp_dir)
    
    gamma_labels = [0.5, 0.6, 0.7, 0.8]
    
    print("=" * 60)
    print("Generating ROOT files using native ROOT + C++ macro")
    print("=" * 60)
    
    # Process each target and gamma combination
    for target in targets:
        for gamma_idx, gamma in enumerate(gammas):
            # Get proton and neutron data for this combination
            px_p_list = momentum_data['proton'][target][gamma]['px']
            py_p_list = momentum_data['proton'][target][gamma]['py']
            pz_p_list = momentum_data['proton'][target][gamma]['pz']
            
            px_n_list = momentum_data['neutron'][target][gamma]['px']
            py_n_list = momentum_data['neutron'][target][gamma]['py']
            pz_n_list = momentum_data['neutron'][target][gamma]['pz']
            
            # Check if data exists
            n_events = len(px_p_list)
            if n_events == 0:
                print(f"Warning: No data for {target}, gamma={gamma_labels[gamma_idx]}")
                continue
            
            print(f"\nProcessing: {target} gamma={gamma_labels[gamma_idx]} ({n_events} events)")
            
            # Create temporary .dat file (same format as original QMD data)
            temp_dat_file = os.path.join(temp_dir, f"{target}_g{gamma}.dat")
            
            with open(temp_dat_file, 'w') as f:
                # Write header (similar to original dbreak.dat format)
                f.write("# Processed data from Python analysis\n")
                f.write("# EventNo  pxp  pyp  pzp  pxn  pyn  pzn\n")
                
                # Write data
                for i in range(n_events):
                    f.write(f"{i+1:6d}  {px_p_list[i]:10.4f}  {py_p_list[i]:10.4f}  {pz_p_list[i]:10.4f}  ")
                    f.write(f"{px_n_list[i]:10.4f}  {py_n_list[i]:10.4f}  {pz_n_list[i]:10.4f}\n")
            
            print(f"  Created temp file: {temp_dat_file}")
            
            # Prepare output ROOT file path
            output_file = os.path.join(output_dir, f"{filename_prefix}_{target}_g{gamma}.root")
            
            # Create ROOT macro to process this file
            macro_content = f'''
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <TFile.h>
#include <TTree.h>
#include <TVector3.h>
#include <TLorentzVector.h>
#include "TBeamSimData.hh"

void convert_{target}_g{gamma}() {{
    std::string inputFile = "{temp_dat_file}";
    TVector3 position(0, 0, 0);
    
    std::ifstream fin(inputFile);
    if (!fin.is_open()) {{
        std::cerr << "Cannot open input file: " << inputFile << std::endl;
        return;
    }}
    
    TFile *f = new TFile("{output_file}", "RECREATE");
    TTree *tree = new TTree("tree", "Input tree for simultaneous n-p");
    
    gBeamSimDataArray = new TBeamSimDataArray();
    tree->Branch("TBeamSimData", &gBeamSimDataArray);
    
    std::string line;
    int eventCount = 0;
    
    // Skip header lines
    std::getline(fin, line);
    std::getline(fin, line);
    
    while (std::getline(fin, line)) {{
        std::istringstream iss(line);
        int no;
        double pxp, pyp, pzp, pxn, pyn, pzn;
        if (!(iss >> no >> pxp >> pyp >> pzp >> pxn >> pyn >> pzn)) continue;
        
        gBeamSimDataArray->clear();
        
        // Proton
        double Mp = 938.27;
        double Ep = sqrt(pxp*pxp + pyp*pyp + pzp*pzp + Mp*Mp);
        TLorentzVector momentum_p(pxp, pyp, pzp, Ep);
        TBeamSimData proton(1, 1, momentum_p, position);
        proton.fParticleName = "proton";
        proton.fPrimaryParticleID = 0;
        proton.fTime = 0.0;
        proton.fIsAccepted = true;
        gBeamSimDataArray->push_back(proton);
        
        // Neutron
        double Mn = 939.57;
        double En = sqrt(pxn*pxn + pyn*pyn + pzn*pzn + Mn*Mn);
        TLorentzVector momentum_n(pxn, pyn, pzn, En);
        TBeamSimData neutron(0, 1, momentum_n, position);
        neutron.fParticleName = "neutron";
        neutron.fPrimaryParticleID = 1;
        neutron.fTime = 0.0;
        neutron.fIsAccepted = true;
        gBeamSimDataArray->push_back(neutron);
        
        tree->Fill();
        eventCount++;
    }}
    
    f->Write();
    f->Close();
    fin.close();
    
    std::cout << "Converted " << eventCount << " events to " << "{output_file}" << std::endl;
}}
'''
            
            # Write macro to temporary file
            macro_file = os.path.join(temp_dir, f"convert_{target}_g{gamma}.C")
            with open(macro_file, 'w') as f:
                f.write(macro_content)
            
            print(f"  Created macro: {macro_file}")
            
            # Run ROOT macro
            print(f"  Running ROOT macro...")
            
            # Set up environment for ROOT macro compilation
            include_path = f"{smdir}/sources/smg4lib/data/include"
            lib_path = f"{smdir}/build/smg4lib"
            
            try:
                # Build the ROOT command with proper include paths and libraries
                root_cmd = [
                    'root', '-l', '-b', '-q',
                    f'{macro_file}+'  # The '+' suffix compiles the macro
                ]
                
                # Set environment variables for ROOT
                env = os.environ.copy()
                env['ROOT_INCLUDE_PATH'] = include_path
                if 'LD_LIBRARY_PATH' in env:
                    env['LD_LIBRARY_PATH'] = f"{lib_path}:{env['LD_LIBRARY_PATH']}"
                else:
                    env['LD_LIBRARY_PATH'] = lib_path
                
                result = subprocess.run(
                    root_cmd,
                    capture_output=True,
                    text=True,
                    timeout=300,
                    env=env
                )
                
                if result.returncode == 0:
                    print(f"  ✓ Successfully created: {output_file}")
                    # Show conversion summary from stdout
                    for line in result.stdout.split('\n'):
                        if 'Converted' in line:
                            print(f"    {line}")
                else:
                    print(f"  ✗ Error running ROOT macro:")
                    print(result.stderr)
            
            except FileNotFoundError:
                print(f"  ✗ ERROR: 'root' command not found!")
                print(f"  Please ensure ROOT is installed and in your PATH")
                return False
            except subprocess.TimeoutExpired:
                print(f"  ✗ ERROR: ROOT macro timed out")
                return False
            except Exception as e:
                print(f"  ✗ ERROR: {e}")
                return False
    
    print("\n" + "=" * 60)
    print("ROOT file generation completed!")
    print(f"Output directory: {output_dir}")
    print("=" * 60)
    
    # Ask if user wants to clean up temp files
    print(f"\nTemporary files are in: {temp_dir}")
    print("You can delete them manually if needed.")
    
    return True

# Execute the function
print("Using native ROOT + C++ approach (compatible with all Python versions)")
print("This method bypasses PyROOT and directly calls ROOT macros\n")

success = save_to_root_using_native_root()

if success:
    print("\n✓ All files generated successfully!")
else:
    print("\n✗ Some errors occurred during file generation")
