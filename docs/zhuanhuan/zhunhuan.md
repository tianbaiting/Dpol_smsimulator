相对论粒子在磁场中的运动方程:
dp/dt = q(v × B)
其中 v = pc²/E = pc²/√(p²c² + m²c⁴)

我们的单位系统:
- 位置: mm
- 时间: ns
- 动量: MeV/c
- 磁场: Tesla
- 电荷: 基本电荷单位 e

关键转换:
c = 299.792458 mm/ns = 299792458.0 m/s
e = 1.602177e-19 C
1 MeV = 1.602177e-13 J
1 MeV/c = 5.344286e-22 kg⋅m/s

洛伦兹力: F = q(v × B)
在我们的单位中:
F [N] = q [C] × v [m/s] × B [T]
其中 v = pc²/E

重新思考速度计算:
如果动量 p 的单位是 MeV/c，那么:
实际动量 = p × (1 MeV/c) = p × 5.344286e-22 kg⋅m/s
速度 v = p实际 / γm = (p × 5.344286e-22) / γm

但在相对论中，更简单的是直接用:
v/c = pc/E，所以 v = pc²/E
如果 p 的数值单位是 MeV/c，E 的数值单位是 MeV，那么:
v [无量纲] = p [数值] × c² / E [数值]
然后 v [m/s] = v [无量纲] × c [m/s]

所以在CalculateForce中:
v [m/s] = momentum [MeV/c的数值] × c² [m²/s²] / E [MeV的数值]

当前代码分析:
velocity = p * (kSpeedOfLight / E)
这里 kSpeedOfLight = 299.792458 mm/ns
所以得到的速度单位是 mm/ns，而不是 m/s!

这就是问题所在！
力的计算需要速度的单位是 m/s，但我们得到的是 mm/ns
mm/ns 到 m/s 的转换: ×1000000

正确的力计算应该是:
1. v [mm/ns] = p × c²/E (当前代码)
2. v [m/s] = v [mm/ns] × 1000000
3. F [N] = q [C] × v [m/s] × B [T]
4. F [MeV/c/ns] = F [N] × 转换因子

1 N = 1.871157e+21 MeV/c/s
1 N = 1.871157e+12 MeV/c/ns

最终转换因子 = 1.602177e-19 × 1000000 × 1.871157e+12
= 2.997925e-01
当前scale = 1.602177e-10
正确/当前 = 1871157347.06