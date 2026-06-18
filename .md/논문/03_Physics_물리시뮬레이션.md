# 03. 물리·시뮬레이션 — 박사 연구 심화

> 이 문서는 `00_PHD_Paper_Guide.md`의 개념 틀을 전제로 한다.
> 특히 §1(구현 vs 기여), §3(thesis statement), §4(구조), §7(평가: 정확도=수치해 오차·안정성·ms·결정론 재현률)을 매 절에서 반복 적용한다.
> 각 세부주제는 **### X.1 핵심 원리(수학) / ### X.2 대표 기존 연구 / ### X.3 알고리즘(의사코드) / ### X.4 박사급 novel 각도 / ### X.5 Thesis statement 예시 / ### X.6 평가 방법 / ### X.7 Winters 연결점** 순서를 따른다.

---

## 0. 이 분야를 박사로 본다는 것 (구현 vs 기여 + top venue)

물리·시뮬레이션은 **"수치적분으로 운동방정식을 푼다"**가 아니다. 그건 학부 과제다. 이 분야의 박사 기여는 거의 항상 다음 네 축 중 하나(또는 조합)에서 **정량적 개선**을 증명하는 것이다.

| 축 | 박사가 다투는 질문 | 대표 metric |
|----|--------------------|-------------|
| **정확도(Accuracy)** | 해석해/고차 reference solver 대비 오차가 얼마나 작은가? | $L_2$ 오차, energy drift, 해석해와의 RMSE |
| **안정성(Stability)** | 큰 timestep·큰 강성에서 폭발하지 않는가? 시간적 일관성은? | 스펙트럼 반지름 $\rho(\mathbf{A})$, energy bound, flicker |
| **성능(Performance)** | 같은 품질을 몇 ms·몇 입자/초로? | frame time(ms), M-particles/s, iteration 수 |
| **결정론(Determinism)** | 같은 입력이 다른 머신에서 **비트 단위로** 같은 결과를 내는가? | 재현률(reproduction rate), 플랫폼 간 state-hash 일치율 |

> **구현 vs 기여 자가진단** (가이드 §1, §12): "나는 천 시뮬을 구현했다"는 기여가 아니다. "**substep 기반 XPBD에 결정론적 제약 정렬을 추가하면, lockstep MOBA에서 천 시뮬을 x86/ARM 비트 단위로 재현하면서 60fps를 유지한다**"가 기여다. 전자는 데모, 후자는 falsifiable thesis다.

### Top venue (가이드 §9)

- **학술 1차**: SIGGRAPH / SIGGRAPH Asia, **SCA (ACM SIGGRAPH/Eurographics Symposium on Computer Animation)**, **Eurographics(EG)**, SGP/EGSR(표현·기하 인접).
- **저널**: **ACM Transactions on Graphics (TOG)** — SIGGRAPH/SIGGRAPH Asia 본회의 논문이 곧 TOG에 게재됨. 물리 시뮬은 IEEE TVCG도 가시권.
- **산업(참고용, 비심사)**: GDC, GPU Gems / GPU Pro 시리즈. → 가이드 §9 주의: 권위는 있으나 동료심사 출판이 아니다. **기여는 학술 venue에 내고 GDC는 영향력 근거로 인용**한다.

> **이 분야 특유의 긴장점(Winters 핵심):** 그래픽스 물리는 전통적으로 "그럴듯하면 됨(plausible)"을 추구해 왔다. 하지만 Winters는 **서버 권위(server-authoritative) + lockstep** MOBA다. 여기서 물리는 plausible로 끝나지 않고 **cross-platform 결정론(deterministic, bit-exact)**이 추가 제약으로 붙는다. 바로 이 "결정론 × 실시간 물리"의 교집합이 그래픽스 물리 문헌에서 **상대적으로 비어 있는 박사급 틈새**다. 이 문서의 모든 절은 이 틈새로 수렴한다.

---

## 1. Inverse Kinematics (IK Solver)

### 1.1 핵심 원리(수학)

Forward Kinematics(FK)는 관절각 $\boldsymbol{\theta} \in \mathbb{R}^n$ → 말단(end-effector) 위치 $\mathbf{e} \in \mathbb{R}^m$ ($m=3$ 위치, $m=6$ 위치+자세)로 가는 사상 $\mathbf{e} = f(\boldsymbol{\theta})$이다. **IK는 그 역**: 목표 $\mathbf{g}$가 주어졌을 때 $f(\boldsymbol{\theta}) = \mathbf{g}$를 만족하는 $\boldsymbol{\theta}$를 찾는다. 일반적으로 $f$는 비선형이고 해가 **없거나(목표가 reach 밖)·하나·무한히 많다(과결정 vs 과소결정)**.

핵심 도구는 **Jacobian** $\mathbf{J}(\boldsymbol{\theta}) \in \mathbb{R}^{m \times n}$:

$$
\mathbf{J}_{ij} = \frac{\partial \mathbf{e}_i}{\partial \boldsymbol{\theta}_j}, \qquad \dot{\mathbf{e}} = \mathbf{J}\,\dot{\boldsymbol{\theta}}.
$$

회전 관절 $j$ (회전축 $\hat{\mathbf{a}}_j$, 관절 위치 $\mathbf{p}_j$)에 대한 Jacobian 열은 표준적으로

$$
\mathbf{J}_{:,j} = \hat{\mathbf{a}}_j \times (\mathbf{e} - \mathbf{p}_j).
$$

목표 오차 $\Delta\mathbf{e} = \mathbf{g} - \mathbf{e}$를 줄이는 관절 갱신 $\Delta\boldsymbol{\theta}$를 푸는 방식이 IK solver를 가른다.

**(a) 해석적 2-bone IK.** 어깨–팔꿈치–손목처럼 회전 자유도가 적은 체인은 **코사인 법칙**으로 닫힌형(closed-form) 해가 있다. 상박 $l_1$, 하박 $l_2$, 목표까지 거리 $d = \lVert \mathbf{g} - \mathbf{p}_{\text{shoulder}} \rVert$일 때 팔꿈치 내각 $\beta$는

$$
\cos\beta = \frac{l_1^2 + l_2^2 - d^2}{2 l_1 l_2}, \qquad d \in [\,|l_1-l_2|,\; l_1+l_2\,].
$$

빠르고 결정론적이며 jitter가 없다. 발/다리, 팔처럼 2-링크 문제의 **1순위 선택**.

**(b) Jacobian Transpose.** 역행렬 없이 $\Delta\boldsymbol{\theta} = \alpha\,\mathbf{J}^{\!\top}\Delta\mathbf{e}$. 이는 오차 제곱 $\tfrac12\lVert\Delta\mathbf{e}\rVert^2$의 음의 기울기 방향(gradient descent)이라 항상 오차를 줄이지만 수렴이 느리고 $\alpha$ 선택이 까다롭다.

**(c) Pseudo-inverse(최소노름 해).** 과소결정($m<n$)에서 $\Delta\boldsymbol{\theta} = \mathbf{J}^{+}\Delta\mathbf{e}$, $\mathbf{J}^{+} = \mathbf{J}^{\!\top}(\mathbf{J}\mathbf{J}^{\!\top})^{-1}$. 이는 $\lVert\Delta\boldsymbol{\theta}\rVert$를 최소화하는 해다. **특이점(singularity)** — $\mathbf{J}$가 rank-deficient(팔이 쭉 펴진 순간 등) — 에서 $\mathbf{J}\mathbf{J}^{\!\top}$가 거의 비가역이 되어 $\Delta\boldsymbol{\theta}$가 폭발한다.

**(d) Damped Least Squares (DLS, Levenberg–Marquardt).** 특이점을 정규화로 길들인다:

$$
\Delta\boldsymbol{\theta} = \mathbf{J}^{\!\top}\!\left(\mathbf{J}\mathbf{J}^{\!\top} + \lambda^2 \mathbf{I}\right)^{-1}\Delta\mathbf{e}.
$$

이는 $\lVert \mathbf{J}\,\Delta\boldsymbol{\theta} - \Delta\mathbf{e}\rVert^2 + \lambda^2\lVert\Delta\boldsymbol{\theta}\rVert^2$를 최소화한다. SVD $\mathbf{J} = \mathbf{U}\boldsymbol{\Sigma}\mathbf{V}^{\!\top}$로 보면 특이값 $\sigma_i$가 $\sigma_i/(\sigma_i^2+\lambda^2)$로 감쇠되어 작은 $\sigma_i$(특이점)에서 발산이 억제된다. **선택적 감쇠(SVD-DLS, Buss & Kim 2005)**는 특이값별로 $\lambda$를 달리해 정확도와 안정성을 동시에 잡는다.

**(e) CCD (Cyclic Coordinate Descent).** 행렬을 전혀 안 쓴다. 말단부터 루트까지 각 관절을 **한 번에 하나씩**, 그 관절–말단 벡터를 관절–목표 벡터에 정렬하도록 회전. 구현이 극히 단순하고 관절 한계(joint limit) 적용이 자연스럽지만, 루트 쪽 관절이 과하게 굽는 "마지막 관절 편향" 경향이 있다.

**(f) FABRIK (Aristidou & Lasenby 2011).** **Forward And Backward Reaching IK.** 회전 대신 **위치(점)** 만으로 푼다. 체인을 점들의 열로 보고 (1) 말단을 목표에 놓고 루트 방향으로 각 점을 링크 길이만큼 재배치(backward), (2) 루트를 원위치에 놓고 말단 방향으로 다시 재배치(forward)를 번갈아 수렴. 반복당 $O(n)$, 부드럽고 자연스러운 자세, 빠른 수렴이 장점이라 게임 full-body IK의 사실상 표준 중 하나가 됐다.

**(g) Full-body IK.** 다중 말단(양손·양발·머리·골반)을 동시에 만족시키는 문제. 우선순위(priority)가 다른 목표들을 **null-space projection**으로 계층화한다: 1순위 작업을 푼 뒤, 그 해의 영공간 $\mathbf{N}_1 = \mathbf{I} - \mathbf{J}_1^{+}\mathbf{J}_1$ 안에서 2순위를 푼다.

$$
\Delta\boldsymbol{\theta} = \mathbf{J}_1^{+}\Delta\mathbf{e}_1 + \mathbf{N}_1\,\mathbf{J}_2^{+}\Delta\mathbf{e}_2 + \cdots
$$

**(h) 학습 기반 IK (neural).** $\mathbf{g} \mapsto \boldsymbol{\theta}$를 신경망으로 회귀하거나, 다해성을 다루기 위해 **정규화 흐름/조건부 생성모델**로 자세 분포 $p(\boldsymbol{\theta}\mid\mathbf{g})$를 학습(예: ProtoRes 2021, 후속 연구들). 자연스러움(motion prior)을 데이터로 학습한다는 장점, 하지만 **결정론·정밀 접지 보장이 약함**.

> **open problem (가이드 §6 기준):** (1) 실시간 다관절(full-body) **안정성** — 우선순위 충돌 시 jitter/특이점; (2) **발 접지(foot placement)의 지형 적응** — 경사·계단 위에서 발바닥 정렬·접촉 유지; (3) **결정론** — 반복형 solver의 반복 횟수·부동소수점 누적이 플랫폼마다 갈리는 문제(Winters 핵심).

### 1.2 대표 기존 연구

- **Wolovich & Elliott (1984)** — robotics에서 Jacobian transpose IK의 초기 정식화.
- **Buss & Kim (2005)** — *Selectively Damped Least Squares for Inverse Kinematics.* DLS의 특이값별 감쇠. IK 안정성 분석의 기준점.
- **Aristidou & Lasenby (2011)** — *FABRIK: A fast, iterative solver for the Inverse Kinematics problem.* Graphical Models. (후속: Aristidou et al. 2016 survey "Inverse Kinematics techniques in Computer Graphics".)
- **Sloan/Unzueta 등 full-body 계열** 및 **Harish et al. (2016)** — GPU 병렬 FABRIK.
- **신경망 IK**: **Oreshkin et al. (2021/2022) ProtoRes** — protocol-aware residual pose 생성으로 sparse 입력에서 full-body 복원.
- 게임 적용 참고(비심사): Epic의 **Control Rig / Full-Body IK**, Ubisoft 발 접지 GDC 발표들.

### 1.3 알고리즘(의사코드)

```text
# DLS(Damped Least Squares) IK 한 스텝
function DLS_IK_step(theta, target, lambda):
    e   = ForwardKinematics(theta)        # 말단 위치
    de  = target - e
    if |de| < eps: return theta           # 수렴
    J   = ComputeJacobian(theta)          # m x n, 열 = a_j x (e - p_j)
    A   = J * Jᵀ + lambda^2 * I           # m x m, 작아서 직접 풀이
    y   = solve(A, de)                    # A y = de  (Cholesky)
    dtheta = Jᵀ * y
    theta = ClampToJointLimits(theta + dtheta)
    return theta
```

```text
# FABRIK (점 기반, 위치 IK)
function FABRIK(p[0..n], target, d[0..n-1], rootPos):   # d=링크 길이
    reach = sum(d)
    if |target - p[0]| > reach:                # 도달 불가 → 일직선
        for i in 0..n-1:
            r = |target - p[i]|;  lam = d[i]/r
            p[i+1] = (1-lam)*p[i] + lam*target
        return p
    for iter in 1..K:                          # 보통 K ≤ 10
        # --- backward: 말단을 목표에 ---
        p[n] = target
        for i in n-1 downto 0:
            r = |p[i+1]-p[i]|; lam = d[i]/r
            p[i] = (1-lam)*p[i+1] + lam*p[i]
        # --- forward: 루트를 원위치에 ---
        p[0] = rootPos
        for i in 0..n-1:
            r = |p[i+1]-p[i]|; lam = d[i]/r
            p[i+1] = (1-lam)*p[i] + lam*p[i+1]
        if |p[n]-target| < eps: break
    return p
```

```text
# 발 접지(foot placement) IK — 지형 적응 (해석적 2-bone + 정렬)
function FootIK(hip, knee, ankle, terrain):
    # 1) 발 밑 지형 raycast로 목표 발 높이/법선
    (footPos, footNormal) = RaycastDown(ankle.world, terrain)
    # 2) 골반(hip) 높이 보정: 가장 낮은 발에 맞춰 pelvis 내림
    pelvisDrop = ComputePelvisDrop(allFeet)
    # 3) 해석적 2-bone로 무릎/발목 각 (코사인 법칙)
    Solve2Bone(hip, knee, ankle, footPos)       # closed-form, jitter 없음
    # 4) 발바닥을 지형 법선에 정렬 (ankle 회전)
    AlignFootToNormal(ankle, footNormal)
```

### 1.4 박사급 novel 각도

1. **결정론적 IK 레이어(deterministic IK).** 반복형 solver(CCD/FABRIK/DLS)는 시뮬 상태에 영향을 주는 순간(예: 발 접지가 충돌·히트박스에 반영되는 경우) 결정론을 깨뜨린다. 기여 후보: **고정 반복 횟수 + 고정소수점/명세된 부동소수점 축약(reproducible FMA, 곱셈순서 고정) + 관절 순회 순서 정렬**로 x86/ARM 비트 일치를 보장하는 IK 파이프라인. → §종합의 결정론 학위논문 1편.
2. **지형 적응 발 접지의 안정성-정확도 trade-off.** 경사·계단에서 발이 미끄러지지 않으면서(접촉 일관성) jitter 없는 pelvis 보정. 기여: 시간적 안정성을 정량화(per-frame 발 위치 분산)하고, 해석적 2-bone + null-space pelvis 보정의 **수렴·flicker 경계**를 분석.
3. **학습 prior × 해석적 정밀의 하이브리드.** neural IK로 "자연스러운 자세"를 제안(prior)하고, 그 위에 해석적/제약 기반 보정으로 **접지 mm 정밀 + 결정론**을 강제. open problem(neural의 정밀·결정론 약점)을 직접 공략.

### 1.5 Thesis statement 예시

> "고정 반복 FABRIK에 관절 순회 결정론적 정렬과 명세된 부동소수점 축약을 결합한 full-body IK는, lockstep MOBA에서 발 접지가 시뮬 상태에 반영되는 경우에도 x86/ARM 간 비트 단위 재현을 보장하면서, 비결정론적 DLS 대비 동등 정확도($L_2$ 말단 오차)와 60fps를 유지한다."

### 1.6 평가 방법

- **정확도**: 말단 위치 오차 $\lVert \mathbf{e}-\mathbf{g}\rVert$, 자세 자연스러움(motion-capture reference 대비 joint angle RMSE), 접지 관통/부유(penetration/float) 거리.
- **안정성**: 프레임 간 발/관절 위치 분산(jitter), 특이점 근방에서 $\Delta\boldsymbol{\theta}$ 노름 폭발 여부, 수렴까지 반복 횟수.
- **성능**: 캐릭터당 IK ms, 다수 캐릭터 scaling.
- **결정론**: 동일 입력으로 x86/ARM·다른 컴파일러에서 최종 본 행렬 state-hash 일치율(%). **Baseline**: DLS(비고정), naive CCD. **Ablation**: 정렬 제거 / 반복 횟수 가변 / FMA 자유화 각각의 재현률 영향.

### 1.7 Winters 연결점

- 본/스켈레톤 파이프라인: `Engine/Public/Resource/Skeleton.h`의 `CSkeleton::ComputeFinalTransforms(localTransforms, outFinal, outGlobal)`가 로컬→글로벌→스키닝 행렬을 만든다. IK는 이 **로컬 트랜스폼을 후처리**(애니 포즈 → IK 보정 → ComputeFinalTransforms)로 끼워 넣는 게 자연스럽다.
- `Engine/Public/Resource/Animator.h`의 `CAnimator`는 `GetFinalBoneMatrices()` / `GetGlobalBoneMatrices()`와 `TryGetBoneGlobalTransform(name, out)`를 노출 — 발/손 말단 본의 글로벌 위치를 IK 목표 계산에 바로 쓸 수 있다.
- **오픈월드 지형 위 발 접지**가 Winters의 1순위 IK 유스케이스(경사 지형·계단). MOBA 측은 챔피언 손/무기 정렬(예: 무기를 적에게 향하게) 정도.
- **핵심 긴장점(결정론)**: 발 접지가 단지 시각용이면 결정론 불필요. 하지만 접지가 히트박스·충돌·카메라에 반영되면 lockstep 재현 대상이 된다. Winters는 이미 `Shared/GameSim/Core/Determinism/DeterministicTime.h`(`kFixedDt = 1/30`, 30Hz 고정 틱)와 `DeterministicRng.h`(xorshift64) 같은 결정론 인프라를 갖추고 있어, **시각 전용 IK(클라이언트 로컬, 결정론 불필요)와 시뮬 반영 IK(결정론 필수)를 경계로 분리**하는 설계 결정이 곧 기여가 된다.

---

## 2. Spherical Harmonics (구면조화 — 조명/신호 표현 수학도구)

> SH는 **물리 시뮬이라기보다 "구면 위 신호를 저차원으로 표현하는 수학 도구"**다. 이 레퍼런스에선 IK·PBD와 함께 묶이지만, **실제 응용은 02_Graphics의 GI(Global Illumination)와 직접 연결**된다. 즉 SH는 GI 챕터의 표현·압축·전송(transport) 수학 기반이며, 여기서는 그 수학과 박사 각도를 정리한다.

### 2.1 핵심 원리(수학)

구면 위 함수 $f(\boldsymbol{\omega})$, $\boldsymbol{\omega}\in S^2$ (방향)는 실수 구면조화 기저 $Y_{l}^{m}(\boldsymbol{\omega})$로 전개된다:

$$
f(\boldsymbol{\omega}) = \sum_{l=0}^{\infty}\sum_{m=-l}^{l} c_{l}^{m}\, Y_{l}^{m}(\boldsymbol{\omega}),
\qquad
c_{l}^{m} = \int_{S^2} f(\boldsymbol{\omega})\, Y_{l}^{m}(\boldsymbol{\omega})\, d\boldsymbol{\omega}.
$$

$Y_l^m$는 **Legendre 다항식** $P_l^m$로 구성되며, $\{Y_l^m\}$은 $S^2$에서 **정규직교(orthonormal)** 기저다($\int Y_l^m Y_{l'}^{m'} = \delta_{ll'}\delta_{mm'}$). $l$이 **band(주파수)**, band $l$까지 쓰면 계수 개수는 $(l{+}1)^2$. 저주파 신호(부드러운 조명)는 **band 2(9개 계수)**로 거의 완전히 표현된다 — 이게 SH가 조명에 강력한 이유.

**핵심 정리들:**

1. **회전 불변(rotational property).** SH 계수 벡터에 회전을 적용하는 것은 각 band 내에서 **블록 대각 회전 행렬**(Wigner D-matrix) 곱이다. band $l$은 $(2l{+}1)\times(2l{+}1)$ 블록. 즉 환경을 통째로 돌려도 band를 섞지 않는다.
2. **Convolution = 곱(Funk–Hecke).** 회전 대칭 커널 $h$로의 구면 컨볼루션은 SH 영역에서 band별 스칼라 곱: $(f \ast h)_l^m = \hat{h}_l\, f_l^m$. **Diffuse(Lambert) BRDF의 cosine 커널은 band 2 이후로 급격히 0에 가까워진다.** 그래서:
3. **Irradiance ≈ band 2 (Ramamoorthi & Hanrahan 2001).** 환경광 $L(\boldsymbol{\omega})$로부터 법선 $\mathbf{n}$ 방향의 diffuse irradiance $E(\mathbf{n})$는 **9개 SH 계수와 $3\times3$/$4\times4$ 행렬 한 번**으로 평가된다 — 적분이 픽셀당 행렬-벡터곱으로 축약. 이론적으로 **band 2가 정확한 irradiance의 99% 에너지**를 담는다는 것이 그들의 핵심 결과.

$$
E(\mathbf{n}) \approx \mathbf{n}^{\!\top} \mathbf{M}\, \mathbf{n}, \qquad \mathbf{M}\in\mathbb{R}^{4\times4}\ \text{(9 SH 계수로 구성)}.
$$

4. **Precomputed Radiance Transfer (PRT, Sloan et al. 2002).** 정적 장면의 **자기그림자·상호반사를 포함한 transport**를 정점/텍셀마다 SH 벡터(또는 행렬)로 사전계산. 런타임엔 입사광 SH $\mathbf{L}$과 transport SH $\mathbf{T}$의 **내적(또는 행렬-벡터곱)** 한 번으로 그림자/GI 포함 셰이딩: $L_{\text{out}} = \mathbf{T}\cdot\mathbf{L}$. 동적 조명·고정 형상이라는 강한 가정이 핵심 한계.

> **open problem:** (1) **고차 SH 비용** — glossy/고주파는 band 2로 부족, band $N$이면 $(N{+}1)^2$ 계수 → ringing(Gibbs)과 메모리; (2) **동적 환경** — PRT는 형상이 고정이어야 함(움직이면 transport 무효). 동적 장면용 SH 프로브(예: irradiance volume)와 시간적 안정화가 미해결.

### 2.2 대표 기존 연구

- **Ramamoorthi & Hanrahan (2001)** — *An Efficient Representation for Irradiance Environment Maps* (SIGGRAPH). diffuse irradiance = 9 SH 계수의 이론·실험 증명.
- **Sloan, Kautz & Snyder (2002)** — *Precomputed Radiance Transfer for Real-Time Rendering in Dynamic, Low-Frequency Lighting Environments* (SIGGRAPH/TOG). PRT 원전.
- **Green (2003)** — *Spherical Harmonic Lighting: The Gritty Details* (GDC, 비심사지만 구현 표준 참고).
- **Sloan (2008)** — *Stupid Spherical Harmonics (SH) Tricks.* SH 회전·곱·zonal 근사 실무.
- **연결:** 02_Graphics의 GI — **DDGI(Majercik et al. 2019)**의 irradiance probe, Valve/Frostbite의 light probe가 모두 SH(또는 SH 유사 기저)로 환경 irradiance를 저장. SH는 그 표현층의 수학.

### 2.3 알고리즘(의사코드)

```text
# 환경맵 → band-2 irradiance 계수(9개) 투영
function ProjectEnvToSH2(envMap):
    c[0..8] = 0
    for each direction w_i (cubemap texel, solid angle dω_i):
        L = envMap.sample(w_i)                 # RGB radiance
        Y[0..8] = EvalSH2(w_i)                 # 9 basis values
        for k in 0..8: c[k] += L * Y[k] * dω_i # Monte-Carlo/quadrature 적분
    return c   # RGB 각 채널마다 9개

# 런타임 셰이딩: 법선 n → diffuse irradiance
function EvalIrradianceSH2(c, n):
    Y[0..8] = EvalSH2(n)
    # cosine-lobe band별 감쇠 A_l: A0=π, A1=2π/3, A2=π/4
    return Σ_k  A_l(k) * c[k] * Y[k]            # = nᵀ M n 형태로도 가능
```

```text
# PRT 사전계산(diffuse, shadowed) — 정점마다 transport 벡터
function PrecomputePRT(mesh):
    for each vertex v:
        t[0..(N+1)^2-1] = 0
        for s in 1..S samples (cosine-weighted over hemisphere(n_v)):
            w = SampleHemisphere(n_v)
            vis = 1 if not Occluded(v, w) else 0   # 자기그림자
            Y = EvalSH(w, N)
            for k: t[k] += vis * dot(n_v, w) * Y[k]
        t *= (1/S) * normalization
        v.transferSH = t
    # 런타임: L_out(v) = dot(v.transferSH, lightSH)
```

### 2.4 박사급 novel 각도

1. **동적 장면용 SH transport의 시간적 안정화.** PRT의 고정형상 가정을 깨는 게 핵심 open problem. 기여 후보: 이동/파괴 객체 주변의 **프로브 SH를 점진적으로 갱신**하면서 band-2 ringing과 프레임 간 flicker를 정량적으로 억제하는 갱신 규칙. (→ 02_Graphics GI 챕터와 한 학위논문으로 묶임.)
2. **결정론적 SH 프로브 베이킹.** Monte-Carlo 투영은 시드·합산 순서에 따라 계수가 갈린다. lockstep에서 **조명이 게임플레이(예: 시야·은신)에 영향을 주지 않는 한 결정론 불필요**하지만, 리플레이/서버-클라이언트 일관성 측면에서 **재현 가능한 SH 베이킹**(고정 quadrature, 합산 순서 고정)은 측정·검증 가능한 기여가 된다.
3. **band 적응(adaptive band).** 텍셀/프로브별로 신호 주파수에 따라 band 2~4를 동적 할당해 메모리-품질 파레토를 개선. (고차 SH 비용 open problem 직격.)

### 2.5 Thesis statement 예시

> "이동·파괴가 빈번한 장면에서 프로브별 band-2 SH irradiance를 시간적으로 안정화하는 점진적 갱신 규칙은, 매 프레임 재베이킹 대비 메모리·계산 동급에서 flicker(시간적 분산)를 X% 낮추면서 정적 PRT와 동등한 diffuse 품질(ΔE)을 동적 장면으로 확장한다."

### 2.6 평가 방법

- **정확도**: ground-truth(오프라인 path tracer)의 irradiance와 SH 근사의 ΔE / RMSE, band 수 대비 오차 곡선.
- **안정성**: 카메라·객체 이동 시 프레임 간 셰이딩 분산(flicker), ringing 가시성.
- **성능**: 투영 ms, 프로브당 메모리(계수 수×채널), 런타임 평가 비용.
- **Baseline/Ablation**: band 1 vs 2 vs 4, 정적 PRT vs 제안 동적 갱신, 시간적 필터 on/off.

### 2.7 Winters 연결점

- **직접 연결은 02_Graphics의 GI/앰비언트.** Winters의 오픈월드(엘든링풍) 야외 조명에서 **diffuse ambient를 SH 프로브로 표현**하면, 챔피언/미니언 다수에 저렴한 환경광을 줄 수 있다.
- 캐릭터가 동굴↔야외를 오갈 때 프로브 SH 보간 → 시간적 안정화가 곧 위 §2.4 기여.
- **결정론 관점**: 조명은 보통 게임플레이에 영향을 주지 않으므로(시야 시스템은 별도) SH는 **결정론 의무에서 면제되는 좋은 예** — 이 경계 정의 자체가 §종합 결정론 논문의 "무엇을 재현 대상에서 뺄 것인가"를 명확히 한다.

---

## 3. Position Based Dynamics (PBD / XPBD)

### 3.1 핵심 원리(수학)

전통적 물리는 힘→가속도→속도→위치($\mathbf{F}\!\to\!\mathbf{a}\!\to\!\mathbf{v}\!\to\!\mathbf{x}$)로 적분한다. 강한 스프링은 explicit Euler에서 **stiff** 문제가 되어 작은 timestep을 강요하거나 폭발한다. **PBD(Müller et al. 2007)**는 발상을 뒤집는다: **위치를 제약 함수가 만족되도록 직접 투영**하고, 속도는 위치 변화에서 역산한다.

제약 $C(\mathbf{x}) = 0$ (등식) 또는 $C(\mathbf{x}) \ge 0$ (부등식). 예측 위치 $\mathbf{x}'$에서 제약을 1차 Taylor로 선형화해 **질량 가중 위치 보정** $\Delta\mathbf{x}$를 구한다. 보정은 제약 기울기 $\nabla C$ 방향이며 운동량 보존(질량 가중)을 만족:

$$
\Delta\mathbf{x}_i = -\,w_i\,\lambda\,\nabla_{\mathbf{x}_i} C,
\qquad
\lambda = \frac{C(\mathbf{x})}{\sum_j w_j \,\lVert \nabla_{\mathbf{x}_j} C \rVert^2},
\qquad w_i = 1/m_i.
$$

거리 제약 $C = \lVert\mathbf{p}_1-\mathbf{p}_2\rVert - d$의 경우 위 식이 그 plan에 적힌 보정식으로 환원된다.

**PBD의 근본 문제 (왜 XPBD가 필요한가):** PBD의 보정에 강성 계수 $k\in[0,1]$를 곱해 "부드러움"을 흉내내지만, **수렴된 강성이 반복 횟수와 timestep에 의존**한다 — 같은 천이 반복 10회와 20회에서 다른 뻣뻣함을 보인다. 물리적으로 무의미하다.

**XPBD (Macklin et al. 2016)**는 이를 **compliance** $\alpha$(강성의 역수, 물리 단위)로 정식화해 해결한다. 암시적(implicit) 적분에서 Lagrange multiplier $\lambda$를 직접 풀며, 보정 증분은

$$
\Delta\lambda = \frac{-\,C(\mathbf{x}) - \tilde{\alpha}\,\lambda}{\sum_j w_j\lVert\nabla_j C\rVert^2 + \tilde{\alpha}},
\qquad
\tilde{\alpha} = \frac{\alpha}{\Delta t^2},
\qquad
\Delta\mathbf{x}_i = w_i\,\nabla_i C\,\Delta\lambda.
$$

$\alpha \to 0$이면 PBD의 rigid 제약으로 수렴하고, $\alpha$가 커지면 물리적으로 정의된 부드러움이 된다 — **반복 횟수·timestep과 무관하게 일관**. $\lambda$는 한 substep 동안 누적된다.

**Small Steps (Macklin et al. 2019).** "반복(iteration)을 늘리기보다 **substep(작은 timestep)을 늘려라**." 같은 연산 예산에서 substep $n$회·반복 1회가 substep 1회·반복 $n$회보다 **에너지 보존·강성 정확도·수렴**이 모두 낫다는 분석. 현대 XPBD의 사실상 표준 루프가 됐다.

> **open problem:** (1) **강성 일관성** — XPBD가 많이 개선했으나 부등식 제약/마찰/접촉의 일관성은 여전히 까다로움; (2) **결정론** — 제약 투영 순서(Gauss–Seidel)·부동소수점 누적이 비결정론의 원천(Winters 핵심); (3) **GPU 대규모** — graph coloring(Jacobi식 병렬)은 결정론엔 유리하나 수렴이 느려지는 trade-off.

### 3.2 대표 기존 연구

- **Müller, Heidelberger, Hennix & Ratcliff (2007)** — *Position Based Dynamics* (Journal of Visual Communication and Image Representation; VRIPHYS 2006). PBD 원전.
- **Macklin, Müller, Chentanez & Kim (2014)** — *Unified Particle Physics for Real-Time Applications* (SIGGRAPH/TOG). 강체·천·유체·기체를 입자+제약으로 통합 → **NVIDIA Flex**.
- **Macklin, Müller & Chentanez (2016)** — *XPBD: Position-Based Simulation of Compliant Constrained Dynamics* (MIG). compliance 정식화.
- **Macklin, Storey, Lu et al. (2019)** — *Small Steps in Physics Simulation* (SCA). substepping 분석.
- **Bender, Müller & Macklin (2017)** — *A Survey on Position Based Dynamics* (Eurographics tutorial). 분야 정리.
- 참고: **Müller et al. (2020)** — *Detailed Rigid Body Simulation with XPBD* (SCA), 강체로 확장.

### 3.3 알고리즘(의사코드)

```text
# XPBD substep 루프 (Small Steps: substeps 多, iter=1)
function SimulateXPBD(particles, constraints, dt, substeps):
    h = dt / substeps
    for s in 1..substeps:
        for each particle i:                       # 1) 예측 (외력)
            x_prev[i] = x[i]
            v[i]     += h * f_ext[i] * w[i]
            x[i]     += h * v[i]
        for each constraint c:  c.lambda = 0        # 2) λ 초기화
        for iter in 1..K:                           # 보통 K=1
            for each constraint c (deterministic order):  # 3) 투영
                ProjectXPBD(c, x, w, h)
        for each particle i:                        # 4) 속도 역산
            v[i] = (x[i] - x_prev[i]) / h
        # (선택) 속도 레벨 마찰/감쇠

function ProjectXPBD(c, x, w, h):
    C   = c.Evaluate(x)
    grad = c.Gradient(x)                            # ∇C per involved particle
    wsum = Σ_j  w[j] * |grad_j|^2
    a_tilde = c.compliance / (h*h)
    dlam = (-C - a_tilde * c.lambda) / (wsum + a_tilde)
    c.lambda += dlam
    for each involved particle j:
        x[j] += w[j] * grad_j * dlam
```

```text
# Graph Coloring 병렬 투영 (GPU/Job, Jacobi식 → 결정론 친화)
function ColorConstraints(constraints):
    # 같은 색 = 공유 파티클 없음 → 병렬 투영 시 data race 없음
    assign colors greedily by particle adjacency
function ParallelProject(colors):
    for color in colors:                # 색 순서는 고정(결정론)
        parallel_for c in color:        # 색 내부는 독립 → 순서 무관
            ProjectXPBD(c, ...)
```

### 3.4 박사급 novel 각도

1. **결정론적 XPBD (cross-platform bit-exact).** Gauss–Seidel 투영은 순서 의존이라 비결정론적이다. 기여 후보: **graph-coloring Jacobi + 고정 색 순서 + 명세된 부동소수점 축약**으로 x86/ARM 비트 일치를 보장하면서, Jacobi의 느린 수렴을 substepping으로 보상해 품질·성능을 유지. ← **그래픽스 물리 문헌이 거의 다루지 않는 각도** (그쪽은 결정론이 요구사항이 아니므로).
2. **강성 일관성의 정량적 경계.** XPBD의 compliance가 substep·반복·해상도에 얼마나 불변인지 **스펙트럼 분석/에너지 측정**으로 경계를 제시. Small Steps의 정성 논증을 정량 정리로.
3. **lockstep 예산 안의 적응 substepping.** 30Hz 고정 틱(Winters의 `kFixedDt`) 안에서, 장면 복잡도에 따라 substep 수를 바꾸되 **결정론을 깨지 않는**(모든 클라가 동일 substep 결정 규칙) 적응 스케줄.

### 3.5 Thesis statement 예시

> "graph-coloring 기반 Jacobi XPBD에 고정 색 순서와 명세된 부동소수점 축약을 적용하면, 30Hz lockstep MOBA에서 망토/천 시뮬을 x86/ARM 비트 단위로 재현하면서, 동일 연산 예산의 Gauss–Seidel XPBD 대비 동등 강성 일관성과 60fps 렌더를 달성한다."

### 3.6 평가 방법

- **정확도/일관성**: 거리 제약 잔차 $|C|$, 천의 정상상태 처짐을 해석/고해상도 reference와 비교, **강성이 substep/반복에 불변인지** 측정(같은 $\alpha$로 substep 스윕).
- **안정성**: 에너지 drift, 큰 timestep에서 폭발 여부, 시간적 jitter.
- **성능**: 입자 수 대비 ms, M-particles/s, GPU/Job scaling.
- **결정론**: 동일 입력으로 플랫폼 간 입자 위치 state-hash 일치율. **Baseline**: Gauss–Seidel XPBD, mass-spring + explicit Euler. **Ablation**: 색 순서 고정 제거 / FMA 자유화 / Gauss–Seidel↔Jacobi / substep 1 vs n.

### 3.7 Winters 연결점

- **망토/천(엘든링풍 캐릭터의 로브·망토)**, MOBA 측 부서지는 깃발·천 장식이 PBD 유스케이스. 기존 계획 `.md/plan/physics/07_STAGE6_PBD.md`에 `CClothSolver`(격자 distance+bending), `CRopeSolver`, `CSoftBodySolver`, XPBD compliance, **graph coloring 병렬화**, GPU compute 이식까지 이미 설계되어 있어 testbed가 준비됨.
- 천이 캐릭터 본을 따라가야 하므로 §1 IK/스켈레톤(`CAnimator::GetGlobalBoneMatrices`)과 결합 — 어태치 본의 글로벌 트랜스폼이 천의 pinned 파티클 목표.
- **핵심 긴장점**: 망토가 순수 시각 효과면 클라이언트 로컬·결정론 불필요(가장 단순·실용적). 하지만 천이 게임플레이(피격 판정·시야 차폐)에 영향을 주는 순간 lockstep 재현 대상 → §3.4의 결정론 XPBD가 필요. Winters의 30Hz 고정 틱(`DeterministicTime::kFixedDt`)이 substep 예산의 상한을 정의한다.

---

## 4. FFT 기반 해양 시뮬레이션 (Ocean Simulation)

### 4.1 핵심 원리(수학)

큰 바다 표면을 입자/격자 PDE로 직접 풀면 비싸다. **Tessendorf(2001)**의 통찰: 해양 표면을 **여러 주파수 정현파의 중첩**으로 보고, **통계적 해양 스펙트럼**으로 각 주파수의 진폭을 정하면, 표면 높이장은 스펙트럼의 **역푸리에변환(IFFT)**으로 한 번에 합성된다.

주파수(파수) $\mathbf{k}$에서의 시간 변화 진폭:

$$
\tilde{h}(\mathbf{k}, t) = \tilde{h}_0(\mathbf{k})\, e^{\,i\,\omega(k)\,t} + \tilde{h}_0^{*}(-\mathbf{k})\, e^{-i\,\omega(k)\,t}.
$$

이 켤레 대칭 형태는 합성된 높이장 $h(\mathbf{x},t)$가 **실수**가 되도록 보장한다. 초기 스펙트럼 $\tilde{h}_0$는 표준정규난수 $\xi$와 해양 스펙트럼 $P(\mathbf{k})$로:

$$
\tilde{h}_0(\mathbf{k}) = \tfrac{1}{\sqrt{2}}\,(\xi_r + i\,\xi_i)\,\sqrt{P(\mathbf{k})}.
$$

**Phillips 스펙트럼**(완전발달 바다): $L = V^2/g$ (바람속 $V$, 중력 $g$), 바람방향 $\hat{\mathbf{w}}$,

$$
P(\mathbf{k}) = A\,\frac{\exp\!\big(-1/(kL)^2\big)}{k^4}\,\lvert \hat{\mathbf{k}}\cdot\hat{\mathbf{w}} \rvert^2.
$$

(고주파 수렴을 위해 $\exp(-k^2 \ell^2)$ 작은 파장 억제를 곱함.) 대안으로 **JONSWAP**(fetch 제한, peak 강조 $\gamma$)·**Pierson–Moskowitz** 스펙트럼이 쓰인다 — fetch(바람이 분 거리)가 유한한 연안에 더 현실적.

**분산 관계(deep water):** $\omega(k) = \sqrt{g\,k}$. 유한 수심 $d$: $\omega(k)=\sqrt{g\,k\,\tanh(kd)}$.

**높이장 합성:** $h(\mathbf{x}, t) = \text{IFFT}\{\tilde{h}(\mathbf{k},t)\}$.

**Choppy(수평 변위, Gerstner-like):** 파마루를 날카롭게 만들려면 수평 변위장 $\mathbf{D}$를 추가:

$$
\mathbf{D}(\mathbf{x}, t) = \text{IFFT}\!\left\{ -i\,\frac{\mathbf{k}}{k}\,\tilde{h}(\mathbf{k},t) \right\}.
$$

**법선·거품:** 기울기 $\partial h/\partial x = \text{IFFT}\{i k_x \tilde h\}$ 등으로 법선. 변위가 접히는 곳은 **Jacobian** $J<0$ → 거품(foam):

$$
J = (1+\partial D_x/\partial x)(1+\partial D_z/\partial z) - (\partial D_x/\partial z)(\partial D_z/\partial x).
$$

FFT는 주기적이므로 결과 타일은 **tileable**(이음매 없는 반복) — 큰 바다를 타일 인스턴싱으로 덮는다.

> **open problem:** (1) **대규모 타일** — 단일 주기 타일은 가까이서 반복이 보임; cascade(여러 주파수 대역 타일)와 LOD가 필요; (2) **상호작용** — 배·캐릭터가 만드는 wake/물보라는 통계적 스펙트럼 모델에 없음(국소 동적 결합); (3) 결정론(아래 §4.4).

### 4.2 대표 기존 연구

- **Tessendorf (2001/2004)** — *Simulating Ocean Water* (SIGGRAPH course notes). FFT 해양의 정전(canonical).
- **Mastin, Watterberg & Mareda (1987)** — FFT로 해수면 합성한 초기 그래픽스 연구(스펙트럼→IFFT 발상의 뿌리).
- **Jensen & Golias (2001)** — *Deep-Water Animation and Rendering* (실시간·거품).
- **Hasselmann et al. (1973)** — JONSWAP 스펙트럼(해양공학 원전), **Pierson & Moskowitz (1964)** — PM 스펙트럼.
- **Dupuy & Bruneton (2012)** — *Real-time Animation and Rendering of Ocean Whitecaps* (cascade·거품 실시간).
- 산업 참고(비심사): **GPU Gems 2 Ch.18 (Mitchell)**, Crest/UE Water 등.

### 4.3 알고리즘(의사코드)

```text
# 사전: 초기 스펙트럼 h0(k) (Phillips) — 한 번만
function InitSpectrum(N, L_patch, wind):
    for (m,n) in N x N grid:
        k = 2π * (m - N/2, n - N/2) / L_patch
        P = Phillips(k, wind)
        (xr, xi) = GaussianPair(seed(m,n))      # 결정론: 고정 시드 함수
        h0[m,n]      = (xr + i*xi)/sqrt(2) * sqrt(P)
        h0conj[m,n]  = conj( (xr'+i*xi')/sqrt(2) * sqrt(Phillips(-k,wind)) )

# 매 프레임: 시간 진화 → IFFT → 변위/법선/거품
function UpdateOcean(t):
    for (m,n):
        w = sqrt(g * |k|)                        # 분산관계
        h_tilde[m,n] = h0[m,n]*exp(i*w*t) + h0conj[m,n]*exp(-i*w*t)
        Dx_tilde[m,n] = -i * (k.x/|k|) * h_tilde[m,n]   # choppy
        Dz_tilde[m,n] = -i * (k.z/|k|) * h_tilde[m,n]
        slopeX[m,n]   =  i * k.x * h_tilde[m,n]          # 법선용
        slopeZ[m,n]   =  i * k.z * h_tilde[m,n]
    height = IFFT2(h_tilde)                      # 실수부
    Dx = IFFT2(Dx_tilde);  Dz = IFFT2(Dz_tilde)
    nrm = NormalsFromSlopes(IFFT2(slopeX), IFFT2(slopeZ))
    foam = where( Jacobian(Dx,Dz) < 0 )          # 접힘 → 거품
    write displacement/normal/foam textures
```

(2D IFFT = 행 1D FFT → 열 1D FFT. Cooley–Tukey radix-2는 `.md/plan/graphics/07_STAGE6_FFT.md`에 GPU/CPU 양쪽 구현 스케치가 있음.)

### 4.4 박사급 novel 각도

1. **결정론적 GPU FFT 해양 (lockstep 친화).** 부동소수점 IFFT는 합산 순서·GPU 모델에 따라 비트가 갈린다. 기여 후보: 해수면 높이가 **게임플레이(수영·배 부력·수면 위 이동)에 영향**을 줄 때, 표면 질의를 **CPU 결정론 경로(고정소수점/명세된 합산)**로 분리하고 시각 GPU FFT와 분리하는 아키텍처. ← 해양 FFT 문헌엔 거의 없는 각도.
2. **양방향 상호작용(two-way coupling) on 통계 모델.** 통계 스펙트럼 위에 배/캐릭터 wake를 국소 PDE(또는 PBD 입자)로 더해 **합성장 + 동적장**을 결합하면서 비용·안정성을 분석. open problem(상호작용) 직격.
3. **cascade 타일의 LOD·반복 가시성 정량화.** 다중 대역 타일의 이음매·반복 artifact를 perceptual하게 측정하고 최소 비용 cascade 구성을 도출.

### 4.5 Thesis statement 예시

> "게임플레이에 영향을 주는 수면 질의를 결정론적 CPU 경로로 분리하고 시각 합성을 GPU IFFT에 두는 이원화 해양 아키텍처는, lockstep 오픈월드에서 배·캐릭터의 부력/수영 판정을 cross-platform 재현하면서 512×512 cascade 해양을 2ms 이내에 렌더한다."

### 4.6 평가 방법

- **정확도**: 합성 스펙트럼의 통계(유의파고 $H_s$, 파장 분포)가 목표 스펙트럼과 일치하는지, 거품 위치의 물리적 타당성.
- **성능**: IFFT ms(해상도·cascade 수 대비), 메모리.
- **안정성/품질**: 타일 반복·이음매 가시성, 시간적 일관성.
- **결정론**: 수면 질의(높이/법선)의 플랫폼 간 일치율. **Baseline**: 단일 타일 FFT, Gerstner 합(§5). **Ablation**: cascade 수, choppy on/off, CPU/GPU 질의 경로 분리 여부.

### 4.7 Winters 연결점

- **오픈월드의 물(바다·큰 호수)** 이 유스케이스. 그래픽스 측 FFT 인프라가 `.md/plan/graphics/07_STAGE6_FFT.md`(Cooley–Tukey CPU/GPU, Phillips, dispersion, choppy, Jacobian foam)에 이미 설계됨 — 시각 해양은 그래픽스 챕터, 물리(부력·수영 판정)는 이 챕터.
- **핵심 긴장점(결정론)**: MOBA는 평지가 대부분이라 해양이 게임플레이에 거의 없음 → 해양은 보통 **시각 전용·결정론 면제**(가장 단순). 그러나 오픈월드에서 수영/배가 게임플레이가 되면 §4.4의 결정론 분리가 필요. 30Hz 고정 틱(`DeterministicTime`) 상에서 CPU 질의 경로만 재현하면 GPU 시각은 자유롭게 둘 수 있다는 설계 분리가 곧 기여.

---

## 5. Gerstner Waves (트로코이드 파)

### 5.1 핵심 원리(수학)

FFT 해양이 "수만 주파수 통계 합성"이라면, **Gerstner wave**는 **소수의 정현파를 명시적으로 더하는** 저비용 모델이다. 핵심 차이: 정점이 위아래로만 움직이는 단순 사인파와 달리, Gerstner는 정점을 **원궤도(circular/trochoidal)**로 움직여 **파마루는 뾰족, 파골은 평평**한 실제 바다 단면(trochoid)을 만든다.

단일 Gerstner wave (파수벡터 $\mathbf{k}$, 진폭 $A$, 각속도 $\omega$, 방향 $\hat{\mathbf{d}}=\mathbf{k}/k$, 위상 $\phi$). 정지격자점 $\mathbf{x}_0=(x_0,z_0)$가 시간 $t$에 가는 위치:

$$
\begin{aligned}
x &= x_0 - \hat{d}_x\,A\,\sin(\mathbf{k}\cdot\mathbf{x}_0 - \omega t + \phi),\\
z &= z_0 - \hat{d}_z\,A\,\sin(\mathbf{k}\cdot\mathbf{x}_0 - \omega t + \phi),\\
y &= A\,\cos(\mathbf{k}\cdot\mathbf{x}_0 - \omega t + \phi).
\end{aligned}
$$

수평 성분이 정점을 마루 쪽으로 모아 trochoid 형태를 만든다. 여러 파를 **합**하면 그럴듯한 바다가 된다(보통 4~16개). 분산 관계는 FFT와 동일하게 $\omega=\sqrt{gk}$(deep water).

**steepness(가파름) 제어:** 진폭 대신 정규화 가파름 $Q_i$를 써서 여러 파를 더할 때 정점이 서로를 뚫고 접히는(self-intersection) 것을 막는다 — $Q_i = q/(\omega_i A_i\, W)$ ($W$=파 개수). $Q$가 1을 넘으면 마루가 접혀 루프가 생긴다.

**법선(해석적):** Gerstner는 위치가 닫힌형이라 **법선도 미분으로 정확히** 얻는다(FFT처럼 IFFT 추가 패스가 필요 없음). 접선 $\partial(x,y,z)/\partial x_0$, $\partial/\partial z_0$의 외적.

**FFT 대비:** 비용이 파 개수에 선형($O(W)$, 정점 셰이더에서 directly), 작은 호수·미관용·모바일/저사양에 적합. 단, 큰 바다의 풍부한 디테일(고주파 다양성)은 FFT가 우월.

> **open problem:** (1) **해안 상호작용** — 수심 감소 시 파가 느려지고 높아지며 부서지는(shoaling/breaking) 비선형 현상은 단순 합으로 표현 불가; (2) **LOD** — 거리별 파 개수/디테일 전환의 매끄러움; (3) FFT와의 하이브리드(근거리 Gerstner 디테일 + 원거리 FFT).

### 5.2 대표 기존 연구

- **Fournier & Reeves (1986)** — *A Simple Model of Ocean Waves* (SIGGRAPH). Gerstner/trochoid를 컴퓨터그래픽스 해양에 도입한 정전. (동시기 **Peachey 1986**도 해양 파 모델 제시.)
- **Finch (2004)** — *Effective Water Simulation from Physical Models* (**GPU Gems** Ch.1). 정점 셰이더 Gerstner 합·steepness·법선의 실무 표준 레시피.
- 고전 유체역학 배경: **Gerstner (1809)**의 trochoidal wave 해 — 비선형 자유표면파의 고전 해석해.
- 하이브리드/후속(참고): UE/Crest의 근거리 Gerstner + 원거리 FFT 합성.

### 5.3 알고리즘(의사코드)

```text
# 정점 셰이더: W개의 Gerstner wave 합 (위치 + 해석적 법선)
function GerstnerVertex(x0, z0, t, waves[0..W-1]):
    P = (x0, 0, z0)
    T = (1,0,0);  B = (0,0,1)             # 접선 누적(법선용)
    for w in waves:
        d   = normalize(w.dir)            # 진행 방향(xz)
        k   = w.k                         # 파수 = 2π/wavelength
        f   = k * dot(d, (x0,z0)) - w.omega*t + w.phase
        Q   = w.steepness / (w.omega * w.amp * W)   # 가파름(접힘 방지)
        s   = sin(f);  c = cos(f)
        WA  = w.omega * w.amp
        # 위치 변위
        P.x += Q * w.amp * d.x * c
        P.z += Q * w.amp * d.y * c
        P.y += w.amp * s
        # 접선/종접선 누적 (해석적 미분)
        T += (-Q*d.x*d.x*WA*s, d.x*WA*c, -Q*d.x*d.y*WA*s)
        B += (-Q*d.x*d.y*WA*s, d.y*WA*c, -Q*d.y*d.y*WA*s)
    N = normalize(cross(B, T))
    return (P, N)
```

```text
# 해안 shoaling 근사(novel 방향): 수심 d로 파라미터 변조
function ShoalingModulate(wave, depth):
    wave.omega = sqrt(g * wave.k * tanh(wave.k * depth))   # 유한수심 분산
    # 수심 얕아지면 파장↓·진폭↑ (에너지 보존 근사), 임계서 breaking 플래그
    if depth < breakDepth(wave): wave.foam = true
    return wave
```

### 5.4 박사급 novel 각도

1. **결정론적·저비용 물(MOBA/모바일).** Gerstner는 닫힌형이라 본질적으로 FFT보다 **결정론에 유리**(IFFT 합산 비결정성 없음). 기여 후보: 게임플레이 영향이 있는 물에서 **Gerstner 합을 결정론 표면 모델로** 쓰고, 비용·정확도를 FFT와 정량 비교(파 개수 대비 perceptual quality vs 결정론 보장).
2. **해안 shoaling/breaking 근사의 실시간 모델.** 단순 합으로 못 푸는 비선형 연안 현상을 수심 의존 파라미터 변조 + breaking 휴리스틱으로 근사하고 **사실성-비용 파레토**를 제시(open problem 직격).
3. **Gerstner↔FFT 하이브리드의 매끄러운 LOD.** 근거리 해석적 Gerstner 디테일과 원거리 FFT 타일을 거리별로 블렌딩하며 popping/이음매를 정량 최소화.

### 5.5 Thesis statement 예시

> "수심 의존 파라미터 변조와 breaking 휴리스틱을 더한 Gerstner 합은, 연안 오픈월드에서 FFT 해양 대비 1/10 비용으로 shoaling을 표현하면서, 닫힌형 특성상 게임플레이 수면 질의를 cross-platform 결정론으로 제공한다."

### 5.6 평가 방법

- **정확도/사실성**: 단면 trochoid 형태·shoaling 거동의 정성·정량(파고 증가율) 비교, perceptual user study(FFT vs Gerstner).
- **성능**: 파 개수 대비 정점 셰이더 비용, 모바일/저사양 frame time.
- **안정성**: steepness 한계에서 접힘(self-intersection) 발생 경계.
- **결정론**: 수면 질의 플랫폼 간 일치율(Gerstner는 닫힌형이라 높을 것으로 가설). **Baseline**: 단순 사인파 합, FFT(§4). **Ablation**: 파 개수, steepness $Q$, shoaling on/off.

### 5.7 Winters 연결점

- **저비용 물(MOBA 강/호수, 오픈월드 근거리 물가)** 이 유스케이스. FFT가 과한 상황의 1순위 — 적은 파로 정점 셰이더에서 직접.
- **핵심 긴장점(결정론)**: Gerstner는 닫힌형·해석적 법선이라 **결정론 비용이 거의 0** — 게임플레이 영향 수면(부력·수영)이 필요할 때 §4의 FFT보다 결정론 친화적. 즉 "시각 디테일이 더 중요 → FFT(시각 전용)", "결정론·저비용이 더 중요 → Gerstner"라는 **분기 자체가 설계 기여**.
- 30Hz 고정 틱(`DeterministicTime::kFixedDt`)에서 Gerstner 위상 $\omega t$의 $t$를 **틱→초 변환(`DeterministicTime::TickToSec`)**으로 계산하면 표면이 결정론적으로 재현된다.

---

## 종합. 통합 학위논문 구조 예시 — "네트워크 결정론 물리"를 three-papers로

가이드 §4의 "Three Papers Make a Thesis"를 이 분야에 적용한다. **하나의 분야(실시간 물리) 안에서 인접한 세 문제**를 골라, 이들을 하나의 명제로 묶는다. Winters의 정체성(서버 권위 + lockstep + 오픈월드)에서 가장 자연스러운 묶음은 **"cross-platform 결정론적 실시간 물리"**다.

### 학위논문 명제(umbrella thesis)

> "실시간 그래픽스 물리(IK·천·물)는 부동소수점 비결정성으로 인해 lockstep 네트워크에서 cross-platform 재현이 불가능하다고 여겨져 왔다. 본 논문은 **제약/적분 순서의 결정론적 정렬 + 명세된 부동소수점 축약 + 시각/시뮬 경계 분리**라는 공통 레시피로, IK·XPBD 천·해양 표면 질의를 x86/ARM 비트 단위로 재현하면서 60fps와 SOTA 동등 품질을 달성함을 보인다."

### 세 논문(= 기여 챕터 3)

```text
Paper 1 (SCA/TOG)   : 결정론적 XPBD
   - graph-coloring Jacobi + 고정 색 순서 + reproducible FMA
   - 기여: cross-platform bit-exact 천 시뮬, Gauss–Seidel 대비 품질·성능 동등
   - 평가: 플랫폼 간 state-hash 일치율, 강성 일관성, ms

Paper 2 (SCA/EG)    : 결정론적 full-body IK + 지형 발 접지
   - 고정 반복 FABRIK + 해석적 2-bone 접지 + 순회 정렬
   - 기여: 발 접지가 시뮬에 반영되어도 재현 보장, jitter·접지 정밀 정량화
   - 평가: 말단 오차, flicker, 재현률

Paper 3 (TOG)       : 시각/시뮬 이원화 물(物) 표면 (FFT 시각 + 결정론 질의)
   - GPU FFT/Gerstner 시각 + CPU 결정론 수면 질의 분리
   - 기여: 부력/수영 판정 cross-platform 재현 + 고품질 시각 양립
   - 평가: 수면 질의 일치율, 시각 품질, ms

엮는 글(Ch1 서론 / Ch N 결론):
   - 공통 레시피(정렬·FMA·경계분리)가 세 도메인에 일반화됨을 논증
   - "무엇을 재현 대상에서 뺄 것인가"(시각 전용 SH·시각 해양)의 원칙 정립
```

### 왜 이게 "구현"이 아니라 "박사"인가 (가이드 §1·§12 자가진단)

- **Novelty**: 그래픽스 물리 문헌은 결정론을 요구사항으로 다루지 않는다. 네트워크 문헌(10_Server)은 물리를 단순화(고정소수점 정수 물리)해 회피한다. **"고품질 실시간 물리 × cross-platform 결정론"의 교집합**이 비어 있다.
- **Rigor**: 모든 기여가 **재현률·품질·ms·안정성** 4축으로 측정되고 baseline(비결정 SOTA)과 ablation을 갖춘다.
- **Significance**: lockstep e스포츠/리플레이/서버 검증에서 물리를 쓸 수 있게 한다(부정행위 탐지·관전·결정론 리플레이).
- **한 줄 자가진단**: thesis = 위 umbrella; 증명 실험 = 플랫폼 간 state-hash + 품질/성능; 비교 = 비결정 SOTA(Gauss–Seidel XPBD, 비고정 DLS, 단일 FFT). 셋 다 즉답 가능 → 박사 단계.

> **주의(scope, 가이드 §11):** 위 세 논문조차 5~6년이다. **셋 다가 아니라 Paper 1 하나만으로도** 충분한 박사 핵심이 된다("결정론적 XPBD" 단독 학위논문). §1의 "분야 목록을 다 자르지 말라"를 여기서도 지킨다 — IK·SH·FFT·Gerstner는 **선택지(area)**이지 의무 목차가 아니다.

---

## 참고문헌

**Inverse Kinematics**
- Wolovich, W. A., & Elliott, H. (1984). *A computational technique for inverse kinematics.* IEEE CDC.
- Buss, S. R., & Kim, J.-S. (2005). *Selectively Damped Least Squares for Inverse Kinematics.* Journal of Graphics Tools, 10(3).
- Aristidou, A., & Lasenby, J. (2011). *FABRIK: A fast, iterative solver for the Inverse Kinematics problem.* Graphical Models, 73(5).
- Aristidou, A., Lasenby, J., Chrysanthou, Y., & Shamir, A. (2018). *Inverse Kinematics Techniques in Computer Graphics: A Survey.* Computer Graphics Forum, 37(6).
- Oreshkin, B. N., et al. (2022). *ProtoRes: Proto-Residual Architecture for Deep Modeling of Human Pose.* ICLR.

**Spherical Harmonics / PRT**
- Ramamoorthi, R., & Hanrahan, P. (2001). *An Efficient Representation for Irradiance Environment Maps.* SIGGRAPH.
- Sloan, P.-P., Kautz, J., & Snyder, J. (2002). *Precomputed Radiance Transfer for Real-Time Rendering in Dynamic, Low-Frequency Lighting Environments.* ACM TOG (SIGGRAPH).
- Green, R. (2003). *Spherical Harmonic Lighting: The Gritty Details.* GDC (industry).
- Sloan, P.-P. (2008). *Stupid Spherical Harmonics (SH) Tricks.* GDC (industry).
- Majercik, Z., et al. (2019). *Dynamic Diffuse Global Illumination with Ray-Traced Irradiance Fields (DDGI).* JCGT. (→ 02_Graphics)

**Position Based Dynamics**
- Müller, M., Heidelberger, B., Hennix, M., & Ratcliff, J. (2007). *Position Based Dynamics.* J. Visual Communication and Image Representation, 18(2).
- Macklin, M., Müller, M., Chentanez, N., & Kim, T.-Y. (2014). *Unified Particle Physics for Real-Time Applications.* ACM TOG (SIGGRAPH).
- Macklin, M., Müller, M., & Chentanez, N. (2016). *XPBD: Position-Based Simulation of Compliant Constrained Dynamics.* MIG.
- Macklin, M., Storey, K., Lu, M., et al. (2019). *Small Steps in Physics Simulation.* SCA.
- Bender, J., Müller, M., & Macklin, M. (2017). *A Survey on Position Based Dynamics.* Eurographics Tutorials.

**FFT Ocean**
- Tessendorf, J. (2001/2004). *Simulating Ocean Water.* SIGGRAPH Course Notes.
- Mastin, G. A., Watterberg, P. A., & Mareda, J. F. (1987). *Fourier Synthesis of Ocean Scenes.* IEEE CG&A.
- Hasselmann, K., et al. (1973). *Measurements of wind-wave growth and swell decay (JONSWAP).* Deutsche Hydrographische Zeitschrift.
- Pierson, W. J., & Moskowitz, L. (1964). *A proposed spectral form for fully developed wind seas.* J. Geophysical Research.
- Jensen, L. S., & Golias, R. (2001). *Deep-Water Animation and Rendering.* Gamasutra/GDC.
- Dupuy, J., & Bruneton, E. (2012). *Real-time Animation and Rendering of Ocean Whitecaps.* SIGGRAPH Asia.
- Cooley, J. W., & Tukey, J. W. (1965). *An Algorithm for the Machine Calculation of Complex Fourier Series.* Math. Comp.

**Gerstner Waves**
- Fournier, A., & Reeves, W. T. (1986). *A Simple Model of Ocean Waves.* SIGGRAPH.
- Peachey, D. R. (1986). *Modeling Waves and Surf.* SIGGRAPH.
- Finch, M. (2004). *Effective Water Simulation from Physical Models.* GPU Gems, Ch.1 (industry).
- Gerstner, F. J. (1809). *Theorie der Wellen.* (고전 trochoidal wave 해석해.)

**Winters 내부 참고(코드·계획)**
- `Engine/Public/Resource/Skeleton.h`, `Engine/Public/Resource/Animator.h`, `Engine/Public/Resource/Bone.h` — 본/스켈레톤/애니 파이프라인(IK 연결점).
- `Shared/GameSim/Core/Determinism/DeterministicTime.h`(30Hz `kFixedDt`), `DeterministicRng.h`(xorshift64) — 결정론 인프라.
- `.md/plan/physics/07_STAGE6_PBD.md` — PBD/XPBD 천·로프·소프트바디 설계.
- `.md/plan/graphics/07_STAGE6_FFT.md` — Cooley–Tukey FFT·Tessendorf 해양 설계(시각 측).
