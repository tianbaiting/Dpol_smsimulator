import xml.etree.ElementTree as ET
import matplotlib
matplotlib.use('TkAgg')
import matplotlib.pyplot as plt
from mpl_toolkits.mplot3d import Axes3D
import numpy as np

xml_file = "../d_work/geometry/SAMURAIPDC.xml"
tree = ET.parse(xml_file)
root = tree.getroot()

fig = plt.figure(figsize=(10, 5))
ax = fig.add_subplot(111, projection='3d')
# 有效面积：1700mm × 800mm
halfx = 850
halfy = 400
for elem in root.findall("SAMURAIPDC"):
    wirepos = float(elem.find("wirepos").text)
    wirez = float(elem.find("wirez").text)
    anodedir = elem.find("anodedir").text.strip()

    if anodedir == 'U':
    # 截距为sqrt(2)*U position y = -x + sqrt(2)*U position,
    # y \in (-400, 400), x \in (-850, 850)
      # y \in (-halfy, halfy), x \in (-halfx, halfx)
        x_board_value = [-halfx, halfx, np.sqrt(2)*wirepos-halfy, np.sqrt(2)*wirepos+halfy]
        # 取中间值
        nums_sorted = sorted(x_board_value)
        x1 = nums_sorted[1]
        x2 = nums_sorted[2]
        y1 = -x1 + np.sqrt(2)*wirepos
        y2 = -x2 + np.sqrt(2)*wirepos
        ax.plot([x1, x2], [y1, y2], [wirez, wirez], color='g', linewidth=0.2)
    elif anodedir == 'V':
        # 截距为-sqrt(2)*v position y = x - sqrt(2)*v position,
        # y \in (-400, 400), x \in (-850, 850)
      # y \in (-halfy, halfy), x \in (-halfx, halfx)
        x_board_value = [-halfx, halfx, np.sqrt(2)*wirepos-halfy, np.sqrt(2)*wirepos+halfy]
        # 取中间值
        nums_sorted = sorted(x_board_value)
        x1 = nums_sorted[1]
        x2 = nums_sorted[2]
        y1 = +x1 - np.sqrt(2)*wirepos
        y2 = +x2 - np.sqrt(2)*wirepos
        ax.plot([x1, x2], [y1, y2], [wirez, wirez], color='b', linewidth=0.2)

    elif anodedir == 'X':
        x1 = wirepos
        y1 = halfy
        x2 = wirepos
        y2 = - halfy
        ax.plot([x1, x2], [y1, y2], [wirez, wirez], color='r', linewidth=0.2)

    else:
        continue  # 未知方向跳过

    

ax.set_xlabel('X (mm)')
ax.set_ylabel('Y (mm)')
ax.set_zlabel('Z (mm)')
ax.set_title('PDC wire 3D structure')
plt.grid(True)
plt.tight_layout()
plt.savefig('pdc_wire_structure_3d.png')
plt.show()