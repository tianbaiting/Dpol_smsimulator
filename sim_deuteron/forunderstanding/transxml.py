import xml.etree.ElementTree as ET

xml_file = "SAMURAIPDC.xml"
tree = ET.parse(xml_file)
root = tree.getroot()

wires = []
for elem in root.findall("SAMURAIPDC"):
    wireid = int(elem.find("wireid").text)
    wirepos = float(elem.find("wirepos").text)
    wirez = float(elem.find("wirez").text)
    wires.append((wireid, wirepos, wirez))

print("std::vector<WireInfo> wires = {")
for wire in wires:
    print(f"    {{{wire[0]}, {wire[1]}, {wire[2]}}},")
print("};")