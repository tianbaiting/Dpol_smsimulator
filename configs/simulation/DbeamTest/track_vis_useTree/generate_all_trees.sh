#!/bin/bash
# =========================================================================
# [EN] Generate ROOT trees for all field/angle configurations
# [CN] 生成所有磁场/角度配置的ROOT树
# =========================================================================
# Proton momentum: (±100, 0, 627) MeV/c and (±150, 0, 627) MeV/c
# =========================================================================

SMSIMDIR="${SMSIMDIR:-/home/tian/workspace/dpol/smsimulator5.5}"
VIZDIR="${SMSIMDIR}/configs/simulation/DbeamTest/track_vis_useTree"
ROOTFILEDIR="${VIZDIR}/rootfiles"

mkdir -p "${ROOTFILEDIR}"

# [EN] Target positions from target_summary files
# [CN] 靶点位置来自target_summary文件
# Format: FIELD ANGLE TX TY TZ

read -r -d '' TARGET_DATA << 'ENDDATA'
080 2.0 5.2866 0.0052 -599.4389
080 4.0 12.2155 0.0064 -513.6437
080 6.0 19.3064 0.0075 -436.9073
080 8.0 26.1312 0.0084 -367.4178
080 10.0 32.4076 0.0094 -304.0095
100 2.0 -7.7287 0.0075 -1151.8117
100 4.0 -18.9698 0.0105 -932.5705
100 6.0 -33.7681 0.0131 -762.6098
100 8.0 -52.6612 0.0157 -608.5314
100 10.0 -76.0454 0.0186 -460.8262
120 2.0 -20.3977 0.0097 -1697.9595
120 4.0 -49.2207 0.0142 -1351.9449
120 6.0 -79.5513 0.0184 -1056.1679
120 8.0 -110.2287 0.0224 -802.4392
120 10.0 -140.5538 0.0264 -587.9127
160 2.0 -45.6012 0.0137 -2775.5735
160 4.0 -108.4693 0.0212 -2158.6699
160 6.0 -152.7034 0.0283 -1571.5049
160 8.0 -172.5082 0.0349 -1100.8568
160 10.0 -191.4073 0.0418 -696.2632
200 2.0 -71.7287 0.0176 -3775.2148
200 4.0 -152.3698 0.0275 -2907.8705
200 6.0 -207.8011 0.0371 -2009.1295
200 8.0 -238.0612 0.0461 -1305.5314
200 10.0 -259.0454 0.0551 -714.8262
ENDDATA

echo "Generating ROOT trees for proton trajectories..."
echo "Proton momenta: Px = ±100, ±150 MeV/c, Py = 0, Pz = 627 MeV/c"
echo ""

while IFS=' ' read -r field angle tx ty tz; do
    [[ -z "$field" ]] && continue
    
    rootfile="${ROOTFILEDIR}/protons_B${field}T_${angle}deg.root"
    
    echo "Generating: protons_B${field}T_${angle}deg.root"
    echo "  Target: (${tx}, ${ty}, ${tz}) mm"
    
    # [EN] Generate ROOT file using ROOT macro
    # [CN] 使用ROOT宏生成ROOT文件
    root -l -b -q "${VIZDIR}/GenProtonTree.C(\"${rootfile}\", ${tx}, ${ty}, ${tz})" 2>/dev/null
    
    if [[ -f "$rootfile" ]]; then
        echo "  [OK] Generated: $rootfile"
    else
        echo "  [WARN] Failed to generate: $rootfile"
    fi
    echo ""
done <<< "$TARGET_DATA"

echo "Done. ROOT files in: ${ROOTFILEDIR}/"
ls -la "${ROOTFILEDIR}/"
