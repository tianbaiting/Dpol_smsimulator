# inputnote

由于ImQMD计算的数据,会有氘核unphysical breakup的现象,

一般来说是在极化方向上, n p相对动量分量在200MeV/c以上. 

人工去除可以选取   $p^p_{i'} - p^n_{i'} \in (-150,150) \mathrm{MeV}$, 这样就可以去除掉unphysical breakup的事件. 

i是极化的方向, 对于z轴极化, i=z.对于y极化 并且是phi_random的情况, 则是y轴的分量.

## 氘核破裂动力学守恒计算

氘核束缚能为2.2MeV,

束缚能完全转化为动能, 则相对动量为: $p \approx \sqrt{2\mu E_b} \approx 45 MeV/c$.

(动能小, 完全可以用牛顿力学计算. )

用相对论计算, 设相对动量为p(n,p动量守恒, 动量相反等大), 已知$m_n, m_p, m_D$, 且$m_D = m_n + m_p - 2.2MeV$,根据$m_D = \sqrt{m_n^2 + p^2} + \sqrt{m_p^2 + p^2}$.

## obersevation 

qmdrawdata 的数据, 我需要将n p 入射后, y的数据不需要随机旋转, 然是z轴需要随机旋转.

旋转后为入射动量, 根据np入射总动量和束流z方向, 确定反应平面 得到xy轴, 其中x轴在反应平面内 $\hat{x} =z \times k_{total}$, 且是总动量为正的方向, y轴垂直于反应平面.

To quantify the isovector reorientation effect, we introduce an observable $R = \frac{N(P_{x}^{\mathrm{p}}>P_{x}^{\mathrm{n}})}{N(P_{x}^{\mathrm{p}}<P_{x}^{\mathrm{n}})}$, representing the ratio of events where the proton's $x$-momentum exceeds that of the neutron to those with the opposite ordering. Figure \ref{fig:Pxp_minus_Pxn_distribution} (a) and (b) present the distribution of $P_{x}^{\mathrm{p}}-P_{x}^{\mathrm{n}}$ for $z'$ and $y'$ tensor polarized deuteron beam on $^{208}$ Pb. The observed asymmetry in the distribution indicates that the proton and neutron experience different impulses in the reaction plane, which directly reflects the isovector reorientation effect. Notably, this effect exhibits strong sensitivity to the symmetry energy parameter $\gamma$.