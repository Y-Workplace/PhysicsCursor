# PhysicsCursor - Simulação Física de Cursor no CachyOS (Wayland / Hyprland)

Aplicativo nativo em **C++20** com **SDL3** projetado para rodar em **CachyOS** sob **Wayland** (e compositores como Hyprland).

O aplicativo implementa uma simulação física completa para o cursor do mouse:
- **Pivô Fixo no Ponto de Evento:** O hotspot de clique (o primeiro pixel / ponta afiada do cursor `(0, 0)`) permanece travado na posição exata do evento do mouse.
- **Balanço e Inércia Angular:** Ao mover o cursor em qualquer direção, o corpo do ponteiro oscila e balança de acordo com a aceleração, velocidade e arrasto aerodinâmico.
- **Mola Torsional e Amortecimento:** O cursor tende a retornar à sua inclinação de repouso natural amortecendo suavemente (sem efeito robótico).

---

## Princípios Físicos Implementados

1. **Pivô não-inercial:**
   - Origem local $(0,0)$ posicionada no ponto de evento do mouse.
   - O corpo do cursor gira em torno desse vértice.

2. **Cinemática ($\vec{v}$ e $\vec{a}$):**
   - Velocidade instantânea: $\vec{v}(t) = \frac{\Delta \vec{p}}{\Delta t}$ (com filtro passa-baixa).
   - Aceleração instantânea: $\vec{a}(t) = \frac{\Delta \vec{v}}{\Delta t}$ (com filtro passa-baixa).

3. **Dinâmica Não-Inercial (2ª Lei de Newton para Rotação):**
   - **Força Fictícia de Inércia:** $\vec{F}_{\text{inércia}} = -m \vec{a}$
   - **Arrasto com o Fluido (Ar):** $\vec{F}_{\text{drag}} = -C_{\text{drag}} \|\vec{v}\| \vec{v}$
   - **Torque Externo:** $\tau_{\text{ext}} = \vec{r}_{cm} \times (\vec{F}_{\text{inércia}} + \vec{F}_{\text{drag}})$
   - **Mola Torsional:** $\tau_{\text{spring}} = -k (\theta - \theta_{\text{rest}})$
   - **Amortecimento Viscoso:** $\tau_{\text{damping}} = -\gamma \omega$
   - **Momento de Inércia:** $I = \frac{1}{3} m L^2$
   - **Equação Diferencial:** $I \frac{d^2\theta}{dt^2} = \tau_{\text{ext}} + \tau_{\text{spring}} + \tau_{\text{damping}}$

---

## Como Executar

### Pré-requisitos
No CachyOS, as bibliotecas necessárias já estão disponíveis:
- Compilador C++ com suporte a C++20 (`gcc` / `g++`)
- `sdl3` (`pacman -S sdl3`)
- `pkg-config` e `make`

### Compilar e Rodar
No diretório do projeto:
```bash
make
make run
```
Ou usando o script launcher:
```bash
bash run.sh
```

Para abrir diretamente no modo overlay / tela cheia transparente:
```bash
bash run.sh --overlay
```

---

## Controles e Teclas de Atalho

| Tecla | Ação |
|---|---|
| **Mover o Mouse** | Move o pivô e induz torque inercial e de arrasto |
| **Clique Esquerdo** | Dispara pulso inercial tátil no ponto de evento |
| **[Space]** | Aplica impulso angular manual para testar oscilação |
| **[1] / [2]** | Diminuir / Aumentar **Massa ($m$)** |
| **[3] / [4]** | Diminuir / Aumentar **Rigidez da Mola ($k$)** |
| **[5] / [6]** | Diminuir / Aumentar **Amortecimento ($\gamma$)** |
| **[7] / [8]** | Diminuir / Aumentar **Arrasto do Ar ($C_{\text{drag}}$)** |
| **[G]** | Ligar / Desligar Gravidade no plano vertical |
| **[V]** | Mostrar / Ocultar **Vetores de Física** em tempo real |
| **[P]** | Mostrar / Ocultar **Pivô (Ponto de Evento)** no 1º pixel |
| **[T]** | Ativar / Desativar **Rastro Inercial (Motion Trail)** |
| **[H]** | Mostrar / Ocultar **Painel de Telemetria (HUD)** |
| **[+] / [-]** | Aumentar / Diminuir tamanho visual do cursor |
| **[R]** | Resetar todos os parâmetros para os valores de fábrica |
| **[F11] ou [O]** | Alternar entre Modo Janela (Playground) e Overlay |
| **[Esc] ou [Q]** | Sair do aplicativo |

---

## Vetores Visuais no Modo Debug ([V])

- **Ponto Vermelho:** O primeiro pixel / ponto de contato (pivô inviolável).
- **Ponto Azul:** Centro de Massa ($CM$) do cursor.
- **Vetor Verde ($\vec{v}$):** Velocidade do cursor.
- **Vetor Laranja ($\vec{a}$):** Aceleração do pivô.
- **Vetor Magenta ($\vec{F}_{\text{inércia}}$):** Força de inércia exercida sobre o corpo.
- **Vetor Ciano ($\vec{F}_{\text{drag}}$):** Arrasto dinâmico do ar.
- **Arco Amarelo:** Ângulo instantâneo de deflexão em relação à posição de repouso.

---

## Dica: Efeito Global no Hyprland

Se você deseja que **todo o sistema CachyOS / Hyprland** aplique esse efeito em todas as janelas nativamente no nível do compositor, você também pode usar o plugin oficial do Hyprland:
```bash
hyprpm update
hyprpm add https://github.com/VirtCode/hypr-dynamic-cursors
hyprpm enable hypr-dynamic-cursors
```
Configuração no `~/.config/hypr/hyprland.conf`:
```ini
plugin {
    dynamic-cursors {
        enabled = true
        mode = rotate
    }
}
```
